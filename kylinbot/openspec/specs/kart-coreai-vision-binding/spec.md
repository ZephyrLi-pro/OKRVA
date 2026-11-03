## ADDED Requirements

### Requirement: SDK 动态加载
系统 SHALL 在运行时通过 `libloading` 动态加载 `libkysdk-coreai-vision.so`，按以下优先级搜索：环境变量 `KYSDK_COREAI_VISION_LIB_PATH` → 系统架构库目录 → SONAME fallback。

#### Scenario: 正常加载
- **WHEN** 系统库路径下存在 `libkysdk-coreai-vision.so`
- **THEN** 成功加载库并解析 OCR 和分割两组符号

#### Scenario: 库不存在
- **WHEN** 所有搜索路径下均无目标 .so 文件
- **THEN** 返回 `KartCoreVisionError::LibraryNotFound` 错误

### Requirement: OCR 会话管理
系统 SHALL 提供 `TextRecognitionSession` 结构体，封装 OCR 的完整生命周期。

#### Scenario: 创建并初始化
- **WHEN** 调用 `TextRecognitionSession::new()`
- **THEN** 依次调用 `text_recognition_create_session()`、`text_recognition_init_session()`、`text_recognition_enable_internal_event_loop(session, true)`，成功返回 session 实例

#### Scenario: 初始化失败
- **WHEN** `text_recognition_init_session()` 返回非零错误码
- **THEN** 调用 destroy 清理资源并返回 `KartCoreVisionError::SessionInitFailed(code)`

#### Scenario: 自动销毁
- **WHEN** `TextRecognitionSession` 被 drop
- **THEN** 自动调用 `text_recognition_destroy_session()` 释放底层资源

### Requirement: OCR 异步识别
系统 SHALL 暴露 `recognize_from_file` 异步方法，接受图片路径，返回识别结果。

#### Scenario: 从文件识别文本
- **WHEN** 调用 `session.recognize_from_file("/path/to/image.jpg")`
- **THEN** 设置回调、调用 `text_recognition_recognize_text_from_image_file_async`、等待回调触发，返回 `OcrResult { text: String, lines: Vec<TextLineResult> }`

#### Scenario: 识别成功带位置信息
- **WHEN** SDK 回调返回且 error_code 为 0
- **THEN** 从 result 提取全文（`get_value`）和各行文字及坐标（`get_text_lines` → `text_line_get_value` + `text_line_get_corner_points`）

#### Scenario: 识别失败
- **WHEN** SDK 回调返回且 error_code 非零
- **THEN** 返回 `KartCoreVisionError::OcrFailed { code, message }`

#### Scenario: 超时
- **WHEN** 超过 60 秒未收到回调
- **THEN** 返回 `KartCoreVisionError::Timeout`

### Requirement: 主体分割会话管理
系统 SHALL 提供 `SubjectSegmentationSession` 结构体，封装分割的完整生命周期（含异步初始化）。

#### Scenario: 创建并异步初始化
- **WHEN** 调用 `SubjectSegmentationSession::new()`
- **THEN** 调用 `subject_segmentation_create_session()`、`subject_segmentation_enable_internal_event_loop(session, true)`，然后通过回调桥接等待 `subject_segmentation_init_session_async` 完成

#### Scenario: 异步初始化成功
- **WHEN** init callback 返回 error_code = 0
- **THEN** session 可用，可执行分割操作

#### Scenario: 异步初始化失败
- **WHEN** init callback 返回非零 error_code
- **THEN** 销毁 session 并返回 `KartCoreVisionError::SessionInitFailed(code)`

#### Scenario: 初始化超时
- **WHEN** 超过 30 秒未收到 init callback
- **THEN** 返回 `KartCoreVisionError::Timeout`

### Requirement: 主体分割执行
系统 SHALL 暴露 `segment_from_file` 异步方法，接受图片路径和分割模式，返回前景图像数据。

#### Scenario: 从文件执行分割
- **WHEN** 调用 `session.segment_from_file("/path/to/image.jpg", SegmentationMode::General)`
- **THEN** 创建配置并设置 mode、设置结果回调、调用 `subject_segmentation_run_from_image_file_async`、等待回调触发，返回 `SegmentationResult { foreground_data: Vec<u8>, format: String, width: u32, height: u32 }`

#### Scenario: 分割成功
- **WHEN** SDK 回调返回且 error_code 为 0
- **THEN** 从 result 通过 `get_foreground_image_data` 提取前景图像数据、宽高，通过 `get_foreground_image_format` 获取格式

#### Scenario: 分割失败
- **WHEN** SDK 回调返回且 error_code 非零
- **THEN** 返回 `KartCoreVisionError::SegmentationFailed { code, message }`

#### Scenario: 超时
- **WHEN** 超过 120 秒未收到结果回调
- **THEN** 返回 `KartCoreVisionError::Timeout`

### Requirement: Stub 模式
系统 SHALL 提供 `stub` feature，在无 SDK 环境下所有 API 调用返回 `LibraryNotFound`。

#### Scenario: stub 模式编译
- **WHEN** 以 `--features stub` 编译
- **THEN** 不执行 bindgen、不尝试加载 .so，编译成功

#### Scenario: stub 模式调用
- **WHEN** 在 stub 模式下调用任何 API
- **THEN** 返回 `KartCoreVisionError::LibraryNotFound` 错误

### Requirement: 错误类型
系统 SHALL 定义 `KartCoreVisionError` 枚举覆盖所有错误场景。

#### Scenario: 错误变体完整性
- **WHEN** 任何操作失败
- **THEN** 错误可映射到：`LibraryNotFound`、`SessionInitFailed(i32)`、`Timeout`、`OcrFailed { code: i32, message: String }`、`SegmentationFailed { code: i32, message: String }`、`ImageNotFound(String)`
