import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12

import "."

/**
 * 统一的朗读按钮控件
 * 支持4个状态：空闲、语音合成、朗读、暂停
 */
Rectangle {
    id: readAloudButton
    Accessible.name: "kylin-aiassistant_ReadAloudButton"
    width: 16
    height: 16
    color: "transparent"
    
    // 状态枚举
    readonly property int stateIdle: 0           // 空闲状态
    readonly property int stateSynthesizing: 1   // 语音合成状态
    readonly property int stateReading: 2        // 朗读状态
    readonly property int statePaused: 3         // 暂停状态

    property bool isStarting: false  // 是否正在启动朗读

    // 当前状态，默认空闲
    property int currentState: stateIdle
    
    // 按钮内部记录当前的朗读索引
    property int chatReadingIndex: -1
    // 按钮内部记录当前自身所在索引
    property int currentIndex: -1

    // 全局“当前朗读”已切到其它消息时，立即复位本按钮 UI（不依赖后端异步 sigReadEnd）
    onChatReadingIndexChanged: {
        if (readAloudButton.currentIndex === readAloudButton.chatReadingIndex)
            return
        if (readAloudButton.currentState === readAloudButton.stateIdle)
            return
        readAloudButton.isStarting = false
        readAloudButton.setState(readAloudButton.stateIdle)
        readAloudButton.sigReadEnd()
    }

    // ToolTip文本
    property string toolTipText: {
        switch(currentState) {
        case stateIdle:
            return qsTr("Read")
        case stateSynthesizing:
            return qsTr("Synthesizing")
        case stateReading:
            return qsTr("Pause Read")
        case statePaused:
            return qsTr("Continue Read")
        default:
            return qsTr("Read")
        }
    }
    
    ToolTip {
        text: toolTipText
        visible: readAloudButtonMouseArea.containsMouse
        delay: Qt.styleHints.mousePressAndHoldInterval
    }
    
    // 信号定义
    signal clicked()                    // 点击信号
    signal errorOccurred(string message,bool is_end,int  index, int errorCode, string model) // 错误信号
    signal interrupted()                // 被打断信号
    signal sigStartRead()               // 启动朗读
    signal sigPauseRead()               // 暂停朗读
    signal sigContinueRead()            // 继续朗读
    signal sigReadEnd()                 // 结束朗读
    signal sigStopSynthesizing()        // 取消合成
    signal sigStateChanged(int state)   // 状态变更信号

    // 公共方法：设置状态
    function setState(newState) {
        console.log("current set new state",newState)
        currentState = newState
        sigStateChanged(currentState)
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
            readAloudButton.isStarting = false  // 清除启动标志
            // 空闲或暂停状态下不响应朗读开始信号，避免被其他朗读来源（如提问气泡右键朗读）误触发
            if (currentState === stateIdle || currentState === statePaused) {
                return
            }
            readAloudButton.setState(stateReading)
        }
        // 监听朗读结束信号
        onSigReadEnd: function(index) {
            // 空闲状态不响应朗读结束信号
            if(currentState === stateIdle) {
                return
            }
            // 如果正在启动，并且当前启动项就是准备启动项，则忽略此信号，
            // 如果正在启动，但是启动项不是当前的启动项目，则执行停止的状态变换
            console.log("当前接到停止信号时的自身索引:",readAloudButton.currentIndex)
            console.log("和当前请求朗读的索引:",readAloudButton.chatReadingIndex)
            if (readAloudButton.isStarting) {
                if(readAloudButton.currentIndex === readAloudButton.chatReadingIndex) {
                    console.log("正在当前回答区朗读按钮的启动中，忽略本次自身发出的的结束信号")
                    return;
                }
                else {
                    console.log("正在index正在启动，接到其他按钮的启动请求，将当前状态进行重置")
                    readAloudButton.setState(stateIdle)
                    readAloudButton.sigReadEnd()
                }
            }

            readAloudButton.setState(stateIdle)
            readAloudButton.sigReadEnd()
        }
        // 监听回复信号，朗读主要返回错误信号
        onReply: function(message, is_end, index, errorCode, model){
            readAloudButton.errorOccurred(message, is_end, index, errorCode, model)
            setState(stateIdle)
        }
    }
    // 主容器
    Row {
        spacing: 4
        anchors.centerIn: parent
        
        // 空闲状态：普通扬声器图标
        Rectangle {
            id: idleStateContainer
            width: 16
            height: 16
            color: "transparent"
            radius: 4
            visible: readAloudButton.currentState === readAloudButton.stateIdle || readAloudButton.currentState === readAloudButton.statePaused
            
            Image {
                id: idleIcon
                source: readAloudButtonMouseArea.pressed 
                        ? "qrc:/html/html/img/chathome/read_active.svg"
                        : (readAloudButtonMouseArea.containsMouse 
                           ? "qrc:/html/html/img/chathome/read_hover.svg"
                           : "qrc:/html/html/img/chathome/read.svg")
                width: 16
                height: 16
                anchors.centerIn: parent
            }
        }
        
        // 语音合成状态：加载/旋转图标
        Rectangle {
            visible: readAloudButton.currentState === readAloudButton.stateSynthesizing
            id: loadingAnimation
            Accessible.name: "kylin-aiassistant_MeetingRecorder_loadingAnimation"
            width: 16
            height: 16
            color: Qt.rgba(38/255,38/255,38/255,0.05);
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
                target: reloadingAnimationImage       // 动画的目标是矩形
                from: 0
                to: 360
                duration: 1000  // 每次旋转的时间为1秒
                loops: Animation.Infinite  // 无限循环
                easing.type: Easing.Linear  // 线性缓动，持续不断的旋转
                running:  loadingAnimation.visible === true
            }
        }
        
        // 朗读状态：音频均衡器动画
        Rectangle {
            id: readingStateContainer
            width: 16
            height: 16
            color: "transparent"
            radius: 4
            visible: readAloudButton.currentState === readAloudButton.stateReading
            
            Loader {
                id: voicePlayAnimationLoader
                sourceComponent: VoicePlayAnimation {
                    visible: true
                }
                anchors.centerIn: parent
                active: readingStateContainer.visible
            }
        }
        
    }
    
    // 鼠标区域
    MouseArea {
        id: readAloudButtonMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        
        onClicked: {
            // 根据当前状态执行不同的操作
            switch(readAloudButton.currentState) {
            case readAloudButton.stateIdle:
                // 空闲状态：点击后应该由外部处理开始朗读，这里只发送信号
                readAloudButton.sigStartRead()
                break
            case readAloudButton.stateSynthesizing:
                // 语音合成状态：处理取消合成请求
                readAloudButton.sigStopSynthesizing()
                break
            case readAloudButton.stateReading:
                // 朗读状态：点击暂停
                readAloudButton.sigPauseRead()
                break
            case readAloudButton.statePaused:
                // 暂停状态：点击继续
                readAloudButton.sigContinueRead()
                break
            }
        }
        
        onPressed: {
            // 按下时的视觉反馈已经在各个状态的容器中处理
        }
        
        onReleased: {
            // 释放时的视觉反馈已经在各个状态的容器中处理
        }
    }
}
