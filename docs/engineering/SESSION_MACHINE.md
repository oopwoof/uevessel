# Vessel · Session Machine

> Session Machine 是 Vessel 的**编排发动机** —— 它决定"一次 agent 会话"怎么走。一切 Planner / Executor / Judge 循环、Context 传递、Memory 写入、Cost 追踪都在这里落地。
> 阅读前提:[ARCHITECTURE.md](ARCHITECTURE.md)(了解三层位置) + [TOOL_REGISTRY.md](TOOL_REGISTRY.md)(了解 tool 调用生命周期)。

---

## 0. 设计目标

1. **显式,不隐式** —— 每一次 agent 决策、tool 调用、judge 评审都有明确的状态节点。没有"LLM 自己循环到不知道在干啥"
2. **可恢复,可 replay** —— session 崩了能从 JSONL 日志恢复;事后 debug 能按步回放
3. **有 budget,不跑飞** —— 步数、成本、时间三层硬顶,任何一个超都终止
4. **HITL 自然插入** —— 审批不是 "session 之外" 的事件,是状态机的一个 await 点
5. **无黑魔法 LLM 链路** —— 短期 memory 用**确定性的 token bound**,不调 LLM 做总结

---

## 1. 状态图

```
                       ┌────────┐
                       │  Idle  │  (新 session)
                       └───┬────┘
                           │ user input
                           ▼
                   ┌───────────────┐
         ┌─────────│   Planning    │◀──┐
         │         └───────┬───────┘   │ Revise / NewStep
         │ LLM ok          │           │
         │                 ▼           │
         │       ┌───────────────────┐ │
         │       │  ToolSelection    │─┘
         │       └─────────┬─────────┘
  plan   │ args validated  │
  invalid│                 ▼
         │       ┌───────────────────┐
         │       │   Executing       │◀─── HITL await
         │       │ (Tool Registry    │      (if RequiresApproval)
         │       │  invoke pipeline) │
         │       └─────────┬─────────┘
         │ tool result     │
         │                 ▼
         │       ┌───────────────────┐
         │       │   JudgeReview     │
         │       └─┬───────┬───────┬─┘
         │         │       │       │
         │ Approve │  Revise│  Reject (not fixable)
         │         │       │       │
         ▼         ▼       │       ▼
   ┌──────────┐ ┌─────────┐│ ┌──────────┐
   │  Failed  │ │ NextStep││ │  Failed  │
   └──────────┘ └────┬────┘│ └──────────┘
                     │     │
          plan done  │     │ plan has more
                     │     └──▶ back to ToolSelection
                     ▼
                 ┌───────┐
                 │ Done  │
                 └───────┘
```

**终态**:`Done`、`Failed`(都写 session log、flush memory、可触发 UI 回调)。

---

## 2. 每个状态详解

表格里的 **Timeout** 和 **Retry** 是默认值,项目级配置可覆盖(见 §6)。

### 2.1 Idle
| 维度 | 内容 |
|---|---|
| 进入条件 | Session 被创建 |
| 退出条件 | 收到 `UserInput` 事件 |
| Timeout | 无(session 可空转) |
| 可触发事件 | `UserInput` → Planning,`UserAbort` → terminate |

### 2.2 Planning
| 维度 | 内容 |
|---|---|
| 进入条件 | 从 Idle 由 UserInput 触发,或从 NextStep / Revise 返回 |
| 动作 | 构造 Planning prompt(见 §3.1)→ 调 LLM → 解析 tool plan |
| 退出条件 | LLM 返回合法的 tool call list → ToolSelection;返回 "refuse" 或无可用 tool → Failed |
| Timeout | LLM 调用 60s,超时 retry 1 次 |
| Retry | LLM 无效响应 retry 2 次,然后 Failed |

### 2.3 ToolSelection
| 维度 | 内容 |
|---|---|
| 进入条件 | Planning / Revise / NextStep 抵达 |
| 动作 | 从 plan 取下一步的 tool call,对参数做 Registry 层 validation(见 [TOOL_REGISTRY.md §4](TOOL_REGISTRY.md) 步骤 2) |
| 退出条件 | Validation 通过 → Executing;validation 失败且 Plan 无 fallback → Planning(重新规划);已无下一步 → Done |
| Timeout | 无(纯本地操作) |

