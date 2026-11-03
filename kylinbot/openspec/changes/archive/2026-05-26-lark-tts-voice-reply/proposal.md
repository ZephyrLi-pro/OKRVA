## Why

飞书渠道目前已支持接收语音消息并转文字（STT），但回复只能以文字卡片形式发送。用户希望 bot 能以语音形式回复，形成完整的语音交互闭环。TTS 基础设施已成熟（支持 OpenAI、ElevenLabs、Google、Edge、Piper、Kart 六种 provider），Telegram 渠道已有 TTS 语音回复的参考实现，现在需要将这一能力扩展到飞书渠道。

## What Changes

- 为飞书渠道添加语音消息发送能力，通过飞书文件上传 API 上传音频并以 `msg_type: "audio"` 发送
- 在 `LarkChannel` 中集成 `TtsManager`，将模型文字回复通过 TTS 转换为语音后发送
- 新增飞书渠道级 TTS 配置项，支持选择回复模式：仅文字、仅语音、语音+文字
- 处理音频格式兼容性（飞书要求 opus 格式，需在 TTS 输出后按需转码）

## Capabilities

### New Capabilities
- `lark-voice-send`: 飞书渠道语音消息发送能力，包括音频文件上传和 audio 类型消息投递
- `lark-tts-reply`: 飞书渠道 TTS 语音回复集成，将模型文字回复转语音并发送，支持可配置的回复模式

### Modified Capabilities

_(无需修改现有 spec 级别的行为)_

## Impact

- **代码变更**: `crates/kylinbot-channels/src/lark.rs`（核心发送逻辑）、`crates/kylinbot-config/src/schema.rs`（LarkConfig 扩展）、`crates/kylinbot-channels/src/audio_transcode.rs`（新增 opus 转码）
- **API 依赖**: 飞书开放平台文件上传 API (`/im/v1/files`，`file_type=opus`)、音频消息发送 API (`/im/v1/messages`，`msg_type=audio`)
- **飞书应用权限**: 需要开启 `im:message`（发送消息）、`im:message:send_as_bot`（以 bot 身份发消息）、`im:resource`（文件上传）权限，应用需处于"已发布"状态
- **音频格式**: 飞书语音消息要求 opus 编码，部分 TTS provider 输出 mp3/wav/pcm，通过现有 ffmpeg 转码模块处理
- **现有功能**: 不影响现有文字卡片回复逻辑，语音回复为附加能力
- **参考实现**: Telegram 渠道的 `synthesize_and_send_voice()` 可作为模式参照
