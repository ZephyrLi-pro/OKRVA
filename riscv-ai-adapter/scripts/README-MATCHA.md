# 当前Matcha中英播报

当前使用Matcha中英模型和16kHz Vocos，ORT CPU四线程，常驻原生子进程；不依赖llama.cpp，也不申请ASR/LLM的TCM。

`tts_server.py`提供`/v1/audio/speech`，保持上层`model=vits-zh`和`voice=zh-0`兼容别名；`tts_native.py`管理隔离子进程、超时和取消；`tts_matcha_bridge.cpp`提供C ABI；`build_matcha_native.sh`构建桥接库。

vendor固定版本和补丁见根目录`patches/spacemit-tts`及[复现说明](../../docs/REPRODUCE.md)。构建使用`TTS_USE_FST=ON`、`USE_SPACEMIT_EP=OFF`，需要ORT、espeak-ng、FST及C++17。缓存由`RVAI_TTS_CACHE_ROOT`指定：

```text
models/tts/matcha-tts/
  matcha-icefall-zh-en/model-steps-3.q.onnx
  matcha-icefall-zh-en/vocab_tts.txt
  vocos-16khz-univ.q.onnx
models/tts/text_norm/v1/zh/{date,phone,number}.fst
thirdparty/cpp-pinyin/res/dict/...
```

当前服务配置在`deploy/systemd/kylinbot-tts.service`。模型与前端资源来自外部依赖，不能缺少FST后仅凭“能出声”判定成功。

小K按自然分句滚动合成并播放，不等待整段LLM生成；单次HTTP合成返回完整WAV。顿号在Matcha入口转成支持的逗号停顿，文本显示保持原样。取消在分句边界生效；宽限期或硬超时后由子进程管理释放资源。

通用Python模块保留API兼容代码，但本目录不再提供旧Melo构建桥接、词典和部署示例。测试入口：在Adapter目录运行`PYTHONPATH=src python3 -B -m unittest discover -s tests`。
