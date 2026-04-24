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
