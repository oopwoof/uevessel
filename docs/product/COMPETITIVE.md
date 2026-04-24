# Vessel · Competitive Landscape

> 回答两个问题:**现在市面上有什么 / Vessel 为什么还要存在**。
> 这不是贬低竞品的文档 —— 大多数竞品都做了 Vessel 没做的事。这份文档是**定位说明**,让读者自己判断 Vessel 是不是他要的东西。

哲学总纲见 [VISION.md](VISION.md)。用户画像见 [PERSONAS.md](PERSONAS.md)。

---

## 总览对照表

| 维度 | UnrealMCP | LangGraph / AutoGen / CrewAI | Ubisoft Ghostwriter | **Vessel** |
|---|---|---|---|---|
| **架构形态** | External MCP server | 通用 Python 编排器 | 公司内部工具 | **Engine-native UE plugin** |
| **目标用户** | 个人开发者 | AI 研发 / 研究者 | Ubisoft 内部叙事团队 | **游戏团队(策划/TA/程序)** |
| **引擎耦合度** | 通过 MCP 挂外部 | 引擎无关 | UE + 自研引擎 | **UE 原生,利用反射** |
| **Tool 定义** | 手写 Python wrapper | Python 装饰器 / 类 | 内部 API | **C++ `UFUNCTION` meta 自动导出** |
| **HITL / 审计** | 无 / 弱 | 需自行实现 | 有(内部要求) | **默认 on,Transaction + Reject-reason 沉淀** |
| **Harness 纪律** | 无明确主张 | 偏 primitive 组合 | 公司内部规范,不外露 | **Guides + Sensors 为架构基石** |
| **部署方式** | 启动外部 server | pip install + 自写入口 | 内部 release pipeline | **drop-in UE 插件** |
| **开源状态** | MIT 开源 | Apache 2.0 / MIT 开源 | **不开源** | **Apache 2.0 开源** |
| **目标场景** | 开发者命令行驱动引擎 | 任意 agent 编排 | NPC 对话生成 | **研发期编辑器工作流** |

---

## 逐家分析

### UnrealMCP · 最接近的参照物

**它做了什么(正面)**:
- 第一个让外部 LLM 能"看进 UE"的开源方案
- 采用 MCP 协议,天然兼容 Claude Desktop / Cursor 等客户端
- 社区活跃,有一定 star 数和示例

**它的局限**:
- **架构是 external bridge** —— 每次使用需要启动外部 Python server,策划 / 美术不会装也不会调
- **Tool 定义手写** —— 每加一个能力要写 Python wrapper + 手维护 schema
- **无审计 / 无 HITL 的明确设计** —— 适合单人开发者"我来主导,agent 跟着干",不适合团队协作场景
- **外部身份** —— UE 项目没法把它当"项目的一部分",它永远是外挂

**Vessel 的差异**:
- Engine-native:作为插件进入项目,和 Blueprint / DataTable 同等公民
- Tool 零手维护:C++ 反射一次声明,所有 surface 自动感知
- HITL / Transaction 是架构默认,不是事后补丁
- 团队级场景(策划 / TA / 程序共用),不仅是单开发者

**一句话**:UnrealMCP 让**外部 LLM 能看进 UE**;Vessel 让 **UE 原生长出 agent 能力并给它套上工程级护栏**。

---

### LangGraph / AutoGen / CrewAI · 通用 agent 框架

**它们做了什么(正面)**:
- 丰富的 agent 编排 primitive —— 状态图、消息总线、多 agent 协作
- 活跃的 Python 生态,跟 LLM 厂商集成最完整
- 适合从零定义一个非特定领域的 agent 应用

**它们的局限**(对 UE 研发场景而言):
- **引擎无关,没有 UE 特化**:没法利用反射,没法自动跑 validator,没法包 `FScopedTransaction`
- **Python-centric,和 UE C++ 反射系统融合成本高**:要么搭 Python ↔ UE 桥(性能 + 部署麻烦),要么放弃 UE 原生体验
- **没有编辑器 UI**:得自己写前端(web app / TUI / Electron...),对策划 / 美术零亲和
- **没有审计 / HITL 的 opinionated 默认**:要团队自己决定 + 自己实现

**Vessel 的定位**:
- Vessel **不和它们竞争通用性**。它们是"任意 agent 应用的编排器";Vessel 是"UE 研发专用 harness"
- 长期来看,Vessel 某些组件(如 Session Machine)的设计**受 LangGraph 启发**,但实现是 UE 原生 C++

**一句话**:如果你要做一个**聊天机器人、客服、文档助手**,选 LangGraph。如果你要**在 UE 项目里让 agent 改 DataTable / 资产 / Blueprint**,选 Vessel。

---

### Ubisoft Ghostwriter · 目标参照(非开源)

**它做了什么(公开信息)**:
- 大规模游戏开发中,用 LLM 批量生成 NPC 对话的内部工具
- 和叙事团队深度集成,有 HITL 审查流
- 被视为业界 "agent 参与内容生产" 的先行者之一
- **关键边界**:Ghostwriter 用于**研发管线阶段**生成**静态**对话数据(随游戏 build 一起打包),**不是** runtime 动态生成对话 —— 这和 Vessel "聚焦研发期、不碰 shipping runtime" 的定位是一致的,二者都停在 build 之前

**它的局限**:
- **不开源** —— 其他工作室无法复用
- **聚焦叙事内容(NPC dialogue)**,不是研发管线的通用助手
- 和 Ubisoft 内部引擎 / pipeline 强耦合,迁移成本高

