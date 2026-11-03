## ADDED Requirements

### Requirement: ImageGen Provider trait
系统 SHALL 定义 `ImageGenProvider` trait，抽象文生图后端的统一接口。

#### Scenario: trait 方法签名
- **WHEN** 实现 `ImageGenProvider`
- **THEN** MUST 实现 `async fn generate(&self, prompt: &str, config: ImageGenRequest) -> Result<GeneratedImage, ImageGenError>` 方法

#### Scenario: provider 名称标识
- **WHEN** 查询 provider 类型
- **THEN** 通过 `fn name(&self) -> &str` 返回唯一标识（如 `"kart"` 或 `"fal"`）

### Requirement: Kart Vision Provider 实现
系统 SHALL 提供基于 `kart-vision-sys` 的 `KartImageGenProvider` 实现。

#### Scenario: 正常生成图像
- **WHEN** 调用 `KartImageGenProvider::generate(prompt, config)`
- **THEN** 创建 SDK session、设置配置、调用异步生成、等待回调、返回图像数据并保存到工作目录

#### Scenario: SDK 不可用
- **WHEN** `kart-vision-sys` 加载失败（LibraryNotFound）
- **THEN** 返回 `ImageGenError::ProviderUnavailable("kart")` 错误

#### Scenario: 生成失败
- **WHEN** SDK 返回错误（审核拦截、文本过长等）
- **THEN** 将 `KartVisionError` 转换为 `ImageGenError::GenerationFailed { provider, reason }` 返回

### Requirement: Provider 配置路由
系统 SHALL 根据配置文件中的 `image_gen.provider` 字段选择对应的 provider 实现。

#### Scenario: 选择 Kart provider
- **WHEN** 配置中 `image_gen.provider = "kart"`
- **THEN** 使用 `KartImageGenProvider` 处理文生图请求

#### Scenario: 选择 fal provider
- **WHEN** 配置中 `image_gen.provider = "fal"`
- **THEN** 使用现有 fal.ai provider 处理文生图请求

#### Scenario: 默认 provider
- **WHEN** 配置中未指定 `image_gen.provider`
- **THEN** 使用 `"fal"` 作为默认 provider

### Requirement: 配置 schema 扩展
系统 SHALL 重构 `ImageGenConfig`，将 fal.ai 专用字段收入子结构，新增 `kart` 配置段和 `provider` 路由字段。

#### Scenario: Kart 配置字段
- **WHEN** 解析配置文件
- **THEN** `image_gen.kart` 段 SHALL 支持以下可选字段：`style`（字符串）、`width`（整数）、`height`（整数）

#### Scenario: Fal 配置字段
- **WHEN** 解析配置文件
- **THEN** `image_gen.fal` 段 SHALL 包含原有的 `default_model`（字符串）和 `api_key_env`（字符串）字段

#### Scenario: provider 字段
- **WHEN** 解析配置文件
- **THEN** `image_gen.provider` 字段 SHALL 接受 `"fal"` 或 `"kart"` 值

#### Scenario: 向后兼容
- **WHEN** 配置文件中 `image_gen` 下仍使用旧的扁平格式（`default_model`、`api_key_env` 在顶层）
- **THEN** 系统 SHALL 将顶层字段视为 `fal` 子结构的值进行兼容解析

### Requirement: Feature gate 隔离
系统 SHALL 通过 Cargo feature `kart-vision` 控制 Kart provider 的编译。

#### Scenario: feature 启用
- **WHEN** 以 `--features kart-vision` 编译
- **THEN** `KartImageGenProvider` 可用，配置中可选择 `"kart"` provider

#### Scenario: feature 未启用
- **WHEN** 未启用 `kart-vision` feature
- **THEN** `KartImageGenProvider` 不编译，选择 `"kart"` provider 时返回 `ProviderUnavailable` 错误

### Requirement: 图像输出处理
系统 SHALL 将生成的图像保存到工作目录并返回文件路径。

#### Scenario: 保存图像文件
- **WHEN** 图像生成成功
- **THEN** 根据 `ImageFormat` 将图像数据保存为对应格式文件（`.png`/`.jpg`/`.webp`），路径为 `{workspace}/images/{timestamp}_{index}.{ext}`

#### Scenario: 返回结果给 tool
- **WHEN** 图像保存完成
- **THEN** 返回 `GeneratedImage { path: PathBuf, format: String, width: u32, height: u32 }` 给上层 tool
