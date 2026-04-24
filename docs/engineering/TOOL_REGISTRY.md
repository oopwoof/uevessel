# Vessel · Tool Registry

> Tool Registry 是 Vessel 的**结构性差异化**所在(见 [VISION.md §2.2](../product/VISION.md) / [ARCHITECTURE.md ADR-002](ARCHITECTURE.md))。本文件规定**如何声明一个 tool、Registry 如何扫描、如何生成 schema、如何调用、错误如何处理**。
> 阅读前提:了解 UE 反射系统(`UFUNCTION` / `UPROPERTY` / `UStruct`)基础。

---

## 0. 设计目标(Why it looks the way it does)

| 目标 | 兑现方式 |
|---|---|
| 一次声明,多处消费 | 反射扫 `UFUNCTION meta`,Editor panel / Commandlet / MCP Server 读同一份 schema |
| Schema 永远和实现同步 | 编译期的反射表是唯一事实来源,不存在"文档和代码漂移" |
| Tool 作者无额外样板 | 写普通 `UFUNCTION`,多加一个 `meta=(AgentTool="true", ...)`,其他都 Registry 做 |
| Agent 可控(HITL 感知) | meta 内置 `RequiresApproval` / `IrreversibleHint` / `BatchEligible`,Registry 把这些标志传给上层 |
| 错误可预期 | 所有调用路径显式走 `FVesselResult<T>`,不让 tool 异常把 Editor 打挂 |

---

## 1. Tool 声明契约

### 1.1 最小示例

```cpp
UCLASS()
class VESSELCORE_API UVesselDataTableTools : public UObject {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, meta=(
        AgentTool="true",
        ToolCategory="DataTable",
        RequiresApproval="false",
        ToolDescription="Read rows from a DataTable asset. Returns rows as JSON."
    ))
    static FString ReadDataTable(
        const FString& AssetPath,
        const TArray<FName>& RowNames);
};
```

**注册发生在**:UE 启动加载 `UVesselDataTableTools` 的 `UClass` 时,Vessel 的反射 scanner 扫到该 `UFUNCTION` 的 `AgentTool="true"` meta,自动入 Registry。**作者不需要写任何注册代码**。

### 1.2 完整 meta 契约

| Meta key | 类型 | 必填 | 语义 |
|---|---|---|---|
| `AgentTool` | `"true"` | ✅ | 标识此函数是 agent 可调用 tool。不写则 Registry 不扫 |
| `ToolCategory` | string | ✅ | 高层分组。v0.1 内置取值:`DataTable` / `Asset` / `Blueprint` / `Meta` / `Code` / `Validator`。可自定义,但建议靠拢已有集 |
| `ToolDescription` | string | ✅ | **自然语言**描述,会被直接塞进 LLM prompt。写得好坏直接决定 agent 能不能调对 |
| `RequiresApproval` | `"true"` / `"false"` | ⬜ 默认 `"true"` | 是否走 HITL。所有 `Category="Write"` 类 tool 必须 true |
| `IrreversibleHint` | `"true"` / `"false"` | ⬜ 默认 `"false"` | 表示此 tool 效果**无法**被 `FScopedTransaction` 撤销(如文件移动、外部 HTTP)。HITL 面板会强化警告 |
| `BatchEligible` | `"true"` / `"false"` | ⬜ 默认 `"false"` | 在 batch 模式下,只有 `BatchEligible="true"` 的 tool 才能跳过逐步人审。一般要求:幂等 + validator 可完整拦截 |
| `EstimatedTokenCost` | int | ⬜ | 供 cost preview 参考。不设则按参数大小估算 |
| `MinVesselVersion` | semver string | ⬜ | 该 tool 要求的 Vessel 最低版本。低于则 Registry 跳过注册(不报错) |
| `ToolTags` | 逗号分隔 string | ⬜ | 细分标签,用于 agent template 过滤。如 `"bulk,read-only"` |

### 1.3 参数类型约定

Registry 支持的参数类型(会自动翻译成 JSON schema):

