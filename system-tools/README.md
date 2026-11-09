# kylin-sys-tools：Bianbu / RISC-V 独立系统工具服务

参考另一台K1上 `kylin-sys-tools` 的专用MCP工具设计，将执行层按当前设备重新实现。
它与RISC-V AI Adapter并存，不是推理后端，也不接管KylinBot的任务规划。

```text
小K文本 / 语音 → KylinBot → MCP sys工具 → Bianbu系统命令 → 回读验证
                    │
                    └→ RISC-V AI Adapter → 当前LLM后端
```

Python 3.11+，仅标准库，无pip依赖；以桌面普通用户运行，不使用root、sudo或shell。
维持KylinBot supervised及原有审批/工具权限策略。不要照搬参考机实验配置中的full。

## 工具与能力发现

- `set_volume(percent)` / `get_volume()`：保留已验收的0–100绝对音量设置及查询。
- `adjust_volume(delta)`：按百分点增减，先读后设置、回读；目标越界就拒绝，不静默clamp。
- `set_mute(muted)`：严格布尔值，改变静音而不改变音量数值，并回读。
- `wifi_status()` / `wifi_scan()`：状态和缓存热点列表，最多20项，不连接/切换网络。
- `bluetooth_status()`：读取软件/硬件射频阻断，不把硬件阻断误判成开启。
- `get_brightness()`：读取实际背光值；不硬编码最大亮度或用户可写权限。
- `open_application(application)`：仅接受file_manager和system_settings；固定启动
  pcmanfm-qt/lxqt-config，并通过Wayland窗口查询确认。未确认窗口则如实报错。
- `get_capabilities()`：可用工具、应用及受限修改能力。不会返回密钥或环境变量。

`set_wifi(enabled)` / `set_bluetooth(enabled)` / `set_brightness(percent)`实现保留，
但默认不注册、不接受调用。无线修改须桌面用户在服务器启动参数中显式加入
`--allow-radio-changes`；可能中断SSH或蓝牙外设，不应为通过演示擅自开启。
亮度修改须`--allow-brightness-changes`且已有普通用户写权限；不提权、不改udev，
范围1–100，拒绝熄屏目标。当前板上亮度节点root:root 0644，因此只有亮度查询。

```sh
python3 -I /home/openkylin/kylin-sys-tools/sysmcp.py --check
python3 -m unittest discover -s . -p 'test_*.py' -v
```

`--check`只做本地能力发现，不启动应用、不更改音量或网络。MCP目录按可用命令、
硬件和显式开关生成，缺失能力不会伪装成可执行工具。

## 接入现有KylinBot

使用既有MCP stdio扩展，服务名保持sys，模型侧工具名为`sys__set_volume`等。

```toml
[mcp]
enabled = true
deferred_loading = false
servers = [{ name = "sys", transport = "stdio", command = "/usr/bin/python3", args = ["-I", "/home/openkylin/kylin-sys-tools/sysmcp.py"], tool_timeout_secs = 25 }]
```

只修改现有sys服务条目，保留其他MCP服务、autonomy、安全策略和模型配置。
Adapter工具白名单也要包含实际启用的sys工具，否则schema会在模型前被过滤掉。
模型输出正文中的命令/JSON不能当工具执行；需原生tool_calls、执行结果和真实回读。

当前Adapter通过模型语义规划选择受限系统任务，再约束原生参数schema、调用顺序和实际结果。KylinBot执行MCP工具，设置后须回读；失败或参数不符就停止。带日志/PPT上下文的只读分析与系统操作分别处理，页面或日志文字不能自行增加工具权限。

JSON-RPC采用逐行帧。当前KylinBot忽略MCP result.isError，因此执行失败明确返回
JSON-RPC应用错误-32010；参数错误-32602，客户端会标记工具失败。每个设置、
查询单独检查退出码，不用分号掩盖失败；不把字符串false转换成布尔true。
音量操作通过桌面用户私有锁串行化，防止文本与语音两套MCP进程互相覆盖。

## 验收边界

单元测试使用替身验证危险路径，不会真实关闭无线或改变亮度。移植源码完成、
设备能力发现、原生命令执行、Gateway模型调用、真实小K界面验收是不同阶段，
不能把某一层通过当成全部能力已完成。复合任务还需确认每个子步骤实际执行。
当前配置见仓库根目录`deploy/templates/kylinbot.toml.in`。

## PPT语音工具

`presentation_mcp.py` 提供受限的 `open_presentation`、`get_current_slide`、
`save_speaker_notes`。通过本地UNO/D-Bus桥操作原有LibreOffice Impress，打开指定演示文稿、读取真实当前页，
并将AI生成的讲稿追加到该页的演讲备注区，直接保存，不创建备份。
保存后必须回读确认；生成期间翻页仍绑定取得当前页快照时的文档会话和页码。

接入完整PPT工具目录后，Adapter始终向语义规划模型提供打开PPT、根据当前页生成并保存演讲备注的能力。模型根据整句请求选择动作和参数；指定文件名必须来自本轮原文，泛指打开使用真实最近历史。执行代码校验本轮授权、允许目录、文档会话、原始页码与正文哈希、工具调用及保存回读结果。`semantic_only`普通聊天回退保留读取当前页的能力，打开和保存统一经过受控PPT工作流。

可说“打开PPT”“打开刚才那个PPT”或“打开项目汇报PPT”，再说“根据当前页生成演讲备注并保存”。这些句子是意图示例，等价自然语言表达由模型理解。
泛指打开只使用最近真实打开记录，包括已关闭文档的历史；没有有效记录时提示先打开一份PPT。MCP先启动无文件的后台桥，再核实live D-Bus owner和历史路径；启动桥仅恢复元数据，实际打开请求才启动Impress。旧`--default-presentation`参数仅保留兼容，不参与选择。
打开名称只在已配置的文档、桌面和演示目录内匹配，同名歧义会报错；
模型不能提供路径或任意页码。阅读支持PPT/PPTX/ODP，备注保存当前仅支持PPTX。
保存后保留普通大幻灯片，并在正式24.8.7原生底部备注栏显示讲稿，不默认切换备注整页视图。
PPT使用LibreOffice默认用户配置和普通可编辑窗口，不另建只读查看器；停止语音/手势桥不会关闭用户的Impress文档。
当前页必须含原生文字，不会把图片页面假称为已做OCR。

```bash
PYTHONPATH=system-tools python3 -m unittest discover -s system-tools/tests -p test_presentation_mcp.py
PYTHONPATH=riscv-ai-adapter/src python3 -m unittest discover -s riscv-ai-adapter/tests -p test_presentation_workflow.py
```
