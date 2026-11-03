## ADDED Requirements

### Requirement: 音频格式自动转码
系统 SHALL 在 Kart STT provider 收到非 PCM/WAV 格式音频时，自动将其转码为 16kHz 16bit 单声道 WAV 格式后再送入识别。支持的输入格式包括：m4a、ogg、mp3、aac、opus、flac。

#### Scenario: 支持的格式自动转码
- **WHEN** KartTranscriptionProvider 收到扩展名为 `.m4a`、`.ogg`、`.mp3`、`.aac`、`.opus` 或 `.flac` 的音频文件
- **THEN** 系统调用 ffmpeg 将其转码为 16kHz 16bit 单声道 WAV，再提取 PCM 送入 Kart 识别器

#### Scenario: PCM/WAV 格式跳过转码
- **WHEN** KartTranscriptionProvider 收到扩展名为 `.pcm` 或 `.wav` 的音频文件
- **THEN** 系统不执行转码，直接走现有 PCM 提取路径

#### Scenario: 不支持的格式
- **WHEN** KartTranscriptionProvider 收到不在支持列表中的格式（如 `.wma`、`.amr`）
- **THEN** 系统仍然尝试调用 ffmpeg 转码；若 ffmpeg 也无法处理，返回转码失败错误

### Requirement: ffmpeg 可用性检测
系统 SHALL 在首次转码时检测 ffmpeg 是否可用，检测结果缓存于内存中。不可用时返回明确错误信息。

#### Scenario: ffmpeg 不可用
- **WHEN** 系统首次尝试转码但 ffmpeg 未安装或不在 PATH 中
- **THEN** 返回错误信息，包含不支持的格式和 ffmpeg 缺失的提示；后续转码请求直接返回缓存错误，不再重复检测

#### Scenario: ffmpeg 可用
- **WHEN** 系统首次尝试转码且 ffmpeg 在 PATH 中可用
- **THEN** 缓存可用状态，正常执行转码流程，后续请求跳过检测

### Requirement: 临时文件清理
系统 SHALL 在转码完成后自动清理产生的临时文件，无论转码成功、失败或超时。

#### Scenario: 转码成功后清理
- **WHEN** ffmpeg 转码成功完成
- **THEN** 临时 WAV 文件被自动删除

#### Scenario: 转码失败后清理
- **WHEN** ffmpeg 转码失败（如格式损坏）
- **THEN** 临时文件仍被自动删除，错误向上传播

#### Scenario: 转码超时后清理
- **WHEN** ffmpeg 子进程超过 30 秒未完成
- **THEN** 子进程被 kill，临时文件被自动删除，返回超时错误

### Requirement: 转码超时保护
系统 SHALL 为每次 ffmpeg 转码设置 30 秒超时，防止子进程卡死。

#### Scenario: 正常转码在超时内完成
- **WHEN** ffmpeg 在 30 秒内完成转码
- **THEN** 正常返回转码结果

#### Scenario: 转码超时
- **WHEN** ffmpeg 子进程运行超过 30 秒
- **THEN** 系统终止子进程，返回超时错误，清理临时文件