### 2.4 Executing
| 维度 | 内容 |
|---|---|
| 进入条件 | ToolSelection 交付可执行 tool call |
| 动作 | 按 [TOOL_REGISTRY §4](TOOL_REGISTRY.md) 的 6 步 pipeline 调用。若 tool 的 `RequiresApproval="true"`,**在步骤 3 阻塞在 HITL Gate**,等用户响应 |
| 退出条件 | Tool 返回 `FVesselResult` → JudgeReview;tool ErrorCode=`UserRejected` → Revise(Reject reason 进 prompt) |
| Timeout | 默认 30s(可 per-tool 覆盖),超时走 `ErrorCode=Timeout` 路径 |
| Retry | 按 [TOOL_REGISTRY §5.1](TOOL_REGISTRY.md) 的 ErrorCode 规范 |

### 2.5 JudgeReview
| 维度 | 内容 |
|---|---|
| 进入条件 | Executing 返回 tool 结果(含 validator 结果) |
| 动作 | 两种 judge: <br>**Computational**:Validator `bBlocksApproval=true` → Reject;否则继续 <br>**Inferential**:Judge prompt 调 LLM(可用与 Planner 不同的模型),根据 rubric 打分,输出 Approve/Revise/Reject + reason |
| 退出条件 | Approve → NextStep;Revise(带 fix 建议)→ Planning;Reject → Failed |
| Timeout | Judge LLM 调用 30s |
| Retry | Judge LLM 失败 retry 1 次,仍失败则**保守默认 Reject**(宁错杀不放过)|

### 2.6 NextStep
| 维度 | 内容 |
|---|---|
| 进入条件 | JudgeReview Approve |
| 动作 | 检查 plan 是否还有未执行步骤 |
| 退出条件 | 有 → ToolSelection;无 → Done |

### 2.7 Done / Failed
| 维度 | 内容 |
|---|---|
| 动作 | Flush session memory 到 JSONL,触发 UI 回调,清理 cost tracker |
| 可否重启 | 同 session_id 不可重启;用户可基于上次 session 的 summary 开新 session |

---

## 3. Context 传递协议

"上一步的输出如何进下一步的 prompt" 是 agent 系统最容易变成黑箱的地方。Vessel 明确规定:

### 3.1 Planning prompt 的组成

```
[System]
You are <agent_template.role>. Follow <agent_template.behavior>.

[Guides]
## AGENTS.md (project-level)
<...contents...>

## SKILL.md (agent-level, if exists)
<...contents...>

## Dynamic context
- Selected asset(s): <...>
- Current open level: <...>
- Available tools (filtered by policy):
  <tool schema list, JSON>

[Memory]
<last N turns, bounded by TOKENS_SHORT_TERM_LIMIT>

[User]
<user_input>

[Prior step summaries, if revise/next-step]
- Step 1: <tool> → <result summary>
- Step 2: <tool> → <result summary>
```

### 3.2 Executor 输入

不调 LLM —— Executor 只接 Planning 输出的结构化 tool call(name + args JSON)。Executor 是**确定性模块**,这是刻意设计。

### 3.3 Judge prompt 的组成

```
[System]
You are a review judge for agent tool execution.
Rubric: <agent_template.judge_rubric>

[Plan context]
Full plan: <...>
Current step: <tool_name>(args)
Expected outcome: <from plan>

[Actual result]
Tool returned: <result JSON>
Validator messages:
  - error: <...>
  - warning: <...>

[Task]
Decide: APPROVE / REVISE / REJECT
If REVISE, provide fix suggestion.
If REJECT, provide explicit reason.
```

Judge 的 LLM call 默认用和 Planner **相同的模型**,但项目可配置 `JudgeModel=<other>`(典型配置:Planner 用 Opus,Judge 用 Haiku,降本)。

### 3.4 禁止事项

- ❌ **不把原始 LLM 回复直接塞下一步 prompt** —— 只传结构化摘要。避免幻觉滚雪球
- ❌ **不用 LLM 总结上一步作为压缩策略** —— 用 "最后 N 轮 + 关键 artifact 指针" 的确定性 bound
- ❌ **不让 Planner 看 AGENTS.md 里的 Tool Policy** —— 不该让 agent 知道"哪些 tool 被禁",避免它请求"帮我开启那个 tool"

---

## 4. Memory 读写点

| 时机 | 短期(session context) | 长期(JSONL 日志) |
|---|---|---|
| 进入 Planning | 读 last N turns | — |
| 离开 Planning | 追加 plan artifact 指针 | 写 `PlanningRecord` 条目 |
| 进入 Executing | — | — |
| 离开 Executing | 追加 tool result summary | 写 `ToolInvocation` 条目 |
| 进入 JudgeReview | 读 plan artifact + result summary | — |
| 离开 JudgeReview | 追加 judge verdict | 写 `JudgeVerdict` 条目 |
| HITL Reject | — | 写 `RejectReason` + 追加到 `AGENTS.md` |
| Done / Failed | **清空** session context | 写 `SessionSummary` 收尾 + close file |

