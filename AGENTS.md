# AGENTS.md · Vessel 项目指南

> 本文件是 Vessel **对自己** 的 guides —— 既给人类贡献者看,也给在此 repo 里工作的 AI 编程 agent(Claude Code / Cursor / Vessel 本身)看。
> Vessel 的哲学是 "guides + sensors 作为一等公民"(见 [docs/product/VISION.md](docs/product/VISION.md) §2.3)。**我们自己的项目也按这个方法论开发。**

---

## 1. 项目是什么

Vessel 是一个 Unreal Engine 原生插件,让 AI agent 以可审计、可撤销、HITL-默认的方式参与引擎研发工作流。

**你在读这份文件的三种典型身份**:
1. **人类贡献者** —— 请按 [docs/process/CONTRIBUTING.md](docs/process/CONTRIBUTING.md) 流程。
2. **AI 编程助手(Claude Code / Cursor)用户在 Vessel repo 上** —— 把本文件作为项目宪法,按 §3 / §4 的模式写代码。
3. **Vessel 自己(meta-recursive)** —— 当开发者用 Vessel 插件去调 Vessel 这个项目的 DataTable / tests 时,Session Machine 会自动把本文件注入 Planner prompt。

---

## 2. 快速心智模型

| 层 | 你要知道的 |
|---|---|
| Philosophy | VISION.md 四条支柱:Engine-native / Reflection-first / Guides+Sensors / Harness-not-Magic |
| Architecture | 三层单向依赖:Surface → Orchestrator → Core |
| Non-goals | 不做通用 agent 框架、不做 runtime NPC、不做 Unity、不默认 autopilot |
| Quality bar | docs-before-code、HITL 默认 on、Reject-with-reason 自动沉淀、Ctrl+Z 边界显式告知 |

**先读**(按顺序): [VISION](docs/product/VISION.md) → [ARCHITECTURE](docs/engineering/ARCHITECTURE.md) → [TOOL_REGISTRY](docs/engineering/TOOL_REGISTRY.md) → [SESSION_MACHINE](docs/engineering/SESSION_MACHINE.md) → [HITL_PROTOCOL](docs/engineering/HITL_PROTOCOL.md)。

---

## 3. 代码约定摘要

完整规范见 [CODING_STYLE.md](docs/engineering/CODING_STYLE.md)。这里只列最容易踩的五条:

1. **UE 前缀一定带全**:`FVessel*` / `UVessel*` / `IVessel*`。Blueprint 可见类必须 `UVessel*`。
2. **不 include Slate 到 `VesselCore`** —— Core 是 runtime-safe 模块,一旦依赖 Slate 就污染 runtime。
3. **不用 `UE_LOG(LogTemp, ...)`** —— 用 `VESSEL_LOG(Category, Verbosity, ...)`,六个 category 按用途分。
4. **不抛异常** —— 错误路径走 `FVesselResult<T>`,错误码见 [TOOL_REGISTRY §5.1](docs/engineering/TOOL_REGISTRY.md)。
5. **check / ensure / verify 不混用** —— `check` 只用于"插件设计假设崩塌",不用于用户输入验证。

**新贡献者最常踩的坑**:把 `UObject*` 直接作为 tool 参数。→ 用 `FSoftObjectPath` 或资产 path 字符串。

---

## 4. 常见开发模式

### 4.1 加一个新的 Tool

1. 在 `Source/VesselCore/Public/Tools/VesselXXXTools.h` 加 `UFUNCTION`,meta 必含 `AgentTool="true"`、`ToolCategory`、`ToolDescription`
2. 在对应 `.cpp` 实现。不要抛异常,返回 `FVesselResult<T>` 或 `FString`
3. 加 automation test:`Source/VesselTests/Tools/TestXXX.cpp`
4. Console 输入 `VesselRegistry.Refresh` 或重启 Editor 验证

完整 step-by-step 见 [TOOL_REGISTRY §6](docs/engineering/TOOL_REGISTRY.md)。

### 4.2 加一个新的 Validator

1. 继承 `UEditorValidatorBase`
2. 重写 `CanValidateAsset_Implementation` 和 `ValidateLoadedAsset_Implementation`
3. 在 `ValidateLoadedAsset_Implementation` 返回 `EDataValidationResult::Invalid` + `AssetFails(...)` 消息
4. 注册 = 无(UE 会自动扫 `UEditorValidatorBase` 子类)

