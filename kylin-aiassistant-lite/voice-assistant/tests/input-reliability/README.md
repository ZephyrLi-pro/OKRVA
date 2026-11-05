# Voice input reliability changes — 2026-10-12

Target: the existing K1/Bianbu deployment. Keep SenseVoice/SpacemiT ASR,
Qwen3-0.6B-Q4_K_M, and the resident bilingual MeloTTS service unchanged.
No upstream commits, pushes, new speech models, ORT rebuilds, or microphone
recordings are part of this work.

## Behavior

- Small-K accepts at most three pending external-ASR questions in FIFO order.
  Questions are not replaced by later arrivals. Full and oversized requests
  receive explicit UI feedback. Agent errors and shutdown cancel the queue.
  Expired capture replies cannot submit a stale instruction; a fresh wake
  handshake is required to accept microphone text again after the watchdog.
- Context lookup is bounded to 1.5 seconds; asynchronous completion cannot
  reorder questions. The native Agent chat publishes the visible unsent draft
  through `org.ukui.kylin_aiassistant.GetVoiceContext`. It is reference data,
  not a system prompt, and is capped at 2,000 characters with a truncation flag.
  The snapshot is attached when the question is queued, not re-read on dequeue.
  Sent conversations in other sessions are NOT automatically merged.
- `org.ukui.voice_assistant.PrepareExternalListening` returns `ready`,
  `speaking`, `muted`, `full`, `capturing`, or `closing`. KylinBot must not play
  the wake acknowledgement or record a question for a rejected request.
  Capturing during a pending text answer suppresses that old answer's speech.
  This is half-duplex protection, not acoustic echo cancellation or barge-in.
- Stopping speech suppresses subsequent speech fragments for that response.
  A finished audio fragment cannot advertise Idle while LLM output is pending.
- An optional persistent Silero worker classifies post-wake microphone frames.
  It uses the installed sherpa-onnx 1.13.2 C API, the already present
  `silero_vad.onnx`, CPU/one thread, and streaming resampling to 16 kHz.
  Original captured audio is kept; SenseVoice's second RMS crop stays disabled.
  The existing onset/pre-roll/silence rules use neural speech activity rather
  than a fixed volume threshold. Worker failure visibly logs energy fallback.
- A recording that reaches its maximum before sentence-end silence is rejected
  with a split-the-instruction prompt instead of sending a truncated command.
- A reader can call `SetReadingContext(title, text, page)` on the voice D-Bus
  interface. Text must be <=2,000 characters and page >=1; empty text clears
  the page. Published page context expires after two minutes. A reader must
  refresh it on page changes and before asking, and clear it on close.
  No real reader/gesture application is wired in this patch. Page questions
  without reference text are refused instead of answered without evidence.
- Reference material is not dumped into Gateway diagnostic logs. Existing tool
  whitelists, path restrictions, confirmation rules, and model settings remain.

## Validation sources

- `test_externalvoicequeue.cpp`: FIFO, context completion ordering, limits,
  cancellation, and stale callbacks.
- `test_voice_input.cpp`: actual MainWindow state handling and an isolated mock
  Unix-domain WebSocket gateway; no test tools, real LLM, TTS, or microphone.
- `test_vad_stream.py`: existing synthesized WAV only; silence, normal/quiet
  speech, white noise, pure tone, reset, and EOF.
- Rust `voice_wake::tests`: capture/onset/pre-roll and permission parsing.

Compilation and device deployment status are recorded separately in the final
report. Offline tests do not establish real microphone wake/recognition accuracy.

## 当前页资料选择修复（2026-11-04）

只有明确提到这一页、这页、当前页或本页的语音问题才携带阅读器快照。普通问题只查询主聊天框草稿，不能因为PPT保持打开就自动携带其正文，也不能把PPT正文和日志草稿拼成一份资料。`test_voice_input.cpp`新增PPT仍打开时询问身份、不得附带当前页正文的回归检查。

## 单轮唤醒模式（2026-11-05）

默认关闭回答后的自动追问：播报结束保留回答历史，状态回到“等待唤醒”，再次提问需说唤醒词。
`~/.config/kylin-aiassistant/settings.ini` 的 `[General]` 下
`voice-followup-enabled=false` 控制此行为；未配置时同样关闭。
关闭模式下 `PrepareFollowupListening` 直接返回 `off`，不启动9秒定时器、不自动认领录音。
测试覆盖流式播报的提前结束信号、历史保留及再次显式唤醒，同时保留显式启用追问时的原有测试。
