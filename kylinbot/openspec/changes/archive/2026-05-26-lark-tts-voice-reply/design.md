## Context

飞书渠道（`LarkChannel`）当前已支持：
- **接收语音**：通过 `/im/v1/messages/{id}/resources/{file_key}` 下载音频，可选通过 `TranscriptionManager` 转文字
- **发送消息**：仅支持 interactive card（markdown 卡片），通过 `/im/v1/messages` 发送

TTS 基础设施已成熟，`TtsManager` 支持 6 种 provider（OpenAI、ElevenLabs、Google、Edge、Piper、Kart），返回音频字节流。Telegram 渠道已实现 TTS 语音回复（`synthesize_and_send_voice`），可作为参考模式。

飞书开放平台支持通过 `/im/v1/files` 上传音频文件获取 `file_key`，再以 `msg_type: "audio"` 发送语音消息。音频格式要求为 **opus**（上传时 `file_type` 参数固定为 `"opus"`）。

使用这些 API 需要飞书应用开启以下权限：
- `im:message` / `im:message:send_as_bot` — 发送消息
- `im:resource` — 文件资源上传
- 应用需处于"已发布"状态（开发版仅限开发者本人可收到消息）

## Goals / Non-Goals

**Goals:**
- 在飞书渠道实现语音消息发送能力（音频上传 + audio 消息类型）
- 集成 TtsManager，将模型文字回复转为语音发送
- 支持可配置的回复模式：`text_only`（默认，保持现有行为）、`voice_only`（仅语音回复）、`voice_and_text`（同时发送文字和语音）
- 处理音频格式转码（TTS 输出 → opus）

**Non-Goals:**
- 不涉及实时语音通话（VoIP）
- 不修改现有 STT 转录逻辑
- 不实现语音消息的流式发送
- 不支持自定义 TTS 触发规则（如 Telegram 的 voice_chat 模式），本期只做全局配置

## Decisions

### 1. 回复模式设计：LarkConfig 扩展 vs Channel trait 层面

**选择**：在 `LarkConfig` 中新增 `reply_mode` 和 `tts` 字段

**理由**：
- 飞书的回复模式配置是渠道级别的，不同渠道有不同的语音消息 API，不适合抽象到 trait 层
- 与 Telegram 的 `tts_config` 模式一致，用户通过 TOML 配置即可启用
- 替代方案（Channel trait 统一 TTS 接口）需改动所有渠道，不符合本次变更最小化原则

**配置示例**：
```toml
[channels_config.lark]
app_id = "..."
app_secret = "..."
reply_mode = "voice_and_text"  # "text_only" | "voice_only" | "voice_and_text"

[channels_config.lark.tts]
enabled = true
default_provider = "openai"
default_voice = "alloy"
default_format = "opus"
```

### 2. 音频上传方式：文件上传 API vs 内联 Base64

**选择**：使用飞书 `/im/v1/files` 文件上传 API

**理由**：
- 飞书音频消息要求先上传文件获取 `file_key`，再通过消息 API 引用
- 这是飞书官方推荐的方式，支持大文件和更好的缓存
- 替代方案（Base64 内联）飞书 audio 消息类型不支持

### 3. 音频格式处理：TTS 输出转 opus

**选择**：优先请求 TTS provider 直接输出 opus，不支持时通过现有 `audio_transcode.rs` 模块调用 ffmpeg 转码

**理由**：
- OpenAI TTS 和 Piper TTS 直接支持 opus 输出格式，零转码开销
- 对于输出 mp3/wav 的 provider（Edge、Google 等），项目已有基于 ffmpeg 的 `audio_transcode.rs` 模块（`crates/kylinbot-channels/src/audio_transcode.rs`），当前支持转 WAV，扩展一个 `transcode_to_opus()` 函数即可
- 复用 ffmpeg 方案而非引入 opus crate：ffmpeg 已是项目依赖，无需新增 Rust crate；ffmpeg 的格式支持更广泛，处理 mp3/wav/aac/pcm → opus 均可一站式完成
- 飞书严格要求 opus 格式，不做转码会导致消息发送失败

### 4. 语音回复的内容过滤

**选择**：复用 Telegram 的 substantive content 判断逻辑，过滤掉非自然语言内容

**理由**：
- 工具输出（URL、JSON、代码块、错误信息）不适合语音播报
- Telegram 已验证过这套过滤规则的有效性
- 提取为共享函数 `is_substantive_for_voice()` 避免重复代码

### 5. 异步语音发送策略

**选择**：在 `send()` 方法中同步判断，`voice_and_text` 模式异步 spawn 发送语音，`voice_only` 模式同步 await

**理由**：
- 语音合成和上传耗时较长（TTS ~1-3s，上传 ~0.5s），`voice_and_text` 模式不能阻塞文字消息回复，用 spawn 异步跟随
- `voice_only` 模式下必须同步 await 等待语音发送完成，否则 `send()` 返回后调用方认为消息已发送但实际尚未完成，失败也无法触发降级
- 与 Telegram 模式一致：文字消息立即发送，语音异步跟随；但 voice_only 是飞书特有需求，需要更严格的完成保证

## Risks / Trade-offs

- **[opus 转码依赖]** → 部分 TTS provider（Edge、Google）输出非 opus 格式，需 ffmpeg 转码。缓解：ffmpeg 已是项目现有依赖（`audio_transcode.rs`），无需新增依赖；优先使用支持 opus 的 provider（OpenAI）减少转码频率
- **[飞书文件上传限额]** → 飞书对文件上传有频率限制。缓解：语音回复已通过 substantive 过滤减少频率，短期内不太可能触达限额
- **[voice_only 模式下的长文本]** → TTS 有 max_text_length 限制（默认 4096 字符），超长文本会截断。缓解：超长时自动降级为 voice_and_text 模式，保证信息完整
- **[异步发送失败无反馈]** → spawn 的语音发送任务失败后用户看不到错误。缓解：失败时 log warn，voice_and_text 模式下文字已发送不影响信息传递

## Open Questions

- **飞书语音消息的文件大小限制** — 飞书文档未明确语音消息的上限，需实测确认。如果有限制，需在 TTS 合成前估算音频时长并做截断
- **群聊 mention_only 模式下的语音回复** — 当 `mention_only=true` 时，bot 只响应 @消息，语音回复在群聊中是否会造成干扰？可能需要群聊场景下默认降级为 text_only
