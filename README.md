# OKRVA：K1 RISC-V 多模态智能体系统

本仓库保存当前在进迭时空K1 / Bianbu 2.3.5 LXQt上调通的方案源码。各组件独立放在根目录，运行配置保留当前生效的设置。

| 目录 | 当前用途 |
|---|---|
| `kylin-aiassistant-lite/` | 小K原生文本/语音界面、文档读取、语音追问、流式播报 |
| `kylinbot/` | KylinBot智能体、Gateway、原生工具执行、当前KWS/VAD辅助程序 |
| `riscv-ai-adapter/` | 模型接口、语义规划约束、上下文、资源调度、PPT阅读器及Matcha TTS桥接 |
| `ukui-air-gesture/` | 当前摄像头手势识别、中心复位、方向翻页与桌面反馈 |
| `system-tools/` | 当前sys/docs MCP服务：音量、应用启动、文档摘要和执行后校验 |
| `asr-service/` | 当前SenseVoice ASR HTTP包装 |
| `assets/wake/` | 当前KylinBot直接播放的固定唤醒提示音 |
| `patches/` | 当前使用的SpacemiT GGML和TTS修改补丁、修改后完整源码及测试 |
| `deploy/` | 这台K1当前生效的一套服务配置和脱敏应用配置 |
| `dependencies/` | 当前方案的外部依赖、模型来源/校验值、组件源码基线 |
| `licenses/` | 原项目许可证、版权和第三方来源说明 |

当前模型方案：普通对话/工具规划使用Qwen3-0.6B-Q4_K_M；PPT当前页总结使用Qwen3-1.7B-Q4_0，生成3线程/预填充4线程；ASR为SenseVoice + SpacemiT EP；TTS为Matcha中英 + Vocos CPU四线程。两个LLM按需互斥驻留，阅读模型空闲60秒后卸载。当前已集成的Q6 RVV优化在GGML补丁中。

仓库保留当前实现所需的完整组件源码、构建工具、测试、许可证和说明。备份、修改前源码副本、整理快照、历史试验配置、未采用的算子分支、模型权重、安装包、密钥和用户数据不纳入当前提交。

在同款K1 / Bianbu 2.3.5新设备的桌面终端安装：

```bash
git clone https://github.com/ZephyrLi-pro/OKRVA.git
cd OKRVA
./install.sh
```

脚本自动准备依赖和模型、构建组件、生成本机配置并启动检查。详细条件、选项及分阶段命令见[联网构建安装说明](deploy/INSTALL.md)。可先执行 `./install.sh --dry-run` 查看计划。

阅读说明：

- [软件栈和调用关系](docs/SOFTWARE_STACK.md)
- [K1构建与部署](docs/REPRODUCE.md)
- [当前方案范围](docs/SUBMISSION_SCOPE.md)
- [验证方式与现有限制](docs/VALIDATION_AND_LIMITS.md)
- [后端修改说明](patches/README.md)

源码校验：`python3 tools/verify_bundle.py`。安装入口按固定版本准备模型与外部运行库；来源和校验记录见[依赖说明](dependencies/README.md)与[安装锁定清单](deploy/installer/lock.json)。
