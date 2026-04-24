# Vessel · Architecture

> 本文件描述 Vessel 的技术骨架。阅读前提是先读过 [VISION.md](../product/VISION.md) 里的四条哲学原则 —— 架构决策是那四条原则在代码里的直接兑现。
> 本文件对**新贡献者和技术审阅者**,应当足以在 10 分钟内建立 Vessel 的整体心智模型。

---

## 0. 架构目标

在具体画图之前,先把**约束**写出来 —— 所有后续设计取舍都源自这些约束。

| 约束 | 由来 |
|---|---|
| **Engine-native** —— 不依赖外部进程 / 外部 server | VISION §2.1 |
| **反射驱动的 tool 注册** —— `UFUNCTION` 一次声明,所有 surface 自动消费 | VISION §2.2 |
| **Guides + Sensors 一等公民** —— 可靠性是架构,不是补丁 | VISION §2.3 |
| **HITL 默认** —— 所有写操作必然弹审批,Batch 是 opt-in | VISION §2.4 / UX_PRINCIPLES 原则 2 |
| **可撤销** —— 写操作经 `FScopedTransaction`,边界显式告知 | UX_PRINCIPLES 原则 4 |
| **Provider 可插拔** —— 企业 IT 合规要求支持 Custom Endpoint / Azure / 自建 proxy,不绑死公共 API | ROADMAP v0.1 要求 |
| **无巨型依赖** —— 不引 LangChain / 向量数据库等会显著膨胀 UE 插件体积的库 | 内部决策(见 ADR-005) |

---

## 1. 三层总览

Vessel 分三层,**严格单向依赖**(上层知下层,下层不知上层)。

```
┌──────────────────────────────────────────────────────────────┐
│  Layer 3 · Surface  (用户触点 / 集成入口)                    │
│                                                                │
│  ┌──────────────────┐  ┌────────────┐  ┌─────────────────┐   │
│  │ Editor Utility   │  │ Commandlet │  │ MCP Server      │   │
│  │ Widget           │  │ (CI)       │  │ (external LLM)  │   │
│  │ [主 surface]     │  │ (v0.2)     │  │ (v0.2)          │   │
│  └──────────────────┘  └────────────┘  └─────────────────┘   │
│           │                  │                  │              │
└───────────┼──────────────────┼──────────────────┼──────────────┘
            │                  │                  │
            ▼                  ▼                  ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 2 · Orchestrator  (编排与策略)                        │
│                                                                │
│  ┌─────────────────┐  ┌─────────────┐  ┌──────────────────┐  │
│  │ Session Machine │  │ Harness     │  │ Memory           │  │
│  │ (Planner/Exec/  │  │ (guides +   │  │ (short-term ctx  │  │
│  │  Judge FSM)     │  │  sensors    │  │  + long-term     │  │
│  │                 │  │  injection) │  │  JSONL log)      │  │
│  └─────────────────┘  └─────────────┘  └──────────────────┘  │
│                                                                │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ HITL Gate  (approval flow, reject-reason sink)          │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
            │                  │                  │
            ▼                  ▼                  ▼
┌──────────────────────────────────────────────────────────────┐
│  Layer 1 · Core  (引擎原生基础设施)                          │
│                                                                │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌─────┐  │
│  │ Tool         │ │ Transaction  │ │ Validator    │ │ LLM │  │
│  │ Registry     │ │ Wrapper      │ │ Hooks        │ │ Adp.│  │
│  │ (reflection  │ │ (FScopedTx)  │ │ (ValidatorBase)│ │     │  │
│  │  scanner)    │ │              │ │              │ │     │  │
│  └──────────────┘ └──────────────┘ └──────────────┘ └─────┘  │
└──────────────────────────────────────────────────────────────┘
```

**核心不变式**:Layer 1 Core 模块之间**也是平级**,不能互相依赖 —— 它们各自暴露接口给 Layer 2 使用,但 Tool Registry 不知道 LLM Adapter 存在,反之亦然。

---

## 2. Layer 1 · Core(引擎原生)

