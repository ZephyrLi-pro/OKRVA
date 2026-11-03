# 当前软件栈

## 调用关系

```text
小K原生文本/语音窗口 ── KylinBot Gateway / Agent
                              │
               ┌──────────────┴────────────────┐
               │                               │
        RISC-V AI Adapter                原生MCP工具执行
     模型接口/上下文/调度/资源约束         sysmcp / document_mcp
               │                         Bianbu LXQt、文件系统
      ┌────────┼───────────┐
      │        │           │
   llama.cpp  SenseVoice   Matcha + Vocos
   Qwen3      ORT + EP     ORT CPU / FST

摄像头 → 独立ukui-air-gesture → 阅读器D-Bus → LibreOffice UNO定向翻页
                                                │
                                  当前页原生文本 → 小K阅读上下文 → Adapter
```

Adapter承担模型服务适配和受限规划/上下文约束；KylinBot仍执行原生工具调用，系统命令由MCP服务执行并回读。模型正文中的“已完成”或JSON文本不会自动变成系统操作。

## 当前组件

| 层 | 采用的组件 | 本项目的改动/组合 |
|---|---|---|
| 前端 | 麒麟AI助手Lite，Qt5/QML/C++ | RISC-V拆开WebEngine加载；文档原生文本替代metadata；语音历史、9秒追问、连续分句播报、焦点修复 |
| 智能体 | openKylin KylinBot fork | 适配Adapter、语音唤醒/捕获与本机MCP；上游Agent、Provider、工具框架完整保留 |
| 适配服务 | `riscv-ai-adapter`，Python3.11+ | OpenAI兼容接口、模型语义规划约束、真实工具结果校验、流式输出、上下文、调度/取消/模型轮换 |
| 普通对话/规划 | Qwen3-0.6B-Q4_K_M | 原外部GGUF；4线程；模型结合schema和任务边界生成参数，再由原生工具执行 |
| 当前页总结 | Qwen3-1.7B-Q4_0 | 从官方Q8_0本地重新量化；生成3线程/预填充4线程；按需加载、空闲60秒释放，与0.6B互斥驻留 |
| LLM运行时 | SpacemiT fork llama.cpp / GGML | Q4_K精度修复、IME多行预填充/CPU单行生成混合、TCM修复及Q6 RVV集成补丁 |
| ASR语音转文字 | SenseVoice，SpacemiT model-zoo-asr + ORT/SpacemiT EP | 自有HTTP包装，正式2线程；模型及vendor ASR源作为外部依赖 |
| 唤醒/句尾 | sherpa-onnx Zipformer KWS / Silero VAD | 门控KWS辅助程序和忙闲资源策略；不是通过LLM持续判断唤醒词 |
| TTS文字转语音 | Matcha中英 + 16kHz Vocos，vendor TTS，ORT CPU | 自有C ABI/HTTP桥接、FST归一化、取消、语速、顿号兼容、小K滚动播放缓存；当前4线程，不用llama.cpp和TCM |
| 文档 | Qt DocumentReader、pdftotext、LibreOffice | TXT/MD/PDF/Office原生文本；独立CLI供文档工具使用；不内嵌OCR |
| 手势 | ukui-air-gesture Bianbu fork，OpenCV Zoo手掌/关键点模型 + ORT | 中心复位状态机、最新帧、异步阅读动作、小手反馈；独立于PPT进程 |
| 阅读器 | Python UNO/D-Bus、LibreOffice Impress | 绑定指定文档的定向翻页、实际页码与当前页文本、上下文续期；右挥下一页/左挥上一页，复位不翻页 |
| 执行 | 本机`kylin-sys-tools`重实现 + 文档MCP | 受限音量/静音/应用启动、能力查询、最近文档/分别保存摘要/受限移入回收站与执行回读 |

默认本地接口：Adapter 18080、ASR 18081、主LLM 18082、TTS 18083、阅读LLM 18084。精确选项以脱敏模板为准。

当前本地推理链路不依赖完整KyTensor/麒麟AI子系统服务，也不走`core-speech.sock`。这不代表小K已经去掉全部麒麟SDK链接：现有Qt前端仍依赖部分`libkyai-*`、`libkysdk-*`开发/运行包，见真实`debian/control`和版本清单。`libkysdk-advanced-ai-dev`与WebEngine包按amd64/arm64限定，RISC-V不要求这两部分。

## 五个比赛场景与边界

1. 文本复合控制：LLM语义计划 → KylinBot原生MCP音量操作/回读 → 文件管理器启动/窗口确认。
2. 语音复合控制：KWS/VAD → SenseVoice → 同一工具链 → Matcha播报。不是另一套硬编码系统命令入口。
3. 文本加语音：主小K未发送的日志草稿 → 受限上下文快照；语音提问与该快照一起分析。不会将粘贴日志当命令执行。
4. 最近三份文档：模型选择受限文档任务 → 实际mtime排序/原生提取 → 每源文件单独生成一个Markdown摘要 → 保存/读回校验。当前摘要采用模型选择的原文要点，不等于任意长文的自由生成式深度总结。
5. 手势阅读：独立手势服务 → 专用阅读器翻页 → 真实当前页文本 → 1.7B总结及播报。当前是原生文本理解，未部署本地VLM；扫描页/只有图片的页面不假称已做OCR。任意手动打开的LibreOffice窗口不会自动被阅读器接管。

浏览器标签页手势切换是未实现的新增场景，不在当前成果中。
