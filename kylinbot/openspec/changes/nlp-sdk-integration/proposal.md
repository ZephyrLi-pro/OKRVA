## 背景

KylinBot 已支持多种 AI Provider（Claude、Gemini、OpenAI 等）。kylin-genai-nlp SDK提供了额外的本地/云端 AI 能力，应当并行集成以扩展 agent 的多样性，同时不影响现有 Provider。

## 变更内容

- 将 kylin-genai-nlp SDK 作为并行 AI Provider 接入 KylinBot
- 通过 FFI 绑定实现 SDK 的 C API 作为新的 Provider
- 通过配置选择使用哪个 Provider
- 不修改任何现有 Provider 实现

## 能力

### 新增能力
- `kart`：集成 kylin-genai-nlp SDK，提供文本生成、对话、模型列表获取、工具调用能力

### 修改的能力
- （无 - 现有能力保持不变）

## 影响范围

- 新增 `kart.rs` 实现 Kart Provider
- 依赖 `libkylin-genai-nlp-dev` 包
- 不影响任何现有 Provider 或功能
- 配置中添加 `provider = "kart"` 选项

## 能力边界

### 功能覆盖对比

| 功能 | 其他 Provider | kart |
|------|--------------|------|
| 文本生成/对话 | ✓ | ✓ |
| 模型列表获取 | ✓ | ✓ |
| 工具调用 | ✓ | ✓ |
| 流式响应 | ✓ | ✓ |
| 多轮对话 | ✓ | ✓ |
| Vision | ✓ | ✗ |
| Prompt Caching | ✓ | ✗ |

### 说明
- kart 专注于文本生成和对话，暂不支持 Vision 和 Prompt Caching
- 其他 Provider 已实现的能力保持不变
