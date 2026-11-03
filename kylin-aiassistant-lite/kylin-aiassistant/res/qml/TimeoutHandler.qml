import QtQuick 2.12

/**
 * 超时处理组件
 * 用于统一管理弱网超时提示逻辑，可被Chat.qml、VoiceSubtitleArea.qml、MiniChat.qml等多个组件复用
 * 
 * 使用方式：
 * 1. 在组件中添加 TimeoutHandler 实例
 * 2. 绑定 isWaiting 属性（表示是否正在等待响应）
 * 3. 绑定 onTimeout 信号处理超时提示
 * 4. 在消息发送时调用 startTimeout()
 * 5. 在消息接收时调用 resetTimeout()
 * 6. 在会话结束时调用 stopTimeout()
 */
Item {
    id: timeoutHandler

    // 超时时间（毫秒），默认15秒
    property int timeoutInterval: 15000

    // 是否正在等待响应
    property bool isWaiting: false

    // 是否已触发超时
    property bool isTimeoutTriggered: false

    // 定时器
    Timer {
        id: timeoutTimer
        interval: timeoutHandler.timeoutInterval
        running: false
        repeat: false
        onTriggered: {
            globals.sendLog("onTriggered " + isWaiting)
            if (timeoutHandler.isWaiting) {
                timeoutHandler.isTimeoutTriggered = true
            }
        }
    }

    /**
     * 启动超时定时器
     * 应在发送消息后调用
     */
    function startTimeout() {
        isTimeoutTriggered = false
        timeoutTimer.restart()
    }

    /**
     * 重置超时定时器
     * 应在收到消息时调用
     */
    function resetTimeout() {
        isTimeoutTriggered = false
        if (timeoutTimer.running) {
            timeoutTimer.restart()
        }
    }

    /**
     * 停止超时定时器
     * 应在会话结束或不需要等待时调用
     */
    function stopTimeout() {
        isTimeoutTriggered = false
        timeoutTimer.stop()
    }

    /**
     * 监听 isWaiting 属性变化，当变为 false 时停止定时器
     */
    onIsWaitingChanged: {
        globals.sendLog("onIsWaitingChanged " + isWaiting)
        if (!isWaiting) {
            stopTimeout()
        } else {
            startTimeout()
        }
    }
}
