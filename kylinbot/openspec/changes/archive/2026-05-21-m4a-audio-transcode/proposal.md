## Why

飞书语音消息默认发送 m4a 格式音频，但 Kart 端侧语音 SDK 只接受 PCM/WAV 格式。当前 Kart STT 提供者在收到非 PCM/WAV 文件时直接报错，导致使用飞书渠道 + Kart 端侧转写的场景完全不可用。

## What Changes

- 在 `crates/kylinbot-channels` 内新增音频转码模块，将 m4a/ogg/mp3/aac 等格式转为 WAV/PCM
- 在 Kart STT 提供者的转写流程中，对不支持的格式自动转码后再送入识别
- 飞书渠道无需改动，转码逻辑对上游透明

## Capabilities

### New Capabilities
- `audio-transcode`: 音频格式转码能力——将 m4a、ogg、mp3、aac 等音频流转为 Kart SDK 所需的 PCM/WAV 格式

### Modified Capabilities

（无现有 spec 需要修改）

## Impact

- `crates/kylinbot-channels`：新增 `audio_transcode.rs` 模块（依赖系统 ffmpeg）
- `crates/kylinbot-channels/src/transcription.rs`：Kart STT 提供者调用前插入转码步骤
- 系统级依赖：ffmpeg（Kylin OS 预装）
- Cargo 依赖：新增 `tempfile`、`which`
