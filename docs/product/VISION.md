# Vessel · Vision

> 本文件是 Vessel 项目的宪法。它回答三个问题:**我们在赌什么 / 我们为谁而做 / 我们明确不做什么**。其他一切文档、代码决策、路线图,都应能回头对齐到这里。

---

## 1. 一句话定义

**Vessel 让 Unreal 策划、美术、程序能用自然语言调用 AI agent 完成引擎内的研发任务 —— agent 的每个改动都可预览、可撤销、可审计。**

英文 tagline(用于 GitHub / Twitter):
> *An open harness for building AI agents that assist Unreal Engine development — designer config, asset pipeline, automated testing, and NPC simulation scaffolding.*

**物理形态**:Vessel 是一个 UE Editor 内置的原生 Slate 停靠面板(`SDockTab`) —— 聊天输入框 + Agent 状态可视化 + Diff 预览视图 + HITL 审批按钮。用户**不离开 Editor、不写代码**:在面板里描述意图 → 看 agent 的 plan → 预览 diff → Approve / Reject / Edit → 改动经 `FScopedTransaction` 写入。此外还有命令行 TUI(开发者)、CI Commandlet(自动化)、MCP Server(外部 LLM)三个辅助 surface,但**策划 / TA 日常只需认 Editor 面板这一个**。

---

## 2. 我们在赌什么 · 四条哲学原则

这四条原则是差异化的核心。每一个功能决策都必须通过它们的四重检验。

### 2.1 Engine-native, not Engine-bridging

Vessel 作为 C++ 插件**活在 UE 内部**,不是一个让外部 LLM 连接进来的 MCP server。

现有方案 UnrealMCP 走的是 bridge 路线:把 UE 变成 MCP server,让 Claude Desktop / Cursor 在外部操作它。Vessel 反过来 —— 因为游戏研发的真实用户是整个团队,而不是每个策划和美术都去装 Claude Desktop。Agent 能力必须在 UE Editor 里**开箱即用**,和 Blueprint、DataTable 是同一等级的公民。

### 2.2 Reflection-first Tool Registry

**一行 C++ meta 改动,所有上层自动感知。**

```cpp
UFUNCTION(BlueprintCallable,
  meta=(AgentTool="true",
        ToolCategory="DataTable",
        RequiresApproval="false",
        ToolDescription="Read rows from a DataTable asset."))
static FString ReadDataTable(
  const FString& AssetPath,
  const TArray<FName>& RowNames);
```

注册一次,Editor 面板、CI commandlet、外部 MCP、测试 mock 读同一份 schema。Unreal 的反射系统是它相对 Unity 最大的资产 —— Vessel 是第一个明确把这个红利作为架构基石的 agent 框架。

### 2.3 Guides + Sensors 是一等公民

Vessel 采纳 Martin Fowler 2026 年 2 月提出的 **Guides / Sensors** 分类学作为架构中心。每个 agent 都必须显式声明:

- 它使用哪些 **computational guides**(schema 约束、`AGENTS.md`、规则文件)
- 它触发哪些 **inferential sensors**(validator、compile check、LLM judge with rubric)
- 它产生什么 **audit trail**

当前开源 agent 框架普遍把 guardrail 当事后补丁。Vessel 从 day 1 把可靠性当架构承诺。

### 2.4 Harness, not Magic

Vessel **不承诺** "AI 自动搞定一切"。

Vessel **承诺** "给你一套工程骨架,让 agent 的行为可预测、可审计、可回滚"。

具体兑现方式:

- 所有写操作自动包 `FScopedTransaction` —— 对 UObject 属性级改动,`Ctrl+Z` 可完整撤销。涉及文件系统操作、外部 HTTP 副作用、或未实现 Transactional 接口的第三方资产,Vessel 会**在执行前明确标注** "此改动无法通过 Ctrl+Z 回退",由版本控制(Perforce / Git)兜底
- 所有写操作**默认**走 HITL 审批,不是 opt-in
- Reject-with-reason 是有价值的信号 —— reason 自动沉淀到 `AGENTS.md`,下次 agent 避开同样的错
- 成本对用户透明,每次 session 显示 LLM 花费

这是 Vessel 最反主流的一条。在 2026 年 "AI agent 自治" 的叙事里,Vessel 卖的是**审慎** —— 而这恰好是大厂技术领导者真正在买的东西。

---

## 3. 我们为谁而做

三个画像(详见 [PERSONAS.md](PERSONAS.md)),锚定优先级:

