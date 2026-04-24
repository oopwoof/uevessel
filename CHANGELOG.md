# Changelog

All notable changes to Vessel will be documented in this file.

Format loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
**v0.x 不保证 API 稳定,v1.0 发布即承诺 semver 1.x 兼容**(见 [ROADMAP](docs/process/ROADMAP.md))。

---

## [Unreleased]

### Added
- Product book:VISION / PERSONAS / USE_CASES / UX_PRINCIPLES / COMPETITIVE
- Engineering book:ARCHITECTURE(含 ADR 001–007)/ TOOL_REGISTRY / SESSION_MACHINE / HITL_PROTOCOL / CODING_STYLE / BUILD
- Process:ROADMAP / CONTRIBUTING
- Repo-root AGENTS.md(dogfood guides + Tool Policy + Session Defaults + Known Rejections 种子)
- Repo-root README(Hero + Why + 差异化表 + 物理形态 ASCII 图 + Quickstart + 架构一瞥 + 核心概念 + Roadmap + 文档索引)
- `.gitignore`(UE plugin 导向,含 Vessel-specific 路径)
- `.gitattributes`(LF 规范化 + UE 资产 binary 标注)
- `.clang-format`(Epic UE 风格近似)
- `.github/ISSUE_TEMPLATE/`(bug / feature / config)
- `.github/workflows/build-plugin.yml`(pre-v0.1 占位 · 文档 lint + 格式检查 · UE 构建 matrix 预留)

### Changed
- n/a(pre-v0.1,设计仍在动荡期)

### Security
- 文档级别规定 API Key 必须走 `EditorPerProjectUserSettings`,不能进入 `DefaultVessel.ini`

### Code (v0.1-alpha.1 scaffold)
- `Vessel.uplugin` —— 3 个模块声明(VesselCore / VesselEditor / VesselTests)
- `Source/VesselCore/` 骨架:module class、6 个 log categories、`VESSEL_LOG` 宏
- `Source/VesselEditor/` 骨架:module class
- `Source/VesselTests/` 骨架 + `Vessel.Smoke.HelloWorld` automation test(确认两个 module 都正确加载)
- 所有 `.Build.cs` 显式 `bEnableExceptions = false`(符合 CODING_STYLE §7)