**短期 memory 的 bound 策略**:
- 维护 `FVesselSessionContext` 结构,最多持有 `TOKENS_SHORT_TERM_LIMIT`(默认 20000 tokens)
- 超过时**从最旧的条目开始淘汰**,但保留:
  - AGENTS.md(不淘汰)
  - 初始 user input(不淘汰)
  - 最后 3 个 step 的完整详情(不淘汰)
  - 其他 step 降级为 "summary pointer"(写文件系统,prompt 里放 path + 一句话)

**Replay 协议**:
- 给定 `Saved/AgentSessions/vs-XXXX.jsonl`,可重建 session state 到任意 step
- `vessel replay vs-XXXX --stop-at 5` 在第 5 步停,方便 debug 单步

---

## 5. 终止条件(Budget)

任何一个触发立即终止(状态→`Failed`)。默认值按**工业化研发管线**场景设计 —— 批量资产导入、批量 NPC 生成、跨多文件代码 review 都要跑得动。交互式小任务可以 per-agent 下调,不会有副作用。

| Budget | 默认值 | 配置 key | 超过行为 |
|---|---|---|---|
| Max Steps | **100** | `VesselSession.MaxSteps` | Failed + 写 "step budget exceeded" |
| Max Cost (USD) | **30.00** | `VesselSession.MaxCostUSD` | Failed + 写当前累计成本 |
| Wall Time | **60 min** | `VesselSession.MaxWallTimeSec` | Failed + 保留已完成步骤 |
| Repeated Error | same (tool, ErrorCode) × **5** | `VesselSession.RepeatErrorLimit` | Circuit-break → Failed |
| Consecutive Revise | **5** 次连续 Revise 无进展 | `VesselSession.MaxConsecutiveRevise` | Failed(防止死循环修改)|

**Batch 操作计为 1 step**:若 tool 是 batch 入口(例如 `SmartImportFBX` 对 200 个文件的一次调用),无论内部处理多少条,在 Session Machine 视角是**一次**调用 = 占用 1 step。这是让 MaxSteps=100 能覆盖批量场景的关键设计 —— agent 靠 tool 组合表达"跑 200 条",不靠 Session Machine 自己迭代 200 次。

**"无进展"的定义**:连续 Revise 产生的 tool call 的 `(name, args_hash)` 全部和之前出现过的相同 → 认为 agent 在原地打转。

**场景参考**(给 agent 作者调 budget 时参考):

| 场景 | 建议 MaxSteps | 建议 MaxCost | 建议 Wall |
|---|---|---|---|
| 策划填 20–50 行 DataTable(交互式) | 30 | $5 | 15 min |
| TA 批量导 200 资产(batch + validator) | 80 | $15 | 45 min |
| Code review 跨 50 文件(每文件独立判断) | 200 | $60 | 2 h |
| 跨资产重构(改字段 + 连带更新 100+ 处) | 300 | $100 | 3 h |

超过上表的场景,agent 作者需要显式在 `agents/<name>.yaml` 的 `session:` 段声明,并在 code review 中讨论 —— 不是随便拍脑袋放大。

---

## 6. 配置来源与优先级

Session Machine 的 budget / retry / timeout 参数从三处合并,**后者覆盖前者**:

| 层级 | 位置 | 典型用途 |
|---|---|---|
| 1. 插件默认 | `VesselCore` C++ 常量 | 保证任何项目开箱即用有合理值 |
| 2. 项目级 | `Config/DefaultVessel.ini` | 项目管理员统一管控(提高 budget、收紧 timeout) |
| 3. Agent 模板 | `agents/<name>.yaml` 的 `session:` 段 | 特定 agent 可覆盖(如 Code Review agent 要更长 wall time) |

**不支持**:session 启动后动态改 budget。若要调,重启 session。理由:budget 半路改动会让 cost 追踪语义混乱。

**读取路径**:Session 初始化时 resolve 一次,把 effective values 打进 session log,便于事后审计"为什么这个 session 跑了那么多步 / 花了那么多钱"。

---

## 7. 失败模式与诊断

### 7.1 典型失败路径

