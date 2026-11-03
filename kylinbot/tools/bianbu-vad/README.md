# Optional Silero speech activity worker

This worker classifies mono float32 input for KylinBot's post-wake capture. It
does not open the microphone, perform ASR, crop a recording, or save audio.
It reuses an installed sherpa-onnx 1.13.2 shared runtime and a separately
provided `silero_vad.onnx`. The vendored C API header and its Apache-2.0 license
are under `include/`.

Build with `bash build-vad.sh /absolute/path/to/sherpa-runtime`. The output is
`kylinbot-vad-stream` in this directory. Install it in the user-owned helper
directory and configure `KYLINBOT_VAD_BINARY` and `KYLINBOT_VAD_MODEL` in the
KylinBot user service. With these variables absent, the existing adaptive-energy
capture remains available. Worker startup/inference failures log fallback.

Provider is CPU with one thread. Input is resampled to 16 kHz using sherpa's
streaming resampler. Its short VAD hangover is followed by the channel's
configured sentence silence deadline; this is not semantic sentence detection.

Protocol: stdin has a little-endian unsigned sample count followed by mono
IEEE-754 float32 samples. Zero count resets VAD and resampler state; counts
above 65,536 or truncated packets fail. Stdout emits `READY` at startup and a
single `0`/`1` line for each request. Runtime diagnostics go to stderr.

`test_vad_stream.py BINARY MODEL SYNTHESIZED_WAV` uses a supplied PCM16 fixture
only and never records the microphone. It covers silence, normal/quiet speech,
white noise, pure tone, reset, and EOF. These checks do not replace device
microphone testing under the actual room noise and speaker playback.
