import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import ButtonState 1.0
import "."

Rectangle {
    id: titleBar

    property bool isSidebarCloseFlag: false
    property int pageNum: 1
    property int taskSessionCount: 0
    property bool isSettingSubpage: false
    property bool isShowIntroduction: false
    // 窗口是否失焦
    property bool isWindowActive: Window.window ? Window.window.active : true

    signal changeIsSidebarClose(bool isSidebarClose)
    signal newConversation()
    signal changeWindowMode()
    signal btnClick()
    signal showSetting()
    color: "#FFFFFF"

    RowLayout {
        anchors.fill: parent
        spacing: 2
        Layout.alignment: Qt.AlignVCenter

        // 左边距
        Rectangle {
            Layout.preferredHeight: 40
            Layout.preferredWidth: isSidebarCloseFlag ? 56 : 36
            color: isSidebarCloseFlag ? "#F5F5F5" : "#FFFFFF"
            enabled: !isShowIntroduction
            Row{
                topPadding: 4
                leftPadding: isSidebarCloseFlag ? 12 : 4
                bottomPadding: 4
                //侧边栏按钮
                Button {
                    width: 32
                    height: 32
                    opacity: isWindowActive ? 1.0 : 0.4
                    focusPolicy: Qt.NoFocus
                    flat: true


                    property string buttonType: "windowButton"

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/icon/icon/window.svg"
                    }

                    onClicked: {
                        changeIsSidebarClose(!isSidebarCloseFlag)
                    }

                    ToolTip {
                        visible: parent.hovered
                        delay: Qt.styleHints.mousePressAndHoldInterval
                        text: {
                            if (isSidebarCloseFlag) {
                                return qsTr("Close sidebar")
                            } else {
                                return qsTr("Open sidebar")
                            }
                        }
                    }
                }
            }
        }

        //新建会话按钮
        Button{
            id: newConversationButton
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            flat: true
            opacity: isTaskLimitReached ? 0.4 : (isWindowActive ? 1.0 : 0.4)
            focusPolicy: Qt.NoFocus

            visible: pageNum === 1

            property string buttonType: "windowButton"
            property bool isTaskLimitReached: pageNum === 1 && ButtonState.isTaskMode && taskSessionCount >= 5
            property string taskLimitReachedText: qsTr("Current task limit reached (5/5), please end or wait for task completion before trying")
            property string newConversationText: qsTr("New conversation")
            property string tooltipText: {
                if(isTaskLimitReached){
                    return taskLimitReachedText
                }else if(pageNum === 1){
                    return newConversationText
                }else{
                    return ""
                }
            }

            Image {
                anchors.centerIn: parent
                source: "qrc:/icon/icon/new-chat.svg"
            }

            onHoveredChanged: {
                if (hovered) {
                    newConversationToolTip.visible = true
                } else {
                    newConversationToolTip.visible = false
                }
            }

            onClicked: {
                if(isTaskLimitReached){
                    return
                }
                newConversation()
            }

            ToolTip {
                id: newConversationToolTip
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: false
                text: newConversationButton.tooltipText
            }

        }

        //语音通话按钮
        Button{
            id: voiceChatButton
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            flat: true
            opacity: isWindowActive ? 1.0 : 0.4

            visible: pageNum === 1

            Image {
                anchors.centerIn: parent
                source: "qrc:/icon/icon/call.svg"
            }

            ToolTip {
                text: qsTr("Voice chat")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }
            onClicked: {
                msgPane.startVoiceChat()
            }
        }

        //返回按钮
        Rectangle{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            opacity: isWindowActive ? 1.0 : 0.4
            color: "transparent"

            property string buttonType: "windowButton"

            Image {
                anchors.centerIn: parent
                source: "qrc:/icon/icon/back.svg"
            }

            MouseArea {
                id: backButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    btnClick()
                }
            }

            ToolTip {
                visible: backButtonMouseArea.containsMouse
                delay: Qt.styleHints.mousePressAndHoldInterval
                text: qsTr("Back")
            }

            visible: root.winMode === 2 && isSettingSubpage && root.currentPageNum === 7
        }

        // 弹簧
        Rectangle {
            Layout.fillWidth: true
            height: 40

            // 鼠标拖动区域
            MouseArea {
                anchors.fill: parent
                enabled: windowCtrl.winMode === 3

                onDoubleClicked: {
                    if (root.visibility === Window.Maximized) {
                        root.visibility = Window.Windowed
                    } else {
                        root.visibility = Window.Maximized
                    }
                }
            }
        }

        //置顶按钮
        Button{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            opacity: isWindowActive ? 1.0 : 0.4
            focusPolicy: Qt.NoFocus
            flat: true

            property string buttonType: "windowButton"

            ToolTip {
                text: windowCtrl.isTopping ? qsTr("Unpin") : qsTr("Pin to top")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }
            icon{
                name:windowCtrl.isTopping ? "ukui-unfixed-symbolic" : "ukui-fixed-symbolic"
            }
            onClicked: {
                var currentState = windowCtrl.isTopping
                var newToppingState = !currentState
                
                console.log("TitleBar: Clicked pin button, current:", currentState, "new:", newToppingState)
                
                // 直接调用 X11 API 设置置顶状态
                if (x11WindowHelper && x11WindowHelper.isX11Environment()) {
                    console.log("TitleBar: Using X11 API")
                    var success = x11WindowHelper.setWindowAlwaysOnTop(root, newToppingState)
                    console.log("TitleBar: X11 API result:", success)
                    if (success) {
                        windowCtrl.isTopping = newToppingState
                    }
                } else {
                    console.log("TitleBar: Not X11 environment, using Qt API")
                    windowCtrl.isTopping = newToppingState
                }
                
                // 同步更新设置页的 Switch 状态
                if (typeof msgPane !== "undefined" && msgPane !== null && typeof msgPane.setWindowAlwaysOnTop === "function") {
                    msgPane.setWindowAlwaysOnTop(newToppingState)
                }
            }
        }

        //选项按钮
        Button{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            opacity: isWindowActive ? 1.0 : 0.4
            enabled: !isShowIntroduction
            focusPolicy: Qt.NoFocus
            flat: true

            property string buttonType: "windowButton"

            ToolTip {
                text: qsTr("Option")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }
            icon{
                name:"open-menu-symbolic"
            }
            onClicked: tittleMenu.visible = true

            MyMenu {
                id: tittleMenu
                y: 40
                visible: false

                MenuItem {
                    id: switchSidebarMenuItem
                    text: qsTr("Switch sidebar")
                    enabled: pageNum !== 3
                    visible: windowCtrl.winMode === 3
                    hoverEnabled: true                   

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        id: switchSidebarLabel
                        text: parent.text
                        elide: Text.ElideRight
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        if (root.visibility === Window.Maximized){

                            root.visibility = Window.Windowed
                        }

                        Qt.callLater(function() {
                            windowCtrl.winMode = 2
                            changeWindowMode()
                        })
                    }

                    ToolTip {
                        id: switchSidebarToolTip
                        visible: switchSidebarMenuItem.hovered && switchSidebarLabel.truncated
                        text: parent.text
                        delay: Qt.styleHints.mousePressAndHoldInterval
                        property var wrapMode: Text.NoWrap
                    }
                }

                MenuItem {
                    id: switchWindowMenuItem
                    text: qsTr("Switch window")
                    visible: windowCtrl.winMode === 2
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        id: switchWindowLabel
                        text: parent.text
                        elide: Text.ElideRight
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        windowCtrl.winMode = 3
                        changeWindowMode()
                    }

                    ToolTip {
                        id: switchWindowToolTip
                        visible: switchWindowMenuItem.hovered && switchWindowLabel.truncated
                        text: parent.text
                        delay: Qt.styleHints.mousePressAndHoldInterval
                        property var wrapMode: Text.NoWrap
                    }
                }

                MenuItem {
                    text: qsTr("Setting")
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        tittleMenu.visible = false
                        showSetting()
                    }
                }

                MenuItem {
                    text: qsTr("Help")
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        tittleMenu.visible = false
                        if (typeof msgPane !== "undefined" && msgPane !== null) {
                            msgPane.showHelp()
                        }
                    }
                }

                MenuItem {
                    text: qsTr("About")
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        tittleMenu.visible = false
                        if (typeof msgPane !== "undefined" && msgPane !== null) {
                            msgPane.showAbout()
                        }
                    }
                }

                MenuItem {
                    text: qsTr("Feedback")
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        tittleMenu.visible = false
                        if (typeof msgPane !== "undefined" && msgPane !== null) {
                            msgPane.showFeedBack()
                        }
                    }
                }

                MenuItem {
                    text: qsTr("Quit")
                    hoverEnabled: true

                    background: Rectangle {
                        color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                        radius: 6
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                    }

                    onClicked: {
                        console.log("[SHUTDOWN] TitleBar Quit clicked")
                        msgPane.stopVoiceChat()
                        Qt.quit()
                    }
                }
            }

        }

        //最小化按钮
        Button{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            opacity: isWindowActive ? 1.0 : 0.4
            focusPolicy: Qt.NoFocus
            flat: true

            property string buttonType: "windowButton"
            visible: windowCtrl.winMode === 3
            ToolTip {
                text: qsTr("Minimize")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }
            icon{
                name:"window-minimize-symbolic"
            }
            onClicked: {
                root.visibility = Window.Minimized
            }
        }

        //最大化按钮
        Button{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            opacity: isWindowActive ? 1.0 : 0.4
            focusPolicy: Qt.NoFocus
            flat: true

            property string buttonType: "windowButton"
            property bool isMaximized: root.visibility === Window.Maximized
            icon{
                name: isMaximized ? "window-restore-symbolic" : "window-maximize-symbolic"
            }
            visible: windowCtrl.winMode === 3

            ToolTip {
                text: parent.isMaximized ? qsTr("Restore") : qsTr("Maximize")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }

            onClicked: {
                if (isMaximized) {
                    root.visibility = Window.Windowed
                } else {
                    root.visibility = Window.Maximized
                }
            }
            onHoveredChanged: {
                if (hovered) {
                    // 创建一个矩形区域，使用按钮在窗口中的位置和大小
                    var buttonRect = Qt.rect(x, y, width, height)
                    windowCtrl.showTileFlyout(buttonRect)
                } else {
                    windowCtrl.hideTileFlyout()
                }
            }
        }

        //关闭按钮
        Button{
            Layout.preferredHeight:32
            Layout.preferredWidth:32
            Layout.minimumHeight:32
            Layout.minimumWidth:32
            Layout.maximumHeight:32
            Layout.maximumWidth:32
            focusPolicy: Qt.NoFocus
            flat: true

            property string buttonType: "windowTitleBarCloseButton"

            ToolTip {
                text: qsTr("Close")
                delay: Qt.styleHints.mousePressAndHoldInterval
                visible: parent.hovered
            }

            icon{
                name:"window-close-symbolic"
            }
            onClicked: {
                root.close()
            }
        }

        // 右边距
        Rectangle {
            width: 4
        }
    }
}
