## ADDED Requirements

### Requirement: 运行时加载 libkysdk-coreai-speech 共享库

kart-speech-sys SHALL 通过 `libloading` 在运行时加载 `libkysdk-coreai-speech.so`，遵循以下搜索顺序：
1. `KART_SPEECH_SDK_PATH` 环境变量（完整路径）
2. 构建目标架构对应的 Debian multiarch 系统库路径（如 `/usr/lib/x86_64-linux-gnu/libkysdk-coreai-speech.so`）
3. 运行二进制文件同目录
4. 裸 SONAME（交由动态链接器 / `LD_LIBRARY_PATH` 解析）

加载后 SHALL 验证库导出了预期符号（`speech_synthesizer_create_session`），验证失败则视为无效库。

#### Scenario: SDK 已安装且符号有效
- **WHEN** `libkysdk-coreai-speech.so` 在搜索路径中被找到且导出预期符号
- **THEN** `is_available()` 返回 `true`，后续 API 调用正常工作

#### Scenario: SDK 未安装
- **WHEN** 所有搜索路径均未找到该共享库
- **THEN** `is_available()` 返回 `false`，所有构造函数返回 `KylinSpeechError::LibraryNotFound`

#### Scenario: 库存在但符号缺失
- **WHEN** 找到的 .so 文件不包含预期的导出符号
- **THEN** 该路径被跳过，继续尝试下一个候选路径

### Requirement: Stub 模式支持

启用 `stub` feature 时，kart-speech-sys SHALL 跳过 bindgen 和真实 SDK 查找，使 crate 在无 SDK 头文件或 libclang 的环境中仍可编译。所有公开 API 在 stub 模式下 SHALL 返回 `KylinSpeechError::LibraryNotFound`。

#### Scenario: CI 环境编译
- **WHEN** 使用 `--features stub` 编译
- **THEN** 编译成功，无需系统安装 `libkysdk-coreai-speech-dev`

#### Scenario: Stub 模式下调用 API
- **WHEN** stub 模式下尝试创建合成器或识别器
- **THEN** 返回 `KylinSpeechError::LibraryNotFound` 错误

### Requirement: 安全的 TTS 合成器封装

kart-speech-sys SHALL 提供 `KylinSpeechSynthesizer` 结构体，封装 C SDK 的 `SpeechSynthesizerSession`。该结构体 SHALL：
- 通过 `new(model_name, deploy_type)` 创建并初始化 session
- 提供 `synthesize(text) -> Result<SynthesisOutput>` 方法，参考 kart-nlp-sys 的回调桥接模式（`Arc<Context>` + `mpsc::UnboundedSender`），在回调中收集音频块，等待 `SPEECH_SYNTHESIS_COMPLETED` 后拼接返回 PCM 音频数据及元信息（采样率、声道数、格式）
- 使用 `enable_internal_event_loop(true)` 驱动 SDK 事件循环
- 实现 `Drop` trait：先销毁 session（停止回调），再通过 `Arc::from_raw` 回收交给 C 的回调上下文 refcount
- 所有 unsafe 代码 SHALL 限制在 extern "C" 调用点，公开 API 为完全安全的 Rust

#### Scenario: 合成文本
- **WHEN** 调用 `synthesize("你好世界")`
- **THEN** 返回包含 PCM 音频数据的 `SynthesisOutput`，包括 audio_data、sample_rate、channels 字段

#### Scenario: 合成过程中发生 SDK 错误
- **WHEN** SDK 回调返回 `SPEECH_ERROR_OCCURRED`
- **THEN** 返回 `KylinSpeechError::SdkError` 包含错误码和消息

#### Scenario: session 被 drop
- **WHEN** `KylinSpeechSynthesizer` 离开作用域
- **THEN** 底层 `SpeechSynthesizerSession` 被正确销毁，无内存泄漏

### Requirement: 安全的 STT 识别器封装

kart-speech-sys SHALL 提供 `KylinSpeechRecognizer` 结构体，封装 C SDK 的 `SpeechRecognitionSession`。该结构体 SHALL：
- 通过 `new(model_name, deploy_type)` 创建并初始化 session
- 提供 `recognize(pcm_data, sample_rate) -> Result<String>` 方法，参考 kart-nlp-sys 的回调桥接模式，注册 `SpeechRecognitionResultCallback`，使用 `audio_config_create_once_audio_input_from_pcm_data` 送入音频，等待 `SPEECH_RECOGNIZED` 回调返回识别文本
- 使用 `enable_internal_event_loop(true)` 驱动 SDK 事件循环
- 实现 `Drop` trait：先销毁 session，再回收回调上下文
- 所有 unsafe 代码 SHALL 限制在 extern "C" 调用点

#### Scenario: 识别 PCM 音频
- **WHEN** 调用 `recognize(pcm_bytes, 16000)` 传入有效的 16kHz PCM 数据
- **THEN** 返回识别出的文本字符串

#### Scenario: 识别过程中发生 SDK 错误
- **WHEN** SDK 回调返回 `SPEECH_ERROR_OCCURRED`
- **THEN** 返回 `KylinSpeechError::SdkError` 包含错误码和消息

#### Scenario: 空音频数据
- **WHEN** 传入空的 PCM 数据
- **THEN** 返回 `KylinSpeechError::InvalidInput` 错误

### Requirement: 错误类型定义

kart-speech-sys SHALL 定义 `KylinSpeechError` 枚举，包含以下变体：
- `LibraryNotFound`：共享库未找到
- `SessionCreateFailed`：session 创建失败
- `InitFailed(i32)`：session 初始化失败
- `InvalidInput(String)`：无效输入
- `SdkError(i32, String)`：SDK 返回的错误码和消息
- `Timeout`：回调等待超时
- `FunctionNotFound(String)`：库中未找到预期函数

#### Scenario: 显示错误信息
- **WHEN** 发生 `LibraryNotFound` 错误
- **THEN** 错误消息包含 `libkysdk-coreai-speech` 和安装提示
