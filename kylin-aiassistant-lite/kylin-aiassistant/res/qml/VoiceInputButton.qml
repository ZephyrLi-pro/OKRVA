import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

// 合并的语音输入按钮组件 - 根据状态自动切换显示语音输入或停止语音输入按钮
Item {
    id: voiceInputButtonContainer
    width: voiceStopButton.visible ? voiceStopButton.width : (synthesizingButton.visible ? synthesizingButton.width : 32)
    height: 32

    // 状态枚举
    readonly property int stateIdle: 0           // 空闲状态
    readonly property int stateSynthesizing: 1   // 语音合成状态
    readonly property int stateRecording: 2      // 录音状态
    readonly property int stateStopping: 3       // 停止中状态

    // 外部属性
    property int speechMode: 0  // 0: 显示语音输入按钮, 1: 显示语音合成状态, 2: 显示停止语音输入按钮（正在录音）
    property bool enabled: true  // 按钮是否可点击，由外部控制
    
    // 公共方法：设置状态
    function setState(newState) {
        console.log("VoiceInputButton setState:", newState)
        speechMode = newState
    }

    function finishVoiceStop() {
        msgPane.recordingUnInhibit()
        setState(stateIdle)
    }

    // 信号
    signal startVoiceInput()
    signal receiveTmpMsg(string text)  // 接收语音识别中间结果
    signal receiveFinalMsg(string text)  // 接收语音识别修正后的最终结果
    signal errorOccurred(string message,bool is_end,int  index, int errorCode, string model) // 错误信号
    signal receiveErrorMsg(string msg)  // 错误信号

    // 暴露 stopVoiceInput 函数供外部调用
    // immediate=true: 强制立即停止（发送/切页/错误等场景）
    // immediate=false: 优雅停止，等待SDK reason=5回调后清理（用户点击停止按钮）
    function stopVoiceInput(immediate = false) {
        if (immediate) {
            msgPane.recordingUnInhibit()
            setState(stateIdle)
        } else {
            setState(stateStopping)
        }
        recorderControl.stopSpeechRecognition();
    }

    // 按钮-语音输入
    Rectangle {
        id: voiceSwapButton
        Accessible.name: "kylin-aiassistant_Chat_voiceSwapButton"
        width: 32
        height: 32
        radius: 6
        ToolTip {
            text: qsTr("Voice input (long press space)")
            visible: voiceSwapButtonMouseArea.containsMouse
            delay: Qt.styleHints.mousePressAndHoldInterval
        }
        color: "transparent"
        visible: speechMode === stateIdle
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            Image {
                id: audioInputIcon
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                source: voiceInputButtonContainer.enabled ? "qrc:/icon/icon/new_voice.svg" : "qrc:/icon/icon/micphone_disable.svg"
            }
        }

        MouseArea {
            id: voiceSwapButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_voiceSwapButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                if (!voiceInputButtonContainer.enabled){
                    return;
                }
                voiceSwapButton.color = "#26000000"
            }
            onExited: {
                if (!voiceInputButtonContainer.enabled){
                    return;
                }
                voiceSwapButton.color = "transparent"
            }
            onPressed: {
                if (!voiceInputButtonContainer.enabled){
                    return;
                }
                voiceSwapButton.color = "#33000000"
            }
            onReleased: {
                if (!voiceInputButtonContainer.enabled){
                    return;
                }
                voiceSwapButton.color = "transparent"
            }
            onClicked: {
                if (!voiceInputButtonContainer.enabled){
                    return;
                }
                // 打断所有音频占用
                recorderControl.interruptAllVoiceUsing()
                // 先进入语音合成等待状态
                setState(stateSynthesizing)
                startVoiceInput()
                msgPane.recordingInhibit()
                if(recorderControl.startSpeechRecognition()){
                }
            }
        }
    }

    // 按钮-停止语音输入
    Rectangle {
        id: voiceStopButton
        Accessible.name: "kylin-aiassistant_Chat_voiceStopButton"
        width: 32
        height: 32
        radius: 6
        visible: speechMode === stateRecording
        ToolTip {
            text: qsTr("Complete input")
            visible: voiceStopButtonMouseArea.containsMouse
            delay: Qt.styleHints.mousePressAndHoldInterval
        }
        color: "transparent"
        anchors.fill: parent

        Row {
            id: voiceStopRow
            Accessible.name: "kylin-aiassistant_Chat_voiceStopRow"
            anchors.fill: parent
            spacing: 4
            leftPadding: 8
            rightPadding: 8

            Rectangle{
                width: 20
                height: 32
                color: "transparent"
                id: playAnimationRect
                // 播放动画
                Loader{
                    sourceComponent: VoicePlayAnimation{
                        visible: true
                    }
                    anchors.verticalCenter: playAnimationRect.verticalCenter
                    anchors.horizontalCenter: playAnimationRect.horizontalCenter
                    active: voiceStopButton.visible
                }
            }
        }

        MouseArea {
            id: voiceStopButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_voiceStopButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 255/255, 0.1);
            }
            onExited: {
                voiceStopButton.color = "transparent"
            }
            onPressed: {
                voiceStopButton.color = Qt.rgba(55/255, 144/255, 255/255, 0.2);
            }
            onReleased: {
                voiceStopButton.color = "transparent"
            }
            onClicked: {
                stopVoiceInput()
            }
        }
    }

    // 语音合成状态按钮
    Rectangle {
        id: synthesizingButton
        Accessible.name: "kylin-aiassistant_Chat_synthesizingButton"
        width: 32
        height: 32
        radius: 6
        visible: speechMode === stateSynthesizing
        ToolTip {
            text: qsTr("Loading voice model, about ten seconds...")
            visible: synthesizingButtonMouseArea.containsMouse
            delay: Qt.styleHints.mousePressAndHoldInterval
        }
        color: Qt.rgba(38/255,38/255,38/255,0.05)
        anchors.fill: parent

        //旋转图标
        Image {
            id: synthesizingAnimationImage
            Accessible.name: "kylin-aiassistant_Chat_synthesizingAnimationImage"
            source: "qrc:/html/html/img/generating.svg"
            visible: true
            width: 16
            height: 16
            anchors.centerIn: parent
        }
        //旋转动画
        RotationAnimation{
            id: synthesizingRotationAnimation
            target: synthesizingAnimationImage
            from: 0
            to: 360
            duration: 1000  // 每次旋转的时间为1秒
            loops: Animation.Infinite  // 无限循环
            easing.type: Easing.Linear  // 线性缓动，持续不断的旋转
            running: synthesizingButton.visible === true
        }

        MouseArea {
            id: synthesizingButtonMouseArea
            Accessible.name: "kylin-aiassistant_Chat_synthesizingButtonMouseArea"
            anchors.fill: parent
            hoverEnabled: true
            // 语音合成状态通常不允许操作，但保留鼠标区域以显示ToolTip
        }
    }

    // 停止中状态指示器（点击停止后，等待SDK reason=5回调期间显示）
    Rectangle {
        id: stoppingIndicator
        Accessible.name: "kylin-aiassistant_Chat_stoppingIndicator"
        width: 32
        height: 32
        radius: 6
        visible: speechMode === stateStopping
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
            Accessible.name: "kylin-aiassistant_Chat_stoppingMouseArea"
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
            // 如果处于语音合成等待状态，收到第一条消息后切换到录音状态
            if (speechMode === stateSynthesizing) {
                setState(stateRecording)
            }
            //非激活模式不处理消息接收
            if (speechMode !== stateRecording && speechMode !== stateStopping) {
                console.log("Recognition results have been stopped from being received.")
                return;
            }
            //对收到的持续语音识别结果进行发信号处理
            let jsonObj = JSON.parse(msg)
            let msgType = jsonObj["msgType"]
            let msgBody = jsonObj["msgBody"]
            if (!msgBody.hasOwnProperty("recvMsg") || (typeof msgBody["recvMsg"] != "string")) {
                console.log("Not carried recvMsg!")
                return
            }
            if (!msgBody.hasOwnProperty("isEnd") || (typeof msgBody["isEnd"] != "boolean")) {
                console.log("Not carried isEnd!")
                return
            }

            // ✅ 新增：检查是否是识别结果（应包含 recognized 字段）
            let isRecognized;
            if(msgBody.hasOwnProperty("recognized")) {
                isRecognized= msgBody["recognized"]
            }
            else {
                isRecognized = true;
            }
            let isEnd = msgBody["isEnd"];

            //错误情况处理
            let errorCode = msgBody["errorCode"]
            if(errorCode !== 0) {
                let errorMsg  =  msgBody["recvMsg"];
                voiceInputButtonContainer.receiveErrorMsg(msg)
                stopVoiceInput(true)
                return;
            }
            //识别中
            if(isRecognized === false) {
                let tempMsg  =  msgBody["recvMsg"];
                voiceInputButtonContainer.receiveTmpMsg(tempMsg)
            }
            else if(isRecognized === true) {
                let lastMsg  =  msgBody["recvMsg"];
                voiceInputButtonContainer.receiveFinalMsg(lastMsg)
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

