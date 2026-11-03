## 1. FFI 绑定层

- [x] 1.1 扩展 `crates/kart-nlp-sys/` crate 添加缺失的 FFI 绑定（底层 FFI crate）
- [x] 1.2 定义 `genai_text_create_session`、`genai_text_destroy_session`、`genai_text_init_session` 包装器
- [x] 1.3 定义 `genai_text_chat_async`、`genai_text_set_model_config`、`genai_text_enable_internal_event_loop` 包装器
- [x] 1.4 定义 `genai_text_get_model_list` 包装器获取模型列表
- [x] 1.5 实现 `ChatResultCallback` → Rust `Receiver` channel 转换
- [x] 1.6 添加 `ChatModelConfig` 创建和配置辅助函数

## 2. Provider 实现

- [x] 2.1 创建 `crates/zeroclaw-providers/kart_nlp.rs` 实现 KartProvider
- [x] 2.2 为 `KartProvider` 实现 `Provider` trait（chat_with_system, chat, list_models 等）
- [x] 2.3 实现 `list_models()` 返回 SDK 模型列表
- [x] 2.4 实现 `supports_native_tools()` 和 `convert_tools()` 支持工具调用
- [x] 2.5 实现会话管理、聊天请求、流式结果处理方法
- [x] 2.6 调用 `genai_text_enable_internal_event_loop()` 启用 SDK 内部事件循环

## 3. 配置集成

- [x] 3.1 在配置中添加 `provider = "kart"` 选项
- [x] 3.2 更新 Provider 工厂函数支持 Kart Provider
- [x] 3.3 添加 `KART_SDK_PATH` 环境变量用于 SDK 路径（feature flag: `kart-nlp`）

## 4. 测试与验证

- [x] 4.1 使用 mock SDK 添加 FFI 绑定单元测试
- [x] 4.2 添加 Kart Provider 集成测试
- [x] 4.3 验证其他 Provider 仍能正常工作
- [x] 4.4 测试模型列表获取功能
- [x] 4.5 测试工具调用功能

## 5. Web 配置支持

- [x] 5.1 在 `crates/zeroclaw-gateway/src/api.rs` 添加 `handle_api_models` 处理函数，读取 `providers.fallback` 实例化 provider 并调用 `list_models()`，返回 `{"models": [...]}` （出错时返回空数组并 WARN 日志）
- [x] 5.2 在 `crates/zeroclaw-gateway/src/lib.rs` 注册 `GET /api/models` 路由，复用现有 auth 中间件
- [x] 5.3 在 `web/src/pages/config/sections/GeneralSection.tsx` 的 `PROVIDER_OPTIONS` 中添加 `{ value: 'kart', label: 'Kart' }`
- [x] 5.4 当选中 provider 不在 `MODELS_BY_PROVIDER` 静态列表中时，发起 `GET /api/models` 请求，结果存入本地 state；加载中禁用下拉并显示 loading 状态
- [x] 5.5 模型字段渲染逻辑：有动态模型时渲染 `<Select>`（当前保存的模型若不在列表中则置顶显示）；无模型时回退到原有 `<input>` 自由输入

## 6. 文档

- [x] 6.1 在 `docs/book/src/providers/catalog.md` 的 **Native** 分组下添加 `### Kart` 条目，包含：TOML 配置示例（`type = "kart"`）、前置依赖（`libkylin-genai-nlp-dev`、`KART_SDK_PATH`）、功能支持表（✓ 文本/对话/工具调用/流式/模型列表；✗ 视觉/Prompt Caching）
- [x] 6.2 在 `docs/book/src/providers/overview.md` 的 native provider 列表中加入 `kart`
- [x] 6.3 在 `docs/book/src/SUMMARY.md` 的 Model Providers 节添加 Kart 导航链接

## 7. 构建验证

- [x] 7.1 `cd web && npm run build` 确认前端构建通过
- [ ] 7.2 启动 gateway，打开 Config 页面，确认 `kart` 出现在 Provider 下拉中
- [ ] 7.3 选中 `kart`，确认模型下拉从 `GET /api/models` 动态加载
- [ ] 7.4 确认其他 provider（如 `anthropic`、`openai`）仍使用静态模型列表，无回归