| 优先级 | 画像 | 痛点 | Vessel 的价值 |
|---|---|---|---|
| **主** | 游戏策划 | 填 30 列 DataTable,和程序沟通成本高 | 自然语言 → 合法的引擎改动,带预览 / 撤销 |
| 次 | 技术美术 (TA) | 重复写资产 import validator,组内脚本碎片化 | 写一个 `UFUNCTION`,自动获得 agent UI + HITL 流 |
| 第三 | Gameplay / AI 程序员 | 项目内部 agent 工具没共用骨架 | 可扩展的工程骨架,聚焦写 tool 和 validator |

**策划是 v0.1 的裁判。** 任何功能决策问一句:"这对策划周二早上的工作有帮助吗?"如果没有,延后到 v0.2 以后。

---

## 4. 我们明确不做什么

这一节比 "我们做什么" 更重要。它是防止项目飘的边界。

- ❌ **不做通用 agent 框架** —— 不和 LangChain / AutoGen / CrewAI 竞争。Vessel 是 UE 专用 harness,不是另一个编排器。
- ❌ **不做运行时 NPC 仿真**(至少 v1.0 前) —— Vessel 聚焦研发期,不碰 shipping game 的运行时。`GameInstanceSubsystem` 层面的长期 NPC agent 是另一个项目的范畴。
- ❌ **不做 Unity 版本**(至少前两个大版本) —— 反射优势是 UE 独有的,跨引擎会稀释差异化。
- ❌ **不做 "autopilot" 默认模式** —— 所有写操作默认需要人审,这是哲学不是 bug。Batch 模式只对**幂等、或 validator 可完整拦截的自动化管线**开放,且必须用户主动开启。
- ❌ **不做云端 LLM 加价代理** —— 用户用自己的 API key 直连,Vessel 不在中间抽税。
- ❌ **不做 GPL 授权** —— 采用 Apache 2.0,让商业工作室能安全引入。
- ❌ **不做向量数据库依赖** —— 长期日志用 SQLite FTS5,避免依赖膨胀。

---

## 5. 成功的定义

衡量 Vessel 成败的标尺必须提前钉死,不然后面会自我欺骗。

### v0.1 (发布后 30 天内)
- ✅ GitHub ≥ 50 star
- ✅ ≥ 3 位真实用户提交非作者的 issue(bug 报告或 feature 请求均算)
- ✅ 一条 Show HN 或 r/unrealengine 的帖子进入当日前 10
- ✅ 一段 30 秒的编辑器内 agent 端到端 demo 视频

### v1.0 (目标:v0.1 后约 6 个月)
- ✅ 进入 Awesome-Unreal 列表
- ✅ 至少一家游戏工作室公开表态在使用(哪怕只是个人项目)
- ✅ API 稳定承诺(semver 1.x 内不破坏)
- ✅ 至少 1 位稳定的外部 contributor

### 不作为成功标准的东西
- ❌ 不以 "多少 feature" 为标准 —— 做多不做精会稀释项目
- ❌ 不以 star 数单独为标准 —— star 可以炒,活跃度和真实使用才是信号
- ❌ 不以商业化收入为标准 —— v1.0 前 Vessel 不考虑收费产品线

---

## 6. 节奏与纪律

Vessel 采取 **6–8 周 MVP** 的发布节奏 —— 先让一个能端到端 demo 的最小版本走出门,再谈扩展。任何想要破坏这个节奏的功能,必须举证自己不是 v0.2 可以推迟的事。

运行纪律:

- **砍 feature 是默认动作,加 feature 需要举证**
- **v0.1 发布后前两周只接 bug 报告,不合并 feature PR** —— 架构还在动荡期,过早合并会让后续调整痛苦
- **每周一篇 devlog** —— 讲项目遇到的一个有意思的工程问题和解法,不是 PR 流水账
- **公开路线图**(见 [ROADMAP.md](../process/ROADMAP.md))既是对读者的承诺,也是对自己的约束 —— 任何偏离都必须先更新路线图再动手
- **版本号 semver** —— v0.x 不保证 API 稳定,v1.0 发布即承诺兼容

详细里程碑见 [ROADMAP.md](../process/ROADMAP.md)。

---

## 7. 引用与思想来源

Vessel 的哲学不是凭空而来。主要思想来源:

- **Martin Fowler (Feb 2026)** —— Guides + Sensors harness 分类学
- **Hashimoto** —— "Engineer the harness, not the model" / `AGENTS.md` 模式
- **Park et al. (2024)** —— *Generative Agents: Interactive Simulacra of Human Behavior*
- **Li et al. (2024)** —— *Agent Hospital* 多 agent 仿真
- **SimClass** —— Session Controller 显式状态机
- **Ubisoft Ghostwriter** —— 内部 NPC dialogue agent 工具(非开源,作为目标参照)

---

*Last reviewed: 2026-04-23 · This document changes slowly. Significant revisions must be PR'd with rationale.*
