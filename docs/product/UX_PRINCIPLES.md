# Vessel · UX Principles

> 七条产品原则。每条都是 **一刀一刻的取舍** —— 可以被违反,但违反前必须先改这份文档,并举证替代方案更好。
> 每条原则包含三段:**原则 / 反面案例 / 我们的决策**。"反面案例"大多来自现有 agent 框架的真实做法;"我们的决策"是 Vessel 的具体兑现方式。

哲学总纲见 [VISION.md](VISION.md)。画像排序见 [PERSONAS.md](PERSONAS.md)。

---

## 原则 1 · Agent 永远透明

**原则**:每个 agent 决策都必须可解释 —— 为什么选这个 tool、为什么这么填参数、为什么认为结果可接受。透明不是 opt-in,是默认。

**反面案例**:竞品普遍展示最终答案,把推理过程藏在后台。用户看到一段"我帮你做好了",但不知道 agent 调了哪些 tool、用了什么参数、踩了哪些失败路径。用户想审查等于逼自己读一份不友好的日志。

**Vessel 的决策**:
- 面板永远显示当前 Session Machine 的状态(🔵 Planner / 🟡 Executing / 🟢 Judge)
- Agent 在写任何东西前必须先给 "plan summary":我准备做什么 / 需要确认什么
- 原始 LLM 回复、tool call trace、token 计数全部可展开查看 —— 默认折叠(不淹没用户),但永远存在入口
- 所有 session 的完整 trace 写 `Saved/AgentSessions/*.jsonl`,支持离线 replay

---

## 原则 2 · Human-in-the-loop 是**默认**,不是 opt-in

**原则**:所有写操作默认需要人审。Autopilot 是 **opt-in** 且受限的 batch 模式,不是主菜单。

**反面案例**:多数 agent 框架默认是 "agent 直接做,用户要 opt-in 才能 review"。这个默认值在研发环境里会**直接导致代码 / 数据被悄悄改坏**。用户发现时往往已经在三天后的 bug 现场。

**Vessel 的决策**:
- 任何 tool 标 `RequiresApproval="true"` 或 `ToolCategory="Write"`,执行前**必然弹 HITL 面板**
- HITL 面板内容:**diff 预览 + agent 理由 + validator 结果** 三段式
- Batch 模式仅对**幂等、或有 validator 全覆盖的自动化管线**开放(例如"批量资产导入 + 命名 validator + LOD validator + 贴图尺寸 validator"四道关卡都通),且必须用户主动开启,不是项目级默认。注意:资产导入本身是**重 write 操作**,之所以能进 batch 不是因为 read-dominant,而是因为 validator 链能把不合规的结果**事后拦截并标红**
- 即使在 batch 模式,validator 报 warning 的单条依然拉回人审

**这条是 Vessel 最反主流的设计**。我们押注:大厂技术决策者真正怕的是失控的 AI,不是"AI 不够自动"。

---

## 原则 3 · Reject 是**有价值的**动作

**原则**:用户拒绝一个 agent 输出,不是失败,是信号。Reject 必须带 reason,reason 自动沉淀成项目级约束。

**反面案例**:多数 agent 框架把 reject 处理成"丢弃这次输出,重新生成"。下次 agent 依然可能犯同样的错,因为拒绝的**理由**没有被捕获。用户拒绝 100 次,agent 学到 0 次。

**Vessel 的决策**:
- HITL 面板的 Reject 按钮**必须**填 reason(空 reason 不让过)
- Reject reason 结构化写入项目根 `AGENTS.md` 的 `## Known Rejections` 段
- 下一次 Planner 的 prompt 自动附带 `AGENTS.md` 内容 —— agent 看到历史拒绝理由,主动规避
- Reject 次数以月为单位被汇总到 `Saved/VesselMetrics/`,方便发现 prompt / sensor 的盲区

**产品后果**:用户拒绝越多,项目越好。这是反直觉但我们相信的闭环。

---

## 原则 4 · Undo 永远可用

**原则**:任何 agent 写操作都必须走 UE 的 Transaction 系统。用户 `Ctrl+Z` 一次,回到写之前的状态。

**反面案例**:部分 UE AI 插件直接调底层 API 改资产 / 数据,绕开 `FScopedTransaction`。一旦 agent 写错,只能靠版本控制救 —— 但策划的本地编辑往往还没提交。

**Vessel 的决策**:
- Core 层的 Transaction Wrapper 对所有标记 write 类的 tool **强制**包 `FScopedTransaction`
- Transaction 描述自动带 session id,便于之后定位 "是哪次 agent 做的"
- Batch 模式下的每一步也是独立 Transaction,不是一个大事务 —— 用户可以只撤销一步而不是全撤
- `Ctrl+Z` 承诺写进 README,作为产品**最重要的心理安全感来源**

**边界说明(诚实版)**:`FScopedTransaction` 的覆盖面仅限于 `UPROPERTY` + 正确调用 `Modify()` 的 UObject 状态变化。以下情况**不在** Transaction 撤销范围:

