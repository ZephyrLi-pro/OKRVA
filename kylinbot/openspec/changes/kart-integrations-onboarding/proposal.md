## 为什么

KylinBot 已经集成了多个麒麟本地 AI SDK（统称为 "Kart"）：NLP、语音、文生图、OCR、主体分割。但目前这些功能在初始化向导中无法配置。用户需要手动编辑配置文件才能使用这些本地能力。

## 变更内容

- 在 Providers 环节的选择列表中加入 "Kart"
- 选中 Kart 后，跳过 API key 提示，一站式配置所有能力：
  - **NLP**：从 SDK 动态获取模型列表让用户选择
  - **TTS**：选择部署类型，手动输入模型名或留空用 SDK 默认值
  - **STT**：同 TTS
  - **文生图**：选择启用/禁用
- 纯工具类（OCR、主体分割）默认启用，不在向导中展示
- 通过 onboarding 向导写入 kart 相关配置

## 能力

### 新增能力

- **kart-onboarding**：Kart 本地 AI 能力在 Provider 环节的一站式配置流程

### 修改能力

（无现有 spec 需要修改）

## 约束

- `kart-nlp` feature 是 Kart 出现在 provider 列表中的前提条件。仅启用 `kart-speech` 或 `kart-vision` 而未启用 `kart-nlp` 时，不会在 onboarding 中展示 Kart 配置入口——这些能力仍需手动编辑配置文件。

## 影响

- `crates/kylinbot-providers/src/lib.rs`：在 `list_providers()` 中加入 Kart（`local: true`），受 `kart-nlp` feature 控制
- `crates/kylinbot-runtime/src/onboard/mod.rs`：在 Providers 环节中集成 Kart 配置流程（跳过 API key、NLP 模型选择、TTS/STT/文生图子步骤）
- `crates/kylinbot-runtime/Cargo.toml`：可能需要添加 `kart-speech`、`kart-vision` feature 开关以控制子步骤的条件编译
