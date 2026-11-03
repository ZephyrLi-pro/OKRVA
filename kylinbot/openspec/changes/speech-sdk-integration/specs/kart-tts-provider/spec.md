## ADDED Requirements

### Requirement: 实现 TtsProvider trait

`KartTtsProvider` SHALL 实现 `kylinbot-channels` 的 `TtsProvider` trait，作为端侧 TTS 供应商注册到 `TtsManager`。

- `name()` SHALL 返回 `"kart"`
- `synthesize(text, voice)` SHALL 通过 `kart-speech-sys::KylinSpeechSynthesizer` 执行端侧语音合成，利用 SDK 回调 → mpsc channel 的异步桥接模式等待合成完成
- `supported_voices()` SHALL 返回空列表（端侧模型的可用语音取决于安装的模型）
- `supported_formats()` SHALL 返回 `["pcm"]`

#### Scenario: 合成文本返回 PCM 音频
- **WHEN** 调用 `synthesize("你好", "default")`
- **THEN** 返回原始 PCM 音频字节 `Vec<u8>`

#### Scenario: SDK 不可用
- **WHEN** `kart-speech-sys::is_available()` 返回 `false`
- **THEN** `synthesize()` 调用返回错误，包含 SDK 不可用的提示信息

#### Scenario: 空文本输入
- **WHEN** `text` 为空字符串
- **THEN** 由上层 `TtsManager` 的校验拒绝（不会到达 provider）

### Requirement: 配置驱动的供应商注册

当 `[tts.kart]` 配置节存在且 `kart-speech` feature 启用时，`TtsManager::new()` SHALL 尝试注册 `KartTtsProvider`。

配置结构 `KartTtsConfig` SHALL 包含：
- `model_name: String`（默认值：空字符串，由 SDK 选择默认模型）
- `deploy_type: String`（默认值：`"on_device"`，可选值：`"on_device"` / `"public_cloud"` / `"private_cloud"`）

#### Scenario: 配置存在且 SDK 可用
- **WHEN** config.toml 包含 `[tts.kart]` 且运行环境安装了 SDK
- **THEN** `TtsManager` 中注册名为 `"kart"` 的供应商

#### Scenario: 配置存在但 SDK 不可用
- **WHEN** config.toml 包含 `[tts.kart]` 但 SDK 未安装
- **THEN** 输出 warn 日志并跳过注册，不影响其他供应商

#### Scenario: 配置不存在
- **WHEN** config.toml 中无 `[tts.kart]` 节
- **THEN** 不尝试注册 `KartTtsProvider`

### Requirement: Feature-gated 编译

所有 kart TTS 相关代码 SHALL 被 `#[cfg(feature = "kart-speech")]` 门控。未启用该 feature 时，相关代码不参与编译，对二进制大小和依赖树零影响。

#### Scenario: Feature 未启用
- **WHEN** 编译时未指定 `kart-speech` feature
- **THEN** `KartTtsProvider` 类型不存在，`TtsManager` 不包含 kart 注册逻辑

#### Scenario: Feature 启用
- **WHEN** 编译时启用 `kart-speech` feature
- **THEN** `KartTtsProvider` 可用，`TtsManager::new()` 中包含 kart 注册路径
