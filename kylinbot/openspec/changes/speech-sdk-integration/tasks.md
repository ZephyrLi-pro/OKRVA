## 1. kart-speech-sys FFI crate 脚手架

- [x] 1.1 创建 `crates/kart-speech-sys/Cargo.toml`，声明依赖（libloading、thiserror、tracing）和 build-dependencies（bindgen），定义 `stub` feature
- [x] 1.2 创建 `crates/kart-speech-sys/wrapper.h`，包含 `<coreai/speech/synthesizer.h>`、`<coreai/speech/recognizer.h>`、`<coreai/speech/config.h>`、`<coreai/speech/audioconfig.h>`、`<coreai/speech/result.h>`、`<coreai/speech/audiodatastream.h>`、`<common/error.h>`
- [x] 1.3 创建 `crates/kart-speech-sys/build.rs`，配置 bindgen 生成绑定（allowlist 公开 API 函数/类型，opaque 不透明类型），设置 `KART_SPEECH_SYSTEM_LIB` 环境变量
- [x] 1.4 在根 `Cargo.toml` 的 `members` 中注册 `crates/kart-speech-sys`

## 2. kart-speech-sys 运行时加载与安全封装（参考 kart-nlp-sys 回调模式）

- [x] 2.1 实现 `src/lib.rs` 的 bindings 模块引入、stub 类型别名、错误类型 `KylinSpeechError`、`SynthesisOutput` 数据结构（audio_data, sample_rate, channels, format）
- [x] 2.2 实现运行时库加载逻辑（`OnceLock<Option<Library>>`、搜索路径链、符号验证 `speech_synthesizer_create_session`）和 `is_available()` 函数
- [x] 2.3 实现 TTS 回调桥接：`SynthesisCallbackContext`（含 `mpsc::UnboundedSender`）、`unsafe extern "C" fn synthesis_result_callback` 解析 `SpeechSynthesisResult`（reason、audio_data、error），发送到 channel
- [x] 2.4 实现 `KylinSpeechSynthesizer` —— `new(model, deploy_type)` 创建/初始化 session + 设置 model_config + audio_config（PCM 输出）+ 初始化后销毁临时 config 对象（`speech_model_config_destroy` / `audio_config_destroy`），`synthesize(text)` 注册回调（`Arc::into_raw`）+ `enable_internal_event_loop(true)` + `synthesize_text_async` + 异步收集 channel 中音频块直到 `SPEECH_SYNTHESIS_COMPLETED`，`Drop` 先销毁 session 再 `Arc::from_raw` 回收
- [x] 2.5 实现 STT 回调桥接：`RecognitionCallbackContext`、`unsafe extern "C" fn recognition_result_callback` 解析 `SpeechRecognitionResult`（reason、text、error）
- [x] 2.6 实现 `KylinSpeechRecognizer` —— `new(model, deploy_type)`，`recognize(pcm_data, sample_rate)` 创建 `audio_config_create_once_audio_input_from_pcm_data` + `audio_config_set_input_audio_rate(config, sample_rate)` 设置采样率 + 注册回调 + `recognize_once_async` + 等待 `SPEECH_RECOGNIZED`，用完后销毁临时 config 对象（`speech_model_config_destroy` / `audio_config_destroy`），`Drop` 同上
- [x] 2.7 添加单元测试：`is_available_does_not_panic`、错误类型 display、stub 模式行为

## 3. 配置 Schema 扩展

- [x] 3.1 在 `crates/kylinbot-config/src/schema.rs` 中添加 `KartTtsConfig` 结构体（model_name、deploy_type 字段及默认值函数）
- [x] 3.2 在 `TtsConfig` 中添加 `pub kart: Option<KartTtsConfig>` 字段
- [x] 3.3 在 `crates/kylinbot-config/src/schema.rs` 中添加 `KartSttConfig` 结构体（model_name、deploy_type、sample_rate 字段及默认值函数）
- [x] 3.4 在 `TranscriptionConfig` 中添加 `pub kart: Option<KartSttConfig>` 字段

## 4. KartTtsProvider 实现

- [x] 4.1 在 `crates/kylinbot-channels/Cargo.toml` 中添加 `kart-speech-sys` 可选依赖和 `kart-speech` feature
- [x] 4.2 在 `crates/kylinbot-channels/src/tts.rs` 中实现 `KartTtsProvider`（`#[cfg(feature = "kart-speech")]` 门控），包含 `new(config)` 构造函数和 `TtsProvider` trait 实现
- [x] 4.3 在 `TtsManager::new()` 中添加 kart 供应商注册逻辑（feature-gated，配置存在时尝试初始化）

## 5. KartTranscriptionProvider 实现

- [x] 5.1 在 `crates/kylinbot-channels/src/transcription.rs` 中实现 `KartTranscriptionProvider`（`#[cfg(feature = "kart-speech")]` 门控），包含 `from_config()` 构造函数和 `TranscriptionProvider` trait 实现
- [x] 5.2 在 `TranscriptionManager::new()` 中添加 kart 供应商注册逻辑
- [x] 5.3 在 `transcribe_audio()` 向后兼容函数中添加 `"kart"` 分支

## 6. 工作区集成与 Feature Flag

- [x] 6.1 在根 `Cargo.toml` 中添加 `kart-speech = ["kylinbot-channels/kart-speech"]` feature
- [x] 6.2 验证 `cargo check` 在不启用 feature 时编译通过
- [x] 6.3 验证 `cargo check --features kart-speech` 编译通过（需要 SDK 头文件环境）