| C++ 类型 | JSON schema |
|---|---|
| `FString`、`FName`、`FText` | `{"type": "string"}` |
| `int32`、`int64` | `{"type": "integer"}` |
| `float`、`double` | `{"type": "number"}` |
| `bool` | `{"type": "boolean"}` |
| `TArray<T>` | `{"type": "array", "items": <T 的 schema>}` |
| `TMap<FString, T>` | `{"type": "object", "additionalProperties": <T 的 schema>}` |
| `UStruct` 子类(USTRUCT + UPROPERTY) | 递归展开成 `{"type": "object", ...}` |
| `UEnum` | `{"type": "string", "enum": [...]}`(字符串形式) |
| `FSoftObjectPath` | `{"type": "string", "format": "vessel/asset-path"}` |
| `FGuid` | `{"type": "string", "format": "uuid"}` |

**不支持**:
- 裸 `UObject*` 指针 —— 必须改用 `FSoftObjectPath` 或资产 path
- `TSubclassOf<T>` 的 tool 参数 —— v0.1 先不做,v0.2 考虑
- `TFunction`、lambda、回调

如果 tool 作者尝试用不支持类型,Registry 在扫描阶段**报 error 日志 + 跳过注册**,不让程序挂掉。

### 1.4 返回值约定

**推荐写法**:返回 `FString`(JSON 序列化后的结果),或返回 `FVesselResult<TStruct>`。

```cpp
// 简单写法
static FString ReadDataTable(...);

// 推荐写法(类型更明确 + 错误自带)
static FVesselResult<FDataTableRows> ReadDataTable(...);

USTRUCT()
struct FVesselResult {
    GENERATED_BODY()
    UPROPERTY() bool bOk = false;
    UPROPERTY() FString ErrorCode;       // 规范取值:ValidationError / IoError / Timeout / NotFound / Internal
    UPROPERTY() FString ErrorMessage;
    UPROPERTY() TStruct Value;           // 仅 bOk==true 时有效
};
```

Registry 会把 `FVesselResult` 自动平摊成 LLM 可消费的 JSON(成功 → 直接返回 value;失败 → 包一层 `{"error": {...}}`)。

---

## 2. 扫描生命周期

```
 Plugin Startup
     │
     ▼
 ┌───────────────────────┐
 │ Scan all UClass /     │    一次性全量扫描
 │ UStruct / UEnum       │    时机:UE StartupModule
 │ for AgentTool="true"  │
 └─────────┬─────────────┘
           │
           ▼
 ┌───────────────────────┐
 │ Build FVesselTool     │    每个 tool 生成一个
 │ Schema entry          │    FVesselToolSchema 对象
 └─────────┬─────────────┘
           │
           ▼
 ┌───────────────────────┐
 │ Freeze into singleton │    FVesselToolRegistry::Get()
 │ FVesselToolRegistry   │    对查询方只读
 └─────────┬─────────────┘
           │
           ▼
     Ready for queries
           │
           ▼ (later, on hot-reload)
 ┌───────────────────────┐
 │ Incremental re-scan   │    监听 FHotReloadModule 事件
 │ of dirty modules      │    只重扫被 reload 的 module
 └───────────────────────┘
```

**关键点**:
- 启动时全量扫一次 → 放进 `TMap<FName, FVesselToolSchema>`
- Hot reload(非 Live Coding,是真正的 module reload)通过 `FCoreUObjectDelegates::ReloadCompleteDelegate` 增量更新
- Editor 运行期 **不支持动态添加非反射来源的 tool**(v0.2 才考虑开放 "从 MCP 外部注入 tool" 的接口)

> ⚠ **Live Coding 不等于 Hot Reload**(UE5 开发经典坑)
>
> UE5 的 Live Coding **只** patch 已有函数的 `.cpp` 实现,**不**重跑 UHT,**不**重建反射表。以下改动 Live Coding 都看不到效果,**必须完整重启 Editor**:
>
> - 新增带 `meta=(AgentTool="true")` 的 `UFUNCTION`
> - 修改已有 `UFUNCTION` 的 meta 值(包括 `ToolDescription` / `RequiresApproval` 等)
> - 新增 / 删除 / 改类型的 `UPROPERTY`
> - 改 `USTRUCT` 布局
>
> `VesselRegistry.Refresh` 只能在**反射表已经更新**(module reload 或 Editor 重启)之后强制重扫,**不能**把新的 `UFUNCTION` 变出来。

