# Matcha 中英混读播报

本轮以用户已试听接受的 Matcha 中英混读音色替代正式 MeloTTS。
模型使用 CPU 推理，不依赖 llama.cpp，也不额外分配 ASR/LLM 使用的 TCM。
2026-10-16 已在 K1 正式切换，57 项板端单元测试及正式 HTTP 接口回归通过。
桌面麦克风唤醒与实际听感仍需用户现场验收。

## 文件

- `tts_server.py`：保留 Melo/VITS 兼容性，增加 `matcha-zh-en` profile。
- `tts_native.py`：隔离进程常驻模型；Matcha 支持分句边界取消，超时杀死子进程。
- `tts_matcha_bridge.cpp`：项目自有 C ABI，调用 SpacemiT TTS 组件；不跨 ctypes 暴露 C++ 结构。
- `build_matcha_native.sh`：构建共享桥接库；需要 ORT、espeak-ng 开发文件及 CMake/C++17。
- `third_party/spacemit-tts`：Apache-2.0 官方组件及局部补丁，保留 LICENSE/NOTICE。

上层仍使用原来的 `/v1/audio/speech`、`model=vits-zh`、`voice=zh-0`、WAV 返回格式。
小K和 KylinBot 源码不改。Adapter 仅对本地 HTTP 语音后端增加等待响应期间的断连传播。
这部分修改暂存在 `work/tts-matcha-deploy-20261015/{backends,server}.py`，
基于板端当前文件制作并已部署，不能用本地历史镜像覆盖板端整个项目。

## 官方组件来源与局部补丁

来源：https://github.com/spacemit-com/model-zoo-tts 。保存的原始 ZIP SHA256：
`65b62beb7c71bd1e301926e0dc97d278f7115f2da99c1522430136c720c4cdd2`。

局部变化：CPU 会话禁忙等（仅开启 `RVAI_TTS_NO_SPIN` 时）、
通过 `RVAI_TTS_CACHE_ROOT` 指定模型/词典/数字日期规则路径、
CMake 增加可选共享桥接库 target；补齐百分数和 ISO 日期的符号预处理。
必须开启 `TTS_USE_FST=ON`，缺少规则文件启动失败，
不能以“能合成”代替数字/日期正确性验收。

模型来源与 SHA256 见 `outputs/tts-backend-evaluation-20261015.md`。
模型作者提示英文效果仍有改进空间；中英混读不意味着所有专名和数字格式都保证读对。

## 稳定运行目录

本机使用 `~/.local/share/riscv-ai-adapter/tts-matcha-20261016` 保存数据，
`~/.local/lib/riscv-ai-adapter/tts-matcha-20261016` 保存桥接库及 espeak-ng 命令。
以下均为实文件，不依赖试验目录的软链接。

`RVAI_TTS_CACHE_ROOT` 应为稳定、完整、只读可用的应用数据目录，结构如下：

```
models/tts/matcha-tts/
  matcha-icefall-zh-en/model-steps-3.q.onnx
  matcha-icefall-zh-en/vocab_tts.txt
  vocos-16khz-univ.q.onnx
models/tts/text_norm/v1/zh/{date,phone,number}.fst
thirdparty/cpp-pinyin/res/dict/...
```

不要让正式服务依赖试验目录的软链接。espeak-ng 命令、桥接库应安装在版本化的应用目录；
不覆盖系统 ORT，不在请求期间下载资源。

## 取消与限制

HTTP 客户端断连通过 Adapter 传给 TTS。当前分句的原生推理不能即时中断；
它结束后丢弃结果，不再合成后续分句。15 秒取消宽限仍未完成时终止子进程，
服务退出后由 systemd 恢复。默认请求硬超时仍为 120 秒，输入最多 2000 字符。
正常取消后保持模型驻留并释放串行占用；不返回部分成功 WAV。

## 验证结果

隔离的板端源码快照在 `work/tts-matcha-deploy-20261015/adapter-base`。
本地选定的 55 项通过；恢复 SSH 后补齐脚本并在板端运行完整 discover，57 项通过。
板端测试发现并修复 Python 3.12 下 HTTPResponse.read(-1) 在 keep-alive 上等 EOF 的问题。
正式入口 `/v1/audio/speech`：短句 1.77–2.04 秒，中英混合句 3.83 秒，
长句 9.48 秒合成 15.22 秒音频。语速 0.75/1.5、取消长文本及再次合成通过，
取消前后子进程 PID 不变；五个正式服务 active，自动重启计数为 0。

日期、百分数、手机号、小数温度的归一化结果通过断言；本机 ASR 对已知合成样本
正确转写“今天是2026年10月16日，系统音量50%。”。不能据此保证任意文本或专名读法。
LLM 通过 Adapter 回答“7乘8等于56。”；ASR unit 和 LLM 配置 hash 未变。

当前仍为整段合成后返回 WAV，不是流式首句播放。分句用于控制长文本和取消边界。

## 回退

原 MeloTTS 模型、桥接库及 unit 保留；当前只通过
`~/.config/systemd/user/kylinbot-tts.service.d/20-matcha.conf` 覆盖 ExecStart。
将此文件移到备份目录，再执行 `systemctl --user daemon-reload` 和
`systemctl --user restart kylinbot-tts.service`，即可用兼容代码恢复原 MeloTTS。
完整切换前备份还在 `/home/openkylin/tts-matcha-deploy-20261015.q16Qkp/before/`。
正式验收通过后已解除本轮自动回退定时器。没有提交或推送代码。

## 2026-10-30 顿号停顿兼容

Matcha 中英混读前端只将中文逗号、句号、问号、感叹号映射到模型词表；顿号 `、` 没有对应条目，会进入未知 token 分支，导致“图像处理、内容生成”等列举项连读。

`TtsServer.synthesize` 仅在 `matcha-zh-en` 的原生合成入口将顿号换成模型支持的 ASCII 逗号停顿。原始回答文字、上下文、HTTP 输入解析和 Qt 分句边界不变；同一分句仍只调用一次原生合成，不按每个顿号拆请求，不插入整段独立 WAV 的头尾静音。其他 TTS profile 不做此替换，取消回调、语速和 speaker 参数保持原值。

已有 TTS/Matcha 28 项回归通过。同一公开测试分句的修正后波形出现约 0.25–0.31 秒的短暂停顿，而原先“图像处理”和“内容生成”处没有达到 80 毫秒的低能量段。此为波形能量观察，不是逐字强制对齐或用户听感验收。

小K流式播放在之前的独立修改中已启用自然分句与滚动缓存；本次只重启 TTS 服务，没有重启窗口、ASR、Adapter 或 KylinBot，也没有切换模型。正式 HTTP 入口按原始顿号文本验证，试听样例由正式返回 WAV 经产品同款边缘静音处理后拼接。证据目录：`/home/openkylin/tts-enumeration-20261030`。未提交或推送代码。