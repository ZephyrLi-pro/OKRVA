import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."

Window {
    id: selectMenuWindow
    visible: false
    width: 180
    height: menuPanel.implicitHeight
    color: "transparent"

    readonly property int menuPaddingTop: 6
    readonly property int menuPaddingBottom: 10
    readonly property int menuPaddingH: 6
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    opacity: 1.0
    // 绑定ConfigManager的新手引导完成状态属性
    property bool isGuideCompleted: configManager.isGuideCompleted
    // 未完成新手引导或引导界面显示时，右键菜单选项需要置灰不可点击
    property bool isEnabled: isGuideCompleted && !IntroductionViewState.isIntroductionVisible
    // true 表示按Esc触发关闭
    property bool closingByEsc: false
    
    // 监听ConfigManager的新手引导完成状态变化
    Connections {
        target: configManager
        onIsGuideCompletedChanged: function() {
            // 强制更新属性绑定
            isGuideCompleted = Qt.binding(function() { return configManager.isGuideCompleted })
        }
    }


    signal sigCloseSelectWindow()
    signal sigMenuClosedByEsc()


    Rectangle {
        id: menuPanel
        width: parent.width
        height: implicitHeight
        implicitHeight: menuColumn.height + selectMenuWindow.menuPaddingTop + selectMenuWindow.menuPaddingBottom
        color: "white"
        radius: 8
        focus: true
        border.width: 1
        border.color: "#d9d9d9"

        // Esc关闭当前菜单，但不直接关闭悬浮条
        Keys.onPressed: {
            console.log("****键盘事件****")
            if (event.key === Qt.Key_Escape) {
                console.log("****Escape键被按下****")
                selectMenuWindow.closingByEsc = true
                selectMenuWindow.close()
                event.accepted = true
            }
        }

        Column {
            id: menuColumn
            width: parent.width - selectMenuWindow.menuPaddingH * 2
            x: selectMenuWindow.menuPaddingH
            y: selectMenuWindow.menuPaddingTop
            spacing: 2

            // 发送到小K
            Rectangle {
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                enabled: selectMenuWindow.isEnabled
                Label {
                    text: qsTr("Send to Qi Ling")
                    color: selectMenuWindow.isEnabled ? Qt.rgba(0,0,0,0.85) : Qt.rgba(0,0,0,0.35)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: parent.enabled
                    onClicked: {
                        if (!parent.enabled) {
                            return
                        }
                        selectMenuWindow.close()
                        sendToAiAssistant()
                    }
                    onEntered: {
                        if (parent.enabled) {
                            parent.color = "#f0f0f0"
                        }
                    }
                    onExited: {
                        parent.color = "transparent"
                    }
                    onPressed: {
                        if (parent.enabled) {
                            parent.color = "#e0e0e0"
                        }
                    }
                    onReleased: {
                        if (parent.enabled && parent.containsMouse) {
                            parent.color = "#f0f0f0"
                        }
                    }
                }
            }

            // 小K首页
            Rectangle {
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                Label {
                    text: qsTr("Qi Ling HomePage")
                    color: Qt.rgba(0,0,0,0.85)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        selectMenuWindow.close()
                        showMainView()
                    }
                    onEntered: parent.color = "#f0f0f0"
                    onExited: parent.color = "transparent"
                    onPressed: parent.color = "#e0e0e0"
                    onReleased: parent.containsMouse ? parent.color = "#f0f0f0" : parent.color = "transparent"
                }
            }

            // 分割条
            Rectangle {
                width: parent.width - 8
                height: 1
                color: Qt.rgba(0, 0, 0, 0.05)
            }

            // 在该应用内禁用
            Rectangle {
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                enabled: false
                visible: false
                id: curAppDisabledRect
                Label {
                    text: qsTr("Disable in this application")
                    color: curAppDisabledRect.enabled  ?  Qt.rgba(0,0,0,0.85) : Qt.rgba(0,0,0,0.55)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: selectMenuWindow.close()
                    onEntered: parent.color = "#f0f0f0"
                    onExited: parent.color = "transparent"
                    onPressed: parent.color = "#e0e0e0"
                    onReleased: parent.containsMouse ? parent.color = "#f0f0f0" : parent.color = "transparent"
                }
            }

            // 全局禁用划词悬浮条
            Rectangle {
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                enabled: true
                id: allDisabledRect
                Label {
                    text: qsTr("Disable globally")
                    color:  Qt.rgba(0,0,0,0.85)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        selectMenuWindow.close()
                        disabledSelectFunction()
                    }
                    onEntered: parent.color = "#f0f0f0"
                    onExited: parent.color = "transparent"
                    onPressed: parent.color = "#e0e0e0"
                    onReleased: parent.containsMouse ? parent.color = "#f0f0f0" : parent.color = "transparent"
                }
            }

            // 分割条
            Rectangle {
                width: parent.width - 8
                height: 1
                color: Qt.rgba(0, 0, 0, 0.05)
            }

            // 设置
            Rectangle {
                id: settingRect
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                enabled: selectMenuWindow.isEnabled
                Label {
                    text: qsTr("Setting")
                    color: selectMenuWindow.isEnabled ? Qt.rgba(0,0,0,0.85) : Qt.rgba(0,0,0,0.35)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: parent.enabled
                    onClicked: {
                        if (!parent.enabled) {
                            return
                        }
                        selectMenuWindow.close()
                        showSettingPage()
                    }
                    onEntered: {
                        if (parent.enabled) {
                            parent.color = "#f0f0f0"
                        }
                    }
                    onExited: {
                        parent.color = "transparent"
                    }
                    onPressed: {
                        if (parent.enabled) {
                            parent.color = "#e0e0e0"
                        }
                    }
                    onReleased: {
                        if (parent.enabled && parent.containsMouse) {
                            parent.color = "#f0f0f0"
                        }
                    }
                }
            }

            // 关闭
            Rectangle {
                width: parent.width
                height: 36
                color: "transparent"
                radius: 4
                Label {
                    text: qsTr("Close")
                    color: Qt.rgba(0,0,0,0.85)
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        selectMenuWindow.close()
                        sigCloseSelectWindow()
                    }
                    onEntered: parent.color = "#f0f0f0"
                    onExited: parent.color = "transparent"
                    onPressed: parent.color = "#e0e0e0"
                    onReleased: parent.containsMouse ? parent.color = "#f0f0f0" : parent.color = "transparent"
                }
            }
        }
    }

    onVisibleChanged: {
        if (!visible) {
            if (closingByEsc) {
                sigMenuClosedByEsc()
            } else {
                sigCloseSelectWindow()
            }

            closingByEsc = false
        }
    }

    // function
    // 发送到Ai助手
    function  sendToAiAssistant() {
       // 划词进入助手时切换到快问快答主页
       msgPane.enterFromWordSelection()
       // 如果语音输入正在进行，先停止语音输入
       msgPane.stopVoiceInputIfActive()
       // 获取当前剪切板内容
       let msg = selectControl.getCurSelectMsg()
       console.log("当前待发送剪切板内容:",msg)
        let jsonObject = {
            msgHead: "",
            promptId: "",
            selectPrompt: "",
            promptIconPath: "",
            isFromSelection: true
        };
        let jsonMsgHead = JSON.stringify(jsonObject)
        msgPane.onStartReplyingToChat()
        msgPane.onSendMsg(jsonMsgHead, msg)
        msgPane.moveScrollBar()
        windowCtrl.showMainWindow()
        // 确保从历史/知识库/设置等页面切回问答页
        msgPane.showWindowPage(1)
        sigCloseSelectWindow()
    }

    // 显示助手首页
    function showMainView() {
        // 划词进入助手时切换到快问快答主页
        msgPane.enterFromWordSelection()
        windowCtrl.showMainWindow()
        // 首次安装未完成新手引导时，跳转到新手引导页
        if (!configManager.isGuideCompleted) {
            msgPane.showWindowPage(0)
        } else {
            // 无论当前在知识库/历史/设置等哪个页面，都切回问答首页
            msgPane.showWindowPage(1)
        }
        sigCloseSelectWindow()
    }

    // 全局禁用划词功能
    function disabledSelectFunction() {
        selectControl.disableScribe()
        sigCloseSelectWindow()
    }

    // 显示设置界面
    function showSettingPage() {
        windowCtrl.showMainWindow() //显示主页
        msgPane.sigShowSettingsPage()
        sigCloseSelectWindow()
    }
}
