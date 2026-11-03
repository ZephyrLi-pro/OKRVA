## ADDED Requirements

### Requirement: Subject Segmentation Tool 注册
系统 SHALL 注册名为 `subject_segmentation` 的 tool，用于从图片中提取前景主体。

#### Scenario: tool 基本信息
- **WHEN** 查询 tool 元数据
- **THEN** name 为 `"subject_segmentation"`，description 说明其功能为对图片进行主体分割并保存前景为透明背景图片

### Requirement: Subject Segmentation Tool 参数
系统 SHALL 接受 `image_path` 必填参数和 `mode` 可选参数。

#### Scenario: 参数 schema
- **WHEN** 查询 tool parameters_schema
- **THEN** required 包含 `"image_path"`（string 类型），optional 包含 `"mode"`（enum: "portrait"/"general"，默认 "general"）

### Requirement: Subject Segmentation Tool 执行
系统 SHALL 调用 kart-coreai-vision-sys 执行主体分割，保存前景 PNG 并返回文件路径。

#### Scenario: 正常分割（general 模式）
- **WHEN** 调用 tool 传入有效图片路径，mode 为 "general"
- **THEN** 以 SUBJECT_SEGMENTATION_MODE_GENERAL 模式执行主体分割，将前景 PNG 保存到 `{workspace}/images/{stem}_segmented.png`，返回 success=true 及文件路径、尺寸信息

#### Scenario: 正常分割（portrait 模式）
- **WHEN** 调用 tool 传入有效图片路径，mode 为 "portrait"
- **THEN** 以 SUBJECT_SEGMENTATION_MODE_PORTRAIT 模式执行人像主体分割，返回同上

#### Scenario: 默认模式
- **WHEN** 未指定 mode 参数
- **THEN** 使用 "general" 模式

#### Scenario: 图片不存在
- **WHEN** 传入的 image_path 文件不存在
- **THEN** 返回 success=false，error 说明文件不存在

#### Scenario: SDK 不可用
- **WHEN** `libkysdk-coreai-vision.so` 未安装
- **THEN** 返回 success=false，error 说明 SDK 不可用

#### Scenario: 分割失败
- **WHEN** SDK 返回错误
- **THEN** 返回 success=false，error 包含错误码和错误消息

### Requirement: Subject Segmentation Tool 安全策略
系统 SHALL 遵循 SecurityPolicy，执行时使用 ToolOperation::Act（涉及文件写入）。

#### Scenario: ReadOnly 模式拒绝
- **WHEN** autonomy 为 ReadOnly
- **THEN** 返回 success=false，error 说明当前为只读模式不允许写文件

#### Scenario: Full 模式允许
- **WHEN** autonomy 为 Full
- **THEN** 正常执行分割并保存文件
