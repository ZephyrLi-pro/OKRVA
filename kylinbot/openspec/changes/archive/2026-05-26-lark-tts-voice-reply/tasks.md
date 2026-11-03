## 1. 配置扩展

- [x] 1.1 在 `LarkConfig`（`crates/kylinbot-config/src/schema.rs`）中新增 `reply_mode: LarkReplyMode` 字段（枚举：`TextOnly`/`VoiceOnly`/`VoiceAndText`，默认 `TextOnly`）
- [x] 1.2 在 `LarkConfig` 中新增 `tts: Option<TtsConfig>` 字段，复用现有 `TtsConfig` 结构
- [x] 1.3 添加配置校验逻辑：当 `reply_mode` 非 `TextOnly` 时，检查 `tts` 存在且 `enabled=true`，否则 warn 并降级为 `TextOnly`

## 2. 飞书音频上传

- [x] 2.1 在 `LarkChannel` 中实现 `upload_audio(&self, audio_bytes: &[u8]) -> Result<String>` 方法，调用飞书 `/im/v1/files` API（multipart/form-data，file_type=opus），返回 `file_key`
- [x] 2.2 实现 token 过期重试逻辑（与现有 `send_text_once` 中的 token refresh 模式一致）

## 3. 飞书音频消息发送

- [x] 3.1 在 `LarkChannel` 中实现 `send_audio(&self, chat_id: &str, file_key: &str) -> Result<()>` 方法，构造 `msg_type: "audio"` + `content: {"file_key": "..."}` 发送
- [x] 3.2 处理 send 的 token 刷新和错误逻辑

## 4. 音频格式转码

- [x] 4.1 实现 opus 格式检测函数 `is_opus(audio_bytes: &[u8]) -> bool`（通过检查 OggS magic bytes + opus head）
- [x] 4.2 在 `audio_transcode.rs` 中新增 `transcode_to_opus(audio_bytes: &[u8], source_format: &str) -> Result<Vec<u8>>` 函数，复用现有 ffmpeg 调用模式，输出 opus 格式（`-acodec libopus`）
- [x] 4.3 在 TTS 合成后集成格式检查和按需转码逻辑

## 5. TTS 语音回复集成

- [x] 5.1 在 `LarkChannel` struct 中添加 `tts_config: Option<TtsConfig>` 和 `reply_mode: LarkReplyMode` 字段，在构造时从 config 初始化
- [x] 5.2 提取 `is_substantive_for_voice(content: &str) -> bool` 为公共函数（从 Telegram 实现中抽取），放在 `crates/kylinbot-channels/src/tts.rs` 或独立 utils 模块
- [x] 5.3 修改 `LarkChannel::send()` 方法：根据 `reply_mode` 分支处理——`TextOnly` 保持现有逻辑，`VoiceAndText` 发送文字卡片后 spawn 异步语音任务，`VoiceOnly` 同步 await 语音发送路径；非实质内容在所有模式下均降级为文字卡片
- [x] 5.4 实现 `synthesize_and_send_voice` 异步方法：TtsManager 合成 → 格式转码 → 上传音频 → 发送音频消息
- [x] 5.5 实现长文本降级：文本超过 `max_text_length` 时，`VoiceOnly` 模式降级为发送文字卡片

## 6. 测试

- [x] 6.1 为 `upload_audio` 和 `send_audio` 编写单元测试（mock 飞书 API）
- [x] 6.2 为 `is_opus` 和 `transcode_to_opus` 编写单元测试（使用真实音频样本字节）
- [x] 6.3 为 `is_substantive_for_voice` 编写单元测试
- [x] 6.4 为 `send()` 的三种 reply_mode 分支编写集成测试
- [x] 6.5 为配置校验逻辑（TTS 缺失降级）编写测试
