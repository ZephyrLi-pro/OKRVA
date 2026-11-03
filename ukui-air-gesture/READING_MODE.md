# 阅读挥手模式

本机实测通过的可选模式，入口仍是 `ukui-air-gesture`。默认继续使用原姿态分类器。

```sh
AIR_GESTURE_PROFILE=reading \
AIR_GESTURE_CAMERA=/dev/v4l/by-id/实际摄像头-video-index0 \
ukui-air-gesture
```

运行前停止已有实例；同一会话只允许一个原生 D-Bus 服务。首次校准可增加
`AIR_GESTURE_READING_PREVIEW=1`，显示完整画面、掌心位置、中间复位框和实时计数。
关闭校准窗不会停止服务。通过服务管理器停止进程才会释放摄像头。

张开手掌，掌心进入画面中间40%至60%的框内，然后横向向一侧越过36%或64%的标记线。
一次向外挥动只发一次动作；回中间只复位。手部丢失后需要再次回中间，不会拼接失踪前后的轨迹。
识别延用已校准的手指可见性、水平轨迹和动作间隔检查；不要求每次停稳。

相机持续采集，模型每次消费最新的一帧，不累积旧帧队列。超过0.65秒的结果不执行动作。
阅读模式只启用左右动作，不同时调用原算法的滚轮、截图或准备图标。
默认 palm/pose 模型各4线程，可用 `AIR_GESTURE_PALM_THREADS`、`AIR_GESTURE_POSE_THREADS`
设为1、2或4。4/4来自本机独立测试；与其他AI服务并行时的资源分配仍需另测。

动作仍经 `org.ukui.AirGesture.horizontal` 到原生 libevdev/uinput 执行器。
新逻辑使用镜像画面中的方向；原生接口保持原来的翻页约定：左挥发 Right，右挥发 Left。
这适合“向左划到下一页”的阅读操作。
目前按键发送给前台窗口，阅读器失去焦点时不能保证命中阅读器；阅读器定向路由另行接入。
D-Bus 收到确认不等于阅读器已翻页，必须在阅读器内验证页码。
发送失败会记录 `action_error`，不自动重试，避免重复翻页。

不保存摄像头图像。默认只输出结构化状态日志。可选 `AIR_GESTURE_EVENT_LOG` 指定数值日志文件，
`AIR_GESTURE_COUNTER_SEED` 指定诊断用计数初值JSON；这两个选项不是运行必需项。

验证：

```sh
python3 -B -m unittest discover -s tests -p 'test_*.py'
```

新算法通过后，还需要验证阅读器定向翻页、当前页文本发布及语音问答的场景五闭环。


## Desktop hand feedback

The reading profile uses the existing native hand icon as a presence indicator. It appears on fresh hand detection and hides after a short missing-hand interval; stale inference also clears it. Indicator updates run independently of the optional camera preview. Closing the preview leaves recognition and the small indicator active. Shutdown hides the icon. The existing native window flags keep it from accepting focus.
