# Vessel · HITL Protocol

> HITL(Human-in-the-loop)是 Vessel 最反主流、也是最重要的差异化(见 [VISION §2.4](../product/VISION.md) / [UX_PRINCIPLES 原则 2/3](../product/UX_PRINCIPLES.md) / [ARCHITECTURE ADR-003](ARCHITECTURE.md))。本文件规定:**哪些操作必须审批 / 哪些豁免 / 面板长什么样 / diff 怎么展示 / reject 的理由去哪 / batch 模式的边界**。
> 这不是 UI 文档,是**协议文档** —— UI 将来会演化,协议不能破。

---

## 0. 设计目标

1. **默认 on,不默认 off** —— 写操作必须弹审批,Batch 是显式 opt-in 且受限
2. **审批信息三段式** —— diff 预览 + agent 理由 + validator 结果,缺一不可
3. **Reject 是资产,不是失败** —— 拒绝理由必须沉淀,供未来 agent 学习
4. **Edit-and-approve 是一等操作** —— 用户不该被迫二选一(approve / reject),必须能改完再 approve
5. **无法撤销的操作显式警告** —— `IrreversibleHint=true` 的 tool,面板强化告警,用户必须二次确认
6. **审计可追** —— 每次 HITL 决策写入 session log,可 replay

---

## 1. HITL Gate 触发规则

### 1.1 必须触发(no escape)

- Tool 的 `RequiresApproval="true"`(Tool Registry meta)
- Tool 的 `ToolCategory` 为写类(`DataTable/Write`、`Asset/Write`、`Blueprint/Write`、`Code/Write`,即使 `RequiresApproval` 没显式声明)
- Tool 的 `IrreversibleHint="true"`(即使 RequiresApproval=false,也必须人审 —— 不可撤销的读操作也算危险,如"拷贝资产到 S3")
- Validator 返回 `bBlocksApproval=true`(即使 agent 已经过一次 approve,validator 反悔也要重审)

### 1.2 豁免(不弹 HITL)

- 纯读 tool:`RequiresApproval="false"` + `ToolCategory` 中不含 "Write" 子串 + `IrreversibleHint="false"`
- Validator 自己的执行(validator 是检查者,不是被检查对象)
- Meta 类查询:`ListAssets`、`ReadAssetMetadata`、`DescribeDataTable` 等
- Plan preview(agent 还没真的执行,只是说"我准备做什么"),**但** plan 本身若有 approval 流程(§2.1),那是 Plan-level 审批,和 Tool-level 审批不同

### 1.3 可配置触发(项目级策略)

项目 `AGENTS.md` 可声明:

```markdown
## HITL Policy

always_approve:
  - tool: RenameAsset          # 即使它标为 RequiresApproval=false,本项目仍要人审
  - category: Blueprint/Read    # 本项目对蓝图读也要人审(敏感代码)

auto_approve_in_batch:
  - tool: SmartImportFBX        # 批量模式下免单步审,但 validator warning 仍拉回人审
```

**冲突规则**:`always_approve` 比 `auto_approve_in_batch` 优先。任何情况下,**不允许完全绕过 HITL 的 tool 必须在 Tool Registry 层就标 `IrreversibleHint=true`**,靠配置绕开的不安全。

---

## 2. 审批流程

### 2.1 Plan-level 审批(opt-in,v0.2+)

Agent 在 `Planning → ToolSelection` 之间,先展示 plan(要调哪些 tool、大概顺序、预计 cost)。用户可:
- Approve plan → 进入 ToolSelection
- Reject plan → Session Failed
- Edit plan → 移除某步 / 改顺序,改完后继续

**v0.1 不默认开启**。Plan-level 审批在策划看来是"又多一步",得在工业化 batch 场景下才明显价值。作为 opt-in 特性 v0.2 上线。

### 2.2 Step-level 审批(默认,v0.1 就要有)

每次 Executing 状态执行到 `RequiresApproval=true` 的 tool,阻塞 Session Machine,弹 HITL 面板:

```
┌─────────────────────────────────────────────────┐
│ Review Agent Change              [? help]  [×]  │
├─────────────────────────────────────────────────┤
│ Session: vs-2026-04-23-0001 · Step 7/100        │
│ Agent:   Designer Assistant                     │
│ Tool:    WriteDataTableRow                      │
│ Target:  /Game/DataTables/DT_NPC_Citizen        │
│ Cost so far: $0.42 / $30.00 budget              │
├─────────────────────────────────────────────────┤
│ ▼ What agent plans to do                        │
│   Upsert 20 rows into DT_NPC_Citizen            │
│                                                 │
│ ▼ Agent's reasoning                             │
│   "基于 DT_NPC_Citizen 现有 schema,生成 20 行  │
│    夜间活动 NPC..."                              │
│   [Expand raw LLM response ▾]                   │
│                                                 │
│ ▼ Diff preview                                  │
│   + NPC_Night_001  Age: 42  District: West ... │
│   + NPC_Night_002  ...                          │
│   [Show all 20 rows ▾]                          │
│                                                 │
│ ▼ Validators                                    │
│   ✓ NamingConventionValidator                   │
│   ✓ SchemaComplianceValidator                   │
│   ⚠ AgeDistributionValidator (1 warning)         │
│                                                 │
├─────────────────────────────────────────────────┤
│                                                 │
│  [Edit selected rows]  [Reject with reason]     │
│                                                 │
│                           [Approve & Execute]   │
│                                                 │
└─────────────────────────────────────────────────┘
```

**三段式内容强制**:
1. **Diff preview** —— 看会写什么
2. **Agent's reasoning** —— 看为什么这么写
3. **Validators** —— 看自动检查通没通

缺任何一段,面板不能弹(Gate 报 Internal error)。这是协议级强制,不是 UI 自由发挥。

### 2.3 无法撤销操作的强化提示

若 tool 是 `IrreversibleHint=true`,面板顶部加红色横条:

```
╔═══════════════════════════════════════════════╗
║ ⚠ 此操作无法通过 Ctrl+Z 回退                  ║
║ 请确保你有版本控制备份。                        ║
║ 二次确认框:[ ] 我已备份,继续                  ║
╚═══════════════════════════════════════════════╝
```

用户必须勾选二次确认框后,Approve 按钮才可点。**这不是可关闭的 feature**,不允许项目配置里关掉。

### 2.4 Tool 执行期间 Editor 可能冻结的提示

Tool 在 game thread 同步执行(UE 的反射 / asset / level 操作基本都要求 game thread)。遇到重 tool(批量加载大 package、完整资产 import)时,Editor UI 会暂时无响应 —— 这是 UE 引擎限制,不是 Vessel bug。

Vessel 面板处理方式:

- Approve 按下瞬间,面板自身 overlay 一个**半透明覆盖层**("Tool executing... 如 UI 冻结属正常") + 🔄 动画
- 后台用 `FSlowTask` 给 UE 自己的进度指示器喂心跳,让标题栏不显示 "Not Responding"
- Tool 超过 10s 未完成,面板右下角升起一个独立的**取消请求按钮**(软 cancel —— 标记 flag,等 tool 到下一个安全点)
- 完成后覆盖层消失,接入 JudgeReview 阶段的正常流程

**不要承诺** "不会卡" —— 这是工程诚实。承诺 "卡是正常,我们会让你知道现在在干什么"。

---

## 3. Diff 格式规范(按资产类型)

不同类型资产的 diff 展示策略不同,UI 实现可换,但语义必须保留下面这些字段。

### 3.1 DataTable

```
[Row: NPC_Night_001]  (NEW / MODIFIED / DELETED)
  Age:       42                           ← 新值
  District:  West                          ← 新值
  Backstory: "在这座城市值夜班十二年..."     ← 新值
  (... other 27 columns collapsed ...)
```

**MODIFIED 行** 并排展示 before / after,逐列高亮变化。**DELETED 行** 整行标红划线。

### 3.2 Asset (UAsset)

```
[Asset: /Game/Characters/SK_NPC_Civilian_A]  (NEW / MODIFIED / DELETED)
  Class:       SkeletalMesh
  File size:   8.2 MB (↑ 1.3 MB)
  LOD count:   3 → 4
  Material[0]: M_Skin → M_Skin_V2
```

**只展示 metadata-level 变化**,不展示 mesh/texture 内容本身 —— 避免 diff 爆炸。若用户想看具体像素/顶点变化,点"Open in Editor"。

### 3.3 Blueprint

Vessel 面板**不**内嵌完整 Blueprint 图形 diff —— `SBlueprintDiff` 深度耦合 Blueprint Editor,剥离嵌入的工程量够单独做一个项目。

v0.1 的方案是**结构化摘要 + 跳转**:

```
[Blueprint: BP_NPC_Citizen]  (MODIFIED)
  Summary:
    + New function:    HandleDayNightChange(bIsNight)
    + Modified:        BeginPlay  (3 nodes added, 1 removed)
    + Unchanged:       Tick, 其它 12 个函数

  [Open in Blueprint Editor (diff view) ⮕]
```

