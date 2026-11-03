import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3
import ButtonState 1.0
import "."

// 设置页面常规内容显示区域
Rectangle {
    id: settingNormalPageRet
    color: "#FFFFFF"

    property string fontFamily: Qt.application.font.family

    // 判断当前是否为侧边栏模式
    property bool isSidebarMode: {
        if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
            return windowCtrl.winMode === 2
        }
        return false
    }

    Component.onCompleted: {
        // 初始化窗口模式状态，优先使用 windowCtrl.winMode 确保与窗口实际状态一致
        sidebarModeRadio.isUpdating = true
        windowModeRadio.isUpdating = true
        if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
            let currentWinMode = windowCtrl.winMode
            if (currentWinMode === 2) {
                sidebarModeRadio.checked = true
                windowModeRadio.checked = false
            } else if (currentWinMode === 3) {
                sidebarModeRadio.checked = false
                windowModeRadio.checked = true
            }
        } else if (globals) {
            let currentMode = globals.getCurrentWindowMode()
            sidebarModeRadio.checked = (currentMode === "sidebar")
            windowModeRadio.checked = (currentMode === "window")
        } else {
            // 如果没有globals，使用msgPane获取
            let currentMode = msgPane.getCurWinMode()
            if (currentMode === 2) {
                sidebarModeRadio.checked = true
                windowModeRadio.checked = false
            } else if (currentMode === 3) {
                sidebarModeRadio.checked = false
                windowModeRadio.checked = true
            }
        }
        sidebarModeRadio.isUpdating = false
        windowModeRadio.isUpdating = false

        // 初始化窗口置顶状态
        if (globals) {
            var isTopping = globals.getWindowAlwaysOnTop()
            windowAlwaysOnTopSwitch.checked = isTopping
        } else {
            // 如果没有globals，使用默认值
            windowAlwaysOnTopSwitch.checked = false
        }

        // 初始化关闭窗口时设置
        if (globals) {
            var isHideToTray = globals.getCloseWindowMode()
            hideWindowRadio.checked = isHideToTray
            exitDirectlyRadio.checked = !isHideToTray
        } else {
            // 如果没有globals，使用默认值
            hideWindowRadio.checked = true
            exitDirectlyRadio.checked = false
        }

        // 初始化默认保存路径
        if (globals) {
            var savePath = globals.getDefaultSavePath()
            if (savePath) {
                defaultSavePathField.text = savePath
            }
        }

        // 初始化追问模式状态
