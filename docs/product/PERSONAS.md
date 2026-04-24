# Vessel · Personas

> 本文件定义 Vessel 的三类目标用户,并明确**优先级排序**。这不是营销口径 —— 它是**功能决策的裁判**。下次纠结要不要做某个 feature,问一句:"这对主用户(策划)的周二早上有帮助吗?"

---

## 优先级排序

| 优先级 | 画像 | 角色 | Vessel 在他日常里的位置 |
|---|---|---|---|
| **主** | **Game Designer · 游戏策划** | 配 NPC / 关卡 / 数值 | 每天开一次,用来批量生成和改 DataTable |
| 次 | Technical Artist · 技术美术 | 资产管线 / 自动化工具 | 每周用几次,写 tool 给自己和美术组用 |
| 第三 | Gameplay / AI Programmer · 程序 | C++ 业务 / AI / 系统 | 作为 **contributor** 多于 user |

**定义裁判规则**:一个 feature 如果只对程序员有用、对策划没价值 —— 默认推迟到 v0.2+。Vessel 不是"为开发者的开发者工具",而是"为团队的研发工具"。

---

## 画像 1 · Game Designer(主用户)

### 谁

- **年龄**:25–35 岁
- **角色**:产品策划 / 关卡策划 / 数值策划 / 系统策划
- **技术程度**:不写 C++。可能会点 Blueprint,更常用的是 DataTable 和 Editor Utility Widget
- **日常工具栈**:
  - UE Editor(主要战场)
  - Excel / 飞书表格(填数值、对表)
  - Confluence / 飞书知识库(看文档、写策划案)
  - 企业微信 / Slack(和程序沟通)
  - ChatGPT / 豆包(个人用,但结果没法回流 UE)

### 他的一天(没有 Vessel 的版本)

早上 10:00,策划收到任务:"下一个城市分区要加 20 个夜间活动的 NPC,各有独立背景,符合世界观。"

1. 打开 `DT_NPC_Civilian`,照着旧行抄 schema
2. 手工填 20 行,每行 30+ 列
3. 一上午填完 8 个,发现命名不一致,回头统一
4. 中午吃饭时想起漏填一个字段,下午再补
5. 找程序帮忙对一个字段的含义,程序在会,等半小时
6. 晚上临下班验完,提交 Perforce / Git

**结果**:一整天只做了一件事,还累。

### 他的痛点

- **重复性的结构化输入** —— 30 列 DataTable 一遍遍填
- **沟通成本高** —— 加字段、对含义、改枚举都要找程序排期
- **AI 工具用不了** —— ChatGPT 能生成文案,但粘回 UE 要手工,而且常被 validator 打回
- **没有一个能对上 UE 语境的 AI 助手** —— 通用 LLM 不懂他的 schema,也不知道他的项目规范

### Vessel 对他的价值

> "让我用自然语言描述意图,直接变成合法的工程改动。"

- **自然语言 → 合法 DataTable 写入**:说一句话,预览 20 行 diff,逐行改,批量批
- **Schema-aware**:Vessel 读取项目 schema,生成的内容一定符合字段约束
- **可撤销**:`Ctrl+Z` 永远有效,心理安全感
- **项目规范自动沉淀**:策划说"这类 NPC 不能用 30 岁以下",agent 下次自动遵守

### 他**不是**谁

- 他不是"AI 爱好者" —— 他不想配 prompt,不想调 temperature。他想说一句话就工作
- 他不是"想学代码的策划" —— Vessel 不是教育工具。给他一个按钮,不要求他读 JSON
- 他日常角色是**团队中的职业策划** —— 独立开发者身兼策划工作的场景 Vessel 同样适配,只是主要设计场景是团队协作

---

## 画像 2 · Technical Artist(次要用户)

### 谁

- **年龄**:28–38 岁
- **角色**:技术美术 / DCC Pipeline / 引擎内工具开发
- **技术程度**:会 Python、Blueprint,能写 C++ utility。能看懂反射系统但不一定深入
- **日常工具栈**:
  - UE Editor + Editor Utility Widget 开发
  - Python 写 Editor Script(批量导资产、清理命名)
  - 外部工具链(Maya / Blender / Substance / Houdini)
  - 公司内部的资产管线(Perforce / Shotgun / 自研)

### 他的一天(没有 Vessel 的版本)

下午 3:00,美术组长说:"今天有 200 个新 FBX 要导入,帮我自动化一下。"

1. 翻上次项目的 Python Editor Script,复制粘贴
2. 改里面的命名规则、LOD 设置、材质约定(每个项目都不一样)
3. 跑一遍,20 个失败,肉眼看 log 一个个排查
4. 改脚本,再跑,再失败两个,再改
5. 最后成功,把脚本丢给美术组,三周后他们反馈"用不来"

**结果**:脚本每个项目重写一次,组内复用率低。

