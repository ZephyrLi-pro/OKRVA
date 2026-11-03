## 新增需求

### 需求：Kart Provider 实现

KylinBot 必须实现 `KartProvider`，通过 FFI 绑定包装 kylin-genai-nlp SDK。Provider 必须实现 `Provider` trait 的所有方法：
- `chat_with_system(system, message, model, temperature)` → 聊天请求
- `chat(request, model, temperature)` → 结构化聊天 API，支持工具调用
- `list_models()` → 返回 SDK 支持的模型列表
- `supports_native_tools()` → 返回是否支持原生工具调用
- `convert_tools(tools)` → 转换工具规格为 Provider 原生格式
- 会话创建和生命周期管理

### 需求：模型列表获取

Kart Provider 必须实现 `list_models()` 方法：
- 调用 `genai_text_get_model_list()` 获取可用模型
- 返回模型列表供配置和路由使用

#### 场景：获取模型列表
- **WHEN** agent 调用 `list_models()` 接口
- **THEN** 返回 kylin-genai-nlp SDK 支持的模型列表

### 需求：工具调用支持

Kart Provider 必须实现原生工具调用能力：
- `supports_native_tools()` 返回 true
- `convert_tools(tools)` 返回 Provider 原生工具格式
- 在 `chat()` 方法中处理工具调用响应

#### 场景：处理工具调用响应
- **WHEN** chat 响应包含工具调用
- **THEN** Provider 返回包含 tool_calls 的 ChatResponse

### 需求：会话生命周期管理

Kart Provider 必须正确管理会话生命周期：
- 调用 `genai_text_enable_internal_event_loop()` 启用 SDK 内部事件循环
- 在任何聊天请求之前调用 `genai_text_init_session()`
- 在清理时调用 `genai_text_destroy_session()`
- 优雅处理初始化错误

#### 场景：会话初始化成功
- **WHEN** `genai_text_init_session()` 返回 0
- **THEN** 会话可用于聊天请求

#### 场景：会话初始化失败
- **WHEN** `genai_text_init_session()` 返回非零值
- **THEN** Provider 返回错误且会话无效

### 需求：流式响应处理

Kart Provider 必须支持流式响应：
- 通过回调接收增量结果
- 从 `ChatResult` 中提取 `assistant_message`
- 检查 `is_end` 标志以判断完成状态

#### 场景：收到流式响应
- **WHEN** `ChatResultCallback` 触发增量结果
- **THEN** 返回 StreamChunk 给调用者

### 需求：显式 Provider 选择

KylinBot 必须通过配置支持显式 Provider 选择：
- `provider = "kart"` 时使用 Kart Provider
- 配置为其他值时使用对应的 Provider

#### 场景：Kart Provider 激活
- **WHEN** 配置指定 `provider = "kart"`
- **THEN** agent 使用 kylin-genai-nlp SDK 处理 AI 请求
