# Vessel · Coding Style

> 薄的一份。目的是让贡献者不必反复问 "这里要怎么起名"、"这个宏用哪个"。
> 和 UE 社区惯例保持一致,不自创。若本文件和 Epic 官方 [Unreal Engine Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) 冲突,**以官方为准**;本文件只补充 Vessel 特有的约定。

---

## 1. 命名

### 1.1 前缀(和 UE 一致)

| 前缀 | 用于 |
|---|---|
| `F...` | POD struct / 值类型 / RAII 封装(如 `FVesselToolSchema`、`FVesselTransactionScope`)|
| `U...` | 继承 `UObject`,可被 GC / 反射 / Blueprint 访问(如 `UVesselDataTableTools`)|
| `A...` | 继承 `AActor`(Vessel 目前不造 Actor,这条预留给未来可能的 runtime 扩展)|
| `I...` | 纯接口(如 `ILlmProvider`)|
| `E...` | Enum 类型(如 `EVesselAgentState`)|
| `S...` | Slate widget(如 `SVesselPanel`)|
| `T...` | 模板(如 `TVesselResult`)|

### 1.2 命名空间

所有 Vessel 的 C++ 符号都加 `Vessel` 前缀 —— 不用 C++ `namespace Vessel {}` 包裹。

**理由**:UE 反射系统对 `namespace` 支持不完整;UE 社区统一走前缀惯例。

### 1.3 Blueprint 可见类必须 `UVessel*`

任何需要在蓝图里出现的类,必须继承 `UObject`(或子类)并以 `UVessel` 开头。例外:不暴露给蓝图的纯 C++ 内部类可以用 `F...`。

### 1.4 文件命名

- 一个类一个 `.h` + `.cpp` 文件,文件名去前缀:`UVesselToolRegistry` → `VesselToolRegistry.h / .cpp`
- 例外:类很小(< 30 行)且和主类逻辑紧耦合的,可以同文件
- Slate widget 的 `S...` 类也走去前缀惯例:`SVesselChatBox` → `VesselChatBox.h / .cpp`

---

## 2. 注释

### 2.1 Public API 必须带 Doxygen

```cpp
/**
 * Read rows from a DataTable asset.
 *
 * @param AssetPath  Soft path to the DataTable (e.g., /Game/Data/DT_NPC_Civilian)
 * @param RowNames   Row name keys. Empty → read all rows.
 * @return JSON-serialized rows, keyed by row name.
 */
UFUNCTION(BlueprintCallable, meta=(...))
static FString ReadDataTable(const FString& AssetPath, const TArray<FName>& RowNames);
```

### 2.2 Private 实现默认不写注释

只在下列情况下加:
- 非显而易见的 tradeoff(解释为什么这么写,不是写什么)
- UE 反射 / GC / threading 的隐藏约束(容易踩坑,后人需要 context)
- 临时 workaround,带 TODO 和 issue 链接

**不写 "what" 类注释** —— 变量名 / 函数名本身表达 what,注释是浪费。

### 2.3 不引用当前 task / PR

注释里不写 "为了 issue #42"、"这是 XX 的 fix" —— 这些归属 commit message / PR 描述。代码里的注释要能独立于任何历史 context 看懂。

---

## 3. 日志

### 3.1 统一宏

```cpp
VESSEL_LOG(Verbose, TEXT("Tool %s registered"), *ToolName.ToString());
VESSEL_LOG(Warning, TEXT("Tool %s args validation failed: %s"), *ToolName.ToString(), *Error);
VESSEL_LOG(Error, TEXT("Session %s timed out"), *SessionId);
```

**不要**直接用 `UE_LOG(LogTemp, ...)` —— 会混入 UE 全局日志,过滤时分不开。

### 3.2 Log category

Vessel 内部划分:

| Category | 用途 |
|---|---|
| `LogVessel` | 总入口,默认 Display 级别 |
| `LogVesselRegistry` | Tool Registry 扫描和查询 |
| `LogVesselSession` | Session Machine 状态转换 |
| `LogVesselHITL` | HITL Gate 的 approve / reject / edit |
| `LogVesselLlm` | LLM Adapter 调用 trace |
| `LogVesselCost` | Cost tracking |

过滤:命令行 `-LogCmds="LogVesselSession Verbose"`。

### 3.3 敏感信息

**禁止**日志里出现:
- 完整 LLM prompt(可能含项目敏感 context)
- LLM API key / Custom endpoint 的 auth header
- 用户输入的 raw 内容(只记哈希或长度)

Verbose 级别可记 prompt 的**结构摘要**(段落数、tokens 数),不记内容。要看具体 prompt → 打开 `Saved/AgentSessions/*.jsonl`。

---

## 4. 模块与依赖

### 4.1 模块列表