---

## 3. 生成的 JSON Schema 示例

给定上面那个 `ReadDataTable` 声明,Registry 生成的 schema:

```json
{
  "name": "ReadDataTable",
  "category": "DataTable",
  "description": "Read rows from a DataTable asset. Returns rows as JSON.",
  "requires_approval": false,
  "irreversible": false,
  "batch_eligible": false,
  "min_vessel_version": "0.1.0",
  "tags": [],
  "parameters": {
    "type": "object",
    "properties": {
      "asset_path": {
        "type": "string",
        "format": "vessel/asset-path",
        "description": "Soft path to the DataTable asset."
      },
      "row_names": {
        "type": "array",
        "items": { "type": "string" },
        "description": "Row name keys to read. Empty array means all rows."
      }
    },
    "required": ["asset_path", "row_names"]
  },
  "returns": {
    "type": "string",
    "description": "JSON-serialized rows, keyed by row name."
  },
  "source": {
    "class": "UVesselDataTableTools",
    "function": "ReadDataTable",
    "module": "VesselCore"
  }
}
```

**参数级 `description` 从哪来**:优先读参数级 `meta=(ToolDescription="...")`(写在 UFUNCTION 里不方便,所以一般不这么写);若无,读函数级 doxygen 的 `@param` 段;最后 fallback 到参数名本身。

---

## 4. Tool 的调用生命周期

```
 Orchestrator (Session Machine Executor state)
     │
     │  ToolCall { name: "ReadDataTable", args: {...} }
     ▼
 ┌──────────────────────────────────┐
 │ 1. Lookup in Registry            │
 │    - Not found → FVesselResult::Err("NotFound")
 │    - MinVersion mismatch → Err   │
 └──────────────┬───────────────────┘
                │ found
                ▼
 ┌──────────────────────────────────┐
 │ 2. Validate args against schema  │
 │    - Missing required → Err("ValidationError")
 │    - Type mismatch → Err         │
 └──────────────┬───────────────────┘
                │ valid
                ▼
 ┌──────────────────────────────────┐
 │ 3. If RequiresApproval → HITL Gate│
 │    (blocking until user approves) │
 └──────────────┬───────────────────┘
                │ approved
                ▼
 ┌──────────────────────────────────┐
 │ 4. Wrap in FScopedTransaction     │
 │    (if Category=Write or          │
 │     RequiresApproval=true)        │
 │    Skipped if IrreversibleHint=true (Transaction无意义)│
 └──────────────┬───────────────────┘
                │
                ▼
 ┌──────────────────────────────────┐
 │ 5. Invoke via UFunction::Invoke  │
 │    Catch exceptions, respect timeout │
 └──────────────┬───────────────────┘
                │
                ▼
 ┌──────────────────────────────────┐
 │ 6. Convert result to FString JSON │
 │    Log to Saved/AgentSessions/    │
 └──────────────┬───────────────────┘
                │
                ▼
         Back to Executor
```

**关键不变式**:
- 步骤 2 的 validation 一定在 invoke 之前 —— 绝不把错误参数交给 tool 作者处理
- 步骤 3 的 HITL gate 阻塞是 **async wait**,不卡 UE game thread
- 步骤 4 的 Transaction 包装是 Registry 做的,tool 作者不用关心
- 步骤 5 的 invoke 包在 try/catch(Windows 上 __try,Linux 上 `std::exception` + UE 的 fatal error hook)

> ⚠ **Tool 作者责任 · 必须显式调 `Modify()` 才能让 Transaction 生效**
>
> `FScopedTransaction` 只是**打开**一个事务作用域,**它不会自动记录任何 UObject 变化**。要让 `Ctrl+Z` 真的能撤销,tool 实现里在修改任何 UObject 前**必须**手动调 `Target->Modify()`:
>
> ```cpp
> UDataTable* DT = Cast<UDataTable>(AssetPath.TryLoad());
> DT->Modify();                 // ← 必不可少,否则 Transaction 形同虚设
> DT->AddRow(NewRow);
> ```
>
> 这是 UE Transaction 系统的基础契约。忘记这一步的 tool,HITL 面板上 Ctrl+Z 可用,但按了没反应,用户会以为 Vessel 撒谎。
>
> CI 会做 static analysis 抓这种遗漏(搜 write 类 tool 里的 `Modify()` 调用,缺失即 warning),但最终责任在 tool 作者。

