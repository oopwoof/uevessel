# Contributing to Vessel

欢迎。Vessel 处于 pre-v0.1 阶段,正在把基座打稳 —— 这意味着我们对贡献**有纪律也有边界**。本文件告诉你**怎么贡献、贡献什么、什么时候别贡献**。

---

## 0. 报 bug / 提 issue 优先

在动手写代码之前,**先开一个 issue**。理由:
- Vessel 架构还在动荡期,功能 PR 可能被我们下一次重构冲掉,对你我都是浪费
- 很多看似 bug 的现象是 "设计如此"(见 [VISION.md](../product/VISION.md))—— 讨论比 PR 便宜

**对所有 issue 我们承诺**:
- 2 小时内给 acknowledge("收到,会看")
- 48 小时内给实质答复(修复计划 / 拒绝理由 / 需要更多信息)

如果超时没回,请 @ 项目维护者。

---

## 1. Issue 规范

### 1.1 Bug 报告必含

- 平台 + UE 版本 + Vessel 版本
- 复现步骤(越精确越好)
- 期望 vs 实际
- `Output Log` 含 `LogVessel*` 的片段
- 如果是 LLM 相关:对应 session 的 JSONL 路径(私密信息自行打码)

### 1.2 Feature 请求必含

- 你要解决的真实场景(不是 "我觉得加 X 会酷")
- 用哪个 [PERSONAS.md](../product/PERSONAS.md) 画像会受益
- 为什么现有机制做不到
- 愿意自己写吗?

**不符合 [VISION.md](../product/VISION.md) "明确不做什么" 范围的 feature 请求,我们会直接关闭 + 解释理由。不是敷衍,是保护项目不漂移。**

---

## 2. PR 规范

### 2.1 PR 前自检

- [ ] 已有对应 issue,或在 PR 描述里解释为什么直接 PR
- [ ] 代码遵循 [CODING_STYLE.md](../engineering/CODING_STYLE.md)
- [ ] 跑过 `clang-format -i` 且无残留 diff
- [ ] 本地编译通过(v0.1 仅 UE 5.7;5.5 / 5.6 兼容是 v0.2 目标)
- [ ] 加了必要的 automation test(或解释为什么不加)
- [ ] 不违反 [UX_PRINCIPLES.md](../product/UX_PRINCIPLES.md) 的反模式清单

### 2.2 PR 描述必含

- 对应 issue 链接
- "What" 和 "Why"(代码 diff 自带 "How")
- 是否涉及哲学级变更(见 §3)
- 测试方式(automation 或手工复现)

### 2.3 我们会 review 什么

- 代码正确性 + 边界条件
- 和 [ARCHITECTURE.md](../engineering/ARCHITECTURE.md) 的跨层依赖规则
- **反模式**(见 HITL_PROTOCOL §8 / UX_PRINCIPLES 末尾)
- 是否引入新依赖(需要 ADR)

我们**不**review 的:个人风格偏好(这些应由 `.clang-format` 或 CI 强制,不是 reviewer 的自由发挥)。

---

## 3. 哲学级变更需先改文档

以下变更**不能**只靠代码 PR:

| 变更类型 | 必须先 PR 的文档 |
|---|---|
| 新 ADR / 改 ADR | `docs/engineering/ARCHITECTURE.md` |
| 加新 UX 原则或修改既有原则 | `docs/product/UX_PRINCIPLES.md` |
| 加新 agent 模板或改默认 agent 行为 | `docs/product/PERSONAS.md` 对应画像 + agent YAML |
| 改 HITL 规则 | `docs/engineering/HITL_PROTOCOL.md` |
| 改 Session Machine 状态集合 | `docs/engineering/SESSION_MACHINE.md` |
| 改 Tool meta 契约 | `docs/engineering/TOOL_REGISTRY.md` §1 |

流程:先 PR 文档,讨论通过合并,再 PR 代码实现。这是为了保证 "设计在代码之前"。

---

## 4. 前两周的 feature freeze

**v0.1 发布后的前 2 周**,维护者**只合并 bug 修复**,不合并 feature PR。

理由:
- 架构还在结构性调整期,过早合并会和即将到来的重构冲突
- 用户反馈的真实 pain point 应该先影响架构设计,再做 feature

feature PR 不会被拒绝,只是被 pin 住,等 2 周窗口过去再讨论。

---

## 5. License 与 DCO

- Vessel 以 [Apache License 2.0](../../LICENSE) 发布
- 所有贡献默认以同一 license 授权
- PR 要求 **DCO signoff**(在 commit message 末尾加 `Signed-off-by: Name <email>`)
  - Git 命令行:`git commit -s`
  - 不做 CLA(轻量原则)

---

## 6. Commit message

格式参考 [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short subject>

<body explains why, not what>

Signed-off-by: Name <email>
```

`<type>` 取值:
- `feat` —— 新功能
- `fix` —— bug 修复
- `docs` —— 只改文档
- `refactor` —— 不改行为的内部重构
- `test` —— 只改测试
- `chore` —— 构建 / CI / 依赖

`<scope>` 推荐:`core`、`editor`、`registry`、`session`、`hitl`、`docs`、`ci`。

---

## 7. Code of Conduct

- 尊重其他贡献者。Issue 和 PR 评论里**针对代码和想法,不针对人**。
- 不在公开 issue / PR 里 @ 非贡献者拉人下水。
- 发现仇恨言论 / 骚扰 / 安全问题 —— 联系维护者私下处理,不在公开区域放大。

具体条款遵循 [Contributor Covenant 2.1](https://www.contributor-covenant.org/)。

---

## 8. 新手友好贡献方向

以下领域对新贡献者友好 —— 技术栈清晰、影响面可控、review 快:

- **新增 Tool**:写一个 `UFUNCTION` + 3–5 个 test case。走 [TOOL_REGISTRY.md §6 的 step-by-step](../engineering/TOOL_REGISTRY.md)
- **新增 Validator**:继承 `UEditorValidatorBase`,接入项目 sensor 链
- **文档改进**:错别字、例子补充、翻译
- **故障排查条目**:你踩过的坑 → [BUILD.md §6](../engineering/BUILD.md) 加一行

不友好方向(除非你已熟悉 Vessel 整体):
- 改 Session Machine 状态集合
- 改 Tool Registry 反射扫描逻辑
- 改 HITL Gate 协议
- 引入新 LLM provider(需要和维护者对齐 interface)

---

## 9. 联系方式

- GitHub Issues / Discussions(首选)
- PR review 在 GitHub 上

**不**通过 email / 微信 / 飞书接受 feature 请求 —— 所有讨论需要有公开记录,方便新贡献者追溯。

---

*Last reviewed: 2026-04-23 · 本文件随项目成熟度演进,v1.0 前规则可能偏严,请理解。*
