# Bianbu K1 low-power KWS worker

This helper preserves the original `kylinbot-kws-stream` positional CLI and
`READY` / `KEYWORD\t...` stdout protocol. It adds opt-in controls, without changing
KylinBot Rust channels, the assistant UI, ASR, TTS, or the LLM backend.

- `KYLINBOT_KWS_VAD_GATE=1`: Silero VAD selects speech, with 0.6 seconds of
  pre-roll and 1.0 seconds of trailing audio. Needs `KYLINBOT_VAD_MODEL`.
  If initialization fails, continue continuous KWS with a warning.
- `KYLINBOT_KWS_CPUSET=0-3`: apply a Linux CPU mask to all helper threads after
  runtime initialization. Invalid/unavailable masks fail startup explicitly.
- `KYLINBOT_KWS_BUSY_FILE`: an optional heartbeat containing `epoch busy`, mode
  0600, owned by the current user. A busy worker consumes/discards audio and
  resets decoding state, rather than accumulating stale audio in a pipe.
  Missing, malformed, unsafe, or >3-second-old heartbeats fail open.

The selected deployment keeps the original epoch-12 INT8 Zipformer KWS model
on CPU0-3, with one inference thread. In the same waveform test, wake timing was
similar to two threads and to FP32 on CPU4-7, while CPU time was about half the
two-thread INT8 case. Quiet gating and busy-time suppression, not mandatory
core segregation, are the primary resource policy. The other cluster remains
available for desktop/vision work; no cores are exclusively reserved for KWS.

FP32 on CPU4-7 remains a tested alternative if simultaneous wake/LLM processing
is needed later. K1's IME-enabled INT8 kernels cannot simply be pinned there:
that private test exited with SIGILL. No system ORT replacement is necessary.

`kws-policy.py` observes the existing llama.cpp loopback metrics (processing and
deferred requests) and the assistant's read-only `GetInputStatus` D-Bus method.
It also observes aggregate CPU ticks of the unique `llama-server --port 18082`
process to cover prefill intervals when metrics queries queue behind inference.
A 20%-of-one-core threshold is an activity hint, not a model performance measure.
PID start-time checks prevent reusing a stale process identity. Missing or
ambiguous processes fail open. It never reads conversation text, environment,
or microphone audio. The small policy daemon
is optional/fail-open; it does not start the assistant through D-Bus. Busy
means LLM inference or assistant speech synthesis/playback; a 0.3-second
cooldown avoids playback tails. This is half-duplex wake behavior: wake-word
barge-in while answering is intentionally disabled. Keyboard/UI interaction
is not blocked. Queries may take up to about 0.65 seconds if endpoints stall.

Build on the target using its existing Sherpa C API header/library:

```sh
cc -O2 -std=c11 -Wall -Wextra -Werror -Iinclude \
  kylinbot-kws-stream.c -L/path/to/sherpa/lib \
  -Wl,-rpath,/path/to/sherpa/lib -lsherpa-onnx-c-api -o kws-gated
python3 -m unittest -v test_policy
```

The dated private experiment's `offline_probe.py` and `test_kws.py` require a
generated/public PCM16 `fixtures/wake-melo.wav`. They never capture a microphone
or play audio. Tests include silence CPU, quiet wake speech, repeated wake,
busy/resume, stale-heartbeat fallback, and actual thread masks. Real microphone
wake must still be verified by the user after deployment.

Install a separate `kylinbot-kws-gated` binary and policy script, keeping the
original helper, old drop-ins, and model files untouched. The new policy unit
and `zz-kws-power.conf` enable these features. The drop-in must sort after the
existing `kws.conf`. To roll back, remove only the new
drop-in, reload systemd, restart KylinBot, and stop/disable the new policy unit.
No other voice, UI, adapter, or LLM configuration needs replacement.