C++ 实现,直接编译进 UE 插件。**不引入任何 Python / 外部进程 / 非 UE 的第三方框架**(例外:HTTP 客户端、JSON 解析等 UE 已内置的基础库)。

### 2.1 Tool Registry

**职责**:启动 + Hot Reload 时扫描所有带 `AgentTool="true"` meta 的 `UFUNCTION`,生成统一的 JSON schema 表,供 Orchestrator 和 Surface 查询。

**输入**:UE 反射表(`UFunction` 集合)
**输出**:`TMap<FName, FVesselToolSchema>` + 可序列化 JSON

**关键设计**:
- 识别契约:`meta=(AgentTool="true", ToolCategory=..., RequiresApproval=..., ToolDescription=...)`
- 运行期动态扫描,不需要代码生成
- 提供 `FVesselToolRegistry::Get()` singleton 接口
- 详细规范见 [TOOL_REGISTRY.md](TOOL_REGISTRY.md)

**为什么重要**:这是 Vessel 架构里**差异化含量最高**的一块。没有它,Vessel 就退化成普通 agent 框架。

### 2.2 Transaction Wrapper

**职责**:为所有标 `RequiresApproval="true"` 或 `ToolCategory="Write"` 的 tool,在执行边界自动包 `FScopedTransaction`。

**接口**:`FVesselTransactionScope`(RAII),作用域展开即开事务,析构时提交。

**边界(诚实版)**:
- ✅ 覆盖 `UPROPERTY` + `Modify()` 调用链的 UObject 状态变化
- ❌ **不覆盖**:文件系统操作(move / rename / delete)、外部 HTTP 副作用、非 Transactional 第三方资产、Editor 本身状态
- 对"不可撤销"类 tool,Registry 自动打上 `IrreversibleHint` flag,HITL 面板在执行前**强制弹** "此改动 Ctrl+Z 无法回退" 提示

### 2.3 Validator Hooks

**职责**:tool 执行后自动跑所有相关的 `UEditorValidatorBase` 子类,结果回传 Orchestrator 供 Judge 消费。

**接口**:
```cpp
struct FVesselValidationResult {
    TArray<FVesselValidationMessage> Errors;
    TArray<FVesselValidationMessage> Warnings;
    bool bBlocksApproval;
};
```

**关键决策**:
- Validator 是**项目可扩展**的 —— 用户写自己的 `UEditorValidatorBase` 子类,Vessel 自动挑选
- v0.1 内置最小集:`NamingConvention`、`SchemaCompliance`、`DuplicateKey`
- Sensors(Fowler 分类学)的**computational** 一半由 Validator 承担;**inferential** 一半由 Orchestrator 的 Judge 承担

### 2.4 LLM Adapter

**职责**:统一的 `ILlmProvider` 接口,屏蔽不同 LLM 厂商差异。

**接口骨架**:
```cpp
class ILlmProvider {
public:
    virtual TFuture<FLlmResponse> SendAsync(
        const FLlmRequest& Request,
        const FLlmCallOptions& Options) = 0;
    virtual FString GetProviderId() const = 0;
    virtual bool SupportsToolCalling() const = 0;
};
```

**v0.1 必须支持**:
- Anthropic provider(唯一内置 provider)
- **Custom API Endpoint / Custom Headers** —— 允许指向 Azure OpenAI、自建企业网关、proxy。这不是可选特性,是**合规底线**,没有它整个"面向商业团队"的定位就是伪命题
- `AllowHttp=true` 仅在 Endpoint 以 `http://localhost` / `http://127.0.0.1` 开头时生效 —— 这是 Core 层 C++ 硬编码检查,不可通过配置绕过
- Streaming + retry + timeout + cost tracking
- **`FVesselJsonSanitizer`** —— LLM(尤其 Claude / Qwen 系)经常返回被 markdown code fence(` ```json ... ``` `)包裹的 JSON,或在 JSON 前后加散文。所有 provider 在 dispatch 前**必须**过这个 sanitizer:剥 fence → 取首个平衡 `{...}` → 仍失败则返回可操作的 LLM-readable 错误。详见 [TOOL_REGISTRY §5.5](TOOL_REGISTRY.md)
- **内置 `FVesselMockProvider`** —— 接受预置 fixture(JSON 文件 / 内存表),按 tool call / prompt hash 命中返回固定 response。**CI 默认用它**,零真实 API 调用。Mock provider 不是可选特性,是 v0.1 必交付(否则 automation test 会既 flaky 又烧钱)

