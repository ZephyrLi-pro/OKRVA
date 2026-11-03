# 本地中英混读回答播报

目标通路：`voice-assistant → Adapter :18080/v1/audio/speech → TTS worker :18083 → sherpa-onnx VITS → WAV → 原有 SDL 播放器`。

部署在 openkylin@192.168.5.167；不恢复麒麟 AI 子系统的 TTS，也不更换 LLM、ASR 或唤醒模型。

## 模型与后端

- 复用已安装的 `sherpa-v1.13.2/bin/sherpa-onnx-offline-tts`，不安装另一套 ORT。
- 使用 CPU Provider，不宣称 TTS 已使用 SpacemiT EP。SenseVoice 的 SpacemiT Provider 与它独立。
- 中英模型：`vits-melo-tts-zh_en`，一个女声音色 `zh-0`，单声道 44.1 kHz PCM16。
- 来源：[sherpa 官方模型说明](https://k2-fsa.github.io/sherpa/onnx/tts/pretrained_models/vits.html#vits-melo-tts-zh-en-chinese-english-1-speaker)，由 MyShell.ai MeloTTS 转换。
- 归档来源：`https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-melo-tts-zh_en.tar.bz2`。
- 归档 SHA256：`e58351ed7149f290a54534538badd4077cdbe6fddc964b24d0bee870415d1514`（本地计算值，不宣称有上游签名）。约 159 MiB；其中 FP32 模型约 163 MiB。
- 归档的 `model.int8.onnx` **只是 133 字节的 Git LFS 指针**，不是可用 ONNX 模型。真实 INT8 权重在作者的 [模型仓库](https://huggingface.co/csukuangfj/vits-melo-tts-zh_en/tree/main)，指针给出的预期大小为 53517430 字节、SHA256 为 `f085f5079e05f039b800aeb542f5253c26a303211b0c6465d0d9387977855a63`。
- 已下载真实 INT8 权重，大小和 SHA256 与上述指针完全一致；正式服务选择 `--model-file model.int8.onnx`。FP32 权重保留，不覆盖。中英隔离测试在模型常驻后，FP32 耗时 84.23 秒，INT8 耗时 59.44 秒；这仍不是低延迟方案。
- 归档附带 MyShell.ai 的 MIT LICENSE 和 README，部署保留原文件。sherpa-onnx 框架及 vendored C header 的 Apache-2.0 许可单独保留。
- `tts_melo_lexicon.txt` 增补 `kylin` / `bot` / `kylinbot` 的英文音素和音调。只影响发音，不把回答内容改成中文别名；词条校验后与原词典合成临时文件，不改下载的原词典。
- 常见英文按词典发音。当前 1.13.2 前端对未收录英文词可回退到逐字母读法，不承诺陌生专有名词都能自然读准。并非自动 G2P 的任意语言 TTS。
- 原 `vits-icefall-zh-aishell3` 中文模型和 8 kHz 输出配置保留为回退方案。

## 文件与配置

`tts_server.py` 安装为目标机 `~/riscv-ai-adapter/scripts/tts_server.py`，由 `kylinbot-tts.service` 启动并开机自启。只监听 127.0.0.1:18083；启动时实际合成一条句子后健康接口才报告 ready。

`tts_native_bridge.c` 是约百行的 C API 包装层，只链接现有 sherpa/ORT 库；不是重新编译推理框架。`tts_native.py` 在隔离子进程中持有常驻 TTS 模型，反复合成不重新加载。原 CLI 模式保留，没有 `--native-bridge` 时使用。

编译包装层：`sh scripts/build_tts_native.sh /home/openkylin/.local/share/kylinbot-asr/sherpa-v1.13.2`。构建使用 `scripts/third_party/sherpa-onnx-1.13.2/c-api.h`，来源为上游 v1.13.2 tag，SHA256 `437b1279047877167d8fadc74a60d47f3df514d703fdac1c1b6851da9bc2fdb4`；构建与运行均有版本检查，不猜测 ctypes 的 C 结构布局。包装层的 RPATH 绑定构建时的运行库路径，搬家后需重编这个小包装层。

Adapter 的 `k1-sherpa-tts` 从 disabled 改为 enabled，仍为非必需后端。`tts-dependency.conf` 为 Adapter 添加 TTS 服务启动依赖；未修改 chat/transcription 配置。

语音窗口读取 `~/.config/kylin-aiassistant/tts.ini`，选择 Adapter 的 loopback HTTP 接口。中英模型使用 `voice=zh-0`；`model=vits-zh` 保留为 Adapter 的兼容路由别名，实际模型在健康接口显示。没有此配置时保留原 SDK 路径；`KYLINBOT_TTS_URL` 可用于独立测试。接入后不再为回答创建麒麟 SDK TTS session。

播放端保留原有分句队列及完整回答尾部对齐。HTTP 合成异步运行，不阻塞 GUI；支持 WAV 格式校验、32 MB 音频上限、130 秒请求超时和取消后的过期回调丢弃。HTTP 重定向被禁用，URL 只接受 loopback HTTP。

worker 限制 2000 字/16 KB 请求、32 MiB 音频，一次一个合成任务。常驻子进程初始化上限 180 秒，单次合成上限 120 秒；超时或子进程崩溃时服务退出并由 systemd 重启，不保留假 ready 状态。常驻模式的 WAV 只在内存和内部 IPC 传递，不记录文本，不持久化回答音频。CLI 回退模式通过参数列表调用，无 shell，并加空格前缀防止输入作为选项。

取消播放会中止客户端请求并清空播放队列，不等于立即中断 Adapter 上游已经开始的推理；后台合成可能运行到完成或 120 秒超时。长文本超过推理时间上限仍会失败；2000 字是输入保护上限，不是实时播报保证。当前无回声消除，也不承诺播报期间全双工打断。

## 范围

本次接入的是 `voice-assistant` 语音窗口的回答播报。主聊天窗口中仍直接调用麒麟 SDK 的其他朗读按钮不在本次接线范围内，不能据此宣称所有朗读入口都已迁移。

首次接线备份在目标机 `~/tts-deploy-20261010/`；中英适配备份和测试在 `~/tts-bilingual-20261010/`。切换 8 kHz 到 44.1 kHz 模型时重启语音窗口，避免旧 SDL 播放器沿用旧采样率。没有提交或推送上游。
