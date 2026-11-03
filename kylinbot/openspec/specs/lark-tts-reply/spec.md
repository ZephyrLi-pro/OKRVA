## 新增需求

### Requirement: 飞书渠道可配置的回复模式
系统必须在 `LarkConfig` 中支持 `reply_mode` 配置字段，提供三个选项：`text_only`（默认）、`voice_only`、`voice_and_text`。回复模式决定 bot 如何回应用户消息。

#### Scenario: 默认回复模式为 text_only
- **WHEN** 飞书配置中未指定 `reply_mode`
- **THEN** 系统默认使用 `text_only`，仅发送交互卡片回复（保持现有行为）

#### Scenario: 配置 voice_only 模式
- **WHEN** `reply_mode` 设置为 `voice_only`
- **THEN** 系统对有实质内容的回复仅发送语音消息（不发送文字卡片）

#### Scenario: 配置 voice_and_text 模式
- **WHEN** `reply_mode` 设置为 `voice_and_text`
- **THEN** 系统对有实质内容的回复同时发送交互卡片和语音消息

### Requirement: LarkConfig 中的 TTS 配置
系统必须接受飞书渠道配置中的可选 `tts` 段。当 `reply_mode` 为 `voice_only` 或 `voice_and_text` 时，`tts` 段必须存在且 `tts.enabled` 必须为 `true`，否则系统记录 warning 日志并降级为 `text_only`。

#### Scenario: TTS 已配置且启用
- **WHEN** `reply_mode` 为 `voice_and_text` 且 `tts.enabled` 为 `true`
- **THEN** 系统为飞书渠道初始化 `TtsManager`

#### Scenario: 未配置 TTS 但设置了语音回复模式
- **WHEN** `reply_mode` 为 `voice_only` 但 `tts` 段缺失或 `tts.enabled` 为 `false`
- **THEN** 系统记录 warning 日志并降级为 `text_only` 模式

### Requirement: 语音回复的内容过滤
系统必须仅对有实质内容的自然语言回复进行语音转换。工具输出、URL、JSON、代码块、错误信息和过短的消息必须排除在语音合成之外。

#### Scenario: 有实质内容的文字回复
- **WHEN** 回复长度超过 40 个字符且包含自然语言文本
- **THEN** 系统将文本合成为语音并发送

#### Scenario: 非实质内容回复（代码块）
- **WHEN** 回复包含代码块（```）
- **THEN** 系统跳过语音合成，降级为发送文字卡片（所有模式下均保证用户收到文字回复）

#### Scenario: 非实质内容回复（URL 或 JSON）
- **WHEN** 回复以 "http"、"{" 或 "[" 开头
- **THEN** 系统跳过语音合成，降级为发送文字卡片

### Requirement: 异步语音合成与发送
系统必须异步执行 TTS 合成和音频上传，不阻塞文字消息的响应。在 `voice_and_text` 模式下，文字卡片必须立即发送，语音消息随后异步跟随。

#### Scenario: voice_and_text 异步流程
- **WHEN** 回复模式为 `voice_and_text` 且回复有实质内容
- **THEN** 先发送文字卡片，然后在 spawn 的异步任务中执行语音合成和上传

#### Scenario: voice_only 同步流程
- **WHEN** 回复模式为 `voice_only` 且回复有实质内容
- **THEN** 在 send 路径中同步 await 语音合成和上传，完成后才返回（不发送文字卡片）

#### Scenario: voice_and_text 模式下 TTS 合成失败
- **WHEN** `voice_and_text` 模式下 TTS 合成失败
- **THEN** 文字卡片已发送，系统记录 warning 日志，不发送语音消息

#### Scenario: voice_only 模式下 TTS 合成失败
- **WHEN** `voice_only` 模式下 TTS 合成失败
- **THEN** 系统记录 warning 日志并降级为发送文字卡片作为兜底恢复

### Requirement: 长文本优雅降级
系统必须处理超出 TTS `max_text_length` 限制的文本，自动进行降级。在 `voice_only` 模式下，当文本超出限制时，系统必须改为发送文字卡片以保证信息完整性。

#### Scenario: 文本在 TTS 限制范围内
- **WHEN** 回复文本在 `max_text_length`（默认 4096 字符）范围内
- **THEN** 系统正常合成并发送语音

#### Scenario: voice_only 模式下文本超出 TTS 限制
- **WHEN** 回复文本超出 `max_text_length` 且回复模式为 `voice_only`
- **THEN** 系统降级为发送文字卡片并记录 info 日志

#### Scenario: voice_and_text 模式下文本超出 TTS 限制
- **WHEN** 回复文本超出 `max_text_length` 且回复模式为 `voice_and_text`
- **THEN** 系统发送文字卡片并跳过语音合成