### 4.3 加一个新的 Agent 模板

1. 在 `Plugins/Vessel/Content/Agents/<name>.yaml` 写配置
2. 声明 `allowed_categories`、`session:` budgets、`judge_rubric`
3. Vessel 面板的 agent 下拉自动识别

### 4.4 改 Session Machine 状态集合

**不要直接改代码**。先 PR `docs/engineering/SESSION_MACHINE.md`,讨论通过,再改代码。见 CONTRIBUTING §3。

---

## 5. Tool Policy(Vessel-on-Vessel)

**这一节被 Vessel Session Machine 自动消费** —— 当 Vessel 在 Vessel repo 上运行时,Planner 只会看到下面 allow 列表里的 tool。

```yaml
## Tool Policy (machine-readable)

allow:
  - category: DataTable/Read
  - category: Asset/Read
  - category: Meta                   # ListAssets, DescribeDataTable, etc.
  - category: Validator

deny:
  - category: Code/Write             # 本项目不让 agent 写 C++,代码必须人写
  - category: Blueprint/Write        # Vessel 自己不该有业务 Blueprint
  - name: DeleteAsset
  - name: ForcePushGit
  - name: RunShellCommand

auto_approve_in_batch: []             # 本项目默认不开 batch

always_require_approval:
  - category: DataTable/Write        # 本项目连 DataTable 写也要人审
```

**语义**:`allow` 是白名单;被 `deny` 的 tool 即使在 `allow` 里也不提供;`auto_approve_in_batch` 默认为空 = 所有写操作都要人审。

---

## 6. Session Defaults(for Vessel-on-Vessel)

```yaml
## Session Defaults

max_steps: 50
max_cost_usd: 10.00
max_wall_time_sec: 1800
repeat_error_limit: 5
max_consecutive_revise: 5
default_model: claude-sonnet-4-6
judge_model: claude-haiku-4-5
```

比 [SESSION_MACHINE §5](docs/engineering/SESSION_MACHINE.md) 的全局默认更保守,因为本 repo 自身不是工业化生产项目,不需要大 budget。

---

## 7. Known Rejections

下面是真实 reject 理由的沉淀。Vessel Planner 看到这些,会规避相同错误。

**初始版本仅放一条示例**(未来由真实使用填充):

### 2026-04-23T00:00:00 · tool=ProposeDocChange · target=docs/product/VISION.md
**reason**: Agent 试图把 VISION.md 的四条哲学原则压缩成三条,声称"更精炼"。但哲学原则不是压缩指标 —— 四条是已经仔细挑选的,对应 ADR-001~004。Reject,并指示 agent 任何 VISION 级别的变更必须先在 issue 讨论。
**session**: (synthetic seed)
**rejecter**: maintainer

---

*(真实 rejections 会被 HITL Gate 自动追加到本段,不需要手动维护)*

---

## 8. 给 AI 编程助手的元指令

如果你是 AI 助手(Claude Code / Cursor / 其他 coding agent),**在 Vessel repo 上工作时**请:

1. **先读 docs/,再读代码**。Vessel 的文档**领先于代码**,代码之前已经先被文档定义。
2. **遵守 docs-before-code 纪律**:若你的改动触及 §3 表格的哲学级领域,先改文档 PR,别直接改代码。
3. **写 tool 时,meta 一次写全**:`AgentTool` + `ToolCategory` + `ToolDescription` + (对 write 类)`RequiresApproval="true"` + (对不可撤销)`IrreversibleHint="true"`。
4. **错误消息要 LLM 可读**(见 [TOOL_REGISTRY §5.4](docs/engineering/TOOL_REGISTRY.md))。写"参数 X 期望类型 Y,收到 Z",不写"invalid args"。
5. **发现不一致时提醒作者,不自作主张修复**:比如你发现 VISION 和 ROADMAP 的 v0.1 功能描述冲突,先在回复里指出,等作者决定哪个是真,再统一。
6. **保持项目 vendor-neutral** —— 不在代码 / 文档 / 示例里引入任何具体公司名、游戏项目代号、或能被关联到特定团队的称谓。Vessel 面向开源社区,所有举例用通用 RPG / 游戏术语(`NPC_Citizen_A`、`DT_Items_Weapons` 等),不是任何 shipping 游戏的资产。

---

*Last reviewed: 2026-04-23 · 本文件随项目演进,欢迎 PR 补充共通约定。*
