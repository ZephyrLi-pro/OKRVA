# Short wake acknowledgement on Bianbu

The K1 deployment can play the existing Mandarin reply, “我在，请讲”, before
capturing a request. Set `KYLINBOT_WAKE_ACK_FILE` to a trimmed WAV file using
the accompanying `wake-ack.conf` systemd drop-in. Without this variable,
KylinBot starts capture without a spoken acknowledgement.

The original recording lasts 2.574 seconds, including approximately 0.900
seconds of trailing silence. `wake_ack_zh_fast.wav` trims the beginning and
end, and applies a 1.1x tempo adjustment while preserving pitch. Its duration
is 1.493 seconds. It was prepared from the existing deployment recording with:

```sh
ffmpeg -nostdin -i wake_ack_zh.wav \
  -af 'atrim=start=0.12:end=1.75,asetpts=PTS-STARTPTS,atempo=1.1' \
  wake_ack_zh_fast.wav
```

`voice_wake.rs` plays the WAV directly with `pw-play --latency 20ms` and keeps
consuming microphone callbacks during playback. It retains only a 350 ms tail
as pre-roll instead of clearing every queued sample after `pw-play` exits.
This protects speech spoken after the audible reply but before the process exits;
it is not acoustic echo cancellation and does not support talking over the reply.
Capture waits up to eight seconds for an 80 ms sustained speech onset. Only
after onset does the configured end-of-speech silence timeout apply, measured
in audio samples rather than inference/D-Bus wall time. A rolling quiet-floor
estimate lowers the utterance onset threshold for quiet microphones, bounded
above by the configured energy threshold and below by RMS 0.001.
The first empty or punctuation-only transcription retries once while keeping
the UI open and preserving audio arriving during ASR. A second empty result,
no speech or ASR error calls `ExternalRecognitionFailed` rather than `Quit`.
The lite UI displays a retry message and stays alive. Deploy the matching UI
and Rust changes together. Capture logs include duration, RMS and threshold;
no raw microphone audio is saved by this change.
The matching SenseVoice bridge sets native `vad_enabled = false`: the backend's
fixed -45 dBFS endpoint crop must not cut quiet words from an already-segmented
utterance. This keeps the supplied WAV intact; it does not change SpacemiT EP.
Playback failures are non-fatal and time out after three seconds.

The lite UI reads UTF-8 prompt INI files explicitly. Its Gateway reply inactivity
deadline is 180 seconds and refreshes on answer chunks, draft/iteration starts
and tool-call/result frames, so progressing multi-step tasks are not treated as
stalled merely because they have not produced visible text yet. Expiry sends the
existing cancellation frame and presents an error instead of waiting forever.
The Adapter's existing K1 prompt/tool budget recognizes only exact greetings,
identity questions and the isolated fragments “我”/“你” as no-tool conversations,
with at most 80 output tokens. Compound system tasks, images, tool-result turns
and forced tool calls retain the Agent path and existing security policy.
This does not install a TTS backend: the prerecorded wake acknowledgement and
generated-answer speech synthesis remain separate capabilities.

Install from the repository root:

```sh
install -d "$HOME/.local/share/kylinbot" \
  "$HOME/.config/systemd/user/kylinbot.service.d"
install -m 0644 tools/bianbu-wake-reply/wake_ack_zh_fast.wav \
  "$HOME/.local/share/kylinbot/wake_ack_zh_fast.wav"
install -m 0644 tools/bianbu-wake-reply/wake-ack.conf \
  "$HOME/.config/systemd/user/kylinbot.service.d/wake-ack.conf"
systemctl --user daemon-reload
systemctl --user restart kylinbot.service
```

This requires PipeWire's `pw-play` command. Speak after the reply ends; speaking
over the reply requires acoustic echo cancellation and is outside this change.

On the target board, the original playback command took 2.809 seconds, and the
trimmed low-latency playback took 1.662 seconds. Removing the previous 300 ms
post-playback sleep reduces the measured wait by approximately 1.45 seconds.