**v0.2+**:OpenAI / Qwen / MiniMax provider。**v0.3+**:本地 llama.cpp / ollama fallback provider。

---

## 3. Layer 2 · Orchestrator(编排)

C++ + 少量可选 Python(通过 UE Python API,非必需)。

### 3.1 Session Machine

**职责**:显式 FSM,驱动单次 agent 会话的生命周期。

状态集合:
```
Idle → Planning → ToolSelection → Executing → JudgeReview
    → {Approve | Reject | Revise} → NextStep | Done | Failed
```

每个状态有明确的 timeout、retry policy、circuit breaker。详见 [SESSION_MACHINE.md](SESSION_MACHINE.md)。

### 3.2 Harness(Guides + Sensors 注入)

**Guides 来源**(静态 + 动态):
- 项目根 `AGENTS.md`(Hashimoto 模式)
- 每个 agent 的 `SKILL.md`(可选)
- 动态 context:当前选中资产、打开关卡、最近的 session log 摘要

**Sensors 触发点**:
- Computational(Validator Hooks 回流)—— 强硬拦截
- Inferential(LLM-as-Judge with rubric)—— 软性打分,结果进入 Judge 状态决策

### 3.3 Memory

**短期**:单 session 内 context 压缩(策略:保留最后 N 轮 + 关键 artifact 指针)。**不做语义压缩,不调 LLM 做总结** —— LLM 总结会导致 session 不可 replay。

**长期**:写 `Saved/AgentSessions/*.jsonl`,一次 session 一个文件,结构化可 grep。v0.2 起用 SQLite FTS5 做全文检索。**不用向量数据库**(见 ADR-005)。

### 3.4 HITL Gate

**职责**:所有写操作的审批收口。

**流程**:
1. Tool 执行请求 → 查 Tool Registry 的 `RequiresApproval` 位
2. 若为 true,阻塞 Session Machine,推送面板提醒
3. 用户 Approve / Reject with reason / Edit and Approve
4. Approve → 解除阻塞;Reject → 写 reason 到 `AGENTS.md` 的 `## Known Rejections` 段;Edit → 改参数后走新一轮 executor
5. Batch 模式下,符合"幂等 + validator 全覆盖"条件时,自动 approve 路径打开,但 validator warning 依然拉回单步人审

详细协议见 [HITL_PROTOCOL.md](HITL_PROTOCOL.md)。

---

## 4. Layer 3 · Surface(用户触点)

### 4.1 原生 Slate Dock Panel(**主 surface**)

**纯 C++ Slate 实现**,通过 `FGlobalTabmanager::RegisterNomadTabSpawner` 注册为 `SDockTab`。**不**用 Editor Utility Widget(EUW)—— EUW 基于 UMG / `UUserWidget`,承载复杂 Diff UI(代码高亮、DataTable 分列 diff、Blueprint diff 视图)会吃亏,且不便用 C++ 细粒度控制。

布局:

```
┌────────────────────────────────────────┐
│  🔷 Vessel          [Agent: ... ▼]     │
├────────────────────────────────────────┤
│  [chat history / diff preview]         │
│  [agent state indicator]               │
│  [approve / reject buttons]            │
├────────────────────────────────────────┤
│  [input box]               [$ cost]    │
└────────────────────────────────────────┘
```

策划和 TA 的 99% 交互发生在这里。其他 surface 是辅助。

### 4.2 Commandlet(v0.2)

`UnrealEditor-Cmd.exe Project.uproject -run=VesselAgent -agent=<template> -task=<...>`

