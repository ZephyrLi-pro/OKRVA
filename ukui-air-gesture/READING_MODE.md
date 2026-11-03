# 当前阅读挥手模式

K1当前服务使用`AIR_GESTURE_PROFILE=reading`、`AIR_GESTURE_READING_TARGET=reader`，摄像头由`AIR_GESTURE_CAMERA`指定。手势服务与PPT阅读器独立运行，通过D-Bus交互。

```sh
systemctl --user start ukui-air-gesture.service
systemctl --user start riscv-ai-reader.service
```

张开手掌，先完整进入画面中间的复位区，再向一侧挥动。**右挥下一页，左挥上一页**。每次向外挥动只执行一次；回中间只复位，手部丢失后需重新进入中间，不拼接丢失前后的轨迹。

识别使用最新帧，不积累旧帧；过期结果不会触发翻页。当前palm/pose模型默认各4线程。动作队列定向调用阅读器的`org.riscv.AIReader1.Navigate`，并按实际页码确认；阅读器关闭时返回不可用，不转为全局按键。

桌面小手图标表示手掌可见性，移开后自动隐藏。完整画面仅用于按需校准，可在手动启动前设置`AIR_GESTURE_READING_PREVIEW=1`；已有实例须先停止。关闭大预览不会关闭手势识别或小手反馈。

不保存摄像头图像。当前`AIR_GESTURE_EVENT_LOG`仅记录结构化状态和方向事件。计数初值等校准选项属于程序的诊断能力，当前服务不会加载旧诊断快照。

运行纯单元测试：`python3 -B -m unittest discover -s tests -p 'test_*.py'`。完整场景还需核对真实挥手、PPT页码、当前页文本和语音回答。
