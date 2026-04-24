# Vessel · Roadmap

> 本文件是公开的自我约束。每个里程碑都回答三个问题:**做什么 / 明确不做什么 / 什么时候算完**。任何偏离路线图的功能决策,必须先 PR 这份文件再动手。

项目哲学见 [VISION.md](../product/VISION.md)。用户画像见 [PERSONAS.md](../product/PERSONAS.md)。

---

## 里程碑一览

| 版本 | 时长 | 主题 | 对外承诺 |
|---|---|---|---|
| **v0.1** | 6–8 周 | MVP · 单 agent 端到端 demo | 可被 Show HN / r/unrealengine 展示 |
| **v0.2** | + 4 周 | 外部接入 · CI + MCP + 第二 agent | 团队场景可用 |
| **v0.3** | + 6 周 | 扩展性 · 本地 SLM + Custom Agent Builder | 社区可定制 |
| **v1.0** | + 持续 | 稳定 · API semver 1.x 承诺 | 生产可用基线 |

语义版本:v0.x 期间 API 随时可破,v1.0 发布即 semver 1.x 承诺。

---

## v0.1 · MVP (6–8 周)

**目标**:一个能被 Show HN / r/unrealengine 公开发布而不丢脸的最小版本。核心是**一个端到端可 demo 的策划场景**。

### 必须包含

**Core (Layer 1)**
- [ ] Tool Registry — 扫描 `UFUNCTION meta=(AgentTool="true", ...)`,自动生成 JSON schema
- [ ] Transaction Wrapper — 写操作自动 `FScopedTransaction`
- [ ] Validator Hooks — 对接 `UEditorValidatorBase`,写后自动跑
- [ ] LLM Adapter · Anthropic provider(仅此一家,其他 provider v0.2)
- [ ] LLM Adapter 必须支持 **Custom API Endpoint / Headers** —— 企业网关 / Azure OpenAI 透传 / 自建 proxy。**不绑死**公共 `api.anthropic.com`,否则所有有 IT 合规要求的商业团队从第一天就无法引入

**Orchestrator (Layer 2)**
- [ ] Agent Session Machine — Planner / Executor / Judge 显式状态机
- [ ] 短期 Memory — session context 压缩,长期日志写 JSONL(无 FTS5 检索)
- [ ] HITL Gate — 写操作默认需审批,Reject-with-reason 写入文件

**Surface (Layer 3)**
- [ ] Editor Utility Widget — 聊天 / diff / 批准拒绝,单 agent 类型

**内置工具(3–5 个,聚焦策划场景)**
- [ ] `ReadDataTable`
- [ ] `WriteDataTableRow`
- [ ] `ListAssets`
- [ ] `ReadAssetMetadata`
- [ ] `RunAssetValidator`

**Demo 场景**
- [ ] "给 `DT_NPC_Demo` 添加 20 个夜间活动的城市 NPC" —— 端到端录 30 秒 demo 视频

**文档**
- [ ] README.md 全部章节齐全(Hero / Why / Quickstart / Architecture 简述 / Roadmap 链接)
- [ ] 所有 `docs/product/` 和 `docs/engineering/` 核心文档定稿
- [ ] `CONTRIBUTING.md` 和 `AGENTS.md` 发布
- [ ] 30 秒 demo 视频(YouTube + B 站双发)

**工程基建**
- [ ] GitHub Actions CI:插件编译检查(至少 UE 5.3)
- [ ] Apache 2.0 LICENSE
- [ ] CHANGELOG.md 从 v0.0.1 起写
- [ ] Issue / PR 模板

### 明确不做

- ❌ MCP server(v0.2)
- ❌ CI commandlet(v0.2)
- ❌ 第二个 agent 模板(v0.2)
- ❌ OpenAI / Qwen / MiniMax provider —— 只接 Anthropic(v0.2)
- ❌ 本地 LLM 降级(v0.3)
- ❌ 多 agent 编排(不在计划,至少 v1.0 前不做)
- ❌ 运行时 `GameInstanceSubsystem` NPC agent(不在计划)
- ❌ 长期 memory 的 FTS5 检索(v0.2)
- ❌ Custom Agent Builder GUI(v0.3)
- ❌ Unity 支持(不在计划)

### 退出标准(怎么算 v0.1 完成)

