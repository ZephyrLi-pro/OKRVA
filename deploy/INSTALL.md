# 在同款K1设备构建安装

新设备须先完成下文的[LibreOffice 24.8运行前提](#libreoffice-248运行前提)。随后在Bianbu桌面终端，以普通桌面用户执行：

```bash
git clone https://github.com/ZephyrLi-pro/OKRVA.git
cd OKRVA
./install.sh
```

脚本会在必要步骤调用`sudo`，按提示输入这台新机器的管理员密码。LibreOffice独立运行环境须提前准备；其余模型、Rust工具链及依赖按安装清单下载，并在K1上编译。耗时取决于网络和设备，长步骤会显示进度及日志位置。中断后可以重新执行同一命令，继续使用已校验的下载缓存和构建产物。

适用基线为**进迭时空K1、8核、Bianbu 2.3.5、Wayland桌面**，建议16GB内存，至少12GiB可用空间，建议20GiB以上。USB摄像头、麦克风和播放设备在安装前连接。新设备需要能够访问GitHub、Gitee、进迭、openKylin、Rust和PyPI的公开下载站点。

## LibreOffice 24.8运行前提

PPT正式入口为`~/.local/bin/libreoffice-24.8`。当前K1使用已验证的24.8.7独立运行库，目录保留为`~/.local/opt/libreoffice-24.8-trial`，用户配置保留为`~/.local/share/libreoffice-24.8-trial/profile`；这些既有路径继续用于正式运行，不在迁移时搬动。后台桥与启动器使用同一UNO pipe：`rvai_trial_248_1000`。

新设备须先准备并验证这个24.8启动器，随后再执行完整安装。安装器检查启动器可执行且`--version`返回24.8或以上；缺少启动器、仍为24.2或库加载失败会明确停止，不会自动下载这套独立LibreOffice或改动系统Office。`deps`阶段的发行版LibreOffice依赖仍用于常规Office文本提取，不代表PPT运行入口已升级。

本仓库目前没有锁定这套独立LibreOffice运行包的下载和安装配方。缺少前置时，先取得与目标K1/Bianbu环境兼容、已验证的riscv64版24.8或以上运行环境，并按该运行环境的说明安装。保留其所需运行库和用户配置，提供能正确设置运行环境并透传参数的启动器；默认位置为`~/.local/bin/libreoffice-24.8`。仅有系统24.2时，不能跳过这一步直接宣称PPT部署完成。

在已经克隆的仓库根目录检查默认入口，再执行安装：

```bash
test -x "$HOME/.local/bin/libreoffice-24.8"
"$HOME/.local/bin/libreoffice-24.8" --version
./install.sh --phase check
./install.sh
```

使用其他安装布局时，将下面的启动器值替换为实际绝对路径。在同一个桌面终端设置变量、检查版本和窗口，再运行安装器；变量值将写入生成的桥服务：

```bash
export RVAI_LIBREOFFICE_LAUNCHER="/absolute/path/to/verified-libreoffice-launcher"
export RVAI_IMPRESS_PIPE="okrva_impress_$(id -u)"
test -x "$RVAI_LIBREOFFICE_LAUNCHER"
"$RVAI_LIBREOFFICE_LAUNCHER" --version
"$RVAI_LIBREOFFICE_LAUNCHER" --impress \
  "--accept=pipe,name=$RVAI_IMPRESS_PIPE;urp;StarOffice.ServiceManager" &
./install.sh --phase check
./install.sh
```

已有Impress实例时，pipe必须与该实例的UNO接收设置一致。当前已部署K1继续使用`rvai_trial_248_1000`以及现有`-trial`运行库和profile路径，不因名称而搬动。启动器负责运行库与用户配置路径，桥不另行更换profile。服务启动等待时间为300秒，覆盖大PPT冷开超过120秒的情况。版本检查不替代实际窗口验收：用可写PPTX核对底部备注栏、当前页以及保存后重新打开的备注内容。

## 安装流程

脚本安装Bianbu依赖与固定版本SpacemiT运行库，从固定版本源码构建小K、KylinBot、RISC-V AI Adapter所需的后端、ASR、TTS、KWS/VAD、手势程序及文档读取器。小K使用独立的Qt GLES开发目录，保持系统的GLES Qt运行库。

模型与第三方数据自动下载并校验。主模型为Qwen3-0.6B-Q4_K_M；阅读模型由固定来源的Qwen3-1.7B-Q8_0重新量化为当前Q4_0文件，并核对最终SHA256。SenseVoice使用SpacemiT EP，Matcha采用CPU中英混读。唤醒模型、VAD、手势模型、TTS词典与FST资源均随安装准备。

每台设备独立生成ASR与Gateway凭据，配置文件权限为600。用户名、用户ID、文档/桌面路径、摄像头和音频设备按本机环境处理。默认单轮语音唤醒：播报结束保留回答历史，等待下一次“你好麒麟”。

程序通过用户级systemd服务启动。手势服务独立运行；PPT默认使用上述LibreOffice24.8原生Impress，后台UNO桥提供语音与手势控制，无文件懒启动，只恢复最近真实打开历史的元数据。安装后的模型、库、程序接口、配对及核心服务由`doctor`检查，并进行一次无录音、无播放的LLM/TTS/ASR推理自检。

## 常用选项

先查看只读计划：

```bash
./install.sh --dry-run
```

将大体积构建缓存放到其他磁盘：

```bash
./install.sh --work-dir /mnt/ssd/okrva-build --jobs 2
```

多个摄像头时指定其中一个：

```bash
./install.sh --camera /dev/video0
```

通常会自动优先选择唯一的USB输入和USB输出。多个USB音频设备时，可在系统设置中选择默认设备，或先用`pactl list short sources`和`pactl list short sinks`查询，再传入`--audio-input`和`--audio-output`的完整设备名。

仅构建安装、稍后再启动：

```bash
./install.sh --no-start
./install.sh --phase start
./install.sh --phase doctor --smoke-test
```

需要排查单个阶段时，可用`--phase check|deps|fetch|build|install|start|doctor`。`fetch`和`build`支持`--component llama|asr|tts|qt|kws|gesture|kylinbot`；完整安装默认按依赖顺序执行全部组件。默认构建并行数为2，范围1至8。

## 文件位置和重复执行

默认下载、构建及日志目录为`~/.cache/okrva-installer`；可通过`--work-dir`调整。当前安装源码以独立组件目录保存于`~/.local/share/okrva/releases/`，运行配置位于用户配置目录，凭据与安装回执位于`~/.local/state/okrva-installer/`。模型按仓库清单安装到用户模型目录。

安装器保留已有用户文档和聊天数据目录。首次安装若检测到其他部署留下的配置，会在修改系统前停止；请先运行`--phase doctor`检查。再次运行时会保留安装后手动调整过的已管理配置。服务按既定策略，在文本或语音进程实际退出后清理对应会话；最小化或隐藏窗口继续保留会话。

## 使用与验收

从应用菜单打开“小K”，文本使用智能体对话。语音说“你好麒麟”，听到提示后提问。使用`~/.local/bin/libreoffice-24.8 --impress`或已迁移的应用菜单入口打开PPT。后台语音与手势桥可从“LibreOffice Impress（语音与手势）”菜单入口启动，或执行：

```bash
systemctl --user start riscv-ai-reader.service
```

可说“打开PPT”“打开刚才那个PPT”或指定文档、桌面中的PPT名称。这些句子是意图示例；PPT能力随工具目录提供给规划模型，模型按整句语义选择动作和参数，执行阶段核对快照与真实回执。泛指打开使用最近真实历史，包括已关闭的PPT；无有效历史时提示先打开一份PPT。服务不预先加载演示文件。右挥为下一页，左挥为上一页，每次回到起手位置并等待就绪；说“根据当前页生成演讲备注并保存”，AI会把讲稿写进该页演讲备注并保存，保留普通大幻灯片，并在24.8原生底部备注栏显示。备注栏可拖动上缘调整高度，布局保存在上述24.8用户配置中；不切换成备注整页视图。备注保存当前支持PPTX，使用普通可编辑窗口和该24.8配置，停止后台桥不会关闭用户的Impress。手势恢复命令为`systemctl --user restart ukui-air-gesture.service`。

安装自检验证软件和推理链路；实际麦克风收音、扬声器可听性和挥手动作需在新设备上人工确认。场景四演示前，将需要分别摘要的三份文件放入系统“文档”目录。

日志在所选工作目录的`logs/`，检查结果为`doctor.json`。常用排查命令：

```bash
./install.sh --phase doctor
journalctl --user -u riscv-ai-adapter -u kylinbot -u kylinbot-asr-sensevoice -u kylinbot-tts -n 100
```

## 安装入口的验证范围

安装器及部署辅助程序已通过34项测试，覆盖配置渲染、凭据配对、GLES开发目录隔离、路径与模型校验、已有文件保护和只读预演。在当前K1上验证了依赖安装模拟、KWS/VAD编译、示例PPT生成，以及Gateway、LLM、TTS、ASR启动自检。1.7B模型重新下载、量化后的文件SHA256与当前正式模型一致。

完整的空白新机从零构建安装尚待首台新设备验收；上述模拟与自检不等同于完整新机安装通过。执行失败会停止并保留日志，请按具体错误处理后重跑，不要绕过版本或校验检查。
