# voice-kyling

`voice-kyling` 现在是 `kylin-aiassistant` 主进程内的 AI 球功能模块，不再作为独立进程构建、安装或注册 D-Bus service。

外部 AI 键服务继续调用主助手 D-Bus：

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall true
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall false
```

`showAIBall(true)` 显示 AI 球并开始语音识别，`showAIBall(false)` 停止识别、播放隐藏动画，并把识别文本直接提交到主助手聊天页。
