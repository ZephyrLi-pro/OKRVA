# 当前K1运行配置

`systemd/`保存这台K1各服务当前生效的完整配置，历史drop-in已合并。`templates/`保存应用参数，密钥位置使用占位符。

- LLM主模型：0.6B，4线程，IME预填充/CPU生成，TCM开启。
- 阅读模型：1.7B-Q4_0，生成3线程/预填充4线程，CPU0–3，TCM关闭，ctx3072，batch/ubatch128，互斥驻留、空闲60秒卸载。
- SenseVoice：SpacemiT EP，两线程，18081。
- Matcha：CPU四线程，CPU0–3，CPUWeight400，18083。
- KWS：门控辅助程序，INT8模型、单线程、CPU0–3；策略脚本观察当前LLM/播报忙闲。
- 手势：当前内置摄像头、reading识别配置、独立于PPT服务。

这是当前设备使用方案的配置文件，不包含用户token、模型权重或机器备份。安装位置及启动方法见[复现说明](../docs/REPRODUCE.md)。