//        if (globals) {
//            var isQuestionModeEnabled = globals.getQuestionModeEnabled()
//            questionModeSwitch.checked = isQuestionModeEnabled
//        } else {
//            // 如果没有globals，使用默认值
//            questionModeSwitch.checked = true
//        }

        // 初始化长按空格语音输入状态
        if (globals) {
            var isSpaceVoiceInputEnabled = globals.getSpaceVoiceInputEnabled()
            spaceVoiceInputSwitch.checked = isSpaceVoiceInputEnabled
        } else {
            // 如果没有globals，使用默认值
            spaceVoiceInputSwitch.checked = true
        }

        // 初始化语音回拨状态
        if (globals) {
            var isVoiceCallbackEnabled = globals.getVoiceCallbackEnabled()
            voiceCallbackSwitch.checked = isVoiceCallbackEnabled
        } else {
            voiceCallbackSwitch.checked = true
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 4
        clip: true
        // 侧边栏模式下隐藏滚动条，窗口模式下按需显示
        ScrollBar.vertical.policy: scrollView.contentHeight > scrollView.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        // 设置平滑滚动行为 (Qt Quick Controls 2 使用 contentItem 访问内部 Flickable)
        Component.onCompleted: {
            if (contentItem) {
                contentItem.boundsBehavior = Flickable.StopAtBounds
            }
        }

        ColumnLayout {
            id: contentColumn
            width: scrollView.width - 14
            spacing: 4

            // 子系统版本过低提示区域
            Rectangle {
                id: subsystemVersionLowTipRet
                Layout.fillWidth: true
                Layout.bottomMargin: 40
                implicitHeight: subsystemVersionLowTipRow.implicitHeight + 16
                visible: globals.isSubsystemVersionTooLow
                color: Qt.rgba(255/255, 125/255, 0/255, 0.1)
                radius: 8
                Row {
                    id: subsystemVersionLowTipRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 8
                    rightPadding: 8
                    Image {
                        id: subsystemVersionLowTipImage
                        width: 16
                        height: 16
                        source: "qrc:/icon/icon/yellow-warning.svg"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        id: subsystemVersionLowTipLabel
                        width: subsystemVersionLowTipRow.width - subsystemVersionLowTipImage.width - goNowRet.width -
                               subsystemVersionLowTipRow.leftPadding - subsystemVersionLowTipRow.rightPadding
                        leftPadding: 8
                        rightPadding: 8
                        wrapMode: Text.WordWrap
                        text: qsTr("The AI subsystem version is too old, and Qi Ling functions are restricted. Please upgrade the system in Settings > Update to unlock the full Qi Ling features")
                        color: "#FF7D00"
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pointSize: globals.fontsize
                        anchors.verticalCenter: parent.verticalCenter
                        ToolTip {
                            id: subsystemVersionLowTipLabelTooltip
                            visible: subsystemVersionLowTipLabelMouseArea.containsMouse && subsystemVersionLowTipLabelMertics.lineCount > 2
                            delay: Qt.styleHints.mousePressAndHoldInterval
                            text: qsTr("The AI subsystem version is too old, and Qi Ling functions are restricted. Please upgrade the system in Settings > Update to unlock the full Qi Ling features")
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                        MouseArea {
                            id: subsystemVersionLowTipLabelMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Label {
                        id: subsystemVersionLowTipLabelMertics
                        width: subsystemVersionLowTipLabel.width
                        leftPadding: 8
                        rightPadding: 8
                        wrapMode: Text.WordWrap
                        font.pointSize: globals.fontsize
                        text: qsTr("The AI subsystem version is too old, and Qi Ling functions are restricted. Please upgrade the system in Settings > Update to unlock the full Qi Ling features")
                        visible: false
                        maximumLineCount: 5
                    }

                    Rectangle {
                        id: goNowRet
                        width: 88
                        height: 36
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#FF7D00"
                        radius: 8
                        Label {
                            id: goNowLabel
                            anchors.fill: parent
                            anchors.topMargin: 1
                            anchors.bottomMargin: 1
                            width: 88
                            text: qsTr("Go Now")
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            id: goNowRetMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                goNowRet.color = "#F27700"
                            }
                            onExited: {
                                goNowRet.color = "#FF7D00"
                            }
                            onPressed: {
                                goNowRet.color = "#CC6400"
                            }
                            onReleased: {
                                goNowRet.color = "#FF7D00"
                            }
                            onClicked: {
                                msgPane.openUkuiUpgrade()
                            }
                        }
                    }
                }
            }

            // 模型设置标题
            Label {
                id: modelTitleLabel
                text: qsTr("Model")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 模型设置组
            Rectangle {
                id: modelGroup
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#F6F6F6"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 8

                    Label {
                        id: modelLabel
                        text: qsTr("Model configuration")
                        color: "#333333"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        id: modelSettingButton
                        Layout.preferredWidth: 88
                        Layout.minimumWidth: 64
                        Layout.preferredHeight: 36
                        radius: 6
                        border.color: "transparent"
                        border.width: 1
                        color: "#E6E6E6"

                        Label {
                            id: modelSettingButtonText
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            text: qsTr("Go to Configure")
                            color: "#333333"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        ToolTip {
                            id: modelSettingButtonTooltip
                            text: qsTr("Go to Configure")
                            visible: modelSettingButtonMouseArea.containsMouse && modelSettingButtonText.truncated
                            delay: 500
                        }

                        MouseArea {
                            id: modelSettingButtonMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            onEntered: {
                                modelSettingButton.color = "#DCDCDC"
                            }
                            onExited: {
                                modelSettingButton.color = "#E6E6E6"
                            }
                            onPressed: {
                                modelSettingButton.color = "#B9B9B9"
                            }
                            onReleased: {
                                modelSettingButton.color = "#DCDCDC"
                            }
                            onClicked: {
                                // 调用C++方法打开设置中心
                                msgPane.openUkuiControlCenter()
                            }
                        }
                    }
                }
            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 窗口设置标题
            Label {
                id: windowTitleLabel
                text: qsTr("Window")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 窗口模式与窗口置顶设置组（参考全局划词总结框与解释框的1px间距）
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                // 窗口模式设置组（上圆角）
                Rectangle {
                    id: windowModeGroup
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                   // Layout.preferredHeight: Math.max(60, windowModeLabel.implicitHeight + 24)
                    color: "#F6F6F6"
                    radius: 6
                    clip: true

                    // 覆盖下圆角，只保留上圆角
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 6
                        color: "#F6F6F6"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 12
                        anchors.bottomMargin: 12
                        spacing: 8
                        z: 1

                        Label {
                            id: windowModeLabel
                            text: qsTr("Window mode")
                            color: "#333333"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 50
                            Layout.rightMargin: 16
                            wrapMode: Text.NoWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight

                            MouseArea {
                                id: windowModeLabelMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            ToolTip {
                                id: windowModeLabelTooltip
                                visible: windowModeLabelMouseArea.containsMouse && windowModeLabel.truncated
                                text: parent.text
                                delay: Qt.styleHints.mousePressAndHoldInterval
                                property var wrapMode: Text.NoWrap
                            }
                        }

                        RadioButton {
                            id: sidebarModeRadio
                            text: qsTr("Sidebar mode")
                            Layout.maximumWidth: isSidebarMode ? 96 : -1
                            property bool isUpdating: false
                            // 自定义指示器，使用主题色
                            indicator: Rectangle {
                                x: root.leftPadding
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: width
                                antialiasing: true
                                radius: (width/2)
                                border.width: sidebarModeRadio.checked ? width*0.25 : 1
                                border.color: sidebarModeRadio.checked ? "#3790FA" : "#CCCCCC"
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width*0.5; height: width
                                    antialiasing: true
                                    radius: (width/2)
                                    color: root.checkedColor
                                    visible: root.checked
                                }
                            }
                            contentItem: Label {
                                id: sidebarModeRadioLabel
                                text: sidebarModeRadio.text
                                color: "#333333"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: sidebarModeRadio.indicator.width + sidebarModeRadio.spacing
                                elide: Text.ElideRight

                                ToolTip {
                                    id: sidebarModeRadioTooltip
                                    text: sidebarModeRadio.text
                                    visible: sidebarModeRadioMouseArea.containsMouse && sidebarModeRadioLabel.truncated
                                }

                            }
                            MouseArea {
                                id: sidebarModeRadioMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                            onCheckedChanged: {
                                // 防止循环触发和信号触发
                                if (!isUpdating && checked && !windowModeRadio.checked) {
                                    // 从窗口模式切换到侧边栏模式时，先保存窗口模式下的设置子页面号
                                    if (typeof root !== "undefined" && root !== null && root.settingShowPageRef && root.settingShowPageRef.currentPageNum > 0) {
                                        root.lastSettingSubPageNum = root.settingShowPageRef.currentPageNum
                                    }
                                    // 与 TitleBar 菜单"切换至侧边栏模式"逻辑一致：Qt.callLater 内设置模式并触发页面切换
                                    if (typeof windowCtrl !== "undefined" && windowCtrl !== null && typeof root !== "undefined" && root !== null) {
                                        Qt.callLater(function() {
                                            windowCtrl.winMode = 2
                                            root.resetAgentTabToChatIfSidebar()
                                            var targetPage = root.currentPageNum
                                            if (ButtonState.isTaskMode && root.mychatRef
                                                    && root.mychatRef.currentTaskTabIndex !== 0) {
                                                targetPage = 1
                                            }
                                            root.onChangeWindowPage(targetPage)
                                            if (globals) {
                                                globals.switchToSideMode()
                                            }
                                        })
                                    }
                                }
                            }
                        }

                        RadioButton {
                            id: windowModeRadio
                            text: qsTr("Window mode")
                            Layout.maximumWidth: isSidebarMode ? 96 : -1
                            property bool isUpdating: false
                            // 自定义指示器，使用主题色
                            indicator: Rectangle {
                                x: root.leftPadding
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: width
                                antialiasing: true
                                radius: (width/2)
                                border.width: windowModeRadio.checked ? width*0.25 : 1
                                border.color: windowModeRadio.checked ? "#3790FA" : "#CCCCCC"
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width*0.5; height: width
                                    antialiasing: true
                                    radius: (width/2)
                                    color: root.checkedColor
                                    visible: root.checked
                                }
                            }
                            contentItem: Label {
                                id: windowModeRadioLabel
                                text: windowModeRadio.text
                                color: "#333333"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: windowModeRadio.indicator.width + windowModeRadio.spacing
                                elide: Text.ElideRight

                                ToolTip {
                                    id: windowModeRadioTooltip
                                    text: windowModeRadio.text
                                    visible: windowModeRadioMouseArea.containsMouse && windowModeRadioLabel.truncated
                                }
                            }
                            MouseArea {
                                id: windowModeRadioMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                            onCheckedChanged: {
                                // 防止循环触发和信号触发
                                if (!isUpdating && checked && !sidebarModeRadio.checked) {
                                    // 与 TitleBar 菜单"切换至窗口模式"逻辑一致：同步设置模式并触发页面切换
                                    if (typeof windowCtrl !== "undefined" && windowCtrl !== null && typeof root !== "undefined" && root !== null) {
                                        windowCtrl.winMode = 3
                                        root.onChangeWindowPage(root.currentPageNum)
                                        if (globals) {
                                            globals.switchToWindowMode()
                                        }
                                    }
                                }
                            }
                        }

                        // 监听 windowCtrl 的窗口模式改变信号，确保按钮状态与窗口实际状态一致
                        Connections {
                            id: windowCtrlConnections
                            target: null
                            onWinModeChanged: function(winMode) {
                                // 更新QML的RadioButton状态
                                sidebarModeRadio.isUpdating = true
                                windowModeRadio.isUpdating = true
                                if (winMode === 2) {
                                    sidebarModeRadio.checked = true
                                    windowModeRadio.checked = false
                                } else if (winMode === 3) {
                                    sidebarModeRadio.checked = false
                                    windowModeRadio.checked = true
                                }
                                sidebarModeRadio.isUpdating = false
                                windowModeRadio.isUpdating = false
                            }
                            Component.onCompleted: {
                                if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
                                    windowCtrlConnections.target = windowCtrl
                                }
                            }
                        }

                        // 监听Globals的窗口模式改变信号
                        Connections {
                            id: windowModeConnections
                            target: null
                            onSigWindowModeChanged: function(mode) {
                                // 更新QML的RadioButton状态
                                sidebarModeRadio.isUpdating = true
                                windowModeRadio.isUpdating = true
                                sidebarModeRadio.checked = (mode === "sidebar")
                                windowModeRadio.checked = (mode === "window")
                                sidebarModeRadio.isUpdating = false
                                windowModeRadio.isUpdating = false
                            }
                            Component.onCompleted: {
                                if (typeof globals !== "undefined" && globals !== null) {
                                    windowModeConnections.target = globals
                                }
                            }
                        }
                    }
                }

                // 窗口置顶设置组（下圆角）
                Rectangle {
                    id: windowAlwaysOnTopGroup
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: "#F6F6F6"
                    radius: 6
                    clip: true

                    // 覆盖上圆角，只保留下圆角
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 6
                        color: "#F6F6F6"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 12
                        anchors.bottomMargin: 12
                        spacing: 24
                        z: 1

                        Label {
                            id: windowAlwaysOnTopLabel
                            text: qsTr("The window is placed on top")
                            color: "#333333"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 60
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Switch {
                            id: windowAlwaysOnTopSwitch
                            checked: true
                            property bool isUpdating: false
                            // 自定义指示器，使用主题色
                            indicator: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                implicitWidth: 50
                                implicitHeight: 24
                                radius: 12
                                color: windowAlwaysOnTopSwitch.checked ? "#3790FA" : "#CCCCCC"
                                Rectangle {
                                    width: 16
                                    height: 16
                                    radius: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: windowAlwaysOnTopSwitch.checked ? parent.width - width - 2 : 2
                                    color: "#FFFFFF"
                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                            onCheckedChanged: {
                                // 防止循环触发
                                if (!isUpdating) {
                                    // 设置窗口置顶
                                    if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
                                        windowCtrl.isTopping = checked
                                    }
                                }
                            }
                        }

                        // 监听windowCtrl的置顶状态改变信号，确保标题栏按钮状态变化时同步
                        Connections {
                            id: windowCtrlToppingConnections
                            target: null
                            onIsToppingChanged: function() {
                                // 更新QML的Switch状态
                                if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
                                    windowAlwaysOnTopSwitch.isUpdating = true
                                    windowAlwaysOnTopSwitch.checked = windowCtrl.isTopping
                                    windowAlwaysOnTopSwitch.isUpdating = false
                                }
                            }
                            Component.onCompleted: {
                                if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
                                    windowCtrlToppingConnections.target = windowCtrl
                                }
                            }
                        }
                    }
                }
            }  // ColumnLayout 窗口模式与窗口置顶

            // 间距（窗口置顶和关闭窗口时距离8px）
            Item {
                Layout.preferredHeight: 8
            }

            // 关闭窗口设置组
            Rectangle {
                id: closeWindowGroup
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#F6F6F6"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 4

                    Label {
                        id: closeWindowLabel
                        text: qsTr("When the window is closed")
                        color: "#333333"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 42
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.preferredWidth: 24
                        Layout.minimumWidth: 24
                    }

                    RadioButton {
                        id: hideWindowRadio
                        text: qsTr("Hide the window")
                        Layout.maximumWidth: isSidebarMode ? 96 : -1
                        property bool isUpdating: false
                        // 自定义指示器，使用主题色
                        indicator: Rectangle {
                            x: root.leftPadding
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16; height: width
                            antialiasing: true
                            radius: (width/2)
                            border.width: hideWindowRadio.checked ? width*0.25 : 1
                            border.color: hideWindowRadio.checked ? "#3790FA" : "#CCCCCC"
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width*0.5; height: width
                                antialiasing: true
                                radius: (width/2)
                                color: root.checkedColor
                                visible: root.checked
                            }
                        }
                        contentItem: Label {
                            id: hideWindowRadioLabel
                            text: hideWindowRadio.text
                            color: "#333333"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: hideWindowRadio.indicator.width + hideWindowRadio.spacing
                            elide: Text.ElideRight

                            ToolTip {
                                id: hideWindowRadioTooltip
                                text: hideWindowRadioLabel.text
                                visible: hideWindowRadioMouseArea.containsMouse && hideWindowRadioLabel.truncated
                            }
                        }
                        MouseArea {
                            id: hideWindowRadioMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                        onCheckedChanged: {
                            // 防止循环触发和信号触发
                            if (!isUpdating && checked && !exitDirectlyRadio.checked && globals) {
                                // 设置关闭窗口时隐藏，使用Globals的接口
                                globals.setCloseWindowMode(true)
                            }
                        }
                    }

                    RadioButton {
                        id: exitDirectlyRadio
                        text: qsTr("Just quit")
                        Layout.maximumWidth: isSidebarMode ? 96 : -1
                        property bool isUpdating: false
                        // 自定义指示器，使用主题色
                        indicator: Rectangle {
                            x: root.leftPadding
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16; height: width
                            antialiasing: true
                            radius: (width/2)
                            border.width: exitDirectlyRadio.checked ? width*0.25 : 1
                            border.color: exitDirectlyRadio.checked ? "#3790FA" : "#CCCCCC"
                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width*0.5; height: width
                                antialiasing: true
                                radius: (width/2)
                                color: root.checkedColor
                                visible: root.checked
                            }
                        }
                        contentItem: Label {
                            id: exitDirectlyRadioLabel
                            text: exitDirectlyRadio.text
                            color: "#333333"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: exitDirectlyRadio.indicator.width + exitDirectlyRadio.spacing
                            elide: Text.ElideRight
                            
                            ToolTip {
                                id: exitDirectlyRadioTooltip
                                text: exitDirectlyRadioLabel.text
                                visible: exitDirectlyRadioMouseArea.containsMouse && exitDirectlyRadioLabel.truncated
                            }
                        }
                        MouseArea {
                            id: exitDirectlyRadioMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                        onCheckedChanged: {
                            // 防止循环触发和信号触发
                            if (!isUpdating && checked && !hideWindowRadio.checked && globals) {
                                // 设置关闭窗口时直接退出，使用Globals的接口
                                globals.setCloseWindowMode(false)
                            }
                        }
                    }

                    // 监听Globals的关闭窗口时设置改变信号
                    Connections {
                        id: closeWindowConnections
                        target: null
                        onSigCloseWindowModeChanged: function(isHideToTray) {
                            // 更新QML的RadioButton状态
                            hideWindowRadio.isUpdating = true
                            exitDirectlyRadio.isUpdating = true
                            hideWindowRadio.checked = isHideToTray
                            exitDirectlyRadio.checked = !isHideToTray
                            hideWindowRadio.isUpdating = false
                            exitDirectlyRadio.isUpdating = false
                        }
                        Component.onCompleted: {
                            if (typeof globals !== "undefined" && globals !== null) {
                                closeWindowConnections.target = globals
                            }
                        }
                    }
                }
            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 语音回拨标题
            Label {
                id: voiceCallbackTitleLabel
                text: qsTr("Voice Callback")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 语音回拨设置组
            Rectangle {
                id: voiceCallbackGroup
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: "#F6F6F6"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 24

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            id: voiceCallbackLabel
                            text: qsTr("Voice Callback")
                            color: "#333333"
                            font.pointSize: globals ? globals.fontsize : 10
                            font.family: root.fontFamily
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: voiceCallbackDescLabel.implicitHeight

                            Label {
                                id: voiceCallbackDescLabel
                                anchors.fill: parent
                                text: qsTr("After voice chat task is completed, the app actively calls back to report the result")
                                color: Qt.rgba(0, 0, 0, 0.6)
                                font.pointSize: globals ? globals.fontsize : 10
                                font.family: root.fontFamily
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap

                                ToolTip.visible: voiceCallbackDescMouseArea.containsMouse && truncated
                                ToolTip.text: text
                                ToolTip.delay: 500
                            }

                            MouseArea {
                                id: voiceCallbackDescMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }

                    Switch {
                        id: voiceCallbackSwitch
                        checked: true
                        property bool isUpdating: false
                        // 自定义指示器，使用主题色
                        indicator: Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 50
                            implicitHeight: 24
                            radius: 12
                            color: voiceCallbackSwitch.checked ? "#3790FA" : "#CCCCCC"
                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                x: voiceCallbackSwitch.checked ? parent.width - width - 2 : 2
                                color: "#FFFFFF"
                                Behavior on x {
                                    NumberAnimation { duration: 150 }
                                }
                            }
                        }
                        onCheckedChanged: {
                            if (!isUpdating && globals) {
                                globals.setVoiceCallbackEnabled(checked)
                            }
                        }
                    }

                    Connections {
                        id: voiceCallbackConnections
                        target: globals
                        function onSigVoiceCallbackEnabledChanged(enabled) {
                            voiceCallbackSwitch.isUpdating = true
                            voiceCallbackSwitch.checked = enabled
                            voiceCallbackSwitch.isUpdating = false
                        }
                    }
                }
            }

            // 监听默认保存路径改变信号
            Connections {
                id: savePathConnections
                target: null
                onSigDefaultSavePathChanged: function(path) {
                    // 更新QML的路径显示
                    if (path) {
                        defaultSavePathField.text = path
                    }
                }
                Component.onCompleted: {
                    if (typeof globals !== "undefined" && globals !== null) {
                        savePathConnections.target = globals
                    }
                }
            }

            // 间距
//            Item {
//                Layout.preferredHeight: 24
//            }

            // 追问模式标题
//            Label {
//                id: questionModeTitleLabel
//                text: qsTr("Question Mode")
//                Layout.leftMargin: 16
//                Layout.topMargin: 0
//                Layout.bottomMargin: 0
//                color: "#333333"
//            }

            // 追问模式设置组
//            Rectangle {
//                id: questionModeGroup
//                Layout.fillWidth: true
//                Layout.preferredHeight: 60
//                color: "#F6F6F6"
//                radius: 6

//                RowLayout {
//                    anchors.fill: parent
//                    anchors.leftMargin: 16
//                    anchors.rightMargin: 16
//                    anchors.topMargin: 12
//                    anchors.bottomMargin: 12
//                    spacing: 8

//                    Label {
//                        id: questionModeLabel
//                        text: qsTr("Automatically Recommend Questions After Answer")
//                        color: "#333333"
//                        Layout.fillWidth: true
//                        Layout.minimumWidth: 60
//                        wrapMode: Text.WordWrap
//                        maximumLineCount: 2
//                        elide: Text.ElideRight
//                    }

//                    Switch {
//                        id: questionModeSwitch
//                        checked: true
//                        property bool isUpdating: false
//                        onCheckedChanged: {
//                            // 防止循环触发
//                            if (!isUpdating && globals) {
//                                // 设置追问模式
//                                globals.setQuestionModeEnabled(checked)
//                            }
//                        }
//                    }

//                    // 监听Globals的追问模式状态改变信号
//                    Connections {
//                        id: questionModeConnections
//                        target: globals
//                        onQuestionModeEnabledChanged: function(enabled) {
//                            // 更新QML的Switch状态
//                            questionModeSwitch.isUpdating = true
//                            questionModeSwitch.checked = enabled
//                            questionModeSwitch.isUpdating = false
//                        }
//                    }
//                }
//            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 文件保存标题
            Label {
                id: fileSaveTitleLabel
                text: qsTr("File Save")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 文件保存设置组
            Rectangle {
                id: fileSaveGroup
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#F6F6F6"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 8

                    Label {
                        id: fileSaveLabel
                        text: qsTr("Default save path")
                        color: "#333333"
//                        Layout.preferredWidth: 80
//                        Layout.minimumWidth: 50
//                        wrapMode: Text.WordWrap
//                        maximumLineCount: 2
//                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    TextField {
                        id: defaultSavePathField
                        Layout.fillWidth: true
                        Layout.preferredWidth: isSidebarMode ? 81 : 396
                        Layout.minimumWidth: 64
                        Layout.maximumWidth: isSidebarMode ? -1 : 396
                        Layout.preferredHeight: 36
                        text: "/home/kylin/下载"
                        font.pointSize: globals.fontsize
                        font.family: fontFamily
                        leftPadding: 8
                        rightPadding: 8
                        horizontalAlignment: TextInput.AlignLeft
                        readOnly: true
                        clip: true
                        onTextChanged: {
                            cursorPosition = 0
                        }
                        Component.onCompleted: {
                            cursorPosition = 0
                        }
                        onFocusChanged: {
                            cursorPosition = 0
                        }
                        background: Rectangle {
                            color: "#E6E6E6"
                            radius: 6
                            border.color: "transparent"
                            border.width: 1
                        }
                    }

                    Rectangle {
                        id: selectPathButton
                        Layout.preferredWidth: 81
                        Layout.minimumWidth: 64
                        Layout.preferredHeight: 36
                        radius: 6
                        border.color: "transparent"
                        border.width: 1
                        color: "#E6E6E6"

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            text: qsTr("Choose")
                            color: "#333333"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            onEntered: {
                                selectPathButton.color = "#DCDCDC"
                            }
                            onExited: {
                                selectPathButton.color = "#E6E6E6"
                            }
                            onPressed: {
                                selectPathButton.color = "#B9B9B9"
                            }
                            onReleased: {
                                selectPathButton.color = "#DCDCDC"
                            }
                            onClicked: {
                                selectSavePath()
                            }

                            // 选择保存路径的函数（使用 QML FileDialog）
                            function selectSavePath() {
                                if (globals) {
                                    var currentPath = globals.getDefaultSavePath()
                                    if (currentPath) {
                                        // 将本地路径转换为 file:// URL 格式
                                        var pathUrl = "file://" + currentPath
                                        selectSavePathDialog.folder = Qt.resolvedUrl(pathUrl)
                                    } else {
                                        // 默认使用用户主目录
                                        selectSavePathDialog.folder = shortcuts.home
                                    }
                                }
                                selectSavePathDialog.open()
                            }
                        }
                    }
                }
            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 辅助功能标题
            Label {
                id: accessibilityTitleLabel
                text: qsTr("Accessibility")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 长按空格语音输入设置组
            Rectangle {
                id: accessibilityGroup
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: "#F6F6F6"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 24

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            id: spaceVoiceInputLabel
                            text: qsTr("Hold space bar for voice input")
                            color: "#333333"
                            font.pointSize: globals ? globals.fontsize : 10
                            font.family: root.fontFamily
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: spaceVoiceInputDescLabel.implicitHeight

                            Label {
                                id: spaceVoiceInputDescLabel
                                anchors.fill: parent
                                text: qsTr("Hold down the space bar in the input box to activate voice input")
                                color: Qt.rgba(0, 0, 0, 0.35)
                                font.pointSize: globals ? globals.fontsize : 10
                                font.family: root.fontFamily
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap

                                ToolTip {
                                    visible: spaceVoiceInputDescMouseArea.containsMouse && spaceVoiceInputDescLabel.truncated
                                    text: spaceVoiceInputDescLabel.text
                                    delay: 500
                                }
                            }

                            MouseArea {
                                id: spaceVoiceInputDescMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }

                    Switch {
                        id: spaceVoiceInputSwitch
                        checked: true
                        property bool isUpdating: false
                        // 自定义指示器，使用主题色
                        indicator: Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 50
                            implicitHeight: 24
                            radius: 12
                            color: spaceVoiceInputSwitch.checked ? "#3790FA" : "#CCCCCC"
                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                x: spaceVoiceInputSwitch.checked ? parent.width - width - 2 : 2
                                color: "#FFFFFF"
                                Behavior on x {
                                    NumberAnimation { duration: 150 }
                                }
                            }
                        }
                        onCheckedChanged: {
                            if (!isUpdating && globals) {
                                globals.setSpaceVoiceInputEnabled(checked)
                            }
                        }
                    }

                    // 监听Globals的长按空格语音输入状态改变信号
                    Connections {
                        id: spaceVoiceInputConnections
                        target: globals
                        onSigSpaceVoiceInputEnabledChanged: function(enabled) {
                            // 更新QML的Switch状态
                            spaceVoiceInputSwitch.isUpdating = true
                            spaceVoiceInputSwitch.checked = enabled
                            spaceVoiceInputSwitch.isUpdating = false
                        }
                    }
                }
            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 检查更新按钮
            Rectangle {
                id: checkUpdateButton
                Layout.preferredWidth: 88
                Layout.minimumWidth: 64
                Layout.preferredHeight: 36
                radius: 6
                border.color: "transparent"
                border.width: 1
                color: "#E6E6E6"

                ToolTip {
                    visible: checkUpdateLabel.implicitWidth > 88 && checkUpdateButtonMouseArea.containsMouse
                    text: qsTr("Check for updates")
                    delay: 500
                }

                Label {
                    id: checkUpdateLabel
                    anchors.fill: parent
                    width: 88
                    anchors.topMargin: 1
                    anchors.bottomMargin: 1
                    text: qsTr("Check for updates")
                    color: "#333333"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: checkUpdateButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onEntered: {
                        checkUpdateButton.color = "#DCDCDC"
                    }
                    onExited: {
                        checkUpdateButton.color = "#E6E6E6"
                    }
                    onPressed: {
                        checkUpdateButton.color = "#B9B9B9"
                    }
                    onReleased: {
                        checkUpdateButton.color = "#DCDCDC"
                    }
                    onClicked: {
                        // 打开软件商店的详情页面
                        msgPane.openSoftwareCenter()
                    }
                }
            }

            // 底部间距24px
            Item {
                Layout.preferredHeight: 24
            }

            // 底部填充
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // 监听窗口模式变化，更新 isSidebarMode 属性
    Connections {
        id: settingPageWindowCtrlConnections
        target: (typeof windowCtrl !== "undefined" && windowCtrl !== null) ? windowCtrl : null
        onWinModeChanged: function(winMode) {
            settingNormalPageRet.isSidebarMode = (winMode === 2)
        }
    }

    // 下载路径选择的文件选择对话框（使用 QtQuick.Dialogs.FileDialog）
    FileDialog {
        id: selectSavePathDialog
        title: qsTr("Select save path")
        selectExisting: false
        selectFolder: true
        folder: shortcuts.home
        onAccepted: {
            var selectedPath = fileUrlToPath(selectSavePathDialog.fileUrl)
            if (selectedPath) {
                // 检查目录是否有写入权限
                if (globals && !globals.checkPathWritable(selectedPath)) {
                    // 弹出权限错误提示弹窗
                    showPermissionErrorDialog(function() {
                        // 重新打开文件选择对话框
                        selectSavePathDialog.open()
                    })
                } else {
                    // 有权限，保存路径
                    if (globals) {
                        globals.setDefaultSavePath(selectedPath)
                    }
                    defaultSavePathField.text = selectedPath
                }
            }
        }
        onRejected: {
            console.log("FileDialog cancelled")
        }
    }

    // 辅助函数：将 file:// URL 转换为本地路径
    function fileUrlToPath(fileUrl) {
        if (!fileUrl || fileUrl === "") {
            return ""
        }
        // 移除 "file://" 前缀
        var path = fileUrl.toString()
        if (path.startsWith("file://")) {
            path = path.substring(7)
        }
        // 移除末尾的斜杠（如果有）
        if (path.endsWith("/")) {
            path = path.substring(0, path.length - 1)
        }
        return path
    }

    // 显示权限错误对话框的辅助函数
    function showPermissionErrorDialog(onConfirmCallback) {
        var dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml")
        if (dialogComponent.status === Component.Ready) {
            var dialog = dialogComponent.createObject(Overlay.overlay)
            dialog.windowWidth = 452
            dialog.iconSource = "qrc:/icon/icon/hint.svg"
            dialog.questionText = qsTr("This directory does not have write permission. Please choose a directory under your user folder to store files.")
            dialog.infoText = ""
            dialog.confirmButtonText = qsTr("OK")
            dialog.cancelButtonText = ""
            dialog.highlightConfirmButton = true
            dialog.highlightCancelButton = false
            dialog.showNoRemindCheckBox = false
            if (typeof root !== "undefined" && root !== null) {
                dialog.transientParent = root
            }

            if (dialog) {
                dialog.confirmed.connect(function(options, noRemindChecked) {
                    dialog.destroy()
                    if (onConfirmCallback) {
                        onConfirmCallback()
                    }
                })

                dialog.canceled.connect(function() {
                    dialog.destroy()
                })

                dialog.showDialog()
            } else {
                console.error("创建弹窗失败：", dialogComponent.errorString())
            }
        } else {
            console.error("加载弹窗组件失败：", dialogComponent.errorString())
        }
    }
}