- 一台干净的 Win 机器,从 `git clone` 到看到第一个 agent 回复 ≤ 30 分钟
- Demo 场景能走完整 Approve 路径,`Ctrl+Z` 能撤销
- README.md 的 Quickstart 段能被第三方按步骤跑通
- 30 秒 demo 视频上线
- 在 GitHub 上公开 repo

---

## v0.2 · 外部接入 (+ 4 周)

**主题**:从"单机工具"进入"团队可接入"阶段。

### 必须包含

- [ ] MCP Server — 暴露 Tool Registry 给外部 LLM(Claude Desktop / Cursor)
- [ ] Commandlet 入口 — `UnrealEditor-Cmd.exe ... -run=VesselAgent` 给 CI 用
- [ ] 第二个 agent 模板 — Asset Pipeline Agent(TA 场景)
- [ ] LLM Adapter 扩展 — OpenAI / Qwen provider 各一个
- [ ] 长期 Memory · SQLite FTS5 —— session 可检索
- [ ] Reject-reason → `AGENTS.md` 的自动沉淀流
- [ ] 一个 GitHub Actions 示范 workflow,用 Vessel commandlet 跑 nightly asset validator

### 明确不做

- ❌ Custom Agent Builder(v0.3)
- ❌ 本地 SLM 降级(v0.3)
- ❌ 第三个 agent 模板(v0.3 或根据反馈)

### 退出标准

- MCP server 能从 Claude Desktop 调用 Vessel 工具
- Nightly CI 能产出 asset validator 报告
- 至少一个 TA 场景的公开 blog 样例

---

## v0.3 · 扩展性 (+ 6 周)

**主题**:让社区能定制自己的 agent,不只是用我们预设的。

### 必须包含

- [ ] Custom Agent Builder(v1) — 选 tools / 选 sensors / 写 prompt,保存为 agent 模板
- [ ] 本地 SLM 降级 — 通过 llama.cpp 或 ollama 支持 Qwen-2.5-Coder-3B 级别
- [ ] 第三个 agent 模板 — Dev Chat(代码 copilot)
- [ ] Judge 的 rubric 配置化 —— 不再 hardcode
- [ ] 公共 `SKILL.md` 格式 —— 单 agent 级别的 guides 载体

### 明确不做

- ❌ 多 agent 协作编排(不在计划)
- ❌ Vessel Cloud 托管版(若做则为独立产品线)
- ❌ 图形化的 agent dag 编辑器

### 退出标准

- 可以不写 C++,只改配置,就能产出一个新 agent 模板
- 本地 SLM 能完成 simple read-only 工具调用(不要求复杂场景)

---

## v1.0 · 稳定 (持续)

**主题**:API 稳定承诺,生产可用基线。

### 必须包含

- [ ] Tool Registry 的 public API 冻结;破坏性改动进 v2.0
- [ ] Agent Session Machine 状态集合冻结
- [ ] 所有 `docs/engineering/` 文档被标为 "stable"
- [ ] 至少一家工作室公开使用声明
- [ ] 至少 1 位稳定的外部 contributor(非作者)
- [ ] 完整的 migration guide(v0.x → v1.0)
- [ ] 签名插件(UE Marketplace 或 FAB 可选上架)

### 不作为门槛

- 不要求 star 数 —— v1.0 由 API 稳定性定义,不由传播指标定义
- 不要求商业化收入 —— 商业化若发生,是 Vessel Cloud 另一条产品线的事

---

## 发布节奏

| 阶段 | 动作 |
|---|---|
| v0.1 开发期 (Week 1–6) | 私仓,偶尔 X/小红书 teaser |
| v0.1 公开日 | Show HN + r/unrealengine + r/gamedev + 知乎 + V2EX + 小红书同发 |
| v0.1 后前 2 周 | 只接 bug report,**不合并 feature PR** —— 架构稳定优先 |
| v0.1 后长尾 | 每周一篇 devlog(讲工程问题,不讲 PR 流水) |
| v0.2 启动 | 公开 GitHub Project board,接受外部 feature 贡献 |

---

## 偏离路线图的规则

如果某个功能需要调整优先级:

1. **不要先动手**。先改 ROADMAP.md,PR 讨论。
2. 举证**为什么不能推迟** —— 默认答案是"推迟"。
3. 如果加入会撑爆当期 budget,必须同时提议删减别的东西。

这不是繁琐流程,是保护项目不死的纪律。开源项目最常见的死法是作者一时兴起加 feature,最后没一个做完。

---

*Last reviewed: 2026-04-23 · Review cadence: end of each milestone, or when scope debate arises.*
