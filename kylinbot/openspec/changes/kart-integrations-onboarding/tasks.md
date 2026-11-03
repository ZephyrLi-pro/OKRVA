## 1. Provider 列表集成

- [ ] 1.1 在 `crates/kylinbot-providers/src/lib.rs` 的 `list_providers()` 中添加 Kart（`local: true`），受 `#[cfg(feature = "kart-nlp")]` 控制
- [ ] 1.2 验证 Kart 出现在 `kylinbot providers list` 输出中

## 2. Provider 环节 - Kart 基础流程

- [ ] 2.1 在 `providers()` 中识别 Kart provider，跳过 API key 提示
- [ ] 2.2 NLP 模型选择：调用 `kart_nlp_sys::get_model_list()` 获取动态列表，通过 select 菜单展示（显示 display_name）
- [ ] 2.3 NLP 模型选择回退：`get_model_list()` 失败时 catch 错误，回退到手动输入并展示安装 SDK 的提示信息

## 3. Provider 环节 - TTS/STT 配置子步骤

- [ ] 3.1 确保 `kylinbot-runtime` 的 Cargo.toml 能传递 `kart-speech` feature 开关
- [ ] 3.2 NLP 模型选完后，追加 TTS 配置子步骤（受 `kart-speech` feature 控制）
- [ ] 3.3 TTS 流程：启用/禁用（默认否）→ deploy_type 选择（on_device/public_cloud/private_cloud，默认 on_device）→ 模型名输入（placeholder 提示"留空使用默认模型"）
- [ ] 3.4 TTS 启用时写入父级 `[tts]`：`default_provider = "kart"`
- [ ] 3.5 TTS 子级配置写入 `[tts.kart]`（model_name、deploy_type）
- [ ] 3.6 追加 STT 配置子步骤，流程同 TTS
- [ ] 3.7 STT 启用时写入父级 `[transcription]`：`enabled = true`、`default_provider = "kart"`
- [ ] 3.8 STT 子级配置写入 `[transcription.kart]`（model_name、deploy_type、sample_rate=16000）

## 4. Provider 环节 - 文生图配置子步骤

- [ ] 4.1 确保 `kylinbot-runtime` 的 Cargo.toml 能传递 `kart-vision` feature 开关
- [ ] 4.2 语音配置后，追加文生图启用/禁用子步骤（受 `kart-vision` feature 控制，默认否）
- [ ] 4.3 文生图启用时写入 `[image-gen-kart]`（style/尺寸使用 SDK 默认值）

## 5. 测试

- [ ] 5.1 添加单元测试：Kart 跳过 API key、NLP 模型选择（含 SDK 不可用回退）
- [ ] 5.2 添加单元测试：TTS/STT/文生图子步骤的配置写入
- [ ] 5.3 添加单元测试：feature 未启用时对应子步骤被跳过
- [ ] 5.4 验证 OCR、主体分割工具在 `kart-coreai-vision` feature 启用且 SDK 可用时自动生效（无需 onboarding 配置）
- [ ] 5.5 运行完整测试套件确保无回归
- [ ] 5.6 手动测试：运行 `kylinbot onboard`，选择 Kart，验证完整配置流程
