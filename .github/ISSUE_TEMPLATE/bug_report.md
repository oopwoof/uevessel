---
name: Bug report
about: 报告一个 Vessel 不按预期工作的情况
title: "[BUG] "
labels: bug
assignees: ''
---

<!--
在提交前,请先读 docs/process/CONTRIBUTING.md §1.1。
维护者承诺 2 小时 ack,48 小时给实质答复。
-->

## 环境

- **平台 / OS**: (例:Windows 11, macOS 14.4)
- **Unreal Engine 版本**: (v0.1 仅在 5.7 上验证;若你在 5.5 / 5.6 上遇到问题,请也提交 issue,但兼容是 v0.2 目标)
- **Vessel 版本**: (`Vessel.uplugin` 里的 Version,或 git commit hash)
- **LLM provider**: (Anthropic 公共 / Azure / 自建网关 / Mock)
- **Model**: (claude-sonnet-4-6 / opus / 其他)

## 复现步骤

1.
2.
3.

## 期望 vs 实际

**期望**:

**实际**:

## 日志片段

相关的 `LogVessel*` 输出(可从 `Output Log` 过滤):

```
(粘贴在此,敏感信息请自行打码)
```

## Session 日志(若 LLM / agent 相关)

对应 session 的 JSONL 路径(路径是 `<ProjectRoot>/Saved/AgentSessions/`,**请检查内容**确保不包含敏感项目资产信息再附上):

```
Saved/AgentSessions/vs-YYYY-MM-DD-NNNN.jsonl
```

## 附加上下文

<!-- 截图 / GIF / 其他说明 -->