用户点 `Open in Blueprint Editor` 按钮,Vessel 把改动前后的临时 copy 喂给 UE 原生 `BlueprintDiffTool` 独立窗口打开,用户在原生工具里审完再回到 Vessel 面板点 Approve / Reject。

**tradeoff**:用户需跳一次窗口。交换的是 v0.1 不陷入 3 周的 Slate 魔改,并避免当 Epic 升级 `BlueprintDiffTool` 时 Vessel 跟着炸。v1.0+ 若社区需求强烈,再考虑内嵌。

### 3.4 Code (C++)

```
[File: Source/Foo.cpp]  (MODIFIED)
  @@ -12,6 +12,10 @@
   void FFoo::Bar() {
  +  if (!IsValid()) return;
     ...
```

标准 unified diff,用 monospace 字体展示。

### 3.5 Config (INI / JSON / YAML)

标准 unified diff + 可选 parsed view 侧栏(显示 key path)。

---

## 4. Reject Reason 协议

### 4.1 Reject 必须带 reason

HITL 面板的 Reject 按钮:
- 空 reason **不让点**(按钮 disabled 直到输入框有字)
- Reason 字符数下限 5(避免用户填 "bad" 这种无意义字符串)
- 提供快捷选项(常见拒绝理由一键选,比如 "naming convention violation"、"incorrect age range"、"duplicate with existing"),但**仍可在此基础上补充自由文本**

### 4.2 Reason 的沉淀路径

Reject reason 会被**自动写入** `AGENTS.md` 的 `## Known Rejections` 段,结构化格式:

```markdown
## Known Rejections

### 2026-04-23T10:14:30 · tool=WriteDataTableRow · target=DT_NPC_Citizen
**reason**: Age 超出项目规定范围 30–55。NPC_Night_001 被生成为 62 岁。
**session**: vs-2026-04-23-0001, step 7
**rejecter**: user

### 2026-04-23T11:02:11 · tool=GenerateUFunction · target=BP_Test
**reason**: 禁止在 Gameplay 类调用 Engine 子系统。Agent 生成了 `UGameplayStatics::GetPlayerController`,这违反项目架构规则。
**session**: vs-2026-04-23-0002, step 3
**rejecter**: user
```

### 4.3 下一次 Planner 自动消费

Planner 构造 prompt 时(见 [SESSION_MACHINE §3.1](SESSION_MACHINE.md)),`AGENTS.md` 整个文件(含 `## Known Rejections`)会被注入 system prompt,agent 自然看到历史拒绝,主动规避。

**Reject 实际构成了项目的"约束语料"**。用户拒绝越多,项目 agent 越贴合项目规范。

### 4.4 Reason 的生命周期

- **写入**:每次 Reject 追加(不修改旧条目)
- **删除**:默认永久保留。如需清理,项目管理员手动编辑 `AGENTS.md`(不提供 UI 删除按钮 —— 这是故意的,防止用户一冲动删掉有价值的约束)
- **归档**:项目根 `Saved/VesselRejectionArchive/` 每月自动归档一份快照(供审计)

---

## 5. Edit-and-Approve 流程

用户看 diff 时经常遇到:"20 行里有 3 行不对,别的都好。"强迫他 reject-and-regenerate 全部 20 行是反人性的。

**Edit-and-Approve** 流程:

1. 用户在 diff 里**点具体一行的 [Edit] 按钮**
2. 弹出输入框,用户写改动要求(例:"把 Age 改成 45")
3. Agent 收到 "edit directive",重新生成**仅这一行**,保留其他 19 行不变
4. Diff 面板局部刷新这一行,其他 19 行不重跑
5. 用户决定再 Edit / 再 Reject 该行 / Approve All

**实现要点**:
- "Edit 指令" 作为一次独立的 tool call 进入 Session Machine(占 1 step)
- Edit 产生的 artifact 与原 artifact 共享 session 上下文,不重置
- 若 Edit 后 validator 新增 warning,HITL 面板自动把该行标黄重新 await

### 5.1 Edit 失败怎么办

如果 Agent 无法满足 Edit 要求(例如用户要求的值超出 schema 约束),Agent 返回"无法满足,理由:…",HITL 面板展示此信息,用户决定 Reject 该行或继续 Approve。

---

## 6. Batch 模式边界

### 6.1 进入 Batch 的必要条件