| 模块 | 位置 | 依赖 |
|---|---|---|
| `VesselCore` | `Source/VesselCore` | `Core`、`CoreUObject`、`Engine`、`HTTP`、`Json` |
| `VesselEditor` | `Source/VesselEditor` | `VesselCore`、`UnrealEd`、`Slate`、`SlateCore` |
| `VesselTests` | `Source/VesselTests`(opt-in) | `VesselCore`、`AutomationController` |

### 4.2 依赖规则

- `VesselCore` 是 Runtime module —— 不依赖 `UnrealEd`,可随游戏 build 打包(为将来 runtime agent 预留)
- `VesselEditor` 是 Editor-only,承载 UI / Widget / Commandlet
- **不要**在 Core 里 include Slate —— 会把 Editor 依赖带进 runtime

### 4.3 第三方依赖最小化

能用 UE 自带的就不加新依赖:

| 需求 | 用 | 不用 |
|---|---|---|
| HTTP | `FHttpModule` | libcurl / cpr |
| JSON | `FJsonObject` / `FJsonSerializer` | nlohmann::json |
| SQLite | UE 5 内置的 `SQLiteCore` 插件 | 外部 sqlite amalgamation |
| 文件 IO | `FFileHelper` / `IPlatformFile` | std::filesystem |

例外要在 ADR 里单独论证。

---

## 5. 代码组织

### 5.1 Header 组织顺序

```cpp
#pragma once
#include "CoreMinimal.h"
// ... other includes ...
#include "VesselXXX.generated.h"

// forward decls
class UOtherThing;

// public enums / structs
enum class EVesselFoo : uint8 { ... };
USTRUCT() struct FVesselBar { GENERATED_BODY() ... };

// main class
UCLASS()
class VESSELCORE_API UVesselXXX : public UObject {
    GENERATED_BODY()
public:
    // static UFUNCTIONs first
    // instance public methods
    // UPROPERTYs
private:
    // ...
};
```

### 5.2 include 纪律

- **使用 `CoreMinimal.h`** 作为第一个 include(不要直接 include 具体 core 类)
- 避免 include-in-header,能 forward declare 的 forward declare
- `#include "...generated.h"` 放在最后一个 include(UHT 要求)

### 5.3 TCHAR / FString 的老话

- 字面量用 `TEXT("foo")`
- `FString` 拼接优先用 `FString::Printf`(比 `+` 操作符更快更清晰)
- 和 LLM / JSON 交互大量字符串处理 —— 小心 `FString::Appendf` 对 `%s` 的参数是 `TCHAR*`,传 `FString` 要加 `*`

---

## 6. 并发

### 6.1 线程模型默认

- Slate widget 的 callback 默认在 game thread
- LLM HTTP 调用必须 async,回调通过 `AsyncTask(ENamedThreads::GameThread, ...)` 回到主线程更新 UI
- Tool 执行默认 game thread(因为 UE 反射 / asset 操作几乎都要求 game thread)
- **禁止**在 game thread 做同步 LLM 调用 —— Editor 卡死

### 6.2 锁

- 尽量避免。Vessel 的大部分状态要么是 single-threaded owned(Session Machine 实例),要么是 read-mostly constant(Tool Registry 扫完就冻结)
- 必须锁时用 `FRWScopeLock`(读多写少场景),不要用 `FScopeLock` 无差别上锁
- 禁止嵌套锁(debug 噩梦)

---

## 7. 错误处理

### 7.1 `check` / `ensure` / `verify` 用哪个

- `check(...)`:违反即崩 —— **仅**用于"违反代表整个插件设计假设崩塌"的地方(比如 Tool Registry 的 singleton 必须存在)
- `ensure(...)`:违反报 warning,继续 —— 用于"不应该发生但不至于崩"的情况
- `verify(...)`:Shipping 下也保留条件 —— Vessel 基本不用
- **不要**用 `check` 包裹用户输入 / LLM 输入的验证 —— 用 `FVesselResult::Err` 返回

### 7.2 异常

**不抛异常**(UE 项目惯例)。所有错误路径走 `FVesselResult<T>`。

### 7.3 `nullptr` 处理

- Tool 参数 / 返回值:不用裸指针。用 `FSoftObjectPath` / 资产 path 字符串传递
- 内部 API:入参为指针且可能 null 时,**总是**先检查,不 `check(...)` 暴力断言

---

## 8. 格式化

### 8.1 clang-format

仓库根有 `.clang-format`(借用 Epic 推荐配置),PR 前必须跑一次:

```bash
clang-format -i Source/**/*.h Source/**/*.cpp
```

CI 会验证格式,不通过的 PR 自动 block。

### 8.2 不讨论的风格

以下是 UE 惯例,不在本文件讨论:
- tab vs spaces(UE 用 tab)
- brace 位置(Allman style)
- `*` / `&` 贴类型还是贴名字(贴类型)
- 行宽(120 字符软限制)

有争议直接按 `.clang-format` 走。

---

*Last reviewed: 2026-04-23 · 新的风格约束原则上不进本文件,改 `.clang-format` 让工具强制。*
