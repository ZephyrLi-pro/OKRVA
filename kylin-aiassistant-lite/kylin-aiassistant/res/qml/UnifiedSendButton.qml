import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import TextEditMenu 1.0
import "."

/**
 * 合并的发送/录音按钮组件
 * 根据输入框是否有文字自动切换：
 * - 无文字时：显示录音按钮
 * - 有文字时：显示发送按钮（激活状态）
 */
Item {
    id: unifiedSendButtonContainer
    width: 40
    height: 40

    // 外部属性
    property bool hasText: false  // 输入框是否有文字、图片或文档
    property bool buttonVisible: true  // 按钮是否可见（与isReplying相关）
    property bool enabled: true  // 按钮是否可点击
    property int speechMode: 0  // 0: 空闲, 1: 语音合成, 2: 录音中

    // 内部状态：是否应该显示发送按钮（有文字时显示发送，无文字时显示录音）
    property bool showSendButton: hasText && buttonVisible

    // 信号
    signal sendClicked()  // 发送按钮点击
    signal voiceInputClicked()  // 录音按钮点击
    signal startVoiceInput()  // 开始录音
    signal receiveTmpMsg(string text)  // 接收语音识别中间结果
    signal receiveFinalMsg(string text)  // 接收语音识别修正后的最终结果
    signal errorOccurred(string message, bool is_end, int index, int errorCode, string model) // 错误信号
    signal receiveErrorMsg(string msg)  // 错误信号

    // 状态枚举
    readonly property int stateIdle: 0           // 空闲状态
    readonly property int stateSynthesizing: 1   // 语音合成状态
    readonly property int stateRecording: 2      // 录音状态
    readonly property int stateStopping: 3       // 停止中状态

    // 公共方法：设置状态
    function setState(newState) {
        speechMode = newState
    }

    function finishVoiceStop() {
        msgPane.recordingUnInhibit()
        setState(stateIdle)
    }

    // 暴露 stopVoiceInput 函数供外部调用
    // 合成阶段（stateSynthesizing）不允许打断，需等待语音合成完成
    function stopVoiceInput(immediate = false) {
        if (speechMode === stateSynthesizing) {
            return
        }
        globals.sendLog("停止录音")
        if (immediate) {
            msgPane.recordingUnInhibit()
            setState(stateIdle)
        } else {
            setState(stateStopping)
        }
        recorderControl.stopSpeechRecognition()
    }

    // 供外部调用：触发语音输入（如空格键长按），QML 无法从外部 emit 信号，需通过此函数
    function triggerVoiceInput() {
        // 非空闲状态（正在合成/录音）时直接返回，防止重复触发
        if (speechMode !== stateIdle) {
            return
        }
        voiceInputClicked()
    }

    // 发送按钮（有文字时显示）
    Rectangle {
        id: sendButton
        Accessible.name: "kylin-aiassistant_Chat_unifiedSendButton"
        width: 40
        height: 40
        radius: 30
        visible: showSendButton && speechMode === stateIdle
        anchors.fill: parent

        property bool isActive: hasText && buttonVisible

        color: isActive ? "#3790FA" : Qt.rgba(55/255, 144/255, 250/255, 0.45)
        border.width: 1
        border.color: "#00000000"

        ToolTip {
            id: sendButtonTooltip
            visible: false
            delay: Qt.styleHints.mousePressAndHoldInterval
            text: qsTr("Send")
        }

        Item {
            anchors.fill: parent
            anchors.margins: 10

            Image {
                id: sendButtonImage
                Accessible.name: "kylin-aiassistant_Chat_unifiedSendButtonImage"
                anchors.centerIn: parent
                source: "qrc:/html/html/img/send_text_white.svg"
            }
        }

        MouseArea {
            id: sendButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_unifiedSendButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true

            onEntered: {
                sendButtonTooltip.visible = true
                if (sendButton.isActive) {
                    sendButton.color = "#3489EE"
                }
            }

            onExited: {
                sendButtonTooltip.visible = false
                if (sendButton.isActive) {
                    sendButton.color = "#3790FA"
                } else {
                    sendButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.45)
                }
            }

            onPressed: {
                sendButtonTooltip.visible = false
                if (sendButton.isActive) {
                    sendButton.color = "#2C73C8"
                }
            }

            onReleased: {
                sendButtonTooltip.visible = false
                if (sendButton.isActive) {
                    sendButton.color = "#3790FA"
                }
            }

            onClicked: {
                if (sendButton.isActive && unifiedSendButtonContainer.enabled) {
                    unifiedSendButtonContainer.sendClicked()
                }
            }
        }
    }

    // 录音按钮（无文字时显示，回复中时隐藏，由 StopButton 替代）
    Rectangle {
        id: voiceButton
        Accessible.name: "kylin-aiassistant_Chat_unifiedVoiceButton"
        width: 40
        height: 40
        radius: 20
        z: 2
        ToolTip {
            id: voiceButtonToolTip
            text: qsTr("Voice input (long press space)")
            visible: voiceButtonMouseArea.containsMouse
            delay: Qt.styleHints.mousePressAndHoldInterval
        }
        // kcomponent-normal (#E6E6E6)，与文件旁语音入口一致
        color: "#E6E6E6"
        border.width: 1
        // kline-component-normal：设计变量为透明描边
        border.color: "#00000000"
        visible: !showSendButton && speechMode === stateIdle && buttonVisible
        anchors.fill: parent

        Image {
            id: audioInputIcon
            anchors.centerIn: parent
            source: unifiedSendButtonContainer.enabled ? "qrc:/icon/icon/new_voice.svg" : "qrc:/icon/icon/micphone_disable.svg"
        }

        MouseArea {
            id: voiceButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_unifiedVoiceButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                if (!unifiedSendButtonContainer.enabled){
                    return;
                }
                voiceButton.color = "#DDDDDD"
            }
            onExited: {
                if (!unifiedSendButtonContainer.enabled){
                    return;
                }
                voiceButton.color = "#E6E6E6"
            }
            onPressed: {
                if (!unifiedSendButtonContainer.enabled){
                    return;
                }
                voiceButton.color = "#D0D0D0"
            }
            onReleased: {
                if (!unifiedSendButtonContainer.enabled){
                    return;
                }
                voiceButton.color = "#E6E6E6"
            }
            onClicked: {
                if (!unifiedSendButtonContainer.enabled){
                    return;
                }
                unifiedSendButtonContainer.voiceInputClicked()
            }
        }
    }

    // 停止录音按钮（录音中时显示，回复中时隐藏）
    Rectangle {
        id: voiceStopButton
        Accessible.name: "kylin-aiassistant_Chat_unifiedVoiceStopButton"
        width: 40
        height: 40
        radius: 20
        visible: speechMode === stateRecording && buttonVisible
        ToolTip {
            id: voiceStopButtonToolTip
            text: qsTr("Complete input")
            visible: voiceStopButtonMouseArea.containsMouse
            delay: Qt.styleHints.mousePressAndHoldInterval
        }
        color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
        anchors.fill: parent

        // 与设计稿一致：40 圆形容器，水平 padding 8，内容垂直水平居中
        Item {
            id: voiceStopRow
            Accessible.name: "kylin-aiassistant_Chat_unifiedVoiceStopRow"
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            Loader {
                sourceComponent: VoicePlayAnimation {
                    visible: true
                }
                anchors.centerIn: parent
                active: voiceStopButton.visible
            }
        }

        MouseArea {
            id: voiceStopButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_unifiedVoiceStopButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.15)
            }
            onExited: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
            }
            onPressed: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
            }
            onReleased: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
            }
            onClicked: {
                stopVoiceInput()
            }
        }
    }

    // 语音合成状态按钮（回复中时隐藏）
    Rectangle {
        id: synthesizingButton
        Accessible.name: "kylin-aiassistant_Chat_unifiedSynthesizingButton"
        width: 40
        height: 40
        radius: 6
        visible: speechMode === stateSynthesizing && buttonVisible
        color: Qt.rgba(38/255,38/255,38/255,0.05)
        anchors.fill: parent

        Image {
            id: synthesizingAnimationImage
            Accessible.name: "kylin-aiassistant_Chat_unifiedSynthesizingAnimationImage"
            source: "qrc:/html/html/img/generating.svg"
            visible: true
            width: 16
            height: 16
            anchors.centerIn: parent
        }
        RotationAnimation{
            id: synthesizingRotationAnimation
            target: synthesizingAnimationImage
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
            easing.type: Easing.Linear
            running: synthesizingButton.visible === true
        }

        ToolTip.text: qsTr("Loading voice model, about ten seconds...")
        ToolTip.visible: synthesizingButtonMouseArea.containsMouse
        ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

        MouseArea {
            id: synthesizingButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_unifiedSynthesizingButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {}
        }
    }

    // 停止中状态指示器（点击停止后，等待SDK reason=5回调期间显示）
    Rectangle {
        id: stoppingIndicator
        Accessible.name: "kylin-aiassistant_Chat_unifiedStoppingIndicator"
        width: 40
        height: 40
        radius: 20
        visible: speechMode === stateStopping && buttonVisible
        color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
        anchors.fill: parent

        Loader {
            sourceComponent: VoicePlayAnimation {
                visible: true
            }
            anchors.centerIn: parent
            active: stoppingIndicator.visible
        }

        ToolTip.text: qsTr("Stopping voice input...")
        ToolTip.visible: stoppingMouseArea.containsMouse
        ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

        MouseArea {
            id: stoppingMouseArea
            Accessible.name: "kylin-aiassistant_Chat_unifiedStoppingMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {}
        }
    }

    // 连接recorderControl信号
    Connections {
        target: recorderControl
        onSigReceiveSpeechRecognitionMessage: function(msg) {
            console.log("onSigReceiveSpeechRecognitionMessage " + msg)
            if (speechMode === stateSynthesizing) {
                setState(stateRecording)
            }
            if (speechMode !== stateRecording && speechMode !== stateStopping) {
                return;
            }
            let jsonObj = JSON.parse(msg)
            let msgType = jsonObj["msgType"]
            let msgBody = jsonObj["msgBody"]
            if (!msgBody.hasOwnProperty("recvMsg") || (typeof msgBody["recvMsg"] != "string")) {
                return
            }
            if (!msgBody.hasOwnProperty("isEnd") || (typeof msgBody["isEnd"] != "boolean")) {
                return
            }

            let isRecognized;
            if(msgBody.hasOwnProperty("recognized")) {
                isRecognized= msgBody["recognized"]
            }
            else {
                isRecognized = true;
            }

            let errorCode = msgBody["errorCode"]
            if(errorCode !== 0) {
                unifiedSendButtonContainer.receiveErrorMsg(msg)
                stopVoiceInput(true)
                return;
            }
            if(isRecognized === false) {
                let tempMsg  =  msgBody["recvMsg"];
                unifiedSendButtonContainer.receiveTmpMsg(tempMsg)
            }
            else if(isRecognized === true) {
                let lastMsg  =  msgBody["recvMsg"];
                unifiedSendButtonContainer.receiveFinalMsg(lastMsg)
                if (speechMode === stateStopping) {
                    finishVoiceStop()
                }
            }
        }

        onSigCancelSendVoice: function() {
            finishVoiceStop()
        }
    }
}
