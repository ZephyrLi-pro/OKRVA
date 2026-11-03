# Current wake acknowledgement on Bianbu K1

The current voice channel directly invokes `pw-play --latency 20ms --volume 0.65` with the WAV path in `KYLINBOT_WAKE_ACK_FILE`. The effective service configuration is in the competition repository's `deploy/systemd/kylinbot.service`; the fixed reply is `assets/wake/wake_ack_zh_fast.wav` ("我在，请讲").

Install the WAV into `~/.local/share/kylinbot/wake_ack_zh_fast.wav`, matching that service setting. There is one current service configuration, with no separate wake-reply drop-in.

The channel waits for playback to drain while consuming microphone callbacks. It preserves a short audio tail as pre-roll, then starts capturing the user's request. Playback errors are non-fatal and the playback deadline is three seconds. Without the environment variable, capture begins without a spoken acknowledgement.

This is a pre-rendered wake response. Generated answers are spoken by the separately configured Matcha TTS service. The current implementation does not invoke a speech-dispatcher shim.
