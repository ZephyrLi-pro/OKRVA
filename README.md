# 面向IRSC-V的多模态智能终端系统

本仓库保存当前在进迭时空K1 / Bianbu 2.3.5 LXQt上调通的方案源码。各组件独立放在根目录，运行配置保留当前生效的设置。

| 目录 | 当前用途 |
|---|---|
| `kylin-aiassistant-lite/` | 小K原生文本/语音界面、文档读取、单轮语音唤醒、流式播报 |
| `kylinbot/` | KylinBot智能体、Gateway、原生工具执行、当前KWS/VAD辅助程序 |
| `riscv-ai-adapter/` | 模型接口、语义规划约束、上下文、资源调度、原生Impress后台控制桥及Matcha TTS桥接 |
| `ukui-air-gesture/` | 当前摄像头手势识别、中心复位、方向翻页、握拳返回桌面与状态反馈 |
| `system-tools/` | 当前sys/docs/ppt MCP服务：音量、应用启动、文档摘要、PPT打开与演讲备注保存及执行后校验 |
| `asr-service/` | 当前SenseVoice ASR HTTP包装 |
| `assets/wake/` | 当前KylinBot直接播放的固定唤醒提示音 |
| `patches/` | 当前使用的SpacemiT GGML和TTS修改补丁、修改后完整源码及测试 |
| `deploy/` | 这台K1当前生效的一套服务配置和脱敏应用配置 |
| `dependencies/` | 当前方案的外部依赖、模型来源/校验值、组件源码基线 |
| `licenses/` | 原项目许可证、版权和第三方来源说明 |

当前模型方案：普通对话、任务意图规划与文档要点选择使用Qwen3-0.6B-Q4_K_M；PPT演讲者备注由Qwen3-1.7B-Q4_0根据当前页标题和原生正文生成，生成3线程/预填充4线程；ASR为SenseVoice + SpacemiT EP；TTS为Matcha中英 + Vocos CPU四线程。两个LLM互斥驻留，按需阅读模型空闲60秒后卸载。当前采用的Q6 RVV优化在GGML补丁中。

当前演示任务为三份文档独立摘要和PPT多模态操作。PPT先通过语音按名称打开，再以右挥下一页、左挥上一页定位页面；语音请求“根据当前页生成一段演讲备注并保存”后，系统向原PPTX追加演讲者备注并回读校验。确认保存后，先张掌停稳，再握拳保持至桌面出现。返回桌面后暂停翻页，再次语音打开PPT建立新会话后恢复。泛指“打开PPT”只复用有效的最近真实打开记录。

语音默认单轮唤醒：每次说“你好麒麟”后提问，回答播报结束保留本轮内容并回到等待唤醒。既有性能数据见[2026-11-06固定文本概括基准](docs/reproducibility/reading-summary-16gb-20261106/README.md)，该基准不包含PPT备注写入或桌面动作；本次源码同步未重新测速。

仓库保留当前实现所需的完整组件源码、构建工具、测试、许可证和说明。备份、修改前源码副本、整理快照、历史试验配置、未采用的算子分支、模型权重、安装包、密钥和用户数据不纳入当前提交。

在同款K1 / Bianbu 2.3.5新设备上，须先准备可执行的**LibreOffice 24.8或以上RISC-V运行环境及启动器**，再在桌面终端安装。安装器检查该前置，不会自动下载或替换这套LibreOffice；准备、检查和自定义启动器方法见[LibreOffice运行前提](deploy/INSTALL.md#libreoffice-248运行前提)。

```bash
git clone https://github.com/ZephyrLi-pro/OKRVA.git
cd OKRVA
./install.sh
```

上述LibreOffice前置满足后，脚本准备其余依赖和模型、构建组件、生成本机配置并启动检查。详细条件、选项及分阶段命令见[联网构建安装说明](deploy/INSTALL.md)。可先执行 `./install.sh --dry-run` 查看计划；它不等于运行环境检查通过。

阅读说明：

- [软件栈和调用关系](docs/SOFTWARE_STACK.md)
- [K1构建与部署](docs/REPRODUCE.md)
- [当前方案范围](docs/SUBMISSION_SCOPE.md)
- [验证方式与现有限制](docs/VALIDATION_AND_LIMITS.md)
- [后端修改说明](patches/README.md)

源码校验：`python3 tools/verify_bundle.py`。安装入口按固定版本准备模型与外部运行库；来源和校验记录见[依赖说明](dependencies/README.md)与[安装锁定清单](deploy/installer/lock.json)。
