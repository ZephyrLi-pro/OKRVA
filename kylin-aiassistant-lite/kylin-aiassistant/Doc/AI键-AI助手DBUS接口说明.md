# AI键-AI助手 DBUS 接口说明

本文档说明 kylin-aiassistant 提供给 AI 键硬件调用的 DBUS 接口。

## 服务信息

| 项 | 值 |
|----|----|
| Service | `org.ukui.kylin_aiassistant` |
| Object Path | `/org/ukui/kylin_aiassistant` |
| Interface | `org.ukui.kylin_aiassistant` |
| Bus | Session Bus |

## 接口列表

| 方法 | 签名 | 说明 |
|------|------|------|
| `showOrHideView` | `void showOrHideView()` | 显示、激活或隐藏 AI 助手主窗口 |
| `showAIBall` | `void showAIBall(bool visible)` | 显示或隐藏 AI 球悬浮窗口 |
| `ShowChatAndSendText` | `bool ShowChatAndSendText(QString text)` | 显示聊天窗口，并把外部文本作为用户问题发送 |

`showOrHideView` 无参数、无返回值。`showAIBall` 参数 `visible=true` 表示显示 AI 球，`visible=false` 表示隐藏 AI 球，无返回值。

AI 键按住说话流程已合并到 `kylin-aiassistant` 主进程内，外部按键服务调用：

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall true
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall false
```

隐藏动画结束后，识别文本会在主进程内直接提交到聊天页。

## 行为说明

### showOrHideView

- 主窗口隐藏或最小化时：显示并激活。
- 主窗口可见但不在前台时：置顶并激活。
- 主窗口可见且有焦点时：隐藏。
- 若存在活动模态对话框，隐藏操作会被阻止。

### showAIBall

- `visible=true`：显示并激活 AI 球。
- `visible=false`：隐藏 AI 球。
- `visible=true` 会开始语音识别。
- `visible=false` 会停止语音识别，隐藏动画结束后提交识别文本。

### ShowChatAndSendText

- `text` 为空时返回 `false`。
- `text` 非空时显示主窗口、切换到聊天页，并按普通文本问题发送。
- 该接口保留给其他外部服务直接提交文本使用；AI 球模块在主进程内直接走信号提交。

## 调用示例

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showOrHideView
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall true
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall false
```

```bash
# 显示/隐藏主窗口
dbus-send --session --dest=org.ukui.kylin_aiassistant \
  --type=method_call --print-reply \
  /org/ukui/kylin_aiassistant \
  org.ukui.kylin_aiassistant.showOrHideView

# 显示 AI 球
dbus-send --session --dest=org.ukui.kylin_aiassistant \
  --type=method_call --print-reply \
  /org/ukui/kylin_aiassistant \
  org.ukui.kylin_aiassistant.showAIBall boolean:true

# 隐藏 AI 球
dbus-send --session --dest=org.ukui.kylin_aiassistant \
  --type=method_call --print-reply \
  /org/ukui/kylin_aiassistant \
  org.ukui.kylin_aiassistant.showAIBall boolean:false
```

## 自动启动

若进程未运行，Session Bus 会通过 service 文件自动启动：

```ini
[D-BUS Service]
Name=org.ukui.kylin_aiassistant
Exec=/usr/bin/kylin-aiassistant
```

默认路径：

```text
/usr/share/dbus-1/services/org.ukui.kylin_aiassistant.service
```

## 排查命令

```bash
# 查看可用方法
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant \
  org.freedesktop.DBus.Introspectable.Introspect

# 监听调用
dbus-monitor --session "destination=org.ukui.kylin_aiassistant"

# 检查进程
pgrep kylin-aiassistant
```
