# Changelog

All notable changes to Vessel will be documented in this file.

Format loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
**v0.x 不保证 API 稳定,v1.0 发布即承诺 semver 1.x 兼容**(见 [ROADMAP](docs/process/ROADMAP.md))。

---

## [Unreleased]

### Added
- Product book:VISION / PERSONAS / USE_CASES / UX_PRINCIPLES / COMPETITIVE
- Engineering book:ARCHITECTURE(含 ADR 001–007)/ TOOL_REGISTRY / SESSION_MACHINE / HITL_PROTOCOL / CODING_STYLE / BUILD
- Process:ROADMAP / CONTRIBUTING
- Repo-root AGENTS.md(dogfood guides + Tool Policy + Session Defaults + Known Rejections 种子)
- Repo-root README(Hero + Why + 差异化表 + 物理形态 ASCII 图 + Quickstart + 架构一瞥 + 核心概念 + Roadmap + 文档索引)
- `.gitignore`(UE plugin 导向,含 Vessel-specific 路径)
- `.gitattributes`(LF 规范化 + UE 资产 binary 标注)
- `.clang-format`(Epic UE 风格近似)
- `.github/ISSUE_TEMPLATE/`(bug / feature / config)
- `.github/workflows/build-plugin.yml`(pre-v0.1 占位 · 文档 lint + 格式检查 · UE 构建 matrix 预留)

### Changed
- n/a(pre-v0.1,设计仍在动荡期)

### Security
- 文档级别规定 API Key 必须走 `EditorPerProjectUserSettings`,不能进入 `DefaultVessel.ini`

### Code (v0.1-alpha.1 scaffold)
- `Vessel.uplugin` —— 3 个模块声明(VesselCore / VesselEditor / VesselTests)
- `Source/VesselCore/` 骨架:module class、6 个 log categories、`VESSEL_LOG` 宏
- `Source/VesselEditor/` 骨架:module class
- `Source/VesselTests/` 骨架 + `Vessel.Smoke.HelloWorld` automation test(确认两个 module 都正确加载)
- 所有 `.Build.cs` 显式 `bEnableExceptions = false`(符合 CODING_STYLE §7)

---

## 版本规划(待交付)

### [0.1.0] · 目标 6–8 周内

最小可公开展示(Show HN / r/unrealengine)。硬性范围见 [ROADMAP § v0.1](docs/process/ROADMAP.md)。

### [0.2.0]

MCP server / CI commandlet / 第二 agent 模板 / OpenAI + Qwen provider / SQLite FTS5 长期 memory。见 [ROADMAP § v0.2](docs/process/ROADMAP.md)。

### [0.3.0]

Custom Agent Builder · 本地 SLM 降级 · Dev Chat 模板。见 [ROADMAP § v0.3](docs/process/ROADMAP.md)。

### [1.0.0]

API 稳定承诺。
