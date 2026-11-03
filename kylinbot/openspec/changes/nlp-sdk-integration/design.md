## 背景

KylinBot 是一个使用 Provider 抽象层的 Rust CLI agent，支持多种 AI 后端（Claude、Gemini、OpenAI 等）。kylin-genai-nlp SDK 通过其 C API（GenAiTextSession）提供替代 AI 后端。集成需要：

- C SDK 提供内部事件循环接口（genai_text_enable_internal_event_loop）
- 实现 Provider trait 的新 Provider
- 异步回调的结果传递

**当前状态**：KylinBot 已支持多种 Provider（Claude、Gemini、OpenAI 等）。

**约束条件**：
- SDK 使用 C ABI，通过 genai_text_enable_internal_event_loop() 启用内部事件循环
- SDK 位于 `/home/kylin/Kylinproject/KylinSDK/kylin-genai-nlp`
- KylinBot 依赖 `libkylin-genai-nlp-dev` 包获取头文件和链接信息

## 目标

- 将 kylin-genai-nlp SDK 作为并行 AI Provider 集成
- 保持现有所有 Provider 不变
- 通过配置支持 Provider 选择

## 设计

### 1. Provider 接口实现

在 `crates/zeroclaw-providers/` 中实现 `Provider` trait 的新 Kart Provider。Provider 处理：
- 会话创建和生命周期
- 异步聊天请求分发
- 模型列表获取
- 工具调用支持
- 错误处理：SDK 返回错误时直接报错

### 2. FFI 绑定层

使用 `bindgen` 自动生成 Rust FFI 绑定：
- 输入：SDK 头文件（`kylin-ai/genai/text/*.h`）
- 输出：`kart-nlp-sys` crate 中的 safe Rust wrapper（底层 FFI crate，不对外暴露）
- 手动实现 `ChatResultCallback` → Rust channel 转换

### 3. 事件循环集成

调用 `genai_text_enable_internal_event_loop()` 让 SDK 自行管理线程和事件调度。

### 4. Provider 注册

在 `providers/mod.rs` 中使用工厂模式注册 Provider。Agent 根据配置选择 Provider。

## 风险与权衡

| 风险 | 缓解措施 |
|------|----------|
| SDK C API 内存泄漏 | FFI 层通过 RAII 包装器强制会话销毁 |
| SDK 初始化失败 | 直接报错退出 |