**Vessel 和它的关系**:
- Vessel **不是**要成为下一个 Ghostwriter —— Ghostwriter 聚焦"内容生产(对话)",Vessel 聚焦"研发管线(配置 / 资产 / 测试)"
- Vessel 是**开源 + UE 原生**,填补 "中小型工作室想有 Ghostwriter 体验但没有 Ubisoft 级别资源自研" 的空位
- Ghostwriter 的存在**反向证明**游戏大厂在认真投资这条路径 —— 这对 Vessel 是利好

**一句话**:Ghostwriter 证明了"有 HITL 的 agent 参与游戏研发"在大厂内部行得通;Vessel 把这个思路**开源化 + 通用化 + 引擎原生化**。

---

## Vessel 独占维度的解读

表格里有几行是 **Vessel 独占** 的维度,值得单独解释为什么重要。

### 1. **反射自动导出 tool** —— 工程审美的差异化

没有反射红利时,每加一个 tool 等于写两份:**业务逻辑 + schema/wrapper**。反射自动化之后,一份 `UFUNCTION` 就是全部。

这不只是省代码 —— 更重要的是 **schema 永远和实现同步**。手写 schema 的系统里,业务改了 schema 忘更 = 运行时错误。反射系统里,这种错误在编译期就被消灭。

**对 UE 生态的意义**:Unity 没有 UE 这么强的反射系统。Vessel 的这个架构选择**天然把它和 Unity 拉开距离**,不会被简单移植走。

### 2. **HITL 默认 on** —— 哲学而非技术

技术上 HITL 不难实现 —— 所有框架都能加。问题是**默认值**。默认 autopilot 的框架,HITL 是 "高级用户 opt-in";Vessel 默认 HITL,autopilot 是 "有限 opt-in"。

这是一条**不可调和的分水岭**。企业用户(大厂 / 付费团队)一定选 HITL 默认的;个人 hacker 可能选 autopilot 默认的。Vessel 选了前者,主动放弃后者。

### 3. **Drop-in UE 插件** —— 部署摩擦的差异

- UnrealMCP:用户要装 Python + MCP 依赖 + 启动 server → 策划望而却步
- LangGraph:用户要 pip install + 写入口脚本 → 美术望而却步
- Vessel:把插件丢进 `Project/Plugins/` 重启编辑器 → **任何 UE 用户都会这个动作**

部署摩擦决定触达范围。

---

## 为什么是现在

Vessel 能成立,依赖三个**在 2026 年才同时就位**的前提。少一个,这项目都做不出来。

### 前提 1 · Harness 工程学被命名(Feb 2026)

Martin Fowler 在 2026 年 2 月的文章里提出了 **Guides + Sensors** 分类学,给此前散落的 "prompt engineering / guardrail / evaluation" 实践**第一次有了统一词汇**。

没有这套词汇,Vessel 也能做,但没法把"可靠性作为架构"这件事讲清楚。Fowler 的分类让 Vessel 的 README 第一段就能说"我们遵循 XXX 框架",读者秒懂。

### 前提 2 · UE5 Interchange API 成熟(5.3+)

旧的 asset import 系统(FactoryAssetImport)碎片化严重,没法在"批量资产导入"这个场景里做出流畅体验。UE 5.3 起的 Interchange API 统一了这一层,让 Asset Pipeline Agent 成为可能。

### 前提 3 · LLM tool calling 稳定

2023–2024 LLM tool calling 还是半成品,schema 遵从率经常崩。2025 下半年起,Claude / GPT / Qwen 的 tool calling 已经稳到生产可用 —— 而 agent 工程骨架要求 tool calling **必须可靠**,否则 Session Machine 就会不停 fallback。

### 结论

这三件事凑一起,才有 Vessel 的立足点。**2024 年做这件事太早** —— Harness 词汇没有,Interchange 没好,tool calling 不稳。**2027 年做这件事太晚** —— 大厂会陆续内部化,开源空位收窄。2026 是窗口。

---

## 我们会和谁重叠?

诚实说:如果你已经用得很满意以下方案,**Vessel 对你可能意义有限**:

- 你是 solo dev,UnrealMCP + Claude Desktop 已经够用
- 你是 AI 研究者,LangGraph 的 primitive 灵活性对你重要,引擎特化不重要
- 你是 Unity 用户 —— 暂时我们没有你

Vessel 主要和下面这类需求重叠:

- 游戏团队想让**策划 / TA / 程序在同一套 agent 工具下协作**
- 项目对**可审计 / 可撤销 / 有规范沉淀**有明确要求
- 项目**已经或即将转 UE5**,不愿接入外部 Python server
- 技术领导**明确反对 autopilot**,希望 agent 像一个 "有权限的助手",不是 "自动化脚本"

---

## 不做恶意比较的底线

Vessel 不应该,也不会:

- ❌ 在 README / 推广文案里嘲讽 UnrealMCP
- ❌ 暗示 LangGraph 做得不好 —— 它做得很好,只是不适合这个场景
- ❌ 把 Ghostwriter 描述成 "闭源的垄断"
- ❌ 用"新一代"之类贬低前辈的词

所有比较都应该**描述差异,不贬低选择**。社区记住的永远是开源项目作者的**工程姿态**,不只是技术。

---

*Last reviewed: 2026-04-23 · Re-review when a new major competitor launches, or when UnrealMCP ships a major release.*
