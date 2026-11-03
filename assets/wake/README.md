# 当前固定唤醒提示音

`wake_ack_zh_fast.wav`是当前KylinBot唤醒后播放的“我在，请讲”。

`deploy/systemd/kylinbot.service`通过`KYLINBOT_WAKE_ACK_FILE`指定该文件，`voice_wake.rs`直接调用`pw-play --latency 20ms --volume 0.65`并等待播放结束。播放失败或超时后继续收音，不调用其他播报脚本。

这是程序使用的预生成固定语音资源，不是用户录音或模型权重。回答内容的动态播报由Matcha TTS处理。