CI-friendly,无 UI,审批走配置文件(预授权的 batch 模式)或失败即停。

### 4.3 MCP Server(v0.2)

把 Tool Registry 通过 MCP 协议暴露给外部 LLM。开发者本地 `vessel mcp serve` 启动。**这不是 Vessel 的主要入口**,是为兼容 Claude Desktop / Cursor 生态的附赠品。

### 4.4 Chat Shell / TUI(v0.2+)

开发者命令行界面。复用 Orchestrator,只是换一个 surface。

---

## 5. 跨层依赖规则

**单向依赖** —— 违反会在 CI 的模块分析阶段失败:

```
Surface   ──依赖──▶  Orchestrator  ──依赖──▶  Core
Surface   ✖不能直接依赖  Core              (除了 Tool Registry 的只读 schema 查询)
Core      ✖不能依赖  Orchestrator
Core 模块之间  ✖不能相互依赖
```

**例外**:Surface 可以直接读 Tool Registry 的 schema(为了 UI 生成 tool 卡片),但**不能**直接调 tool 执行 —— 执行必须走 Orchestrator。这条例外是 UX 必需的,记录在案。

---

## 6. 模块依赖图

```
                    ┌──────────────────┐
                    │ Session Machine  │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼─────────────────────┐
        ▼                    ▼                     ▼
┌───────────────┐   ┌───────────────┐    ┌────────────────┐
│ Harness       │   │ HITL Gate     │    │ Memory         │
└───────┬───────┘   └───────┬───────┘    └────────────────┘
        │                   │
        │    ┌──────────────┴──────────────┐
        ▼    ▼                             ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│ Tool Registry │   │ Transaction   │   │ Validator     │
│               │   │ Wrapper       │   │ Hooks         │
└───────────────┘   └───────────────┘   └───────────────┘

        ┌────────────────────────────┐
        │ LLM Adapter                │    ← 被 Session Machine 独占使用
        └────────────────────────────┘
```

**关键不变式**:
- `Tool Registry` 是一个**纯数据 singleton**,谁都能读,但谁都不能改(构建期 + hot reload 外)
- `LLM Adapter` 只被 `Session Machine` 的 Planner/Judge 状态直接使用,不对外露
- `Memory` 被 `Session Machine` 读写,但 `Harness` 也可以读(用于上下文注入)

---

## 7. Architecture Decision Records(ADR-lite)

这里记录关键决策的**取舍**和 **放弃的替代方案**。每条 3–5 句话。

### ADR-001 · Engine-native 插件,而非外部 MCP server

**决策**:Vessel 以 UE 插件形式存在,而不是把 UE 变成 MCP server 让外部 LLM 访问。

**放弃的方案**:UnrealMCP 风格的 external bridge。

**理由**:策划 / TA 不会装 Claude Desktop。Agent 能力只有作为**引擎内置公民**才能被非程序员触达。外部 bridge 对 solo hacker 友好,对团队协作是负资产。

### ADR-002 · Reflection-based Tool Registry,不手写装饰器

**决策**:用 UE 反射系统扫 `UFUNCTION meta`,自动导出 tool。

**放弃的方案**:LangChain / AutoGen 风格的 Python 装饰器,或手动维护 YAML tool list。

**理由**:手写 schema 和实现会**漂移**。反射自动化 = 编译期就消灭 schema 不同步问题。这是 Vessel 相对其他框架的结构性优势,也是它天然绑定 UE、不易被跨引擎移植的**护城河**。

### ADR-003 · HITL 默认 on,Autopilot 是 opt-in

**决策**:所有写操作默认需人审。Autopilot / batch 是项目管理员显式开启且受限的特性。

**放弃的方案**:多数竞品的默认值 —— autopilot on,用户 opt-in 才有 review。

**理由**:研发环境里,"agent 悄悄改坏东西,三天后在 bug 现场被发现" 是最昂贵的失败模式。我们愿意用"多一次点击"换"永不悄悄失败"。这条是产品对企业用户的核心承诺。