---

## 5. 错误处理

### 5.1 `FVesselResult::ErrorCode` 规范值

| 码 | 含义 | 上层动作 |
|---|---|---|
| `ValidationError` | 参数不合 schema | Judge 直接判失败,让 LLM 重试参数 |
| `NotFound` | Tool 或目标资产不存在 | 提示 agent 换 tool |
| `IoError` | 文件 / 网络读写失败 | Retry 最多 3 次,然后 fail |
| `Timeout` | Tool 执行超时 | Retry 不超过 1 次,超时阈值按 tool 配置(默认 30s) |
| `PermissionDenied` | 项目配置黑名单该 tool,或权限不足 | 不 retry,直接 fail,理由入 session log |
| `Internal` | Tool 内部 assert / crash 被捕获 | **不 retry**,直接 fail,上报 bug report 建议 |
| `UserRejected` | HITL gate 用户拒绝 | Reject reason 入 `AGENTS.md`,agent 换思路 |

### 5.2 Tool 抛异常怎么办

Registry 在 invoke 层包 try/catch:
- 可恢复异常(文件不存在、参数越界)→ tool 作者应返回 `FVesselResult::Err(...)`,**不要**扔异常
- 不可恢复(null 解引用、断言失败)→ Registry 捕获,包成 `ErrorCode=Internal`,session 终止,给用户弹"Vessel 内部错误,见日志"

### 5.3 Timeout

每个 tool 可在 meta 声明 `ToolTimeoutSec="30"`,默认 30 秒。Registry 在 invoke 前启动 async 任务 + cancellation token,超过时间 cancel 并返回 `Timeout`。**对涉及 Editor 主线程操作的 tool,cancel 只能软 cancel**(标记 flag,等下一个安全点),不能强杀 —— 这是 UE 的 threading model 限制。

### 5.4 参数验证失败

在 Registry 层完成,不走 tool:
- 缺必填字段 → `ValidationError` with `missing_field`
- 类型不匹配(LLM 传了 string 但 schema 要 int)→ `ValidationError` with `expected_type` / `actual_value`
- Enum 值越界 → 带上 `allowed_values` 列表返回给 LLM

**重点**:返回给 LLM 的错误信息**必须足够让 LLM 自己改正**。写 "invalid args" 是烂错误;写 "参数 row_names 期望 array of string,收到 string 'foo'" 是好错误。

### 5.5 LLM 返回 JSON 的清洗层(强制)

LLM(尤其是 Claude / Qwen 系列)经常在 tool call JSON 外包一层 Markdown:

````
```json
{"asset_path": "/Game/DT_Foo", "row_names": ["Row1"]}
```
````

UE 的 `FJsonSerializer::Deserialize` 直接喂这个字符串会 false,返回 ValidationError 撞墙。Vessel 的 LLM Adapter 层**必须**在 dispatch 到 Registry 之前包一个清洗器:

1. 剥掉 Markdown code fence(` ```json ... ``` ` / ` ``` ... ``` `)
2. 若仍不合法,取第一个平衡 `{...}` 块
3. 若仍不合法,返回明确的 LLM 可读错误:"你的回复不是合法 JSON,请只输出一个 JSON 对象,不要加 markdown 或解释文字"

这个清洗器属于 **LLM Adapter 职责**,不让每个 tool 作者自己处理。参考实现:`FVesselJsonSanitizer::ExtractFirstJsonObject(const FString& Raw)`。

**不在清洗层硬编码 prompt 工程**(比如"告诉 LLM 只返回 JSON")—— 那是 Planner prompt 的事。清洗层**只做** lexer 级别的容错,不改变语义。

---

## 6. 实战:添加一个新 tool 的 step-by-step

**目标**:让 agent 能列出一个目录下所有资产。