### 他的痛点

- **Editor Utility UI 难写** —— Slate 陡峭,UMG 简单但不够强
- **没有复用的 agent 骨架** —— 自己想加"跑完后 LLM 总结报告"的能力,要从零接 HTTP、处理 streaming、写重试
- **组内脚本碎片化** —— 每个 TA 一套自己的,同一个命名 validator 被写了 5 次
- **Python Editor Script 调试痛** —— 没有好 IDE 支持

### Vessel 对他的价值

> "写一个 `UFUNCTION`,自动获得 agent UI + HITL 流 + 日志沉淀。"

- **Tool 一次声明,多处复用**:写一个 `UFUNCTION meta=(AgentTool="true")`,Editor 面板、CI commandlet、外部 MCP 都能调
- **UI 是免费的** —— Vessel 给 tool 自动生成 HITL 面板,TA 只写业务逻辑
- **LLM 集成免写外壳** —— Anthropic / Qwen / OpenAI 已经包好,直接用
- **Sensors 系统统一 validator** —— 命名规则、贴图尺寸、LOD 检查都走同一个框架

### 他**不是**谁

- 他不是"全职 tool developer" —— TA 有美术任务,Vessel 必须轻量,不能要求他变成半个程序员
- 他不是"agent 框架研究者" —— 他不想理解 LangGraph vs AutoGen 的区别,他要能用的骨架

---

## 画像 3 · Gameplay / AI Programmer(第三用户,兼 contributor)

### 谁

- **年龄**:25–40 岁
- **角色**:C++ 工程师,Gameplay / AI / 系统方向
- **技术程度**:熟悉反射系统、GC、UObject 生命周期、Subsystem 模式
- **日常工具栈**:
  - Visual Studio / Rider
  - UE Editor + C++ 开发循环
  - Perforce / Git
  - 公司内部性能工具 / trace

### 他的痛点

- **想让项目引入 agent,但工程量大** —— 从零搭 orchestrator / memory / HITL 一个月过去,项目负责人问"业务进度呢"
- **内部 agent 工具不能开源** —— 没法和同行交流
- **LangChain / AutoGen 不贴 UE** —— Python 生态和引擎反射系统融合不进去

### Vessel 对他的价值

> "可扩展的 agent 工程骨架 —— 我只需写 tool 和 validator,不需要重做 orchestrator / HITL / memory。"

- 直接拿插件进项目,改两个配置就跑
- 用 C++ 写 tool,继承公司已有的代码规范
- 反射自动注册,不用手工维护 tool 列表
- Transaction / Validator 已经接好,写 tool 像写普通 UFUNCTION 一样自然

### 他**不是**谁

- 他**不是** Vessel 的日常 user —— Vessel 帮他同事,不是帮他自己填 DataTable
- 他**是** Vessel 最重要的 contributor 来源 —— 外部 PR、新 tool 贡献、架构讨论都来自这群人

### 给这群人的额外承诺

- 代码风格向 UE 社区惯例靠拢,不自创
- 架构文档(`ARCHITECTURE.md`、`TOOL_REGISTRY.md`)写得足够深,能看懂取舍
- 关键决策留 ADR-lite 记录,方便外部 reviewer 追溯

---

## 决策裁判表 · 给未来的自己

| 场景 | 问题 | 默认答案 |
|---|---|---|
| 某 feature 只对程序员有用 | 要做吗? | **不,推迟** |
| 某 feature 需要策划学 JSON / prompt | 要做吗? | **不,找 GUI 替代** |
| 某 feature 对策划价值大但工程复杂 | 要做吗? | **要做,但可能切到 v0.2** |
| 某 feature 让 TA 少写 50 行 Python | 要做吗? | **要做,TA 是重要次要用户** |
| 某 feature 只服务"研究者 / agent 爱好者" | 要做吗? | **不,不是我们的用户** |

---

## 我们**不服务**的画像

明确排除,防止定位漂移:

- ❌ **AI 研究者 / agent framework 研究者** —— 他们想要可组合的 primitive,我们想要可 demo 的工具
- ❌ **非 UE 用户** —— Unity / Godot / 自研引擎用户不是我们的靶点
- ❌ **纯 runtime shipping game 场景** —— Vessel 聚焦研发期,不碰出厂产品的运行时

**关于 Solo Dev**:Vessel 的架构优化目标是团队协作(审计 / HITL / reject 沉淀),但 **Solo Dev 同样可以把它当作个人工作流的严谨护栏** —— 事实上,开源项目早期最热情的用户、bug reporter 和 PR 贡献者大概率就是 Solo Dev 群体,我们**欢迎并服务**这群人,只是他们不是主要**产品决策权重**的那一群。

---

*Last reviewed: 2026-04-23 · Update when a new persona emerges from real user feedback, not speculation.*
