# 外部依赖和模型

`system-packages.tsv`是本机相关已安装软件的版本快照，不是可直接跨发行版执行的apt安装列表。`platform.json`保存Bianbu和CPU信息。Python/Qt/Rust依赖还需分别遵循各项目清单。

## 框架来源

| 组件 | 来源/固定版本 | 交付方式 |
|---|---|---|
| SpacemiT llama.cpp | https://github.com/spacemit-com/llama.cpp ，commit `17ce6aa07bd38057040ca43da8660ee496c3cb6d` | 外部源码 + 本包补丁 |
| 系统llama工具 | `llama.cpp-tools-spacemit 0.1.1-bpo1+6` | 原系统包；不包含在本包 |
| ORT/EP | `spacemit-onnxruntime 2.0.3-bpo1`；ORT1.24.2+spacemit.a1，EP2.x | 外部运行库；加载库本身不等于该模型用了EP |
| ASR vendor | https://github.com/spacemit-com/model-zoo-asr ，commit `dd0bed4d4d9f7425f11adccc2efbca3e2d330aae` | 原始源码无跟踪文件修改；本包提供HTTP包装 |
| TTS vendor | https://github.com/spacemit-com/model-zoo-tts ，commit `a13613dfa41d5dd971d1f897eb4b9e668540f2cb` | 外部源码 + 本包4文件补丁 |
| KWS/VAD | https://github.com/k2-fsa/sherpa-onnx ，C API 1.13.2 | 外部库/模型；本包包含改造辅助程序源码 |
| 手势模型 | https://github.com/opencv/opencv_zoo | 外部ONNX；保留模型SHA和对应源代码归属 |

## 当前模型

精确大小、目标路径和SHA256见`model-manifest.json`；没有打包任何权重。

- **普通对话/规划**：`Qwen3-0.6B-Q4_K_M.gguf`，来源 `https://archive.spacemit.com/spacemit-ai/model_zoo/llm/Qwen3-0.6B-Q4_K_M.gguf`。
- **阅读总结**：`Qwen3-1.7B-Q4_0.gguf`。从 `https://archive.spacemit.com/spacemit-ai/model_zoo/llm/Qwen3-1.7B-Q8_0.gguf` 本地执行 `llama-quantize --allow-requantize INPUT OUTPUT Q4_0 2` 生成。不是训练新模型，也不是所有tensor强制纯Q4；保留工具默认混合类型。
- **ASR**：`https://archive.spacemit.com/spacemit-ai/model_zoo/asr/sensevoice.tar.gz`，正式使用`model_quant_optimized.onnx`、`tokens.txt`、`am.mvn`。VAD另需Silero模型。旧Paraformer目录不是当前正式识别模型。
- **TTS**：`https://archive.spacemit.com/spacemit-ai/model_zoo/tts/matcha-tts/matcha-icefall-zh-en.tar.gz`、`https://archive.spacemit.com/spacemit-ai/model_zoo/tts/vocoder/vocos-16khz-univ.q.onnx`。实际使用原分发的量化模型`model-steps-3.q.onnx`和词表；不是本项目训练的音色，也不把未采用的Melo/XSlim试验列为当前加速。原模型作者为`dengcunqin/matcha_tts_zh_en_20251010`。
- **KWS**：sherpa-onnx的`kws-models` release中`zipformer-wenetspeech-3.3M-2024-01-01`，正式encoder/decoder/joiner用int8文件。关键词文本见`assets/wake`。
- **手势**：OpenCV Zoo的`palm_detection_mediapipe_2023feb.onnx`及`handpose_estimation_mediapipe_2023feb.onnx`。按手势项目说明放置到其models/安装位置；与项目内`MODELS.sha256`核对。

TTS还需要`models/tts/text_norm/v1/zh/{date,phone,number}.fst`和cpp-pinyin词典、espeak-ng资源。完整缓存布局见`riscv-ai-adapter/scripts/README-MATCHA.md`。这些是外部前端数据依赖，不应遗漏后仅凭“能出声音”判定复现成功。

发布资源可能发生变更；下载后必须核对本包记录的文件SHA。若不一致，先查版本，不要绕过校验。模型及第三方软件的许可应按各原始发行来源保留。