### ADR-004 · 单 agent 优先,多 agent 编排放到 v1.0 之后

**决策**:v0.1 只支持单 agent session。多 agent(A 和 B 互相调用、消息总线、共享 blackboard)明确不做。

**放弃的方案**:CrewAI / AutoGen 风格的多 agent 开箱即用。

**理由**:单 agent 把 HITL / memory / sensors 做到位就已经工程量饱和。多 agent 会把"调试复杂度"乘以 N。等单 agent 产品化之后再考虑。

### ADR-005 · 用 SQLite FTS5 做长期 memory 检索,不用向量数据库

**决策**:Long-term session log 用 JSONL + SQLite FTS5 做全文检索。

**放弃的方案**:Chroma / LanceDB / FAISS 等向量库。

**理由**:
- UE 插件对体积敏感,向量库依赖膨胀(模型 + ONNX runtime)不可接受
- 研发期 agent 的检索场景是"找上次我怎么改的 DT_NPC_Citizen",关键词检索命中率极高,向量语义检索 ROI 低
- FTS5 随 SQLite 内置,零新依赖

如果未来证明关键词不够用,可以在 v1.0+ 补充可选的向量 backend,但不是默认。

### ADR-006 · LLM Adapter 必须支持 Custom Endpoint / Headers,v0.1 起

**决策**:即使 v0.1 只内置 Anthropic 一个 provider,也必须支持指向 Azure OpenAI、企业自建网关、proxy 的配置。

**放弃的方案**:"先只支持公共 api.anthropic.com,企业网关等 v0.2"。

**理由**:大中型商业团队的 IT 合规红线是"未公开项目数据严禁发公共 LLM API"。如果 v0.1 不支持 custom endpoint,所有目标客户在接触那一刻就被 IT 部门否决 —— 卖给商业团队的定位整个崩塌。这是 Gemini review 指出的最严重缺失之一。

### ADR-007 · 不依赖 LangChain / LlamaIndex / 任何 Python agent 框架

**决策**:Orchestrator 自研 Session Machine + Harness,不抄也不依赖 Python agent 框架。

**放弃的方案**:通过 UE Python API 在引擎里跑 LangGraph。

**理由**:
- UE 的 Python 是 Editor-only,生产部署不可用
- LangChain / LlamaIndex 的版本兼容性对长期维护是毒药
- 自研的 Session Machine 更贴合 UE 的 async / thread model
- 尊重设计来源:架构思路受 LangGraph 启发,但不直接依赖 —— 致敬在 README / VISION 里点名

---

## 8. 未来扩展点(预留接口,v1.0 前不实现)

留些"钩子"让后续版本能加入,但**现在不做**:

- **多 agent 协作**:Session Machine 内部的 `SubSessionSpawn` hook,目前总是 no-op
- **向量检索 backend**:Memory 的检索接口分两个实现,v0.x 只有 FTS5,预留 VectorSearch 实现点
- **本地 SLM 降级**:LLM Adapter 的 provider 链,v0.3 插入 local fallback
- **MCP tool 导入**:Tool Registry 除了扫 UFUNCTION,留一个"从 MCP 外部 server 拉 tool"的接口,但目前关闭

**纪律**:预留接口只要求"形状不冲突",**不要求实现** —— 不为假想需求写代码。

---

## 9. 阅读路径建议

按依赖顺序从下往上读:
1. 本文(总览)
2. [TOOL_REGISTRY.md](TOOL_REGISTRY.md) —— Core 层最核心
3. [SESSION_MACHINE.md](SESSION_MACHINE.md) —— Orchestrator 的发动机
4. [HITL_PROTOCOL.md](HITL_PROTOCOL.md) —— 哲学 §2.4 的具体兑现
5. [CODING_STYLE.md](CODING_STYLE.md) —— 贡献前必读
6. [BUILD.md](BUILD.md) —— 上手的第一步

---

*Last reviewed: 2026-04-23 · Architectural changes require a PR with an ADR entry, not just code diff.*
