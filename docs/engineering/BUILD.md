# Vessel · Build & Debug

> 目标:一个干净的 UE5 开发机,从 `git clone` 到看到 Vessel 面板第一个 agent 回复,**≤ 30 分钟**。
> 如果你花了超过 30 分钟,算 Vessel 的 bug,欢迎开 issue。

---

## 1. 前置要求

### 1.1 必须

- **Unreal Engine 5.5+**(v0.1 最低支持 5.5;5.6 / 5.7 日常验证;5.4 及以下不支持 —— 5.5 起的 Interchange / 反射行为与 Vessel 假设一致)
- **Visual Studio 2022**(Windows,Desktop + Game Development with C++ workload)
  - 或 **Rider for Unreal Engine 2024+**(推荐)
- **Git**(任意近期版本)
- **LLM API key**:Anthropic(v0.1 唯一内置 provider),或 Azure OpenAI / 企业网关的 endpoint + auth header(见 §4)

### 1.2 可选

- **SQLite 命令行工具**(调试 session log 用,非必须)
- **.NET 6+**(UE 自己会检查,一般已经装了)
- **Python 3.10+**(仅若你要跑 v0.2+ 的可选 Python 扩展)

### 1.3 平台支持矩阵

| 平台 | Editor | Commandlet | Chat Shell |
|---|---|---|---|
| Windows | ✅ 主开发平台 | ✅ | ✅ |
| macOS | ⚠ 验证中 | ⚠ 验证中 | ✅ |
| Linux | ❌ 未验证 | ❌ 未验证 | ✅ |

Windows 是 gold path。其他平台的 issue 欢迎提,但修复优先级低于 Windows。

---

## 2. 快速上手(30 分钟)

### 步骤 1 · Clone 并放入 Plugins 目录

```bash
cd <YourUnrealProject>/Plugins
git clone https://github.com/<你的 org>/uevessel.git Vessel
```

### 步骤 2 · 右键项目的 `.uproject`,Generate Visual Studio project files

或命令行:

```bash
"C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" \
  -projectfiles -project="<ProjectPath>/<Project>.uproject" -game -engine
```

### 步骤 3 · 打开 `.sln`,Build Configuration 设 `Development Editor`,平台 `Win64`

F7 / Ctrl+Shift+B 编译。第一次约 5–10 分钟。

### 步骤 4 · 启动 Editor,打开 Plugins 菜单,确认 "Vessel" 已启用

默认 enabled,如未启用则勾选,重启 Editor。

### 步骤 5 · 配置 LLM provider

Editor 菜单 → `Edit` → `Editor Preferences` → 搜 "Vessel",填:
- Provider:Anthropic(v0.1 唯一)
- API Key:从 https://console.anthropic.com/ 获取
- Endpoint:留空(默认公共 api)或填企业网关(见 §4)
- Model:默认 `claude-sonnet-4-6`(日常研发场景 Sonnet 性价比最佳;Opus 适合复杂 plan / 跨资产重构,Haiku 适合批量简单任务)

点 `Test Connection`,成功后保存。

> ⚠ **安全红线 · API Key 绝不进 Git**
>
> Vessel 的 API Key 声明为 `UCLASS(config=EditorPerProjectUserSettings)`,**只**写入 `<ProjectRoot>/Saved/Config/WindowsEditor/VesselUserSettings.ini`(天然被 UE 默认 `.gitignore` 排除),**绝不**写入 `Config/DefaultVessel.ini`(这个会进 git)。
>
> 这是为什么步骤 5 的菜单是 `Editor Preferences`(per-user)而不是 `Project Settings`(per-project)—— `Project Settings` 的配置会被存到 `Config/DefaultVessel.ini` 并进版本库,把 key 泄露给队友 / 公开 repo。
>
> 企业 / CI 场景请用环境变量 `VESSEL_ANTHROPIC_API_KEY` 或 Custom Endpoint 走企业网关(见 §4),**永远不要**把 key 放 `Config/Default*.ini`。

### 步骤 6 · 打开 Vessel 面板

