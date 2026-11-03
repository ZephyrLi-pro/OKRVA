## Context

KylinBot 当前的语音子系统由两个 Manager 驱动：`TtsManager`（文字转语音）和 `TranscriptionManager`（语音转文字），均位于 `kylinbot-channels` crate。每个 Manager 维护一个 `HashMap<String, Box<dyn XxxProvider>>`，支持多供应商注册和按名路由。

Kart 语音 SDK（`libkysdk-coreai-speech.so`）是一个 C 库，随麒麟桌面系统安装，提供：
- **合成器（TTS）**：异步回调式 API，文字 → PCM 音频数据
- **识别器（STT）**：异步回调式 API，PCM 音频 → 文字，支持一次性识别和连续识别
- 支持端侧模型和云端模型，通过 `ModelDeployType` 枚举切换

参考实现：
- **`kart-nlp-sys`**：绑定 `libkysdk-genai-nlp.so`，同样是异步回调驱动的 C SDK。使用 `Arc<CallbackContext>` + `Arc::into_raw` 将回调上下文交给 C 层，通过 `tokio::sync::mpsc::UnboundedSender` 将回调数据桥接到 async 世界。支持 `enable_internal_event_loop`。Speech SDK 的 `SpeechSynthesisResultCallback` / `SpeechRecognitionResultCallback` 与 NLP SDK 的 `ChatResultCallback` 模式一致，kart-nlp-sys 是回调桥接、session 管理和构建基础设施的全面参考。

## Goals / Non-Goals

**Goals:**
- 将 `libkysdk-coreai-speech` 的 TTS 和 STT 能力接入 KylinBot 的现有 Provider 架构
- 遵循 `kart-nlp-sys` 的回调桥接模式（`Arc<Context>` + `mpsc` channel + `enable_internal_event_loop`）和构建基础设施（bindgen、env var 搜索路径、stub feature）
- 用户可通过 `config.toml` 中的 `[tts.kart]` / `[transcription.kart]` 启用端侧语音
- 保持与其他供应商完全并行——不破坏现有 TTS/STT 功能

**Non-Goals:**
- 连续流式识别（streaming ASR）——本次仅实现一次性识别（`recognize_once_async`），流式识别可后续扩展
- AudioDataStream 流式输入——本次 STT 仅支持 PCM 数据块输入
- 麦克风/扬声器直连——本次 TTS 输出为内存 PCM 数据，STT 输入为 PCM 字节，不直接操作硬件设备
- 语音唤醒词集成——不在本次范围内

## Decisions

### 1. 独立的 `kart-speech-sys` crate

**选择**：创建单独的 `crates/kart-speech-sys/`，与 `kart-nlp-sys` 平级。

**理由**：Speech SDK（`libkysdk-coreai-speech.so`）有独立的头文件和符号空间，作为独立 crate 可使 `stub` feature 与其他 SDK crate 互不影响。遵循 `kart-nlp-sys` 的整体 crate 结构（Cargo.toml、build.rs、wrapper.h、src/lib.rs）。

### 2. TTS：回调 → mpsc channel → 异步收集（参考 kart-nlp-sys）

**选择**：参考 `kart-nlp-sys` 的 `chat_stream()` 模式：创建 `Arc<SynthesisCallbackContext>` 包含 `mpsc::UnboundedSender`，通过 `Arc::into_raw` 交给 C 回调，在 `unsafe extern "C" fn` 回调中解析 `SpeechSynthesisResult`，将音频块发送到 channel。Provider 端 `await` 接收所有 chunk 直到 `SPEECH_SYNTHESIS_COMPLETED`，拼接后返回 `Vec<u8>`。使用 `enable_internal_event_loop(true)` 驱动事件循环。

**理由**：与 NLP SDK 的回调桥接模式完全一致——`SpeechSynthesisResultCallback` 的签名 `fn(result, user_data)` 与 `ChatResultCallback` 相同。使用 tokio mpsc channel 而非 condvar 可以自然地与 async Provider trait 集成，无需 `spawn_blocking`。

**备选**：`Mutex<Vec<u8>>` + `Condvar` 同步收集 + `spawn_blocking` —— 可行但不如 mpsc channel 惯用，且与 nlp-sys 已验证的模式不一致。

### 3. STT：一次性识别 + mpsc channel 等待结果

**选择**：用 `audio_config_create_once_audio_input_from_pcm_data` 创建音频输入配置，注册 `SpeechRecognitionResultCallback`（同样使用 `Arc<Context>` + `mpsc` 模式），调用 `speech_recognizer_recognize_once_async`，通过 channel 等待 `SPEECH_RECOGNIZED` 结果。

**理由**：`TranscriptionProvider::transcribe()` 接收完整音频字节，适合一次性识别。回调桥接方式与 TTS 和 nlp-sys 保持一致。

### 4. Feature-gated 集成

**选择**：在 `kylinbot-channels` 中通过 `kart-speech = ["dep:kart-speech-sys"]` feature 门控，根 Cargo.toml 暴露 `kart-speech = ["kylinbot-channels/kart-speech"]`。

**理由**：与 `kart-nlp-sys` 在 `kylinbot-channels` 中的集成方式一致。非麒麟环境不启用该 feature，零编译开销。

### 5. Session 管理：参考 kart-nlp-sys 的 RAII 模式

**选择**：`KylinSpeechSynthesizer` / `KylinSpeechRecognizer` 持有 `*mut Session` + `callback_raw: *const Context`，参考 `KylinNlpSession` 的 RAII 模式。Drop 时先销毁 session（停止回调），再通过 `Arc::from_raw` 回收交给 C 的 refcount。

**理由**：与 nlp-sys 已验证的回调生命周期管理模式一致。Drop 顺序（先 session 后 context）是正确性的关键——session 销毁后 C 层不再触发回调，此时回收 raw pointer 是安全的。

每次 `synthesize()` / `recognize()` 调用创建新 session（短生命周期），因为 TTS/STT 调用频率远低于 NLP 的多轮对话。

**备选**：维持长生命周期 session（如 nlp-sys 的 `KylinNlpSession`）+ 每次调用重新注册回调 —— 可作为后续性能优化。

## Risks / Trade-offs

- **[SDK 不可用]** → 通过 `is_available()` 运行时检测，Manager 初始化时 warn 并跳过注册，不影响其他供应商。Stub 模式保证编译通过。
- **[音频格式不匹配]** → 现有 `TranscriptionProvider` 接收多种格式（ogg/mp3/wav 等），但 Kart STT 仅接受 PCM。需在 provider 层做格式检查或转换。本次先限制为仅接受 PCM 输入，后续可集成音频格式转换。→ STT provider 的 `supported_formats()` 返回 `["pcm", "wav"]`。
- **[回调线程安全]** → 参考 nlp-sys 的 `Arc<CallbackContext>` + `mpsc::UnboundedSender` 模式。回调中仅执行 `tx.send()`（无锁、线程安全），不阻塞 C 线程。
- **[session 创建开销]** → 每次调用创建/销毁 session。若延迟不可接受，后续可改为 session pool。
- **[TTS 输出格式固定为 PCM]** → 与其他 TTS 供应商返回 mp3/opus 不同。上层调用者需处理 PCM → 其他格式的转换（已有先例：Edge TTS 返回 mp3，Piper 返回多种格式）。
