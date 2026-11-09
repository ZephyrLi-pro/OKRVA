# 当前K1运行配置

`systemd/`保存这台K1各服务当前生效的完整配置，历史drop-in已合并。`templates/`保存应用参数，密钥位置使用占位符。

- LLM主模型：0.6B，4线程，IME预填充/CPU生成，TCM开启。
- 阅读模型：1.7B-Q4_0，生成3线程/预填充4线程，CPU0–3，TCM关闭，ctx3072，batch/ubatch128，互斥驻留、空闲60秒卸载。
- SenseVoice：SpacemiT EP，两线程，18081。
- Matcha：CPU四线程，CPU0–3，CPUWeight400，18083。
- KWS：门控辅助程序，INT8模型、单线程、CPU0–3；策略脚本观察当前LLM/播报忙闲。
- 手势：摄像头按本机设备选择，reading识别配置，独立于PPT服务。
- PPT：LibreOffice24.8.7原生Impress，`~/.local/bin/libreoffice-24.8`；UNO pipe `rvai_trial_248_1000`，桥启动等待300秒。

这是当前设备使用方案的配置文件，不包含用户token、模型权重或机器备份。新设备必须先准备已验证的LibreOffice 24.8或以上RISC-V启动器；安装器检查它，但不会自动获取或替换这套Office运行环境。准备、检查及自定义启动器命令见[安装前提](INSTALL.md#libreoffice-248运行前提)，其余安装位置及启动方法见[复现说明](../docs/REPRODUCE.md)。

语音录音连接随音频服务恢复：`kylinbot.service`通过`PartOf`关联`pipewire.service`和`pipewire-pulse.service`，并在两者之后启动。通过systemd重启音频服务时，KylinBot会一同重启并重新打开默认麦克风，避免保留已经失效的录音连接。

手势服务启动前等待 Wayland 桌面套接字可连接、所选摄像头设备存在，避免登录阶段提前启动 Qt 导致退出。安装服务时先执行：

```bash
install -Dm644 deploy/helpers/wait-gesture-ready.py ~/.local/libexec/okrva-wait-gesture-ready.py
```

手动启动或恢复：`systemctl --user restart ukui-air-gesture.service`。查看状态：`systemctl --user status ukui-air-gesture.service`。

## 对话记录的进程生命周期

文本和语音窗口的会话在进程运行期间保留，最小化、隐藏到托盘或重新显示窗口不触发清理。`okrva-aiassistant.service`和`okrva-voice-pipewire.service`在实际进程启动前、退出后调用`helpers/clear-conversation-history.py`，清理各自的本地聊天记录、会话资料引用和KylinBot Gateway会话。Gateway暂时不可用时保留待清理会话ID，下一次启动或退出时重试。记录ID计数保留递增，避免复用旧会话ID。

安装辅助程序（仓库根目录）：

```bash
install -Dm644 deploy/helpers/wait-ready.py ~/.local/libexec/okrva-wait-ready.py
install -Dm644 deploy/helpers/clear-conversation-history.py ~/.local/libexec/okrva-clear-conversation-history.py
install -Dm755 deploy/helpers/start-smallk.sh ~/.local/bin/okrva-smallk
install -Dm644 deploy/systemd/okrva-aiassistant.service ~/.config/systemd/user/okrva-aiassistant.service
install -Dm644 deploy/systemd/okrva-voice-pipewire.service ~/.config/systemd/user/okrva-voice-pipewire.service
systemctl --user daemon-reload
```

应用菜单的小K入口执行`~/.local/bin/okrva-smallk`，先启动受管理的主进程，再通过QtSingleApplication激活现有窗口。对应桌面入口模板见`templates/kylin-aiassistant.desktop.in`，替换`@HOME@`后安装到`~/.local/share/applications/kylin-aiassistant.desktop`。语音沿用用户D-Bus激活到上述语音服务的入口。

初次启用并需要清空原有全部聊天记录时，先结束文本及语音进程，再执行`python3 ~/.local/libexec/okrva-clear-conversation-history.py all`。该命令删除聊天记录，随后启动服务即为空白会话。用户文档、已生成的摘要、设置和定时任务继续保留。回归测试：`python3 -m unittest discover -s deploy/tests -v`。

## 配置服务异常重启排查

部分Bianbu安装环境中的`libkysdk-conf2`缺少`/etc/kylin-config/basic`目录。若`journalctl -u kysdk-conf2.service`确认在该目录注册文件监控时报`FileNotFoundError`，服务会反复导入依赖、退出和重启，消耗CPU。补齐该目录即可让服务完成初始化；不修改已有配置内容：

```bash
sudo install -d -m 0755 -o root -g root /etc/kylin-config/basic
systemctl show kysdk-conf2.service -p MainPID -p NRestarts -p CPUUsageNSec
```

等待原有自动重启后，确认进程ID与重启计数稳定，空闲CPU消耗不再持续增加。该修复不需要停止录屏、ASR或语音窗口。

## 演示录屏

在K1的Wayland桌面终端运行`~/record-k1.sh`，选择实际播放声音的设备；看到“录屏已就绪”后开始演示，按Ctrl+C停止。输出为家目录中带时间戳的MP4，声音同时包含该设备的播放监听源及当前默认麦克风。

当前配置为1280×720、25帧/秒、H.264硬件编码与AAC混合音轨。使用Bianbu FFmpeg的`h264_stcodec`，采集和音频线程绑定CPU4–7，与主LLM的CPU0–3分开。画面按更新采集，由fps滤镜生成25帧输出，避免强制持续抓取未变化的屏幕。

停止录制后，`finalize-recording.py`对硬件编码器的参数包和时间戳做无重新编码的封装整理，检查分辨率、帧率、音轨和首段解码，成功后原子替换文件；失败则保留原始录制文件。初始化日志位于`~/.cache/okrva-recording/`。结束时仅卸载本次创建的混音模块并恢复原音频设备。

从仓库根目录安装：

```bash
install -Dm644 deploy/helpers/finalize-recording.py ~/.local/libexec/okrva-finalize-recording.py
install -m755 deploy/helpers/record-k1.sh ~/record-k1.sh
```

需要`wf-recorder`、支持`h264_stcodec`的Bianbu FFmpeg及`ffprobe`、`pactl`、`taskset`和Python3。`taskset`由`util-linux`提供。先在系统音频设置中选择USB摄像头麦克风作为默认输入，再运行脚本。

语音默认采用单轮唤醒：每次说“你好麒麟”后提问，播报结束保留回答并回到等待唤醒。`settings.ini` 的 `[General]` 下 `voice-followup-enabled=false`，缺省时也关闭自动追问。

## LibreOffice 24.8正式入口

`riscv-ai-reader.service`显式设置`RVAI_LIBREOFFICE_LAUNCHER=%h/.local/bin/libreoffice-24.8`和`RVAI_IMPRESS_PIPE=rvai_trial_248_1000`，接入同一原生Impress实例。独立运行库和profile沿用`~/.local/opt/libreoffice-24.8-trial`、`~/.local/share/libreoffice-24.8-trial/profile`，路径名保留，不另建阅读器UI。正常显示为大幻灯片加可调整高度的底部备注栏。

安装器只检查既有启动器版本，不自动下载/搬动24.8或替换系统包。缺少合格启动器时`all/check/install/start`明确失败；`doctor`报告版本检查结果，实际底栏与保存行为仍需窗口验收。默认入口可先检查：

```bash
test -x "$HOME/.local/bin/libreoffice-24.8"
"$HOME/.local/bin/libreoffice-24.8" --version
./install.sh --phase check
```

使用其他已验证布局时，在同一个终端设置`RVAI_LIBREOFFICE_LAUNCHER`为实际绝对路径，设置与Impress实例一致的`RVAI_IMPRESS_PIPE`，再执行安装器。完整命令见[安装说明](INSTALL.md#libreoffice-248运行前提)。默认`-trial`路径仍被当前正式启动器和桥引用，不能仅按路径名删除。
