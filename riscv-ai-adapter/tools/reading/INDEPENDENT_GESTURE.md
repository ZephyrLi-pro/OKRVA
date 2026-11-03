# 手势服务与 PPT 阅读器独立启停

手势服务负责摄像头采集、手部识别和方向动作；PPT 阅读器负责演示文档、定向翻页和当前页文本。两者通过已有 D-Bus 接口配合，服务生命周期分别管理。

## 控制方式

```sh
# 单独控制手势识别
systemctl --user start ukui-air-gesture.service
systemctl --user stop ukui-air-gesture.service
systemctl --user status ukui-air-gesture.service

# 单独控制这份演示 PPT
systemctl --user start riscv-ai-reader.service
systemctl --user stop riscv-ai-reader.service
```

旧名 `ukui-air-gesture-reading.service` 作为同一手势服务的别名保留，不会另起一个摄像头进程。当前手势服务手动启动；本次没有新增登录自动启动。

## 关系变化

- 手势服务移除对阅读器的 BindsTo、PartOf 和 After 关系。
- 阅读器移除对手势服务的 Wants 关系。
- 关闭或退出 PPT，手势识别继续运行；打开 PPT 不负责启动手势服务。
- 阅读器存在时，方向操作调用 `org.riscv.AIReader1.Navigate`，按当前文档实际页码确认结果。
- 阅读器不存在时，识别继续；翻页接口返回不可用，不会因此启动 PPT，也不会转为对其他窗口发送全局按键。

摄像头选择、模型、线程数和已校准的挥手判定保持原样。`AIR_GESTURE_PROFILE=reading` 是当前左右挥手的识别配置名称，不再表示进程从属于 PPT。

## 回归范围

验证包括实际停止、重开阅读器时手势 PID 和进程启动标识不变、识别帧持续推进，以及重开后的定向翻页和页面上下文确认。这里不把程序调用翻页接口称为真人挥手测试；实际挥手可在服务启动后继续测试。
