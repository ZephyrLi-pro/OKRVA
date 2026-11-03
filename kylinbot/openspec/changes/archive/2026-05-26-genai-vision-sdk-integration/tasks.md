## 1. kart-vision-sys Crate 基础结构

- [x] 1.1 创建 `crates/kart-vision-sys/` 目录结构（Cargo.toml、build.rs、wrapper.h、src/lib.rs）
- [x] 1.2 编写 Cargo.toml（依赖 libloading、thiserror、tokio、bindgen）
- [x] 1.3 编写 wrapper.h，include SDK 头文件（genai/vision/image.h 及其依赖）
- [x] 1.4 编写 build.rs：bindgen 配置，allowlist genai_image_* 和 image_config_* 和 image_result_* 函数及类型
- [x] 1.5 生成 bindings.rs 并验证编译通过

## 2. kart-vision-sys 安全封装

- [x] 2.1 实现动态库加载逻辑（搜索路径、符号解析、KartVisionError::LibraryNotFound）
- [x] 2.2 实现 ImageGenerationSession 结构体（RAII，create/init/destroy 生命周期）
- [x] 2.3 实现回调桥接（Arc<CallbackContext> + mpsc::UnboundedSender，C 回调 → channel）
- [x] 2.4 实现 generate 异步方法（设置 config → generate_image_async → 等待单次回调 → 返回 ImageOutput）
- [x] 2.5 定义 KartVisionError 枚举，实现 SDK 错误码映射
- [x] 2.6 定义输出类型 ImageOutput（data、format、width、height）
- [x] 2.7 实现风格/尺寸/数量查询方法（supported_styles、supported_sizes、max_generation_number）
- [x] 2.8 实现 stub feature（条件编译，跳过 bindgen 和加载）

## 3. Workspace 集成

- [x] 3.1 在根 Cargo.toml workspace.members 和 workspace.dependencies 中注册 kart-vision-sys
- [x] 3.2 在 kylinbot-channels 的 Cargo.toml 中添加 kart-vision-sys 可选依赖和 kart-vision feature

## 4. Provider 层实现

- [x] 4.1 在 kylinbot-channels 中定义 ImageGenProvider trait（generate 方法、name 方法）
- [x] 4.2 实现 KartImageGenProvider（封装 kart-vision-sys::ImageGenerationSession）
- [x] 4.3 将现有 fal.ai 逻辑适配为 FalImageGenProvider（实现同一 trait）
- [x] 4.4 实现 provider 工厂函数：根据配置字符串返回对应 provider 实例

## 5. 配置扩展

- [x] 5.1 重构 ImageGenConfig：将现有 fal.ai 字段（default_model、api_key_env）收入 FalImageGenConfig 子结构
- [x] 5.2 新增 KartImageGenConfig 结构体（style、width、height）
- [x] 5.3 新增 provider 字段和 kart 配置段
- [x] 5.4 实现向后兼容：支持旧配置格式（顶层 default_model/api_key_env）的反序列化

## 6. Tool 层路由

- [x] 6.1 修改 ImageGenTool 使用 ImageGenProvider trait 替代直接调用 fal.ai
- [x] 6.2 在 tool 初始化时根据配置注入对应 provider
- [x] 6.3 处理图像保存逻辑（格式判断、文件命名、目录创建）

## 7. 验证与测试

- [x] 7.1 编写 kart-vision-sys 单元测试（stub 模式下验证 API 签名和错误返回）
- [x] 7.2 编写 KartImageGenProvider 集成测试（mock 或真实 SDK）
- [x] 7.3 验证 feature gate：无 kart-vision feature 时项目正常编译
- [x] 7.4 端到端测试：通过 ImageGenTool 调用 kart provider 生成图片
