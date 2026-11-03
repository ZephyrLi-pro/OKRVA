## Context

KylinBot 已有成熟的 SDK 集成模式（`kart-speech-sys`、`kart-nlp-sys`）。麒麟 CoreAI Vision SDK（`libkysdk-coreai-vision`）已通过系统包安装，提供两组异步回调式 C API：

**OCR（文字识别）**：
- 会话模式：`create_session` → `init_session` → `enable_internal_event_loop(true)` → `set_callback` → `recognize_text_from_image_file_async` → callback → `destroy`
- 结果：全文文本 + TextLine 数组（每行包含文字和 4 个角点坐标）

**主体分割**：
- 会话模式：`create_session` → `enable_internal_event_loop(true)` → `init_session_async`（异步初始化，有自己的回调）→ `set_config` → `set_result_callback` → `run_from_image_file_async` → callback → `destroy`
- 结果：前景图像数据（PNG 格式）+ alpha mask + 边缘点组
- 配置：`SubjectSegmentationMode`（PORTRAIT / GENERAL）

两个 API 的关键区别：主体分割的 init 是异步的（需要加载模型），OCR 的 init 是同步的。

## Goals / Non-Goals

**Goals:**
- 新增 `ocr` 和 `subject_segmentation` 两个独立 tool，不破坏现有架构
- 遵循现有 sys crate 模式（动态加载、stub feature、回调桥接、feature gate）
- 两项能力共用一个 sys crate（同一 .so 库）
- 支持从文件路径输入图片

**Non-Goals:**
- 不实现从内存 buffer 直接传入图片（仅文件路径，与现有 tool 交互模式一致）
- 不实现流式/实时 OCR
- 不处理主体分割的边缘点组数据（仅返回前景 PNG）
- 不实现 OCR 的区域选择/裁剪功能

## Decisions

### 1. 两项能力共用 `kart-coreai-vision-sys` crate

**选择**: OCR 和主体分割共用一个 sys crate，对应同一个 `libkysdk-coreai-vision.so`。

**理由**: 两组 API 来自同一个动态库，共享类型定义（PixelPoint、SegmentationMaskData）；拆分为两个 crate 会导致重复绑定和版本同步问题。

**备选方案**: 每个能力一个 crate → 拒绝，共享类型无法跨 crate 直接使用。

### 2. 直接作为 Tool 接入，不走 provider trait

**选择**: 在 `kylinbot-tools` 中新增 `OcrTool` 和 `SubjectSegmentationTool`，直接依赖 sys crate。

**理由**: 这两项能力是独立的工具（不像 TTS/image_gen 有多后端选择），没有 provider 抽象的需求；保持简单，与现有 `image_info` tool 同级。

**备选方案**: 在 kylinbot-channels 加 provider trait → 拒绝，过度抽象，目前只有一个后端。

### 3. 会话生命周期：每次调用新建

**选择**: 每次 tool 调用时创建新的 SDK session，完成后销毁。

**理由**: SDK session 初始化开销可接受（OCR 同步 init < 100ms，分割异步 init 稍长但可复用策略后续优化）；避免长期持有 session 带来的状态管理复杂度。

**未来优化**: 如果性能不满足要求，可改为 session pool 复用。

### 4. 必须启用 internal event loop

**选择**: 两个 API 都调用 `enable_internal_event_loop(session, true)`。

**理由**: KylinBot 是 tokio 异步运行时，不运行 GLib main loop；启用 SDK 内部事件循环后回调在 SDK 线程触发，再通过 mpsc channel 桥接到 async。

### 5. 主体分割的异步初始化处理

**选择**: 用与结果回调相同的 Arc+channel 模式桥接 init callback，await 初始化完成后再执行分割。

**理由**: init 需要加载模型，可能耗时数秒；同步等待会阻塞 tokio runtime。与结果回调的桥接模式完全一致，代码可复用。

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|----------|
| SDK 未安装时编译失败 | `stub` feature 跳过 bindgen 和动态加载 |
| 主体分割初始化慢（模型加载） | 设置合理超时（默认 30s），错误返回给 tool caller |
| OCR/主体分割回调未触发 | 统一超时机制（OCR 60s，主体分割 120s） |
| 大图片处理耗时 | 由 SDK 内部处理，tool 层不做额外限制，超时保护即可 |
| 与 genai-vision-sdk 命名冲突 | 使用 `kart-coreai-vision-sys` 区分于 `kart-vision-sys`（genai） |
