## ADDED Requirements

### Requirement: SDK 动态加载
系统 SHALL 在运行时通过 `libloading` 动态加载 `libkysdk-genai-vision.so`，按以下优先级搜索：环境变量 `KYSDK_GENAI_VISION_LIB_PATH` → 系统架构库目录 → SONAME fallback。

#### Scenario: 正常加载
- **WHEN** 系统库路径下存在 `libkysdk-genai-vision.so`
- **THEN** 成功加载库并解析所有必需符号（`genai_image_create_session`、`genai_image_destroy_session`、`genai_image_init_session`、`genai_image_set_config`、`genai_image_result_set_callback`、`genai_image_generate_image_async`、`image_config_create`、`image_config_destroy`、`image_config_set_style`、`image_config_set_size`、`image_config_set_generation_number`、`image_result_get_data`、`image_result_get_format`、`image_result_get_size`、`image_result_get_error_code`、`image_result_get_error_message`、`image_result_get_total`、`image_result_get_index`）

#### Scenario: 库不存在
- **WHEN** 所有搜索路径下均无目标 .so 文件
- **THEN** 返回 `KartVisionError::LibraryNotFound` 错误

#### Scenario: 通过环境变量指定路径
- **WHEN** `KYSDK_GENAI_VISION_LIB_PATH` 环境变量设置为有效 .so 路径
- **THEN** 优先从该路径加载，忽略系统路径

### Requirement: 会话生命周期管理
系统 SHALL 提供 RAII 风格的 `ImageGenerationSession` 结构体，创建时初始化 SDK session，Drop 时自动销毁。

#### Scenario: 创建会话
- **WHEN** 调用 `ImageGenerationSession::new()`
- **THEN** 依次调用 `genai_image_create_session()` 和 `genai_image_init_session()`，成功返回 session 实例

#### Scenario: 初始化失败
- **WHEN** `genai_image_init_session()` 返回非零错误码
- **THEN** 调用 `genai_image_destroy_session()` 清理资源并返回 `KartVisionError::SessionInitFailed(code)` 错误

#### Scenario: 自动销毁
- **WHEN** `ImageGenerationSession` 被 drop
- **THEN** 自动调用 `genai_image_destroy_session()` 释放底层资源

### Requirement: 异步回调桥接
系统 SHALL 将 C 回调转换为 async Rust future，通过 mpsc channel 传递结果。

#### Scenario: 正常接收生成结果
- **WHEN** SDK 回调被触发且 `image_result_get_error_code()` 返回 0
- **THEN** 从 `VisionImageResult` 提取图像数据、格式、尺寸，通过 channel 发送 `Ok(ImageOutput)`

#### Scenario: SDK 回调报告错误
- **WHEN** SDK 回调被触发且 `image_result_get_error_code()` 返回非零值
- **THEN** 提取错误码和错误消息，通过 channel 发送 `Err(KartVisionError::GenerationFailed { code, message })`

#### Scenario: 超时未收到回调
- **WHEN** 调用 `generate_image_async` 后超过超时时间（默认 120 秒）未收到回调
- **THEN** 返回 `KartVisionError::Timeout` 错误

### Requirement: 图像生成 API
系统 SHALL 暴露 `generate` 异步方法，接受 prompt 和可选配置，返回单张生成的图像数据。

#### Scenario: 基本文生图
- **WHEN** 调用 `session.generate("一只猫在月光下", None)` 
- **THEN** 返回 `ImageOutput { data: Vec<u8>, format: ImageFormat, width: u32, height: u32 }`

#### Scenario: 带配置的文生图
- **WHEN** 调用 `session.generate(prompt, Some(config))` 且 config 指定 style="水彩"、size=1024x1024
- **THEN** 应用配置后生成并返回指定参数的图像

### Requirement: 风格与尺寸查询
系统 SHALL 暴露查询方法，获取 SDK 支持的图像风格列表和尺寸列表。

#### Scenario: 查询支持的风格
- **WHEN** 调用 `session.supported_styles()`
- **THEN** 调用 `genai_image_get_supported_image_style()` 并返回 `Vec<String>`

#### Scenario: 查询支持的尺寸
- **WHEN** 调用 `session.supported_sizes()`
- **THEN** 调用 `genai_image_get_supported_image_size()` 并返回 `Vec<ImageSize>`（width, height 对）

#### Scenario: 查询支持的最大生成数量
- **WHEN** 调用 `session.max_generation_number()`
- **THEN** 调用 `genai_image_get_supported_image_number()` 并返回 `u32`

### Requirement: Stub 模式
系统 SHALL 提供 `stub` feature，在无 SDK 环境下跳过 bindgen 和动态加载，所有 API 调用返回 `KartVisionError::LibraryNotFound`。

#### Scenario: stub 模式编译
- **WHEN** 以 `--features stub` 编译
- **THEN** 不执行 bindgen、不尝试加载 .so，编译成功

#### Scenario: stub 模式调用
- **WHEN** 在 stub 模式下调用任何 API
- **THEN** 返回 `KartVisionError::LibraryNotFound` 错误

### Requirement: 错误类型定义
系统 SHALL 定义 `KartVisionError` 枚举，覆盖所有可能的错误场景。

#### Scenario: 错误映射完整性
- **WHEN** SDK 返回任何 `GenAiVisionErrorCode` 或 `AiSdkCommonErrorCode` 中的错误码
- **THEN** 能映射到对应的 `KartVisionError` 变体：`LibraryNotFound`、`SessionInitFailed(i32)`、`Timeout`、`InputTextTooLong`、`InvalidStyle`、`InvalidSize`、`InvalidNumber`、`GenerationBlocked`、`GenerationFailed { code: i32, message: String }`
