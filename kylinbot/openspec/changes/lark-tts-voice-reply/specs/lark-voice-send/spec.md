## 新增需求

### Requirement: 上传音频文件到飞书
系统必须将音频字节上传到飞书文件上传 API（`/im/v1/files`），指定 `file_type: "opus"`，并返回 `file_key` 用于音频消息发送。上传必须携带有效的 tenant access token，使用 `multipart/form-data` 编码。

#### Scenario: 音频上传成功
- **WHEN** 系统持有 opus 音频字节和有效的 tenant access token
- **THEN** 系统将音频上传到 `/im/v1/files`（`file_type=opus`），并从响应中获取 `file_key`

#### Scenario: 上传时 token 过期
- **WHEN** 上传请求返回 token 过期错误
- **THEN** 系统刷新 tenant access token 并重试一次

#### Scenario: 上传失败
- **WHEN** 文件上传 API 在重试后仍返回非成功状态
- **THEN** 系统记录 warning 日志并返回错误

### Requirement: 通过飞书发送音频消息
系统必须构造 `msg_type: "audio"` 载荷（包含上传获得的 `file_key`），通过 `/im/v1/messages` 向飞书会话发送音频消息。

#### Scenario: 音频消息发送成功
- **WHEN** 系统持有有效的 `file_key` 和 `chat_id`
- **THEN** 系统发送 `msg_type: "audio"`、`content: {"file_key": "<key>"}` 的消息到飞书消息 API

#### Scenario: 发送失败并刷新 token
- **WHEN** 音频消息发送返回 token 过期错误
- **THEN** 系统刷新 token 并重试一次，与现有文字发送的 token 刷新行为一致

### Requirement: 音频格式必须为 opus
系统必须确保发送到飞书的所有音频为 opus 格式。如果源音频不是 opus，系统必须在上传前进行转码。

#### Scenario: TTS provider 直接输出 opus
- **WHEN** TTS provider 返回 opus 格式的音频
- **THEN** 系统直接上传音频字节，无需转码

#### Scenario: TTS provider 输出非 opus 格式
- **WHEN** TTS provider 返回 mp3、wav 或 pcm 格式的音频
- **THEN** 系统在上传前将音频转码为 opus

#### Scenario: 转码失败
- **WHEN** opus 转码失败
- **THEN** 系统记录 warning 日志并返回错误，不发送音频消息