### Code (Settings + LLM Adapter foundation)
- `UVesselProjectSettings`(`config=Vessel, defaultconfig`)—— team-shared 非敏感字段(Provider / Endpoint / Model / NonSecretHeaders / bAllowHttp)
- `UVesselUserSettings`(`config=EditorPerProjectUserSettings`)—— per-user 敏感字段(AnthropicApiKey / GatewayAuthorization / AzureApiKey),带 `PasswordField=true` meta
- `FVesselAuth` —— 敏感值解析优先级(env var > user settings,**不**回落到 project settings);`IsEndpointPermitted` 硬编码 localhost-only HTTP 规则;`Redact` 日志安全
- `ILlmProvider` + `FLlmRequest` / `FLlmResponse` / `FLlmToolCall` 等 POD 类型
- `FLlmProviderRegistry` —— 进程级 singleton,`FRWLock` 保护,提供 `InjectMock` 给 CI
- `FVesselMockProvider`(ARCHITECTURE.md §2.4 的 v0.1 必交付)—— 按"最后 user message 内容"命中 fixture,默认 fallback 报 ConfigError
- `FAnthropicProvider` —— 真实 HTTP 骨架,调用 `FHttpModule`,读取 `UVesselProjectSettings` + `FVesselAuth`;TODO(step4) 标注 tool_use 序列化
- `FVesselJsonSanitizer` —— 剥 ```json fence + 提取首个平衡 `{...}` 对象,尊重字符串引号内的 `{` / `}`

### Tests (8 new automation tests)
- `Vessel.Settings.ConfigScope` —— 防回归:API key 字段留在 EditorPerProjectUserSettings
- `Vessel.Settings.EndpointPermit` —— https / http / localhost / 非 localhost 覆盖
- `Vessel.Settings.Redact` —— redact 输出不含原值且带长度
- `Vessel.Llm.MockProvider.{FixtureHit, DefaultFallback, RegistryLookup}` —— 三态覆盖
- `Vessel.Util.JsonSanitizer.{BareObject, FencedJson, PreludeText, Nested, BracesInStrings, Unbalanced, NoObject}` —— 七种输入场景

### Code (Tool Registry infrastructure · Step 3a)
- `EVesselResultCode` + `FVesselResult<T>` + `FVesselVoidResult` —— 错误承载,7 种错误码对齐 [TOOL_REGISTRY §5.1](docs/engineering/TOOL_REGISTRY.md)
- `FVesselParameterSchema` / `FVesselToolSchema` —— 纯 POD,不依赖反射头文件
- `FVesselReflectionScanner` —— `TObjectIterator<UClass>` + `TFieldIterator<UFunction>`;支持 string / int / float / bool / array / map / enum / struct(`Guid` / `SoftObjectPath` 有 well-known format);`WITH_EDITOR` 守卫(runtime build 会跳过扫描并 log warning)
- `FVesselToolRegistry` —— 进程级 singleton,FRWLock 保护,提供 `ScanAll` / `FindSchema` / `ListToolNames` / `InjectSchemaForTest` / `ToJsonString`

### Tests (8 more automation tests · Step 3a)
- `UVesselTestToolFixture`(fixture UCLASS)—— 三个 UFUNCTION:`FixtureRead`(读类)、`FixtureIrreversibleWrite`(带 irreversible + batch_eligible + tags)、`NotAnAgentTool`(负样本)
- `Vessel.Registry.Scanner.{Discovers, MetaReadback, PolicyFlags}` —— 扫描器覆盖
- `Vessel.Registry.ToolRegistry.{ScanAll, Inject, JsonShape}` —— 注册表覆盖
- `Vessel.Registry.Result.Basics` —— `FVesselResult` + `VesselResultCodeToString`

### Code (Invoke pipeline + first tool · Step 3b)
- `FVesselTransactionScope` —— RAII 包装 `FScopedTransaction`;策略:`IrreversibleHint` 赢过一切 → 跳过;否则 `RequiresApproval` 或 `Category` 含 "Write" → 开事务;只在 `WITH_EDITOR` 生效
- `VesselCore.Build.cs` 条件依赖 `UnrealEd`(`if Target.bBuildEditor`)—— 保持 Runtime 兼容
- `FVesselToolInvoker` —— `Invoke(name, argsJson, options)` 管线:registry lookup → json 清洗 + parse → 反射 param buffer 填充 → `FVesselTransactionScope` → `CDO->ProcessEvent` → return value JSON 化 → 清理。Step 3b 支持类型:FString / FName / int32 / int64 / bool / float / double / TArray
- `UVesselDataTableTools::ReadDataTable` —— **第一个真实 agent tool**。可从 SoftObjectPath 加载 DataTable,按 RowNames 过滤(空数组返回全部),把每行 UPROPERTY 渲染为 JSON object。可测试入口 `ReadRowsJson(UDataTable*, ...)` 方便不走资产加载
- `FVesselTestRow` USTRUCT —— 测试用的最小 DataTable 行类型,继承 `FTableRowBase`
- `FixtureRead` 改为 echo 参数(用于验证 Invoker 端到端参数 round-trip)

### Tests (10 more automation tests · Step 3b)
- `Vessel.Registry.Invoker.{NotFound, RoundTrip, MissingParam, WrongType, FencedArgs, TransactionPolicy}` —— Invoker 六维度覆盖
- `Vessel.Tools.DataTable.{ReadAll, ReadSelected, NullTable, UnknownRow}` —— DataTable tool 四场景

**累计测试数**:32 (1 smoke + 13 settings/mock/sanitizer + 8 registry/scanner + 10 invoker/datatable)

### CI
- `build-plugin` workflow:clang-format job 改为 `continue-on-error: true`(pre-v0.1 临时放行 —— 作者本地尚未装 clang-format;等装好并清理 drift 后升回 blocking)。docs lint / forbidden-brand / api-key 检查保持强制

### Fix (Gemini 3 Pro Preview code review)
- `FVesselToolSchema::Function` 从 raw `UFunction*` 改 **`TWeakObjectPtr<UFunction>`** —— Live Coding / module reload 会重建 UClass/UFunction,raw 指针会悬空 → Invoker 调用时 AV
- `FVesselToolInvoker::Invoke` 顶部加 **`checkf(IsInGameThread(), ...)`** —— `ProcessEvent` 硬要求 game thread,async 回调直接驱动会底层 assert
- `FVesselReflectionScanner` 跳过**纯 OutParm**(`CPF_OutParm && !CPF_ReferenceParm`)—— 避免 LLM 被要求传引擎内部输出参数;Invoker 仍分配 + 初始化内存保持 ProcessEvent 安全
- 移除 `const_cast<FRWLock&>(Lock)` 反模式(2 处:`VesselToolRegistry.cpp` / `LlmProviderRegistry.cpp`)—— 成员已经 `mutable`,cast 多余且潜在 UB
- MockProvider tests 去掉 `Future.Wait() + Future.Get()` 范式,改 `Future.Get()` 单调用 —— 该模板若被真 HTTP provider 抄走会死锁 game thread
- 测试 DataTable 用 **`TStrongObjectPtr<UDataTable>`** 包装,避免未来 latent test GC 悬空

### Code (Step 3c · 剩余 4 个 v0.1 tool)
- `UVesselDataTableTools::WriteDataTableRow` —— 第二个 DataTable tool,`DataTable/Write` 分类,`RequiresApproval=true`。通过 `FJsonObjectConverter::JsonObjectToUStruct` 把任意 JSON 映射到 row struct,走 `Modify()` + `GetNonConstRowMap()` + `HandleDataTableChanged` 三件套。`WITH_EDITOR` 守卫
- `UVesselAssetTools::ListAssets` —— IAssetRegistry 过滤 + JSON 数组输出
- `UVesselAssetTools::ReadAssetMetadata` —— 不加载 asset body,直接读 `FAssetData` 的 class / package / tags
- `UVesselValidatorTools::RunAssetValidator`(VesselEditor 模块)—— 调 `UEditorValidatorSubsystem::IsObjectValidWithContext`,收集 errors/warnings 返 JSON
- `VesselCore.Build.cs` 增加 `AssetRegistry` public dep;`VesselEditor.Build.cs` 增加 `DataValidation` + `AssetRegistry`;`VesselTests.Build.cs` 增加 `VesselEditor` public dep(测试需访问 Validator tool 头文件)

### Tests (8 more automation tests · Step 3c)
- `Vessel.Tools.DataTable.{WriteInsert, WriteReplace, WriteRejectsBadJson}` —— WriteRowJson 的插入 / 替换 / 非法输入三态
- `Vessel.Tools.Asset.{ListShape, ListUnknownPath, MetadataMissing}` —— ListAssets 形状 + 空路径;ReadAssetMetadata 不存在的 asset
- `Vessel.Tools.Validator.{MissingAsset, ReportShape}` —— RunAssetValidator 空 asset + 返回值形状

**累计测试数**:40(1 smoke + 13 settings/mock/sanitizer + 8 registry/scanner + 10 invoker/datatable 3b + 8 step 3c tools)

### Code (Session Machine foundations · Step 4a.1)
- `FVesselSessionTypes.h` —— POD 类型:`EVesselSessionState`(8 态 FSM)、`EVesselSessionOutcomeKind`(5 种终止)、`FVesselPlanStep` / `FVesselPlan` / `FVesselJudgeVerdict` / `FVesselSessionOutcome`;配套字符串转换
- `FVesselSessionConfig.h` —— 三层合并后的 session 配置:`FVesselSessionBudget`(industrial 默认)、`FVesselAgentTemplate`(带 `MakeMinimalFallback()`)、`FVesselSessionConfig`。辅助 `GenerateSessionId()` + `MakeDefaultSessionConfig()` 从 `UVesselProjectSettings` 填充默认
- `FVesselSessionLog` —— JSONL append-only 日志;`IPlatformFile::OpenWrite(bAppend=true, bAllowRead=true)` 解决 Gemini 指出的 Windows 文件锁问题;每条 record 写完立刻 flush(崩溃最多丢最后一条,**不会**半行损坏);`FCriticalSection` 保护多 session tick 并发
- Session 文件路径:`<Project>/Saved/AgentSessions/<SessionId>.jsonl`(符合 ARCHITECTURE / SESSION_MACHINE 承诺)

### Tests (4 more automation tests · Step 4a.1)
- `Vessel.Session.Log.Basic` —— 打开 → append 两条 record → close,验证文件存在 + 两行合法 JSON + 每行含 type/ts/session/payload
- `Vessel.Session.Log.Reopen` —— `Open` 是 idempotent,重开关掉旧 handle 切到新 session
- `Vessel.Session.Config.Default` —— `GenerateSessionId` 唯一递增;`MakeDefaultSessionConfig` 所有关键字段非空
- `Vessel.Session.Types.Strings` —— 三个枚举转字符串稳定

**累计测试数**:44

### Code (Planner prompts + FSM runner · Step 4a.2)
- `FVesselPlannerPrompts` —— 构建 Planning / Judge `FLlmRequest`;过滤 tool(按 agent `AllowedCategories` / `DeniedTools`);解析 plan / judge 响应(走 `FVesselJsonSanitizer` 容 markdown fence);**Judge 解析失败默认 Reject**(safety-by-default)
- `FVesselSessionMachine` —— 8 态显式 FSM(Idle/Planning/ToolSelection/Executing/JudgeReview/NextStep/Done/Failed);每个状态切换点**都查 budget** + abort;LLM async 回调通过 `AsyncTask(ENamedThreads::GameThread)` 回跳(尊重 Gemini 的 game-thread 警告);**TWeakPtr + TSharedFromThis** 模式防止回调在 session 被析构后触发
- Tool error 路径:`(Tool, ErrorCode)` 计数,超 `RepeatErrorLimit` 断路器 → Failed;非断路场景回 Planning 带错误 directive
- Judge Revise → 回 Planning 带 directive,连续 Revise 达 `MaxConsecutiveRevise` → Failed
- `FEditorDelegates::OnEditorClose` hook(Gemini review 要求)—— Editor 关闭时 session 输出 `AbortedOnEditorClose`
- 每个状态转换 + plan / step / verdict / summary 都写入 JSONL log

### Tests (8 more automation tests · Step 4a.2)
- `Vessel.Session.Prompts.{PlanParseOk, PlanParseFenced, PlanParseMissingField}` —— plan JSON 解析正/负路径
- `Vessel.Session.Prompts.{JudgeParseApprove, JudgeParseRevise, JudgeParseReject, JudgeParseMalformedDefaultsReject}` —— judge 四种决策 + 安全默认
- `Vessel.Session.Prompts.FilterByAllowedCategory` —— agent 模板 `AllowedCategories` 过滤生效

**累计测试数**:52

### Tests (6 more automation tests · Step 4a.3 · 端到端)
- `Vessel.Session.E2E.SingleStepApprove` —— Planner mock → 调 `FixtureRead` → Judge approve → Done;**额外验 JSONL 日志 5 种 record type 都落盘**
- `Vessel.Session.E2E.PlannerMalformed` —— Planner 返非 JSON → Failed + reason 含 "Planner" / "plan"
- `Vessel.Session.E2E.UnknownTool` —— Planner 指向 registry 外的 tool → Failed + reason 含 tool 名
- `Vessel.Session.E2E.JudgeReject` —— Judge Reject → Failed + reject reason 上浮到 Outcome.Reason
- `Vessel.Session.E2E.ConsecutiveReviseBudget` —— `MaxConsecutiveRevise=2` + Judge 永远 Revise → Failed + reason 含 revise/Consecutive
- `Vessel.Session.E2E.EmptyPlan` —— Planner 返 `{"plan":[]}` → Done + `StepsExecuted=0`

**累计测试数**:58(Session Machine 端到端 6 维覆盖)

### Code (HITL Gate · Step 4b)
- `FVesselApprovalRequest` / `FVesselApprovalDecision` / `EVesselApprovalDecisionKind`(Approve / Reject / EditAndApprove)POD 类型
- `IVesselApprovalClient` 抽象接口 + 三个内置实现:`FVesselAutoApprovalClient`(CI 默认)、`FVesselAutoRejectClient`(失败路径测试)、`FVesselScriptedApprovalClient`(按 tool name 逐一映射 + default 回落)
- `FVesselRejectionSink` —— 两处持久化:`<Project>/AGENTS.md ## Known Rejections` 段(若 section 不存在自动创建,带 auto-managed 注释)+ `<Project>/Saved/VesselRejectionArchive/<yyyy-mm>.jsonl` 月度结构化归档
- `FVesselSessionMachine::SetApprovalClient` —— 必须在 `RunAsync` 之前调用;未设置时默认 `FVesselAutoApprovalClient`(仅限 CI / mock 场景)
- `FVesselSessionMachine::StepNeedsApproval(Schema)` —— 三条触发规则对齐 HITL_PROTOCOL §1.1:`RequiresApproval` / `IrreversibleHint` / Category 含 "Write"
- `EnterExecuting` 分叉:需审批 → `RequestApprovalForStep` async → `HandleApprovalDecision` 回跳 game thread:
  - Approve → `InvokeStep`(原 invoke 逻辑)
  - EditAndApprove → 用 `RevisedArgsJson` 替换 step args(同步到 `CurrentPlan` 让 replay 看到实际执行内容)→ InvokeStep
  - Reject → `FVesselRejectionSink::Record` 沉淀 → `EnterFailed`
