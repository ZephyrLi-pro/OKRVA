## Context

KylinBot 当前通过 fal.ai 云端 API 提供文生图能力（`plugins/image-gen-fal` 及 `kylinbot-tools/src/image_gen.rs`）。麒麟 GenAI Vision SDK（`libkysdk-genai-vision`）已通过系统包安装，提供端侧/私有化部署的异步回调式文生图 C API。

现有 SDK 集成模式已在 `kart-speech-sys`（语音）、`kart-nlp-sys`（NLP）、`kart-embedding-sys`（向量化）中验证成熟。本次接入遵循相同架构：sys crate → channel provider → tool 路由。

SDK API 概要：
- 会话模式：`genai_image_create_session` → `init` → `set_config` → `generate_image_async` → callback → `destroy`
- 异步回调：`ImageResultCallback(VisionImageResult*, void* user_data)`
- 配置项：风格（style）、尺寸（ImageSize）、生成数量（number）
- 结果：原始图像字节 + 格式（JPEG/PNG/WEBP）+ 错误码

## Goals / Non-Goals

**Goals:**
- 提供与 fal.ai 平行的本地文生图后端，通过配置切换
- 遵循现有 sys crate 模式（动态加载、stub feature、回调桥接）
- Feature-gate 确保无 SDK 环境可正常编译
- 暴露 SDK 支持的风格/尺寸查询能力

**Non-Goals:**
- 不替换现有 fal.ai 实现（两者共存）
- 不实现图片编辑/修改（仅 text-to-image）
- 不处理模型管理/部署（由 SDK 和系统服务负责）
- 不在本次引入流式输出（SDK 为单次回调模式）

## Decisions

### 1. 新建 `kart-vision-sys` crate 作为唯一 unsafe 边界

**选择**: 独立 crate `crates/kart-vision-sys/`，对外暴露纯 safe Rust API。

**理由**: 与 `kart-speech-sys` 一致的隔离模式；bindgen 生成的 FFI 类型不泄露到上层；unsafe 调用点集中便于审计。

**备选方案**: 直接在 kylinbot-channels 中写 FFI → 拒绝，违反 unsafe 隔离原则。

### 2. 运行时动态加载（libloading）而非链接时绑定

**选择**: 通过 `libloading` 在运行时打开 `libkysdk-genai-vision.so`。

**理由**: 保持与 speech/nlp sys crate 相同策略；允许在未安装 SDK 的机器上编译和运行（graceful fallback）；搜索路径可通过环境变量覆盖。

**搜索顺序**: `KYSDK_GENAI_VISION_LIB_PATH` env → `/usr/lib/x86_64-linux-gnu/` → `/usr/lib/aarch64-linux-gnu/` → `libkysdk-genai-vision.so`（SONAME fallback）

### 3. 回调桥接模式：Arc + mpsc channel

**选择**: C 回调通过 `Arc<CallbackContext>` 持有 `tokio::sync::mpsc::UnboundedSender`，将结果发送到 async receiver。

**理由**: 
- C 回调在 SDK 内部线程调用，不能直接 await
- `UnboundedSender::send()` 是线程安全且非阻塞的
- 上层 async 代码 await receiver 即可获取结果
- Drop 顺序确保 session 销毁先于 context

### 4. 在 `kylinbot-channels` 增加 ImageGen provider trait

**选择**: 定义 `ImageGenProvider` trait，Kart 实现和 fal.ai 实现各自实现此 trait。

**理由**: 统一抽象层，`ImageGenTool` 通过配置选择 provider，不直接耦合 SDK 细节。

**备选方案**: 在 ImageGenTool 中 if-else 分支 → 拒绝，扩展性差、职责混乱。

### 5. 配置结构

**选择**: 重构 `ImageGenConfig`，将 fal.ai 专用字段收入 `fal` 子结构，新增 `kart` 和 `provider` 字段：

```rust
pub struct ImageGenConfig {
    pub enabled: bool,
    pub provider: String,          // "fal" | "kart"，默认 "fal"
    pub fal: FalImageGenConfig,    // 原有字段迁入
    pub kart: Option<KartImageGenConfig>,
}

pub struct FalImageGenConfig {
    pub default_model: String,
    pub api_key_env: String,
}

pub struct KartImageGenConfig {
    pub style: Option<String>,
    pub size: Option<ImageSizeConfig>,
}
```

**向后兼容**: 通过 serde 的 `#[serde(flatten)]` 或自定义反序列化支持旧配置格式（`default_model`/`api_key_env` 在顶层时视为 `fal` 子结构的值）。

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|----------|
| SDK 未安装时编译失败 | `stub` feature 跳过 bindgen 和动态加载，提供编译通过的空实现 |
| 回调未触发导致永久阻塞 | 设置 timeout（默认 120s），超时返回错误 |
| SDK 内部线程 panic 导致进程崩溃 | C 回调仅做 channel send，不执行复杂逻辑 |
| 图片审核被拦截（`IMAGE_GENERATION_BLOCKED`） | 作为正常错误返回给 tool caller，不 panic |
| 多平台支持（x86_64/aarch64） | 搜索路径覆盖两种架构的系统库目录 |