- 物理文件移动 / 重命名 / 删除(由 Perforce / Git 兜底)
- 外部 HTTP 调用的副作用(例如 agent 向远程服务发请求)
- 第三方资产未实现 Transactional 接口
- Editor 本身状态(窗口布局、选中对象)的改变

Vessel 面板在执行此类操作前**必然**加一层提示 "此改动无法通过 Ctrl+Z 回退,请确保你有版本控制备份" —— 由用户显式确认,不隐瞒。

**为什么这条重要**:心理安全感决定用户敢不敢让 agent 写东西。敢让写,才有使用;不敢让写,Vessel 是废的。

---

## 原则 5 · 成本可见

**原则**:LLM 不是免费的。每个 session 花了多少钱,用户应该实时知道。

**反面案例**:多数 agent 框架把 LLM 成本藏在后台,用户月底看 Anthropic / OpenAI 账单才发现一次操作烧了 $20。缺乏实时反馈 → 用户对成本没直觉 → 不敢放心用,或者用爆预算。

**Vessel 的决策**:
- 面板右下角永远显示 "本 session 已用 $X.XX",每次 tool call 后刷新
- 项目级可设月度 budget 上限,超过后 agent 拒绝启动新 session(不静默 fail,明确提示)
- Cost 追踪分层:prompt tokens / completion tokens / cache hit / tool 调用次数 —— 打开 detail 能看到每一层
- 所有 cost 数据入 JSONL 日志,便于之后做成本回顾

---

## 原则 6 · Offline 降级

**原则**:云端 LLM 不可达时,Vessel 必须优雅降级到本地 SLM 或明确告知用户 "now unavailable"。绝不静默失败。

**反面案例**:网络抖一下,agent 框架卡 30 秒然后抛一堆 stack trace,用户看不懂。或者 fallback 到一个比前者差得多的模型,但不告知用户 —— 用户以为 agent 变笨了,其实是降级生效。

**Vessel 的决策**(v0.3 起完整实现,v0.1 至少做明确告知):
- Provider 层统一接口,本地 llama.cpp / ollama 和云端是同等公民
- 云端不可达 → 先尝试本地 SLM(Qwen-2.5-Coder-3B 级别) → 不行则明确弹窗"当前 LLM 不可用,本次 session 暂停"
- UI 永远显示当前用的是哪个 provider + 哪个模型(不是"AI"这种含糊词)
- 降级生效时面板顶部加黄条提示

---

## 原则 7 · 不做魔法

**原则**:Vessel 不吹 "让 AI 自动搞定一切"。我们吹 "给你一套让 agent 行为可预测的骨架"。

**反面案例**:2026 年 agent 营销普遍是 "autonomous""self-healing""你只要下指令"。这些承诺在 demo 里漂亮,在真实项目里**就是那些让项目组 rollback 的事故来源**。

**Vessel 的决策**:
- README / 宣传口径一律用 "harness" "scaffold" "assist",**绝不用** "autonomous" "magic" "自治"
- 所有能力说明必须附带边界说明 —— 例如"Agent 能批量生成 DataTable 行"后面一定跟"前提是 schema 已定义且有 validator"
- 对看起来太神奇的 feature 请求,第一反应是**拆解成可预测的组件** —— 如果无法拆解,不做
- 宁可 demo 看起来朴素,不要吹牛

**产品后果**:Vessel 不是 "AI 替你工作",是 "工程师用 AI 时的安全带和方向盘"。**这是我们对技术决策者的承诺,也是对 junior 用户的诚实。**

---

## 决策裁判(当原则冲突时)

偶尔原则之间会张力,优先级从高到低:

1. **原则 4 (Undo)** ← 安全
2. **原则 2 (HITL 默认)** ← 信任
3. **原则 1 (透明)** ← 可审计
4. **原则 3 (Reject 有价值)** ← 进化
5. **原则 7 (不做魔法)** ← 诚实
6. **原则 5 (成本可见)** ← 成本理性
7. **原则 6 (Offline 降级)** ← 可用性

换句话说:宁可损失一次"体验丝滑",也不损失 Undo / HITL / 透明。

---

## 反模式清单(我们看到就要警惕)

贡献者 PR 或社区讨论里,出现这些表述时警觉:

- ❌ "这样能减少用户点击" —— 可能在削弱 HITL
- ❌ "这样体验更丝滑" —— 可能在削弱 Undo / 透明
- ❌ "用户可以 opt-in 打开严格模式" —— 不,严格模式是默认
- ❌ "为了简化,我们先不提示成本" —— 不,成本永远可见
- ❌ "这个 agent 可以自主决定 X" —— 检查是不是偷跑 HITL
- ❌ "这样 agent 显得更聪明" —— 聪明不是目标,可靠才是

---

*Last reviewed: 2026-04-23 · Principle changes must be PR'd with a counter-example demonstrating why the old principle fails.*
