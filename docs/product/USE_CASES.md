# Vessel · Use Cases

> 三个具体场景,展示 Vessel 在真实工作日里替代了什么动作。每个场景包含:**没 Vessel 之前怎么干 / 有 Vessel 之后怎么干 / 节省了什么**。
> 另有一个 **反用例** —— Vessel 故意不解决的场景 —— 用来展示哲学边界。

画像定义见 [PERSONAS.md](PERSONAS.md)。哲学见 [VISION.md](VISION.md)。

---

## Use Case 1 · 策划批量配 NPC

**主角**:游戏策划(主用户,见 PERSONAS.md 画像 1)

### 任务

> 早上 10:00,策划收到需求:"给 `DT_NPC_Citizen` 加 20 个夜间活动的城市 NPC,各自有独立背景,符合项目世界观。"

### 没有 Vessel · 原本流程

| 步骤 | 时长 | 痛点 |
|---|---|---|
| 打开 `DT_NPC_Citizen`,参照旧行抄 schema | 15 min | 30+ 列的 schema,眼力活 |
| 手工填第 1 行,调整命名规范 | 10 min | 命名要和现有项目对齐 |
| 继续填 19 行,穿插查 Confluence 看世界观设定 | 4 hr | 脑力 + 体力双重疲劳 |
| 填完发现命名不一致,回头统一 | 30 min | 低级错误 |
| 验一遍,提交 Perforce / Git | 15 min | — |
| **合计** | **约 5 小时**(一整个上午 + 下午一部分) | |

### 有 Vessel · 新流程(含真实重试开销)

| 步骤 | 时长 | 动作 |
|---|---|---|
| 打开 UE Editor,点右上角 Vessel 面板 | 5 s | — |
| 输入:"给 `DT_NPC_Citizen` 加 20 个夜间活动的 NPC,沿用现有 schema,符合项目世界观" | 30 s | 自然语言表达意图 |
| Agent 回复:"我准备读 `DT_NPC_Citizen`、参考现有 20 条作为风格锚,生成 20 条新行。确认几点:年龄分布 30–55?避开已有姓名?" | 5 s | 透明的 plan |
| 策划点"是、是",批准计划 | 5 s | 轻量交互 |
| Agent 输出 diff,20 行新行逐行可预览 | 30 s | 可视化 diff |
| **现实插曲**:Validator 报 2 行 age 字段超范围、1 行 name 和已有 NPC 重复 | — | 这是 LLM 生成的常态,不是例外 |
| 策划扫一眼,确认问题,点 "Reject with reason: 'age 必须 30–55'" | 1 min | Reject reason 沉淀到 AGENTS.md |
| Agent 根据反馈重生成问题行 | 1 min | 第二轮 |
| 策划再扫一眼,发现有 3 行背景偏颇,对这 3 行点 "Edit",写改进要求 | 3 min | 精修 |
| Agent 重生成这 3 行 | 1 min | 第三轮 |
| 点 "Approve All" → Transaction 写入 | 2 s | 走 FScopedTransaction,可 Ctrl+Z |
| **合计** | **约 15–20 分钟**(含 2–3 轮 validator / edit 重试) | |

### 节省了什么

- 时间:**5 小时 → 15–20 分钟**(约 **15× 加速**,已计入 LLM 重试 / validator 反驳 / 精修来回)
- 心智:策划没离开 UE Editor,没看代码,没粘贴 JSON
- 可靠性:
  - Schema 由 Vessel 读取,不会填错列
  - Validator 自动跑,命名不一致会被拒
  - 不合规的行策划可 reject,理由沉淀到 `AGENTS.md`,下次 agent 自动避开
- 可撤销:`Ctrl+Z` 永远能回退

### 关键 UX 原则在这里的体现

- **原则 1 透明**:Agent 先说"我准备做什么",再做
- **原则 2 HITL 默认**:策划不按 Approve 就不会写入
- **原则 3 Reject 有价值**:精修的 3 行产生了未来可复用的约束

---

## Use Case 2 · TA 批量导资产

