## ADDED Requirements

### Requirement: 实现 TranscriptionProvider trait

`KartTranscriptionProvider` SHALL 实现 `kylinbot-channels` 的 `TranscriptionProvider` trait，作为端侧 STT 供应商注册到 `TranscriptionManager`。

- `name()` SHALL 返回 `"kart"`
- `transcribe(audio_data, file_name)` SHALL 通过 `kart-speech-sys::KylinSpeechRecognizer` 执行端侧语音识别，利用 SDK 回调 → mpsc channel 的异步桥接模式等待识别完成
- `supported_formats()` SHALL 返回 `["pcm", "wav"]`（仅支持 PCM 原始数据和 WAV 格式）

#### Scenario: 转录 PCM 音频
- **WHEN** 调用 `transcribe(pcm_bytes, "recording.pcm")`
- **THEN** 返回识别出的文本字符串

#### Scenario: 转录 WAV 音频
- **WHEN** 调用 `transcribe(wav_bytes, "recording.wav")`
- **THEN** 跳过 WAV 头部，提取 PCM 数据后送入 SDK 识别，返回文本

#### Scenario: 不支持的音频格式
- **WHEN** 调用 `transcribe(data, "recording.mp3")`
- **THEN** 返回错误，提示 kart STT 仅支持 PCM/WAV 格式

#### Scenario: SDK 不可用
- **WHEN** `kart-speech-sys::is_available()` 返回 `false`
- **THEN** `transcribe()` 调用返回错误，包含 SDK 不可用的提示信息

### Requirement: 配置驱动的供应商注册

当 `[transcription.kart]` 配置节存在且 `kart-speech` feature 启用时，`TranscriptionManager::new()` SHALL 尝试注册 `KartTranscriptionProvider`。

配置结构 `KartSttConfig` SHALL 包含：
- `model_name: String`（默认值：空字符串，由 SDK 选择默认模型）
- `deploy_type: String`（默认值：`"on_device"`，可选值：`"on_device"` / `"public_cloud"` / `"private_cloud"`）
- `sample_rate: u32`（默认值：`16000`，输入音频的采样率）

#### Scenario: 配置存在且 SDK 可用
- **WHEN** config.toml 包含 `[transcription.kart]` 且运行环境安装了 SDK
- **THEN** `TranscriptionManager` 中注册名为 `"kart"` 的供应商

#### Scenario: 配置存在但 SDK 不可用
- **WHEN** config.toml 包含 `[transcription.kart]` 但 SDK 未安装
- **THEN** 输出 warn 日志并跳过注册，不影响其他供应商

#### Scenario: 配置不存在
- **WHEN** config.toml 中无 `[transcription.kart]` 节
- **THEN** 不尝试注册 `KartTranscriptionProvider`

### Requirement: Feature-gated 编译

所有 kart STT 相关代码 SHALL 被 `#[cfg(feature = "kart-speech")]` 门控，与 TTS 共享同一 feature flag。

#### Scenario: Feature 未启用
- **WHEN** 编译时未指定 `kart-speech` feature
- **THEN** `KartTranscriptionProvider` 类型不存在

#### Scenario: Feature 启用
- **WHEN** 编译时启用 `kart-speech` feature
- **THEN** `KartTranscriptionProvider` 可用
