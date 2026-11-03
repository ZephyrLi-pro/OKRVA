# OKRVA：K1 RISC-V 多模态智能体系统

快照日期：2026-11-03。平台：进迭时空 K1 / Bianbu 2.3.5 LXQt / riscv64。

本仓库保存当前设备上的完整项目源码及尚未提交的修改，并附第三方后端修改补丁、算子优化分支源码、依赖版本、模型校验值和复现步骤。各组件直接以独立源码目录收录，不使用子模块或压缩包。原始第三方后端作为固定版本依赖，项目修改另附源码和补丁。

## 提交内容

| 目录 | 内容 |
|---|---|
| `riscv-ai-adapter` | 统一模型接口、模型语义规划约束、流式转发、调度/生命周期、上下文、文档工作流、PPT 阅读桥接、TTS 桥接 |
| `kylin-aiassistant-lite` | 小K完整修改源码：RISC-V Lite、原生界面、DocumentReader、语音追问/历史、流式播报、焦点修复 |
| `kylinbot` | KylinBot完整修改源码及KWS/VAD辅助工具，包含上游原有代码和本项目适配 |
| `ukui-air-gesture` | 手势完整修改源码：OpenCV/ORT路径、阅读挥手状态机、异步翻页、桌面反馈 |
| `system-tools` | 本机独立MCP系统工具和文档工具，含执行后回读与受限文件操作 |
| `asr-service` | SenseVoice HTTP包装及保留的旧ASR包装；正式使用前者 |
| `runtime-integration` | 设备实际使用的KWS资源策略、门控C辅助程序、`spd-say`语音桥接 |
| `patches/spacemit-llama` | 正式GGML后端的9文件修改：Q4_K正确性、混合路径、TCM、Q6 RVV等 |
| `patches/spacemit-tts` | 官方TTS组件的4文件局部补丁及前后源码 |
| `optimizations/rvv` | 用户17项算子优化对应的15个独立分支补丁、完整改动文件、固定版本 |
| `deploy` / `dependencies` | 脱敏配置参考、运行版本、模型来源与SHA256；不含模型权重 |
| `provenance` / `licenses` | 原始仓库版本、工作区差异、许可证及归属信息 |

比赛仓库为 [ZephyrLi-pro/OKRVA](https://github.com/ZephyrLi-pro/OKRVA)。各组件在根目录可直接浏览；本仓库收录了设备快照时尚未提交到组件仓库的新代码。

进迭时空提供的原始推理框架、ORT/EP、模型、系统软件包作为外部依赖，不重复打包；我们对第三方源码的修改必须交付，因此保留了补丁、完整修改文件和固定基线。没有把这些框架、模型或上游已有能力称为本项目原创。

## 阅读顺序

1. [软件栈和调用关系](docs/SOFTWARE_STACK.md)
2. [提交范围与贡献说明](docs/SUBMISSION_SCOPE.md)
3. [依赖和模型来源](dependencies/README.md)
4. [源码复现步骤](docs/REPRODUCE.md)
5. [17项算子优化索引](optimizations/rvv/README.md)
6. [验证结果及限制](docs/VALIDATION_AND_LIMITS.md)
7. [许可证索引](licenses/README.md)

根目录 `SHA256SUMS` 覆盖当前源码快照文件；在仓库根目录运行 `python3 tools/verify_bundle.py` 校验。模型、个人配置、密钥、对话数据库、用户录音、演示PPT、编译产物、第三方安装包、Git历史和缓存不在仓库内。`assets/wake/wake_ack_zh.wav` 是固定“我在，请讲”的公开生成提示音，是唯一有意保留的音频资源。

各项目内部保留了原有README和阶段记录，其中可能描述旧模型或旧配置；本仓库根目录文档及 `deploy/templates` 描述此快照的状态。
