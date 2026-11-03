## 1. kart-coreai-vision-sys Crate 基础结构

- [x] 1.1 创建 `crates/kart-coreai-vision-sys/` 目录结构（Cargo.toml、build.rs、wrapper.h、src/lib.rs）
- [x] 1.2 编写 Cargo.toml（依赖 libloading、thiserror、tokio、bindgen）
- [x] 1.3 编写 wrapper.h，include coreai/vision 所有头文件（textrecognition.h、subjectsegmentation.h 及依赖）
- [x] 1.4 编写 build.rs：bindgen 配置，allowlist text_recognition_*、subject_segmentation_*、text_line_* 函数和相关类型
- [x] 1.5 生成 bindings.rs 并验证编译通过

## 2. kart-coreai-vision-sys OCR 安全封装

- [x] 2.1 实现动态库加载逻辑（搜索路径、符号解析、错误处理）
- [x] 2.2 实现 TextRecognitionSession 结构体（RAII，create/init/enable_internal_event_loop/destroy）
- [x] 2.3 实现 OCR 回调桥接（Arc<CallbackContext> + mpsc channel）
- [x] 2.4 实现 recognize_from_file 异步方法（设置回调 → 调用 async → await receiver with 60s timeout）
- [x] 2.5 定义 OcrResult（text: String, lines: Vec<TextLineResult>）和 TextLineResult（text, corner_points）

## 3. kart-coreai-vision-sys 主体分割安全封装

- [x] 3.1 实现 SubjectSegmentationSession 结构体（RAII，create/enable_internal_event_loop/init_async/destroy）
- [x] 3.2 实现异步初始化回调桥接（init callback → channel，30s timeout）
- [x] 3.3 实现分割结果回调桥接（result callback → channel，120s timeout）
- [x] 3.4 实现 segment_from_file 异步方法（设置 config/mode → 设置回调 → 调用 async → await）
- [x] 3.5 定义 SegmentationResult（foreground_data: Vec<u8>, format: String, width: u32, height: u32）

## 4. 公共部分

- [x] 4.1 定义 KartCoreVisionError 枚举（LibraryNotFound、SessionInitFailed、Timeout、OcrFailed、SegmentationFailed、ImageNotFound）
- [x] 4.2 实现 stub feature（条件编译，跳过 bindgen 和加载）

## 5. Workspace 集成

- [x] 5.1 在根 Cargo.toml workspace.members 和 workspace.dependencies 中注册 kart-coreai-vision-sys
- [x] 5.2 在 kylinbot-tools 的 Cargo.toml 中添加 kart-coreai-vision-sys 可选依赖和 kart-coreai-vision feature
- [x] 5.3 在 kylinbot-runtime 的 Cargo.toml 中转发 kart-coreai-vision feature
- [x] 5.4 在根 Cargo.toml 的 default features 中添加 kart-coreai-vision

## 6. OcrTool 实现

- [x] 6.1 创建 `kylinbot-tools/src/ocr.rs`，实现 OcrTool 结构体
- [x] 6.2 实现 Tool trait：name、description、parameters_schema（required: image_path）
- [x] 6.3 实现 execute：验证文件存在 → 调用 sys crate → 格式化结果为 JSON（全文 + 各行文字及坐标）
- [x] 6.4 安全策略：enforce ToolOperation::Observe（只读操作）
- [x] 6.5 在 kylinbot-tools/src/lib.rs 中注册模块，feature gate

## 7. SubjectSegmentationTool 实现

- [x] 7.1 创建 `kylinbot-tools/src/subject_segmentation.rs`，实现 SubjectSegmentationTool 结构体
- [x] 7.2 实现 Tool trait：name（"subject_segmentation"）、description、parameters_schema（required: image_path, optional: mode）
- [x] 7.3 实现 execute：验证文件 → 解析 mode → 调用 sys crate → 保存前景 PNG → 返回路径
- [x] 7.4 安全策略：enforce ToolOperation::Act（涉及文件写入）
- [x] 7.5 在 kylinbot-tools/src/lib.rs 中注册模块，feature gate

## 8. 配置与注册

- [x] 8.1 在 kylinbot-config schema 中添加 ocr 和 subject_segmentation 的 enabled 配置段
- [x] 8.2 在 tool 注册入口根据配置条件注册两个新 tool

## 9. 验证与测试

- [x] 9.1 编写 kart-coreai-vision-sys 单元测试（stub 模式下验证 API 签名和错误返回）
- [x] 9.2 编写 OcrTool 单元测试（参数校验、文件不存在、SDK 不可用场景）
- [x] 9.3 编写 SubjectSegmentationTool 单元测试（参数校验、mode 默认值、安全策略）
- [x] 9.4 验证 feature gate：无 kart-coreai-vision feature 时项目正常编译
