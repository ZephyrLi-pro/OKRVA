# 当前K1低功耗语音唤醒

`kylinbot-kws-stream.c`编译为当前`kylinbot-kws-gated`，维持KylinBot既有READY/KEYWORD协议。当前使用epoch-12 INT8 Zipformer、1推理线程、CPU0–3。

`KYLINBOT_KWS_VAD_GATE=1`使用Silero语音门控，保留前滚和尾音；`KYLINBOT_KWS_BUSY_FILE`读取策略心跳，在LLM/TTS忙时丢弃音频并重置解码，避免积压。失效或过期心跳会恢复监听。`kws-policy.py`是设备当前版本，观察18082/18084、Adapter重推理活动及小K播报状态；不读取会话正文或录音。

构建时使用设备匹配的sherpa-onnx C API头文件和库：

```sh
cc -O2 -std=c11 -Wall -Wextra -Werror -I/path/to/sherpa/include   kylinbot-kws-stream.c -L/path/to/sherpa/lib   -Wl,-rpath,/path/to/sherpa/lib -lsherpa-onnx-c-api -o kylinbot-kws-gated
python3 -B -m unittest -v test_policy
```

当前服务与环境设置统一位于仓库根目录`deploy/systemd/kylinbot.service`及`kylinbot-kws-policy.service`，不另保留旧drop-in。手势/桌面可使用另一簇CPU，KWS没有独占其余四核。