- 新增 JSONL log record type:`ApprovalRequested`、`ApprovalDecision`

### Tests (5 more automation tests · Step 4b)
- `Vessel.HITL.Predicate.RequiresApproval` —— 三条触发规则 + 纯读路径负样本
- `Vessel.HITL.E2E.ApprovePath` —— Scripted Approve → Done + JSONL 含 ApprovalRequested/Decision
- `Vessel.HITL.E2E.RejectSinksToAgentsMd` —— Reject → Failed + AGENTS.md `## Known Rejections` + 月度 archive JSONL 落盘;测试"快照+恢复"不污染 repo
- `Vessel.HITL.E2E.EditAndApprove` —— Scripted edit → stored plan args 同步更新 + JSONL 含 revised args
- `Vessel.HITL.E2E.ReadOnlyBypass` —— 纯读 tool 下 ApprovalClient **零查询**

**累计测试数**:63

### Code (Slate Dock Panel 骨架 · Step 4c.1)
- `SVesselChatPanel` —— 纯 C++ Slate widget(**非** EUW / UMG,对齐 ARCHITECTURE.md §4.1 ADR);布局 = Header(agent + status + cost)/ `SSplitter` 垂直切(chat history 上 60% / diff preview 下 40%)/ approval 栏(Edit · Reject with reason · Approve & Execute 三按钮,默认 disabled)
- 最小公共 API:`AppendUserMessage` / `AppendAssistantMessage` / `SetAgentStatus` / `SetDiffPreview` / `SetCostLabel` —— 供 Step 4c.2 的 session-to-UI bridge 消费
- `VesselTabIds::ChatPanel` namespace 常量
- `VesselEditor::StartupModule` 通过 `FGlobalTabmanager::RegisterNomadTabSpawner` 挂 dock tab 到 Workspace / Tools 分类;`UToolMenus::RegisterStartupCallback` 加 Window 菜单项 "Vessel Chat"
- 所有按钮回调 Step 4c.1 都是 stub(log 一行),真实 session 集成在 Step 4c.2