菜单 `Window` → `Vessel Chat`,面板停靠在右侧。输入任何句子(例:"列出 `/Game/` 下所有 DataTable"),看 agent 规划并调 `ListAssets`。

**成功标志**:面板显示 agent 的 plan,给出资产列表。

如果 30 分钟内没到这一步,见 §6 故障排查。

---

## 3. 依赖管理

Vessel 自己**不带第三方二进制**。编译所需依赖都来自:
- UE 内置(HTTP / JSON / SQLite / Slate)
- Anthropic REST API(运行时,不是编译时依赖)

**不需要做**的事:
- 不需要 `vcpkg install`
- 不需要 pip install
- 不需要手动下载 SDK
- 不需要 Git submodule update(Vessel 暂不用 submodule)

如果你看到插件要你"先装一个外部库",那不是 Vessel 官方流程,请核对来源。

---

## 4. 企业 / Custom API Endpoint 配置

Vessel 对商业团队的承诺(见 [ARCHITECTURE ADR-006](ARCHITECTURE.md))是:**不绑死公共 api.anthropic.com**。

### 4.0 配置文件的分工(读前必看)

Vessel 的配置分两个文件:

| 文件 | 进 git? | 存放内容 |
|---|---|---|
| `Config/DefaultVessel.ini` | ✅ 是,随项目共享 | **只放不敏感的团队级默认**:`Endpoint`(若是公开网关)、`Model`、`AllowHttp` 等 |
| `Saved/Config/WindowsEditor/VesselUserSettings.ini` | ❌ 否,本地 | **所有敏感值**:`ApiKey`、`Authorization` header 里的 Bearer token、`api-key` header 里的 Azure key |

**绝不**把 API Key / Bearer token / Azure key 写进 `DefaultVessel.ini`。Vessel Core 的 settings 类强制把 `ApiKey` 声明为 `EditorPerProjectUserSettings`,写到 `DefaultVessel.ini` 的 key 会被 Editor **在启动时忽略并报 warning**。

### 4.1 Azure OpenAI

**团队共享部分**(`Config/DefaultVessel.ini`):
```ini
[/Script/VesselCore.VesselSettings]
Provider=AzureOpenAI
Endpoint=https://<your-azure-endpoint>.openai.azure.com/
ApiVersion=2024-02-15-preview
Model=gpt-4o
```

**个人敏感部分**(Editor Preferences → Vessel → "Azure API Key",写到本地 `Saved/`):
```
(在 UI 里填,不手工编 ini;填完后 Vessel 写到 VesselUserSettings.ini)
```

### 4.2 自建企业 proxy

**团队共享部分**(`Config/DefaultVessel.ini`):
```ini
[/Script/VesselCore.VesselSettings]
Provider=Anthropic
Endpoint=https://llm-gateway.yourcompany.com/v1
CustomHeaders=X-Team:game-prod         ; 非敏感 routing header 可以放这里
Model=claude-sonnet-4-6
```

**个人敏感部分**(Editor Preferences):
- `Authorization` 的 Bearer token 走 UI 填写,Vessel 运行时自动拼进请求

### 4.3 CI / 无头环境

CI 不能用 Editor Preferences。走环境变量:

```bash
export VESSEL_ANTHROPIC_API_KEY=sk-ant-...
export VESSEL_AZURE_API_KEY=...
export VESSEL_GATEWAY_TOKEN=Bearer_...

UnrealEditor-Cmd.exe Project.uproject -run=VesselAgent ...
```

Vessel 的 settings 类会**优先读环境变量**,然后读 `VesselUserSettings.ini`,最后读 `DefaultVessel.ini`(仅非敏感字段)。

### 4.4 本地测试用的假 endpoint

开发期可用 `mitmproxy` 或 Vessel 自带的 `AnthropicMockProvider`(见 [ARCHITECTURE §2.4](ARCHITECTURE.md))截获请求:

```ini
; DefaultVessel.ini(团队共享 —— OK,此 endpoint 无敏感)
[/Script/VesselCore.VesselSettings]
Endpoint=http://localhost:8080
AllowHttp=true   ; 明确声明接受非 HTTPS,仅限 localhost
```

