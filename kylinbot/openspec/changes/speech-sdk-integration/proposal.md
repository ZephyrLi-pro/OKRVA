## Why

KylinBot 已有多供应商 TTS 和 STT 子系统（TTS: OpenAI、ElevenLabs、Google、Edge、Piper；STT: Groq、OpenAI Whisper、Deepgram、AssemblyAI、Google、Local Whisper），但所有供应商都需要云端 API 密钥或外部服务。Kart 语音 SDK（`libkysdk-coreai-speech`）随每台麒麟桌面出厂，提供零网络依赖的端侧 TTS 和 ASR——使语音能力可离线使用且无 API 费用。该集成参考现有 `kart-nlp-sys` 的回调桥接模式（`Arc<Context>` + `mpsc` channel + `enable_internal_event_loop`）。

## What Changes

- **新建 crate `kart-speech-sys`**：通过 `bindgen` + `libloading` 运行时加载绑定 `libkysdk-coreai-speech.so` 的 FFI sys crate。为合成器（TTS）和识别器（STT）会话 API 提供安全 Rust 封装。包含用于 CI/非麒麟构建的 `stub` feature。
- **新增 TTS 供应商 `kart`**：在 `kylinbot-channels` 中实现 `TtsProvider` 的 `KartTtsProvider`，注册到 `TtsManager`。利用 SDK 回调 → mpsc channel 的异步桥接模式等待合成完成，返回 PCM 音频数据。
- **新增 STT 供应商 `kart`**：在 `kylinbot-channels` 中实现 `TranscriptionProvider` 的 `KartTranscriptionProvider`，注册到 `TranscriptionManager`。使用一次性识别器 API 处理 PCM 音频数据。
- **配置扩展**：新增 `[tts.kart]` 和 `[transcription.kart]` 配置节，包含模型名称和部署类型字段。
- **工作区注册**：将 `kart-speech-sys` 添加为工作区成员，在 `kylinbot-channels` 中添加 `kart-speech` feature flag 门控集成。

## Capabilities

### New Capabilities
- `kart-speech-sys`：为 libkysdk-coreai-speech 提供安全 Rust 绑定的 FFI sys crate（合成器 + 识别器会话、音频配置、模型配置、结果处理）。
- `kart-tts-provider`：作为现有多供应商 TTS 子系统中可插拔 `TtsProvider` 后端的端侧 TTS 供应商实现。
- `kart-stt-provider`：作为现有多供应商转录子系统中可插拔 `TranscriptionProvider` 后端的端侧 STT 供应商实现。

### Modified Capabilities
<!-- 无现有规格级别的需求变更——我们在现有 trait 接口内添加新的供应商实现。-->

## Impact

- **新 crate**：`crates/kart-speech-sys/`（Cargo.toml、build.rs、wrapper.h、src/lib.rs）
- **修改 crate**：`crates/kylinbot-channels/` —— 新增对 `kart-speech-sys` 的可选依赖，在 `tts.rs` 和 `transcription.rs` 中添加 feature-gated 的 `kart-speech` 代码
- **配置 schema**：`crates/kylinbot-config/src/schema.rs` —— 新增 `KartTtsConfig` 和 `KartSttConfig` 结构体
- **工作区**：根 `Cargo.toml` —— 新成员 + feature flag
- **构建依赖**：编译时需要 `libkysdk-coreai-speech-dev`（头文件 + libclang）；运行时需要 `libkysdk-coreai-speech0`。均可通过 `stub` feature 设为可选。
