## ADDED Requirements

### Requirement: OCR Tool 注册
系统 SHALL 注册名为 `ocr` 的 tool，用于从图片中识别文字。

#### Scenario: tool 基本信息
- **WHEN** 查询 tool 元数据
- **THEN** name 为 `"ocr"`，description 说明其功能为从图片中提取文字内容

### Requirement: OCR Tool 参数
系统 SHALL 接受 `image_path` 必填参数。

#### Scenario: 参数 schema
- **WHEN** 查询 tool parameters_schema
- **THEN** required 包含 `"image_path"`（string 类型，图片文件路径）

### Requirement: OCR Tool 执行
系统 SHALL 调用 kart-coreai-vision-sys 执行文字识别并返回结构化结果。

#### Scenario: 正常识别
- **WHEN** 调用 `ocr` tool 并传入有效图片路径
- **THEN** 返回 success=true，output 包含识别出的全文文本及每行文字的边界框坐标（JSON 格式）

#### Scenario: 图片不存在
- **WHEN** 传入的 image_path 文件不存在
- **THEN** 返回 success=false，error 说明文件不存在

#### Scenario: SDK 不可用
- **WHEN** `libkysdk-coreai-vision.so` 未安装
- **THEN** 返回 success=false，error 说明 SDK 不可用

#### Scenario: 识别失败
- **WHEN** SDK 返回错误（图片损坏、格式不支持等）
- **THEN** 返回 success=false，error 包含错误码和错误消息

### Requirement: OCR Tool 安全策略
系统 SHALL 遵循 SecurityPolicy，在 ReadOnly 模式下允许执行（OCR 为只读操作）。

#### Scenario: ReadOnly 模式
- **WHEN** autonomy 为 ReadOnly
- **THEN** OCR tool 可正常执行（enforce ToolOperation::Observe）
