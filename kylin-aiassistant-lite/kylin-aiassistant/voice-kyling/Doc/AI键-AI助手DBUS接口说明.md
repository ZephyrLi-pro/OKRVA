# AI键-AI助手 D-Bus 接口说明

AI 球已合并到 `kylin-aiassistant` 主进程内，外部 AI 键服务只需要调用主助手 D-Bus 服务。

## 服务信息

| 项 | 值 |
|----|----|
| Service | `org.ukui.kylin_aiassistant` |
| Object Path | `/org/ukui/kylin_aiassistant` |
| Interface | `org.ukui.kylin_aiassistant` |
| Bus | Session Bus |

## AI 球接口

| 方法 | 签名 | 说明 |
|------|------|------|
| `showAIBall` | `void showAIBall(bool visible)` | 显示或隐藏 AI 球 |

## 调用方式

显示 AI 球并开始语音识别：

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall true
```

隐藏 AI 球、停止语音识别并提交识别文本：

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall false
```

等价 `dbus-send`：

```bash
dbus-send --session --type=method_call --dest=org.ukui.kylin_aiassistant \
  /org/ukui/kylin_aiassistant \
  org.ukui.kylin_aiassistant.showAIBall boolean:true

dbus-send --session --type=method_call --dest=org.ukui.kylin_aiassistant \
  /org/ukui/kylin_aiassistant \
  org.ukui.kylin_aiassistant.showAIBall boolean:false
```

## 运行流程

```text
按住 AI 键
  -> showAIBall(true)
  -> 主助手进程内显示 QML AI 球
  -> 开始语音识别

松开 AI 键
  -> showAIBall(false)
  -> 停止语音识别
  -> AI 球播放隐藏动画
  -> 识别文本直接发送到主助手聊天页
```

不再需要 `org.ukui.voice_kyling`、`/usr/bin/voice-kyling` 或额外的 D-Bus activation service。
