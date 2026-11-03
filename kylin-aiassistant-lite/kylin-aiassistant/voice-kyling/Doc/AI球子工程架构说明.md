# AI 球模块架构说明

## 定位

`kylin-aiassistant/voice-kyling` 是主助手进程内的 AI 键“按住说话”模块。它负责 AI 球 QML 动画、语音识别、音量驱动效果、识别文本缓存和隐藏后提交。

该模块不再是独立子工程，不再注册独立 D-Bus 服务，也不再启动或协调第二个进程。

## 工程结构

```text
kylin-aiassistant/
└── voice-kyling/
    ├── aiballmanager.h/.cpp
    ├── aiballcontroller.h/.cpp
    ├── speechrecognitioncontroller.h/.cpp
    ├── speecherrormessage.h/.cpp
    ├── qml/
    │   ├── main.qml
    │   └── AIBall.qml
    ├── voice-kyling.qrc
    └── Doc/
```

## 编译接入

`kylin-aiassistant/kylin-aiassistant.pro` 直接编译模块源文件：

```qmake
SOURCES += \
    voice-kyling/aiballcontroller.cpp \
    voice-kyling/speecherrormessage.cpp \
    voice-kyling/speechrecognitioncontroller.cpp

RESOURCES += \
    voice-kyling/voice-kyling.qrc
```

顶层 `kylin-aiassistant.pro` 不再包含独立的 `voice-kyling` 子工程。

## 主流程

1. `WindowController::initDbusWathcer()` 创建 `AIBallManager`。
2. `KADBusOperator::showAIBall(bool visible)` 接收外部 D-Bus 调用。
3. `visible=true` 时，`AIBallManager` 调用 `AIBallController::showBall()`，QML AI 球显示并开始语音识别。
4. `visible=false` 时，`AIBallManager` 调用 `AIBallController::hideBall()`，停止识别并进入隐藏动画。
5. QML 隐藏动画结束后调用 `hideAnimationFinished()`。
6. `AIBallController` 发出 `textSubmitted(text)`。
7. `WindowController` 显示主窗口，并通过 `sendVoiceTextToChat(text)` 把文本交给聊天页发送。

## D-Bus 入口

```bash
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall true
qdbus org.ukui.kylin_aiassistant /org/ukui/kylin_aiassistant org.ukui.kylin_aiassistant.showAIBall false
```

`showAIBall(true)` 只负责显示 AI 球和开始识别；`showAIBall(false)` 负责停止识别、隐藏 AI 球并提交文本。
