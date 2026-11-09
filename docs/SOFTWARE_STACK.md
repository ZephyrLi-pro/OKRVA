# 当前软件栈

## 调用关系

```text
小K原生文本/语音窗口 ── KylinBot Gateway / Agent
                              │
               ┌──────────────┴────────────────┐
               │                               │
        RISC-V AI Adapter                原生MCP工具执行
     模型接口/上下文/调度/资源约束         sysmcp / document_mcp / presentation_mcp
               │                         Bianbu LXQt、文件系统
      ┌─────────────────┐
      │                 │
   llama.cpp       Matcha + Vocos
   Qwen3           ORT CPU / FST

摄像头 → 独立ukui-air-gesture → UNO/D-Bus桥 → 原有LibreOffice Impress翻页
                       └→ Wayland窗口最小化 → 返回桌面
KylinBot语音采集 → SenseVoice ASR（ORT + EP）→ 同一任务会话
PPT MCP → 当前页快照 → Adapter生成演讲备注 → 原PPTX保存与回读
```

Adapter承担模型服务适配和受限规划/上下文约束；KylinBot仍执行原生工具调用，系统命令由MCP服务执行并回读。模型正文中的“已完成”或JSON文本不会自动变成系统操作。PPT工具目录可用时，打开与备注保存能力始终进入规划schema，模型按本轮语义选择动作和参数，再由工作流校验授权、文件范围、快照及真实回执；`semantic_only`普通聊天回退仅保留PPT读取能力。

## 当前组件

| 层 | 采用的组件 | 本项目的改动/组合 |
|---|---|---|
| 前端 | 麒麟AI助手Lite，Qt5/QML/C++ | RISC-V拆开WebEngine加载；文档原生文本替代metadata；语音历史保留、单轮唤醒、连续分句播报、焦点修复 |
| 智能体 | openKylin KylinBot fork | 适配Adapter、语音唤醒/捕获与本机MCP；上游Agent、Provider、工具框架完整保留 |
| 适配服务 | `riscv-ai-adapter`，Python3.11+ | OpenAI兼容接口、模型语义规划约束、真实工具结果校验、流式输出、上下文、调度/取消/模型轮换 |
| 普通对话/规划及文档要点 | Qwen3-0.6B-Q4_K_M | 原外部GGUF；4线程；模型结合schema和任务边界生成参数或选择原文要点，再由原生工具执行 |
| PPT演讲者备注 | Qwen3-1.7B-Q4_0 | 根据当前页标题和原生正文生成备注；从官方Q8_0重新量化；生成3线程/预填充4线程；按需加载、空闲60秒释放，与0.6B互斥驻留 |
| LLM运行时 | SpacemiT fork llama.cpp / GGML | Q4_K精度修复、IME多行预填充/CPU单行生成混合、TCM修复及Q6 RVV集成补丁 |
| ASR语音转文字 | SenseVoice，SpacemiT model-zoo-asr + ORT/SpacemiT EP | 自有HTTP包装，正式2线程；模型及vendor ASR源作为外部依赖 |
| 唤醒/句尾 | sherpa-onnx Zipformer KWS / Silero VAD | 门控KWS辅助程序和忙闲资源策略；不是通过LLM持续判断唤醒词 |
| TTS文字转语音 | Matcha中英 + 16kHz Vocos，vendor TTS，ORT CPU | 自有C ABI/HTTP桥接、FST归一化、取消、语速、顿号兼容、小K滚动播放缓存；当前4线程，不用llama.cpp和TCM |
| 文档 | Qt DocumentReader、pdftotext、LibreOffice | TXT/MD/PDF/Office原生文本；独立CLI供文档工具使用；不内嵌OCR |
| 手势 | ukui-air-gesture Bianbu fork，OpenCV Zoo手掌/关键点模型 + ORT | 中心复位状态机、最新帧、异步阅读动作、张掌后握拳返回桌面、小手反馈；独立于PPT进程 |
| PPT控制 | LibreOffice24.8.7原生Impress、Python UNO/D-Bus桥 | 独立24.8启动器与已有24.8用户配置、普通可编辑窗口和底部备注栏；定向翻页、实际页码与正文、生成并保存演讲备注；右挥下一页/左挥上一页，复位不翻页 |
| 执行 | 本机`kylin-sys-tools`重实现 + 文档/PPT MCP | 受限系统操作、最近文档/分别保存摘要、按名称打开PPT、锁定当前页生成并保存讲稿，均要求真实结果回读 |

默认本地接口：Adapter 18080、ASR 18081、主LLM 18082、TTS 18083、阅读LLM 18084。精确选项以脱敏模板为准。

当前本地推理链路不依赖完整KyTensor/麒麟AI子系统服务，也不走`core-speech.sock`。这不代表小K已经去掉全部麒麟SDK链接：现有Qt前端仍依赖部分`libkyai-*`、`libkysdk-*`开发/运行包，见真实`debian/control`和版本清单。`libkysdk-advanced-ai-dev`与WebEngine包按amd64/arm64限定，RISC-V不要求这两部分。

## 演示任务与可用接口

1. 最近三份文档：模型选择受限文档任务 → 实际mtime排序/原生提取 → 每源文件单独生成一个Markdown摘要 → 保存/读回校验。当前摘要采用模型选择的原文要点，不等于任意长文的自由生成式深度总结。
2. PPT多模态操作：语音按名称打开原有LibreOffice Impress中的PPT → 手势翻页 → 语音请求根据当前页生成演讲者备注、追加到该页并保存 → 张掌停稳后握拳保持返回桌面。泛指打开仅复用有效的最近真实记录。备注保存限可写PPTX，并保留原有备注；生成期间翻页仍保存到取得快照时的目标页。返回桌面后暂停翻页，再次语音打开PPT建立新会话后恢复。

PPT UNO桥使用24.8启动器保留的用户配置和固定pipe，跟随实际Impress演示文稿；停止桥不关闭用户的LibreOffice。新设备须先准备已验证的LibreOffice 24.8或以上RISC-V运行环境，见[安装前提](../deploy/INSTALL.md#libreoffice-248运行前提)。页面资料通过原生文本取得，纯图片页不能依赖该路径生成备注。

文本和语音仍可调用已开放的受限系统工具，完成音量回读、文件管理器启动等操作；主小K日志草稿可作为语音分析的只读上下文。语音使用单轮唤醒，回答播报结束等待下一次“你好麒麟”。

性能依据见[2026-11-06固定文本概括基准](reproducibility/reading-summary-16gb-20261106/README.md)。本次同步未重测；该基准用于记录1.7B推理吞吐与资源条件，不是演讲备注生成、写入、回读和桌面动作的端到端测量。

浏览器标签页手势切换是未实现的新增场景，不在当前成果中。
