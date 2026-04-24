# Vessel

> **An open harness for building AI agents that assist Unreal Engine development.**
> 让 Unreal 策划、美术、程序能用自然语言调用 AI agent 完成研发期任务 —— agent 的每个改动都可预览、可撤销、可审计。

[![status](https://img.shields.io/badge/status-pre--v0.1--alpha-orange)](docs/process/ROADMAP.md)
[![license](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)
[![UE](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-lightgrey)](docs/engineering/BUILD.md)

> ⚠ **pre-v0.1 alpha**. API is unstable. 现阶段是公开的 design phase,代码实现跟随文档推进。进度看 [ROADMAP.md](docs/process/ROADMAP.md)。

---

## 为什么(Why)

现代 agent 工程的共识是 [Agent = Model + Harness](https://martinfowler.com/articles/agent-engineering-guides-sensors.html)(Fowler, 2026)—— 工程价值在 **harness**,不在模型。然而在 Unreal 生态里:

- **通用 agent 框架**(LangChain / AutoGen / CrewAI)不懂 UE 反射 / 反序列化 / 资产管线,做出来的东西策划美术没法用。
- **UE 原生的 AI 插件**(如 UnrealMCP)走 external bridge 路线 —— 需要策划美术装 Claude Desktop 或 Cursor,天然和团队协作隔绝。
- **大厂内部工具**(Ubisoft Ghostwriter 等)不开源。

Vessel 填的是这个空位:**一个 UE 原生、反射驱动、HITL 默认 on、guides+sensors 为架构基石的开源 agent harness**,让策划 / TA / 程序在同一个 Editor 面板里让 agent 参与研发,并对每个改动保留**预览、撤销、审计**三件套。

完整 thesis 见 [VISION.md](docs/product/VISION.md)。

---

## 核心差异化(Differentiation)

| 维度 | 通用 agent 框架 | UnrealMCP 类方案 | **Vessel** |
|---|---|---|---|
| 引擎耦合 | 无 | 外部 bridge | **Engine-native plugin** |
| Tool 定义 | 手写装饰器 | 手写 Python wrapper | **C++ `UFUNCTION` meta 反射自动导出** |
| 审计 / HITL | 需自行实现 | 无 | **默认 on,Transaction + Reject-reason 沉淀** |
| Harness 纪律 | 无 | 无 | **Guides + Sensors 一等公民(Fowler 2026)** |
| 部署 | pip / docker | 外部 server | **drop-in UE 插件** |
| 目标用户 | AI 研究者 | 单开发者 | **游戏团队(策划 / TA / 程序)** |

详细对比见 [COMPETITIVE.md](docs/product/COMPETITIVE.md)。

---

## 它长什么样

Vessel 是一个 UE Editor 内的停靠面板:

```
┌────────────────────────────────────────────┐
│  🔷 Vessel         [Agent: Designer ▼]     │
├────────────────────────────────────────────┤
│  You: 给 DT_NPC_Citizen 加 20 个夜间 NPC   │
│                                             │
│  Vessel:                                    │
│    🔵 Planning → 🟡 Executing → 🟢 Judge    │
│    Plan: Read DT → Generate 20 rows →      │
│           Validate → Upsert                 │
│                                             │
│  [Diff preview · 20 rows]                   │
│  [Validators ✓✓⚠]                           │
│  [Agent reasoning...]                       │
│                                             │
│  [Edit]  [Reject w/ reason]  [Approve]      │
└────────────────────────────────────────────┘
```

每次写操作都**必然**经过 HITL 面板:看 diff → 看 agent 理由 → 看 validator 结果 → 点 Approve / Reject-with-reason / Edit-and-Approve。Reject 理由会**自动**沉淀到项目的 `AGENTS.md`,下一次 agent 看到历史拒绝会主动规避。

(demo GIF/video coming in v0.1 公开发布版本)

---

## 快速开始(Quickstart)

**前置**:Unreal Engine 5.5+,Anthropic API key(或企业 gateway / Azure OpenAI endpoint)。

```bash
cd <YourUnrealProject>/Plugins
git clone https://github.com/<org>/uevessel.git Vessel
```

Generate VS project files → 编译 → 启动 Editor → 填 API key → 打开 `Window → Vessel Chat`。

完整 30 分钟 bootstrap + 故障排查见 [BUILD.md](docs/engineering/BUILD.md)。

---

## 架构一瞥

```
Surface (Slate Dock Panel / Commandlet / MCP Server)
    ↓
Orchestrator (Session Machine / Harness / HITL Gate / Memory)
    ↓
Core (Tool Registry / Transaction Wrapper / Validator / LLM Adapter)
```

三层严格单向依赖。详细取舍与 ADR 见 [ARCHITECTURE.md](docs/engineering/ARCHITECTURE.md)。

---

## 核心概念

### 🔧 Tool Registry(反射驱动)

写一个带 meta 的 `UFUNCTION`,Vessel 自动识别 + 生成 JSON schema + 在所有 surface 可见:

```cpp
UFUNCTION(BlueprintCallable, meta=(
    AgentTool="true",
    ToolCategory="DataTable",
    RequiresApproval="false",
    ToolDescription="Read rows from a DataTable asset."
))
static FString ReadDataTable(const FString& AssetPath, const TArray<FName>& RowNames);
```

规范见 [TOOL_REGISTRY.md](docs/engineering/TOOL_REGISTRY.md)。

### 🧭 Session Machine(显式 FSM)

每次 agent 会话走显式状态机:`Planning → ToolSelection → Executing → JudgeReview → {Approve | Revise | Reject}`。每个状态有 timeout、retry、budget。**不做 LLM 自循环黑箱**。

规范见 [SESSION_MACHINE.md](docs/engineering/SESSION_MACHINE.md)。

### 🚦 HITL Gate(默认 on)

所有写操作必然弹审批面板,三段式内容强制:diff + reasoning + validators。Reject 必须填 reason,自动沉淀到 `AGENTS.md`。不可撤销操作强化警告,二次确认。

规范见 [HITL_PROTOCOL.md](docs/engineering/HITL_PROTOCOL.md)。

### 🛡 Guides + Sensors(Fowler 2026 分类学)

每个 agent 声明它用的 **computational guides**(schema、项目 `AGENTS.md`)和触发的 **inferential sensors**(validator、LLM judge)。可靠性是架构承诺,不是事后补丁。

---

## Roadmap

| 版本 | 主题 | 状态 |
|---|---|---|
| v0.1 | MVP · 单 agent 端到端 | 进行中,目标 6–8 周 |
| v0.2 | MCP server + CI commandlet + 第二 agent 模板 | 规划 |
| v0.3 | 本地 SLM + Custom Agent Builder | 规划 |
| v1.0 | API 稳定承诺 | 远期 |

明确路线图、里程碑和 "明确不做" 清单见 [ROADMAP.md](docs/process/ROADMAP.md)。

---

## 文档索引

**产品册**(理解 Vessel 做什么、为什么、为谁):
- [VISION.md](docs/product/VISION.md) · 项目宪法与四条哲学
- [PERSONAS.md](docs/product/PERSONAS.md) · 三类目标用户
- [USE_CASES.md](docs/product/USE_CASES.md) · 真实场景与加速倍数
- [UX_PRINCIPLES.md](docs/product/UX_PRINCIPLES.md) · 七条产品原则
- [COMPETITIVE.md](docs/product/COMPETITIVE.md) · 竞品对照

**工程册**(理解 Vessel 如何实现):
- [ARCHITECTURE.md](docs/engineering/ARCHITECTURE.md) · 三层架构与 7 条 ADR
- [TOOL_REGISTRY.md](docs/engineering/TOOL_REGISTRY.md) · 反射扫描与 tool 生命周期
- [SESSION_MACHINE.md](docs/engineering/SESSION_MACHINE.md) · Agent 会话状态机
- [HITL_PROTOCOL.md](docs/engineering/HITL_PROTOCOL.md) · 审批协议
- [CODING_STYLE.md](docs/engineering/CODING_STYLE.md) · 代码规范
- [BUILD.md](docs/engineering/BUILD.md) · 编译与调试

**协作册**:
- [ROADMAP.md](docs/process/ROADMAP.md) · 路线图
- [CONTRIBUTING.md](docs/process/CONTRIBUTING.md) · 贡献指南
- [AGENTS.md](AGENTS.md) · 项目级 guides 文件(repo 根)

---

## 贡献

PR / Issue 都欢迎,但请先读 [CONTRIBUTING.md](docs/process/CONTRIBUTING.md)。

**v0.1 发布后前两周**只合并 bug 修复,不接 feature PR —— 架构稳定期优先。对 issue 承诺 2 小时内 acknowledge,48 小时内实质答复。

---

## License

[Apache License 2.0](LICENSE) —— 允许商业使用、修改、再分发。不选 GPL 是刻意的,让商业工作室能安全引入。

---

## Inspiration & Citation

Vessel 的哲学不是凭空而来。主要思想来源:

- **Martin Fowler (Feb 2026)** — Guides + Sensors harness taxonomy
- **Hashimoto** — "Engineer the harness, not the model"; `AGENTS.md` pattern
- **Park et al. (2024)** — *Generative Agents: Interactive Simulacra of Human Behavior*
- **Li et al. (2024)** — *Agent Hospital* multi-agent simulation
- **SimClass** — explicit Session Controller state machine
- **Ubisoft Ghostwriter** — in-studio NPC dialogue agent (non-OSS, reference target)

架构思路亦受 [LangGraph](https://github.com/langchain-ai/langgraph) 的状态图设计启发,但 Vessel 实现是 UE 原生 C++,不依赖 Python 生态。

---

*Vessel is pre-v0.1 software. Expect incomplete features, breaking changes, and incidents.*
*Vessel 当前为 alpha,使用需自行承担风险。欢迎在 issue 里讨论如何一起让它稳定。*