| 症状 | 可能原因 | 诊断路径 |
|---|---|---|
| Planning → Failed(repeatedly) | Tool policy 过严,无可用 tool 满足需求 | 查 `Saved/AgentSessions/*.jsonl` 的 `PlanningRecord.allowed_tools` 列表 |
| Executing → Timeout 反复 | Tool 实现有阻塞 Editor 主线程的操作 | Console `Vessel.Trace.Threading` 看 tool 占用线程 |
| JudgeReview → Reject 占比高 | Planner 和 Judge 的模型不一致,Judge 的 rubric 太严 | 考虑让 Judge 和 Planner 使用同级模型,或放宽 rubric |
| Session 突然静默不动 | HITL Gate 阻塞等用户但 UI 隐藏了 | 面板右上角应有未响应计数器;v0.2 加强通知 |

### 7.2 诊断工具

- `VesselSession.Dump <session_id>` —— 打印完整状态机路径
- `VesselSession.Replay <session_id>` —— 基于日志回放
- `VesselSession.Trace` —— 开实时事件 log(高开销,仅 dev)
- Tool 调用层的 trace 见 [TOOL_REGISTRY.md §8](TOOL_REGISTRY.md)

---

## 8. 实现提示(给贡献者)

### 8.1 不要把 Session Machine 做成 `UObject`

`UObject` 参与 GC,状态机的 tick 频率会和 GC pauses 冲突。用**普通 C++ class** + 由 Subsystem 持有。

### 8.2 不要把 LLM 调用做成同步

LLM 调用必然经 HTTP,延迟 1–60s 不等。用 `TFuture` + UE 的 task graph。阻塞 game thread = Editor 卡死。

### 8.3 HITL await 的实现陷阱

HITL 阻塞 session,但不能阻塞 UE 线程。实现模式:
- Session Machine 是**状态对象,不是线程**
- 到 HITL 等待点时,记录 `PendingUserDecision`,**从 tick 队列退出**
- 用户点 Approve/Reject → 触发事件 → Session Machine 被重新调度,继续 tick

这个模式类似 UE 的 Latent Action,但由 Vessel 自己管理,不依赖 Blueprint action infrastructure。

### 8.4 Session 不要跨进程共享

每个 UE Editor 进程自己一份 Session Machine。Commandlet 跑批时每个 commandlet 实例自己的 session。**不做** shared / networked session —— 这是"不是魔法"哲学的延伸。

### 8.5 JSONL 日志的 Windows 文件锁

`FFileHelper::SaveStringToFile` 的 Append 模式在 Windows 下每次打开/关闭文件,若**用户同时在 VSCode / tail 工具开着日志,或被杀软扫描**,追加写会因文件锁失败、极端情况触发 UE Fatal Error。

**实现要求**:

- 用 `IPlatformFile::Get().OpenWrite(Path, /*bAppend=*/true, /*bAllowRead=*/true)` 拿到持久 `IFileHandle`,**带** `FILE_SHARE_READ | FILE_SHARE_WRITE` flag(Windows 通过 Win32 API 直接设置,跨平台由 UE wrapper 默认启用共享读)
- Session 生命周期内**复用同一 handle**,不要每写一条 flush + close + reopen
- 写入走**异步队列**(`TQueue<FString>` + async task),game thread 只丢消息不等 IO
- 崩溃场景的**最小保证**:每条 JSON line 写完立刻 flush,日志可能缺最后一条,但**不会**出现半行损坏

### 8.6 Editor 关闭时优雅终止

**必须**hook `FEditorDelegates::OnEditorClose` 和 `FCoreDelegates::OnPreExit`:

- 迭代所有 active session,对每个 session:
  - 取消 in-flight LLM HTTP request(保存 `FHttpRequestPtr`,调 `CancelRequest()`)
  - 若在 Executing 状态,软 cancel tool(设 flag,tool 下一个安全点退出)
  - 写 `SessionSummary { outcome: "aborted_on_editor_close" }` 到日志
  - 关闭 JSONL handle
- 若有 pending HITL approval 阻塞,直接判 `UserRejected` 终止(不再等待)

**反模式**:在 editor close 时强杀 HTTP 线程 —— UE 在 `StaticShutdownAfterError` 阶段强杀会常见 crash,而且会在用户的 crash reporter 里留下 false positive。

- **Sub-session spawning** —— Session Machine 内部 `SubSessionSpawn` hook,目前 no-op;v1.0+ 考虑多 agent 时打开
- **Checkpoint/resume mid-session** —— 当前只能 Done/Failed 后 replay,无法从中途恢复。v0.3+ 研究
- **Streaming HITL** —— 长工具执行期间向 HITL 面板推送进度,目前只有开始/结束两点

---

*Last reviewed: 2026-04-23 · 状态集合冻结到 v1.0 —— 如需增减状态,必须先改本文件并给出升级路径。*
