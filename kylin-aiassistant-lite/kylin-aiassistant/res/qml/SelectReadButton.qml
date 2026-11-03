import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."

Rectangle {
    id:selectReadButton
    Accessible.name: "kylin-aiassistant_Select_readRect"
    height:28
    width: buttonLayout.width
    // 背景颜色根据状态动态变化
    color: {
        if (!selectReadButton.enabled) {
             // 禁用状态
            return "transparent"
        } else if (selectReadButtonMouseArea.pressed) {
            // 按下状态
            if (currentState === stateReading) {
                return Qt.rgba(55/255, 144/255, 250/255, 0.2)
            } else {
                return Qt.rgba(0/255, 0/255, 0/255, 0.2)
            }
        } else if (selectReadButtonMouseArea.containsMouse) {
            // 悬浮状态
            if (currentState === stateReading) {
                return Qt.rgba(55/255, 144/255, 250/255, 0.15)
            } else {
                return Qt.rgba(0/255, 0/255, 0/255, 0.15)
            }
        } else {
            // 默认状态
            return "transparent"
        }
    }
    radius: 4
    // enabled 属性由父组件控制，用于响应新手引导状态
    property bool enabled: true

    property string iconPath: ""
    property bool isActive : true
    property bool isSelectedEmpty: false
    property string selectText: ""
    property bool isStarting: false  // 是否正在启动朗读

    // 状态枚举
    readonly property int stateIdle: 0           // 空闲状态
    readonly property int stateSynthesizing: 1   // 语音合成状态
    readonly property int stateReading: 2        // 朗读状态
    readonly property int statePaused: 3         // 暂停状态
    // 当前状态，默认空闲
    property int currentState: stateIdle

    // 信号定义
    signal clicked()                    // 点击信号
    signal errorOccurred(string message,bool is_end,int  index, int errorCode, string model) // 错误信号
    signal interrupted()                // 被打断信号
    signal sigStartRead()               // 启动朗读
    signal sigPauseRead()               // 暂停朗读
    signal sigContinueRead()            // 继续朗读
    signal sigStopSynthesizing()        // 取消合成
    signal sigResetOpacity()             // 重置透明度

    // 公共方法：设置状态
    function setState(newState) {
        console.log("current set new state",newState)
        currentState = newState
    }

    // 公共方法：处理错误
    function handleError(errorMessage) {
        errorOccurred(errorMessage)
        setState(stateIdle)
    }

    // 公共方法：处理打断
    function handleInterrupt() {
        interrupted()
        setState(stateIdle)
    }

    // 状态转换逻辑
    onCurrentStateChanged: {
        // 状态变化时的处理逻辑可以在使用该控件的地方通过信号来处理
         console.log("ReadAloudButton state changed to:", currentState)
    }

    // 监听错误和打断信号（由外部通过函数调用触发）
    Component.onCompleted: {
        // 初始化完成

    }
    Connections {
        target: readerControl
        // 监听朗读开始信号
        onSigPlayingStart: function() {
            selectReadButton.isStarting = false  // 清除启动标志
            // 空闲状态不响应朗读启动信号
            if(currentState === stateIdle) {
                return
            }
            selectReadButton.setState(stateReading)
        }
        // 监听朗读结束信号
        onSigReadEnd: function(index) {
            // 如果正在启动中，忽略结束信号（可能是其他模块的结束信号）
            if (selectReadButton.isStarting) {
                console.log("正在启动中，忽略可能是其他模块的结束信号")
                return
            }
            // 空闲状态不响应朗读结束信号
            if(currentState === stateIdle) {
                return
            }
            selectReadButton.setState(stateIdle)
        }
        // 监听回复信号，朗读主要返回错误信号
        onReply: function(message, is_end, index, errorCode, model){
            if(currentState === stateIdle) {
                return
            }
            selectReadButton.errorOccurred(message, is_end, index, errorCode, model)
            setState(stateIdle)
        }
    }

    Row{
        id:buttonLayout
        Accessible.name: "kylin-aiassistant_Select_buttonLayout"
        anchors.centerIn: parent
        leftPadding: 4
        rightPadding: 4
        spacing: 6 // 动画区域和文字之间的间距，可根据需要调整
        width: animationRect.width + selectButtonText.implicitWidth + 15
        // 左侧：状态动画区域（固定28x16，居中垂直）
        Rectangle {
            id: animationRect
            width: 28
            height: 16
            anchors.verticalCenter: parent.verticalCenter // 垂直居中
            color: "transparent" // 透明背景，不影响视觉
            radius: 4
            // 各状态的动画/图标容器（原有逻辑完全保留）
            Row{
               anchors.fill: parent
               anchors.centerIn: parent
               // 空闲/暂停状态：普通扬声器图标
               Rectangle {
               id: idleStateContainer
               width: 28
               height: 16
               color: "transparent"
               radius: 4
               visible: selectReadButton.currentState === selectReadButton.stateIdle || selectReadButton.currentState === selectReadButton.statePaused
               Image {
                   id: idleIcon
                   source: "qrc:/html/html/img/chathome/read.svg"
                   width: 16
                   height: 16
                   anchors.centerIn: parent
                   opacity: selectReadButton.enabled ? 1 : 0.35
               }
               }
               // 语音合成状态：加载/旋转图标
               Rectangle {
               visible: selectReadButton.currentState === selectReadButton.stateSynthesizing
               id: loadingAnimation
               Accessible.name: "kylin-aiassistant_MeetingRecorder_loadingAnimation"
               width: 28
               height: 16
               color: "transparent"
               radius:  4
               //旋转图标
               Image {
                   id: reloadingAnimationImage
                   Accessible.name: "kylin-aiassistant_MeetingRecorder_reloadingAnimationImage"
                   source: "qrc:/html/html/img/generating.svg"
                   visible:  true
                   width: 16
                   height: 16
                   anchors.centerIn: parent
               }
               //旋转动画
               RotationAnimation{
                   id: reloadRotationAnimation
                   target: reloadingAnimationImage
                   from: 0
                   to: 360
                   duration: 1000
                   loops: Animation.Infinite
                   easing.type: Easing.Linear
                   running:  loadingAnimation.visible === true
               }
           }
               // 朗读状态：音频均衡器动画
               Rectangle {
               id: readingStateContainer
               width: 28
               height: 16
               color: "transparent"
               radius: 4
               visible: selectReadButton.currentState === selectReadButton.stateReading
               Loader {
                   id: voicePlayAnimationLoader
                   sourceComponent: VoicePlayAnimation {
                       visible: true
                   }
                   anchors.centerIn: parent
                   active: readingStateContainer.visible
               }
           }
           // 各状态容器结束
            }
        }

        // 右侧：文字Label区域（自适应宽度，垂直居中）
        Label {
            id: selectButtonText;
            Accessible.name: "kylin-aiassistant_Select_readButtonText"
            text: selectText
            // 文字颜色根据状态动态变化
            color: {
                if (!selectReadButton.enabled) {
                    return Qt.rgba(0/255, 0/255, 0/255, 0.35)  // 禁用状态
                }
                if (currentState === stateReading) {
                    // 朗读中状态
                    if (selectReadButtonMouseArea.pressed) {
                        return "#2C73C8"  // 按下
                    } else if (selectReadButtonMouseArea.containsMouse) {
                        return "#3489EE"  // 悬浮
                    } else {
                        return "#3790FA"  // 默认
                    }
                } else {
                    // 朗读、合成中状态
                    return Qt.rgba(0/255, 0/255, 0/255, 0.85)
                }
            }
            anchors.verticalCenter: parent.verticalCenter // 和左侧动画垂直居中对齐

        }
        }
    // ========== 核心布局调整结束 ==========

    // 鼠标区域
    MouseArea {
        id: selectReadButtonMouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: selectReadButton.enabled
        cursorShape: Qt.ArrowCursor  // 与悬浮条其他选项一致，不使用手型

        onClicked: {
            if (!selectReadButton.enabled) {
                return
            }
            // 根据当前状态执行不同的操作
            switch(selectReadButton.currentState) {
            case selectReadButton.stateIdle:
                // 空闲状态：点击后应该由外部处理开始朗读，这里只发送信号
                selectReadButton.sigStartRead()
                break
            case selectReadButton.stateSynthesizing:
                // 语音合成状态：处理取消合成请求
                selectReadButton.sigStopSynthesizing()
                break
            case selectReadButton.stateReading:
                // 朗读状态：点击暂停
                selectReadButton.sigPauseRead()
                break
            case selectReadButton.statePaused:
                // 暂停状态：点击继续
                selectReadButton.sigContinueRead()
                break
            }
        }

        onEntered: {
             selectReadButton.sigResetOpacity()
        }
        onExited: {
        }
        onPressed: {
        }

        onReleased: {
            // 释放时的视觉反馈已经在各个状态的容器中处理
        }
    }

}