**所有**满足:
- Tool `BatchEligible="true"`(Registry meta)
- 项目 `AGENTS.md` 的 `auto_approve_in_batch` 段包含该 tool
- Agent 模板 `session: batch_mode: true`
- Tool 的 **所有** 相关 validator 实现(没有 sensor gap)
- **非** `IrreversibleHint=true`(不可撤销 tool 永远不能 batch)

### 6.2 Batch 中仍强制拉回单步人审的条件

即使进入 batch,遇到下面任何情况,**该条**必须从 batch 退出,走正常 HITL:

- Validator 返回 `Warning`(任何 warning,不区分严重度 —— "warning 是项目说出来的怀疑,必须人听到")
- Validator 返回 `Error`(显然)
- Tool 运行时 `ErrorCode != Ok`
- Consecutive-same-error 计数在 batch 内达到阈值(独立于 Session budget)
- 单条操作耗时超过该 tool 的 p99 基线(可能是异常,需要人看)

### 6.3 Batch 结束后的总结面板

Batch 跑完弹汇总:

```
Batch: SmartImportFBX
  Total items: 200
  ✓ Auto-approved: 182
  ⚠ Pulled back to human: 15   [Review Each ▾]
  ✗ Failed: 3                   [Inspect ▾]

Total cost: $4.23 · wall time: 32 min
```

**`Auto-approved` 那 182 条也要逐条可审计** —— 点击展开能看到每条的 diff 摘要和 validator 结果。不允许 "agent 做完就忘"。

### 6.4 紧急停止

Batch 运行期,面板始终显示大红色 [Pause] / [Abort] 按钮:
- Pause:保留已完成,session 转入 await 状态,用户可 inspect 后决定继续/中断
- Abort:立即停,已完成的**保留**(不会回滚),未完成的不开始。session → Failed

---

## 7. 审计轨迹

每次 HITL 决策写入 session 的 JSONL 日志,schema:

```json
{
  "type": "HITLDecision",
  "timestamp": "2026-04-23T10:14:30Z",
  "session_id": "vs-2026-04-23-0001",
  "step_id": 7,
  "tool": "WriteDataTableRow",
  "target": "/Game/DataTables/DT_NPC_Citizen",
  "diff_hash": "sha256:...",
  "validator_summary": {
    "errors": 0,
    "warnings": 1,
    "warning_codes": ["AgeDistribution.OutOfRange"]
  },
  "agent_reasoning_excerpt": "...",
  "decision": "Approved",
  "decision_timestamp": "2026-04-23T10:14:58Z",
  "decider": "user:<uid>",
  "reject_reason": null,
  "edits_before_approve": 1
}
```

字段 `edits_before_approve` 能帮后续分析:某个 tool 被编辑次数多 → prompt 质量有问题 → 考虑改 tool description / 加 schema 约束。

---

## 8. 反模式(看到立刻警觉)

| 反模式 | 为什么危险 |
|---|---|
| 为了"体验丝滑"默认把 approval 关掉 | 违反哲学 §2.4,等于把 Vessel 降级为 UnrealMCP |
| 把 Reject 按钮改成"再试一次" | Reject reason 就丢了,失去"拒绝即资产"的价值 |
| 允许 skip validator 的 warning 进 batch | 批量场景下 warning 淹没,真实问题被漏掉 |
| 把 IrreversibleHint 的警告改成可关闭 | 第一次删错文件就是灾难,一次性的惨痛教训 |
| 在 batch 模式里不展示 auto-approved 条目 | 用户没法审计 "你替我做了什么" |
| 让 AGENTS.md 的 Reject 历史被自动删除 | 约束语料的价值在于累积,自动删 = 白积累 |

**这些反模式若出现在 PR 里,默认 block,需要 VISION / UX_PRINCIPLES / 本文件中至少一处先被修改并给出理由,才能讨论是否放行。**

---

## 9. 未来扩展(v0.1 不做)

- **Multi-approver / role-based HITL** —— 大项目里不同操作可能要不同 role 批(策划 lead / TA lead)。v1.0+ 研究
- **异步审批** —— 把 pending approval 推到 Slack / 飞书,用户不在 Editor 也能批。v0.2 探索
- **Reject reason 的自动去重 / 聚类** —— 当 `Known Rejections` 膨胀到千条级,需要去重和聚类压缩。v0.3+

---

*Last reviewed: 2026-04-23 · 本文件的任何改动都视为哲学级变更,需要 PR + 讨论,不能静默调整。*