**主角**:技术美术(次要用户,见 PERSONAS.md 画像 2)

### 任务

> 下午 3:00,美术组长交来 200 个新 FBX,命名遵循 `SM_` / `SK_` 前缀,要求:按命名规则导入、跑 validator、不合规的单独标注。

### 没有 Vessel · 原本流程

| 步骤 | 时长 | 痛点 |
|---|---|---|
| 翻上次项目的 Python Editor Script | 10 min | 每个项目不一样,要改 |
| 改里面的命名规则、LOD 设置 | 30 min | 分散的配置 |
| 跑一遍,20 个失败,看 log 排查 | 1 hr | 日志在 Output Log 里难读 |
| 改脚本,再跑,再失败 5 个 | 30 min | — |
| 手工列不合规资产,写进 Confluence 报告 | 20 min | 重复劳动 |
| **合计** | **约 2.5 小时** | |

### 有 Vessel · 新流程

前置准备(TA 只需做一次):写一个 tool

```cpp
UFUNCTION(BlueprintCallable, meta=(AgentTool="true",
    ToolCategory="Asset",
    RequiresApproval="true",
    ToolDescription="Import an FBX with Interchange, auto-tag by filename pattern."))
static FVesselImportResult SmartImportFBX(const FString& Path);
```

之后每次类似任务:

| 步骤 | 时长 | 动作 |
|---|---|---|
| 在 Vessel 面板切到 "Asset Pipeline" agent | 2 s | 预设 agent |
| 输入:"扫 `/Incoming/` 下所有 FBX,按命名规则导入,跑 validator,生成报告" | 20 s | — |
| Agent 列 plan:3 步(list → import each → validate → report) | 5 s | 透明 |
| Agent 进入 batch 模式(已在配置中对 asset 导入类工具授权) | — | 一次性批量授权,不逐个审 |
| Vessel 面板实时滚:导入进度、validator 结果、不合规列表 | 12 min | 观察 |
| 中途有 15 个资产 validator 不过,进入"标红暂停"队列,TA 逐一看 warning 决定 retry / skip / fix | 5 min | 真实的 HITL 介入 |
| 产出报告 `Saved/VesselReports/FBX_Import_2026_04_23.md` | 2 s | — |
| **合计** | **约 20 分钟**(含 warning 处理) | |

### 节省了什么

- 时间:**2.5 小时 → 20 分钟**(约 **7× 加速**,含 warning 逐个处理)
- 复用性:**这套流程可以传给组内其他 TA,甚至策划** —— 因为面板是 Vessel 免费送的,不是 TA 的 Python 手工盘
- 报告自动化:不合规列表已结构化,直接能贴到 Confluence
- 维护性:下次 import 规则变化,改 `SmartImportFBX` 的 C++ 实现,而不是在五个散装 Python 脚本里找

### 关键 UX 原则在这里的体现

- **Batch 模式的边界**:对 read-heavy + 有 validator 兜底的工具开放 batch,对高风险写操作(如改 Blueprint 的 ClassDefaults)不开放
- **Tool 的多态使用**:同一个 `SmartImportFBX` 工具,策划用不到,TA 写完后让 agent 批量调 —— 一次声明,多处复用

---

## Use Case 3 · 程序员加代码审查 agent

**主角**:Gameplay / AI 程序员(第三用户,见 PERSONAS.md 画像 3)

### 任务

> 程序组长说:"每个 PR 合并前,让 AI 先跑一遍蓝图复杂度检查,给个建议。"

### 没有 Vessel · 原本流程

| 步骤 | 时长 |
|---|---|
| 评估要搭:HTTP client、prompt 模板、结果解析、diff UI、审批流、日志 | 0.5 day |
| 从零写调用 Anthropic API 的 C++ / Python 壳 | 1 day |
| 写 Slate widget 显示结果 | 1 day |
| 接 GitHub Actions / CI hook | 0.5 day |
| 调试、联调 | 1 day |
| **合计** | **约 4 天** |

### 有 Vessel · 新流程