**说明**:Slate widget 自动化测试需渲染循环,属 v0.2 改进范围。4c.1 通过模块启动 + 手动打开面板做 smoke 验证。

### Code (Session ↔ Panel 桥接 + 真实 HITL 流 · Step 4c.2)
- `FVesselSlateApprovalClient` —— `IVesselApprovalClient` 的 Slate 实现。收到 request 时,通过 `OnApprovalRequested` 多播把 `TPromise<FVesselApprovalDecision>` 丢给 widget;widget 在按钮回调里 SetValue 完成决策。**安全默认**:delegate 未绑 / 已有 pending → 立刻 auto-reject,session 不卡
- `SVesselChatPanel` 接通 session:
  - `HandleSendClicked` 创建 `FVesselSessionMachine` + `FVesselSlateApprovalClient`,`SetApprovalClient`,`RunAsync`;session 完成的 callback hop 回 game thread 后喂 `OnSessionComplete`
  - `HandleApprovalRequested` 把 request 挂成 `PendingPromise`,UI 进入 approval 模式:diff 区显示 summary + args JSON、agent status 变 "awaiting approval",Approve/Reject 按钮 enable
  - Approve → `SetValue(MakeApprove)` → session 继续
  - Reject → 切换 `SWidgetSwitcher` 到 reason 输入视图;**强制 ≥5 字符**(对齐 HITL_PROTOCOL §4.1);Confirm → `SetValue(MakeReject)`
  - Edit 按钮 Step 4c.2 仍 disabled(EditAndApprove UI 留 4c.3 或后续)
