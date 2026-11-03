# Bounded hands-free follow-up — 2026-10-16

After one explicit wake and a successful answer, the external-ASR voice window
offers a nine-second follow-up window. Both the gateway response and speech
playback must have finished. This is half-duplex conversation, not barge-in or
acoustic echo cancellation. ASR, LLM, TTS models and tool permissions are unchanged.

The optional D-Bus method `PrepareFollowupListening` returns:

- `waiting`: a response, playback, existing capture, or queued request is active.
- `ready:<milliseconds>`: atomically claim one capture; the remaining budget is
  between 1 and 9000. Only the external recorder owns the microphone.
- `off`: unavailable, closed, hidden, muted, expired, failed, or no successful
  preceding response. Old voice-window versions also fail closed.

KylinBot polls only after handing a recognized question to the voice window.
Polling does not activate an absent UI. The initial wake still plays its existing
acknowledgement; follow-up captures do not replay it. The same gateway session
and per-input context snapshots are reused, without merging other chat sessions.

Only a bounded 350 ms audio tail is kept in RAM. Before claiming a follow-up,
the recorder removes samples preceding the UI's playback-completion boundary.
Buffered post-playback audio is processed through the normal VAD. Once speech
has started, normal sentence-end and maximum-duration rules apply; the nine-second
budget is an onset deadline, not a nine-second hard truncation of the question.

No speech returns quietly to wake-word mode. Exact normalized phrases
`结束对话` and `退出对话` call `EndExternalConversation`, without asking the LLM
or executing a system tool. A missing UI during a follow-up drops the input
instead of silently moving it to a different agent session.

`test_voice_input.cpp` exercises the real MainWindow with a mock gateway:
playback/response gates, deadline, one-shot capture, stable session, timeout,
mute, explicit end, hidden window, and existing queue/context guards.
Rust voice-wake tests cover permission parsing, post-playback pre-roll, onset
deadlines, resumed normal-wake policy, and exact end-command matching.
These are not a substitute for physical microphone/noise/echo testing.