**步骤 1**:在 `Source/VesselCore/Public/Tools/VesselAssetTools.h` 加声明:

```cpp
UCLASS()
class VESSELCORE_API UVesselAssetTools : public UObject {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, meta=(
        AgentTool="true",
        ToolCategory="Asset",
        RequiresApproval="false",
        ToolDescription="List assets under a given content path. Returns array of asset paths."
    ))
    static TArray<FString> ListAssets(
        const FString& ContentPath,
        bool bRecursive);
};
```

**步骤 2**:在 `.cpp` 实现:

```cpp
TArray<FString> UVesselAssetTools::ListAssets(
    const FString& ContentPath, bool bRecursive) {
    FARFilter Filter;
    Filter.PackagePaths.Add(*ContentPath);
    Filter.bRecursivePaths = bRecursive;

    IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        "AssetRegistry").Get();

    TArray<FAssetData> Assets;
    AR.GetAssets(Filter, Assets);

    TArray<FString> Out;
    for (const FAssetData& A : Assets)
        Out.Add(A.GetSoftObjectPath().ToString());
    return Out;
}
```

**步骤 3**:**完整重启 Editor**(新增 `UFUNCTION` 是 UHT 级别变更,Live Coding 不涵盖,`VesselRegistry.Refresh` 也救不了 —— 详见 [§2 / Live Coding 警示](#2-扫描生命周期))。

**步骤 4**:打开 Vessel 面板,在 agent 对话中:"列出 `/Game/Characters/` 下所有资产"。

**无需**:
- 写 Python wrapper
- 维护 schema JSON
- 注册任何回调
- 改 Orchestrator

这就是反射红利。

---

## 7. 权限与过滤

### 7.1 项目级白/黑名单

项目根 `AGENTS.md` 的 `## Tool Policy` 段可以精细控制:

```markdown
## Tool Policy

allow:
  - category: DataTable
  - name: ReadDataTable
  - name: WriteDataTableRow

deny:
  - name: DeleteAsset       # 太危险,不让 agent 碰
  - category: Code/Write    # 这个项目不允许 AI 写 C++
```

Registry 在提供 schema 给 Planner 时**预先过滤**,被 deny 的 tool 根本不进 prompt —— 而不是等 agent 调了再拒绝,避免 LLM 反复尝试。

### 7.2 Agent 级过滤

每个 agent 模板可以声明自己的 `allowed_categories`:

```yaml
# agents/designer-assistant.yaml
allowed_categories: [DataTable, Asset/Read, Meta]
denied_tools: []
```

Registry 做交集:项目 allow ∩ agent allow = 最终可用集。

---

## 8. 测试与调试

### 8.1 Registry 内省

Console commands:
- `VesselRegistry.List` —— 打印所有已注册 tool
- `VesselRegistry.Describe <ToolName>` —— 打印单个 tool 的完整 schema
- `VesselRegistry.Refresh` —— 强制重扫(dev-only,prod 用 hot reload)
- `VesselRegistry.Validate <ToolName> <json-args>` —— 干跑一次 validation 不调用 tool

### 8.2 Unit Test 支持

Registry 提供 mock 入口:

```cpp
FVesselToolRegistry::GetMutable().InjectMock(
    "ReadDataTable",
    [](const FVesselArgs& Args) -> FVesselResult<FString> {
        return FVesselResult<FString>::Ok(TEXT("mocked result"));
    });
```

单测跑完 `ClearMocks()`。

---

## 9. 已知限制与未来扩展

**v0.1 的已知限制**:
- 不支持 `TSubclassOf<T>` 参数
- 不支持异步 tool(tool 必须同步返回或通过 future)
- 不支持 tool 之间的显式依赖声明(agent 要自己理清调用顺序)
- MCP 外部 tool 导入是**预留接口**,未实现

**v0.2+ 计划**:
- 异步 tool 支持(基于 `TFuture`)
- MCP tool 导入(`VesselRegistry::ImportFromMcp(server_url)`)
- Tool versioning(同名多版本共存,agent 可选)

---

*Last reviewed: 2026-04-23 · Registry 的核心 API 在 v1.0 前保留破坏性变更权利。任何变更必须先改本文件,再改代码。*
