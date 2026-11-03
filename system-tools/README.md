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

当前Adapter对整句匹配的明确系统指令限制可用工具，并把用户明确指定的数字、
开关和应用限制在原生参数schema中，降低0.6B混淆参数的概率。对于“设置/增减
音量，然后打开文件管理器/系统设置”这两种明确流程，每步仍经KylinBot原生调用，
只有匹配的成功结果才能进入下一步；失败或参数不符立即停止，不自动重复执行。
这是窄范围的指令路由，不是完整Agent规划能力；带上下文、引用、否定、图片、
文件及其他复杂任务仍保留原有的KylinBot规划路径，不应据此宣称五个比赛场景都完成。

JSON-RPC采用逐行帧。当前KylinBot忽略MCP result.isError，因此执行失败明确返回
JSON-RPC应用错误-32010；参数错误-32602，客户端会标记工具失败。每个设置、
查询单独检查退出码，不用分号掩盖失败；不把字符串false转换成布尔true。
音量操作通过桌面用户私有锁串行化，防止文本与语音两套MCP进程互相覆盖。

## 验收边界

单元测试使用替身验证危险路径，不会真实关闭无线或改变亮度。移植源码完成、
设备能力发现、原生命令执行、Gateway模型调用、真实小K界面验收是不同阶段，
不能把某一层通过当成全部能力已完成。复合任务还需确认每个子步骤实际执行。
本目录未自动创建提交或推送仓库；参考机未被修改。