- 会话生命周期防错:Send 期间 input / Send 按钮 disable,避免并发 session;session 完成后恢复

### Tests (3 more automation tests · Step 4c.2)
- `Vessel.HITL.Slate.NoDelegateAutoRejects` —— delegate 未绑 → 自动 Reject(session 不能卡)
- `Vessel.HITL.Slate.DelegateFulfills` —— 绑定 delegate → promise 可正常 fulfill,HasPending 清零
- `Vessel.HITL.Slate.SecondRequestAutoRejects` —— 并发第二请求立即 auto-reject,不队列化(避免死锁;队列由 UI 自己决定)

**累计测试数**:66

---

## 版本规划(待交付)

### [0.1.0] · 目标 6–8 周内

最小可公开展示(Show HN / r/unrealengine)。硬性范围见 [ROADMAP § v0.1](docs/process/ROADMAP.md)。

### [0.2.0]

MCP server / CI commandlet / 第二 agent 模板 / OpenAI + Qwen provider / SQLite FTS5 长期 memory。见 [ROADMAP § v0.2](docs/process/ROADMAP.md)。

### [0.3.0]

Custom Agent Builder · 本地 SLM 降级 · Dev Chat 模板。见 [ROADMAP § v0.3](docs/process/ROADMAP.md)。

### [1.0.0]

API 稳定承诺。