| 步骤 | 时长 | 动作 |
|---|---|---|
| 写一个 tool `AnalyzeBlueprintComplexity`(C++ 读 Blueprint 并计算度量) | 1 hr | — |
| 写一个 sensor `ComplexityThresholdValidator`(阈值检查) | 30 min | — |
| 在 Vessel 配置里声明一个 "Code Review Agent",关联上述 tool + sensor + prompt | 15 min | 写 YAML / JSON |
| 调用 `UnrealEditor-Cmd.exe Project.uproject -run=VesselAgent -agent=CodeReview -pr=...`(v0.2 commandlet) | 5 min | 写 CI 脚本 |
| **合计** | **约 2 小时** |

### 节省了什么

- 时间:**4 天 → 2 小时**(16× 加速)
- 复用性:这个 Code Review Agent 可以被其他程序组加新 sensor 扩展(加命名规范检查、GC 压力检查...)
- 维护:外壳基建(HTTP/UI/审批)不是这位程序员的职责了,Vessel 负责

### 关键 UX 原则在这里的体现

- **Tool 声明一次,commandlet 自动可调**:同一个 `AnalyzeBlueprintComplexity` 工具,程序员在 editor 里手动触发也行,CI 自动跑也行
- **Vessel 不替程序员做判断**,只把结构化结果交给 LLM,让 LLM 写 review 建议 —— 最后合并与否还是人决定

---

## 反用例 · Vessel **故意不解决**的场景

这一节和正用例同样重要。它展示 Vessel 的哲学边界 —— 我们不是万能的。

### 反用例 · "帮我从零想 20 个 NPC 背景"

**场景**:策划没有任何 schema,没有任何项目世界观文档,说:"随便帮我想 20 个 NPC 背景,之后我慢慢挑。"

**Vessel 的回应**:不做。

**为什么不做**:

Vessel 的哲学是 **"在约束内填充,不做从零创作"**。理由有三:

1. **可靠性**:没有 schema 就没有 validator,agent 产出的内容可能完全无法直接进 DataTable —— 这违反"每个改动可审计"的承诺
2. **定位**:从零头脑风暴是通用 LLM 的活(ChatGPT / 豆包),不是 Vessel 的活。强行做会把 Vessel 挤进红海的通用 agent 框架赛道
3. **产品反模式**:如果 Vessel 能从零生产"创意素材",策划会产生 "AI 替我想" 的依赖 —— 这破坏"人在方向盘后"的原则

**Vessel 的替代建议**:
- 先在 ChatGPT / 豆包里头脑风暴出 schema 和 3–5 个样例
- 把这些定义写进项目的 `AGENTS.md`(或引用一份世界观文档)
- 再让 Vessel 在**已定义的约束内**批量填充

这个反用例不是 Vessel 做不到,是 Vessel 选择不做 —— 这是哲学 §2.4 "Harness, not Magic" 的直接应用。

---

## 时间节省汇总

| 用例 | 原流程 | 用 Vessel(含重试 / 精修) | 加速倍数 |
|---|---|---|---|
| 策划批量配 NPC | 5 小时 | 15–20 分钟 | ~15× |
| TA 批量导资产 | 2.5 小时 | 20 分钟 | ~7× |
| 程序员加代码审查 agent | 4 天 | 3–4 小时 | ~8× |

**注意**:

- 这些数字是**包含真实 LLM 重试、validator 反驳、HITL 介入**的估计,不是 happy-path demo 数字。广告宣传里常见的 "60×" 假设 LLM 一次正确 —— 那不是真实研发环境
- 实际加速会因项目 schema 复杂度、LLM 质量、validator 覆盖度变化
- **Vessel 的真正价值不是绝对加速倍数,而是让这 15–20 分钟是**可审计、可撤销、可复用的工作,而原本的 5 小时是一次性苦力**
- 真实世界用户反馈收集后会更新这张表

---

*Last reviewed: 2026-04-23 · Update with real user feedback data as collected. Estimates above are engineering intuition, not measured.*