**安全红线**:`AllowHttp=true` 只在 Endpoint 以 `http://localhost` 或 `http://127.0.0.1` 开头时生效 —— 这是 Core 层 C++ 硬编码检查,不可通过配置绕开。任何外部地址必须 HTTPS。

---

## 5. 插件内部结构

**当前(v0.1-alpha.1 scaffold)**:

```
Plugins/Vessel/
├── Vessel.uplugin                    # plugin metadata (3 modules declared)
└── Source/
    ├── VesselCore/                   # runtime-safe module skeleton
    │   ├── VesselCore.Build.cs
    │   ├── Public/
    │   │   ├── VesselCore.h          # module class
    │   │   └── VesselLog.h           # 6 log categories + VESSEL_LOG macro
    │   └── Private/
    │       └── VesselCore.cpp
    ├── VesselEditor/                 # editor-only module skeleton
    │   ├── VesselEditor.Build.cs
    │   ├── Public/
    │   │   └── VesselEditor.h
    │   └── Private/
    │       └── VesselEditor.cpp
    └── VesselTests/                  # automation tests (DeveloperTool type)
        ├── VesselTests.Build.cs
        └── Private/
            ├── VesselTestsModule.cpp
            └── Tests/
                └── HelloWorldTest.cpp
```

**目标(v0.1 交付时)**:

```
Plugins/Vessel/
├── Vessel.uplugin
├── Config/
│   └── FilterPlugin.ini              # shipping filter
├── Source/
│   ├── VesselCore/
│   │   ├── Public/
│   │   │   ├── ToolRegistry/
│   │   │   ├── Transaction/
│   │   │   ├── Validator/
│   │   │   └── LlmAdapter/
│   │   └── Private/
│   ├── VesselEditor/
│   │   ├── Public/
│   │   │   ├── SessionMachine/
│   │   │   ├── Harness/
│   │   │   ├── HITL/
│   │   │   └── Widgets/
│   │   └── Private/
│   └── VesselTests/
├── Resources/
│   └── Icon128.png
└── Content/                          # demo assets (optional)
```

Core vs Editor 的分割严格按 [CODING_STYLE §4](CODING_STYLE.md) 的模块规则。

---

## 6. 故障排查

### 6.1 编译失败

| 症状 | 大概率原因 | 处理 |
|---|---|---|
| `fatal error C1083: Cannot open include file: 'VesselCore.h'` | 项目 Regenerate 没跑 | 右键 `.uproject` → Generate VS project files |
| `error MSB3073: UnrealBuildTool ... exit code 5` | Antivirus / Dev Drive 权限问题 | 把 UE + 项目 + Plugins 加到杀软白名单 |
| UHT 报 `Unrecognized meta AgentTool` | `VesselCore` 模块没 include 到宏头 | 检查 `Vessel.uplugin` 的 Modules 列表,Enabled=true |
| `LNK2019 unresolved external symbol` | 模块缺了 `PublicDependencyModuleNames` | 改 `VesselEditor.Build.cs`,加回 `VesselCore` |

### 6.2 运行失败

| 症状 | 大概率原因 | 处理 |
|---|---|---|
| Vessel 面板打不开 | Slate 模块未加载 | `Output Log` 搜 "Vessel",看错误堆栈 |
| `Test Connection` 401 | API key 错 | Anthropic console 重新拷贝 |
| `Test Connection` 超时 | 公司网 + 没配企业网关 | 走 §4 的 Custom Endpoint |
| Agent 回复一直 loading | LLM streaming 没 fallback | 调试 `-LogCmds="LogVesselLlm Verbose"` 看细节 |
| Tool 调用 `NotFound` | Registry 没扫到该 tool | `Output Log` 输入 `VesselRegistry.List`,确认是否注册 |

### 6.3 Tool 注册但 Planner 看不到

Planner 看到的 tool 列表 = 全 Registry ∩ 项目 policy allow ∩ agent 模板 allowed_categories。检查:

```
VesselRegistry.List                     # 全量
VesselRegistry.Effective <agent-name>   # 项目 × agent 过滤后
```

---

## 7. 调试技巧

### 7.1 打断点到 Tool Registry 扫描

断点位置:`UVesselToolRegistry::ScanAll` 的循环内。看每个被扫到的 `UFunction*` 的 meta 值。

### 7.2 看 agent 发出的原始 LLM prompt

**不要**从日志看(日志出于安全只记结构摘要,见 [CODING_STYLE §3.3](CODING_STYLE.md))。打开对应 session 的 JSONL:

```
<ProjectRoot>/Saved/AgentSessions/vs-<date>-<n>.jsonl
```

里面每条 `PlanningRecord` 都有完整 prompt + response。

### 7.3 Replay 一次 session

```bash
vessel replay vs-2026-04-23-0001              # 跑完
vessel replay vs-2026-04-23-0001 --stop-at 5  # 停第 5 步
vessel replay vs-2026-04-23-0001 --dry-run    # 不执行 tool,只打印流程
```

### 7.4 Automation Test

```bash
"UnrealEditor-Cmd.exe" <Project>.uproject -ExecCmds="Automation RunTests Vessel.;Quit"
```

或 Editor 里 `Window` → `Test Automation` 勾选 `Vessel.*`。

### 7.5 性能 profile

Vessel 自身开销极低(Registry 扫描一次性,Session Machine 调度轻量)。如果 Editor 卡,多半是 tool 实现阻塞 game thread,按 [CODING_STYLE §6](CODING_STYLE.md) 检查。

---

## 8. 开发循环建议

### 8.1 改 C++ 函数体 → Live Coding 即可

UE 5.5+ 的 Live Coding 对**已有 `UFUNCTION` 的函数体实现**(`.cpp` 内部逻辑)改动能热更新。例如:改 `ReadDataTable` 里的解析逻辑、改 Session Machine 某个状态的 tick 实现 —— 都可以直接 Ctrl+Alt+F11 热更。

### 8.2 **新增 `UFUNCTION` / 改 meta / 改 UPROPERTY / 改 USTRUCT 必须重启 Editor**

⚠ 这是 UE5 Live Coding 的经典坑 —— Live Coding **不**重跑 UHT,也**不**重建反射表。下列改动 Live Coding **都不会生效**,必须完整重启 Editor:

- 新增标 `meta=(AgentTool="true")` 的 `UFUNCTION`(Registry 看不到)
- 改已有 `UFUNCTION` 的 `meta` 值(Registry 旧 schema 不刷新)
- 新增 / 删除 `UPROPERTY`
- 改 `USTRUCT` 布局
- 改 `UCLASS` 继承关系

`VesselRegistry.Refresh` **只能**在 UHT 已经跑过的前提下强制重扫反射表 —— 它**不能**替代重启。"写个新 tool 然后 console 里 refresh"这条路走不通,别尝试。

### 8.3 改 Slate / SDockTab UI → 支持 Reload

Slate widget 大部分改动 Live Coding 也能推;若不生效,关 Editor 重开。

---

## 9. CI / 自动化

仓库根 `.github/workflows/build-plugin.yml` 在 PR 时自动跑:
- Windows UE 5.5 编译(最低支持基线)
- Windows UE 5.7 编译(作者日常开发版本)
- clang-format 检查
- 基础 automation test

**在本地跑一次 full build** 推荐放到 pre-push hook,避免推了才发现 CI 红。

---

## 10. 提问与报错

不顺利就直接开 issue,请带:
- 平台(Win / Mac / Linux 版本号)
- UE 版本
- `Vessel.uplugin` 的 version(Vessel 自己的版本)
- `Output Log` 含 `LogVessel*` 的片段
- 如果能 reproduce,附一份最小复现步骤

不用客气,我们修不好的 issue 会坦率讲 "这不修",不会让你 issue 石沉大海。

---

*Last reviewed: 2026-04-23 · 30 分钟 bootstrap 目标是本项目的 SLA,不达标即 bug。*
