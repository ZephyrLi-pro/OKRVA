import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."
// 设置页面全局划词内容显示区域
Rectangle {
    id: settingSelectPageRet
    color: "#FFFFFF"

    // 判断当前是否为侧边栏模式
    property bool isSidebarMode: {
        if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
            return windowCtrl.winMode === 2
        }
        return false
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        clip: true
        ScrollBar.vertical.policy: scrollView.contentHeight > scrollView.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: contentColumn
            width: scrollView.width
            spacing: 4

            // 划词悬浮条标题
            Label {
                id: scribeTitleLabel
                text: qsTr("Word Selection Floating Bar")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 启用划词悬浮条 + 唤起方式（1px 间距、圆角分组样式与常规页「窗口模式」一致）
            ColumnLayout {
                Layout.fillWidth: true
                spacing: enableScribeSwitch.checked ? 1 : 0

                // 启用划词悬浮条（仅上圆角）
                Rectangle {
                    id: enableScribeTopRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: "#F6F6F6"
                    radius: 6
                    clip: true

                    // 与下方「唤起方式」拼接时压住下圆角；仅一行显示时隐藏，保留整块圆角
                    Rectangle {
                        visible: enableScribeSwitch.checked
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
                            id: enableScribeLabel
                            text: qsTr("Enable Floating Bar for Word Selection")
                            color: "#333333"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 60
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Switch {
                            id: enableScribeSwitch
                            property bool isUpdating: false
                            indicator: Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                implicitWidth: 50
                                implicitHeight: 24
                                radius: 12
                                color: enableScribeSwitch.checked ? "#3790FA" : "#CCCCCC"
                                Rectangle {
                                    width: 16
                                    height: 16
                                    radius: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: enableScribeSwitch.checked ? parent.width - width - 2 : 2
                                    color: "#FFFFFF"
                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                            Component.onCompleted: {
                                // 从配置文件读取初始状态
                                if (typeof globals !== "undefined" && globals !== null) {
                                    var isEnabled = globals.getScribeEnabled()
                                    isUpdating = true
                                    checked = isEnabled
                                    isUpdating = false
                                }
                            }
                            onCheckedChanged: {
                                // 防止循环触发
                                if (!isUpdating && typeof globals !== "undefined" && globals !== null) {
                                    globals.setScribeEnabled(checked)
                                }
                            }
                        }
                    }
                }

                // 唤起方式（仅下圆角；单选布局与常规页「窗口模式」一致；仅启用划词悬浮条时显示）
                Rectangle {
                    id: invokeMethodRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: enableScribeSwitch.checked ? 60 : 0
                    Layout.maximumHeight: enableScribeSwitch.checked ? -1 : 0
                    visible: enableScribeSwitch.checked
                    color: "#F6F6F6"
                    radius: 6
                    clip: true

                    Rectangle {
                        visible: invokeMethodRow.visible
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
                        spacing: 8
                        z: 1

                        Label {
                            id: invokeMethodLabel
                            text: qsTr("Invocation method")
                            color: "#333333"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 50
                            Layout.rightMargin: 16
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        RadioButton {
                            id: autoInvokeRadio
                            text: qsTr("Automatic invoke")
                            Layout.maximumWidth: isSidebarMode ? 96 : -1
                            property bool isUpdating: false

                            HoverHandler {
                                id: autoInvokeTipHoverHandler
                            }
                            ToolTip.delay: 500
                            ToolTip.visible: autoInvokeTipHoverHandler.hovered && invokeMethodRow.visible
                            ToolTip.text: qsTr("After selecting text, the floating bar appears automatically.")

                            indicator: Rectangle {
                                x: root.leftPadding
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: width
                                antialiasing: true
                                radius: (width/2)
                                border.width: autoInvokeRadio.checked ? width*0.25 : 1
                                border.color: autoInvokeRadio.checked ? "#3790FA" : "#CCCCCC"
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
                                text: autoInvokeRadio.text
                                color: "#333333"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: autoInvokeRadio.indicator.width + autoInvokeRadio.spacing
                                elide: Text.ElideRight
                            }
                            onCheckedChanged: {
                                if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !shortcutInvokeRadio.checked) {
                                    globals.setScribeAutoInvoke(true)
                                }
                            }
                            Component.onCompleted: {
                                if (typeof globals !== "undefined" && globals !== null) {
                                    var autoOn = globals.getScribeAutoInvoke()
                                    isUpdating = true
                                    shortcutInvokeRadio.isUpdating = true
                                    checked = autoOn
                                    shortcutInvokeRadio.checked = !autoOn
                                    shortcutInvokeRadio.isUpdating = false
                                    isUpdating = false
                                }
                            }
                        }

                        RadioButton {
                            id: shortcutInvokeRadio
                            text: qsTr("Shortcut invoke")
                            Layout.maximumWidth: isSidebarMode ? 96 : -1
                            property bool isUpdating: false

                            HoverHandler {
                                id: shortcutInvokeTipHoverHandler
                            }
                            ToolTip.delay: 500
                            ToolTip.visible: shortcutInvokeTipHoverHandler.hovered && invokeMethodRow.visible
                            ToolTip.text: qsTr("After selecting text, press the shortcut keys (Super + Alt + C) to display the floating bar.")

                            indicator: Rectangle {
                                x: root.leftPadding
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: width
                                antialiasing: true
                                radius: (width/2)
                                border.width: shortcutInvokeRadio.checked ? width*0.25 : 1
                                border.color: shortcutInvokeRadio.checked ? "#3790FA" : "#CCCCCC"
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
                                text: shortcutInvokeRadio.text
                                color: "#333333"
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: shortcutInvokeRadio.indicator.width + shortcutInvokeRadio.spacing
                                elide: Text.ElideRight
                            }
                            onCheckedChanged: {
                                if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !autoInvokeRadio.checked) {
                                    globals.setScribeAutoInvoke(false)
                                }
                            }
                        }

                        Connections {
                            id: scribeAutoInvokeGlobalsConnections
                            target: (typeof globals !== "undefined" && globals !== null) ? globals : null
                            function onSigScribeAutoInvokeChanged(autoInvokeEnabled) {
                                autoInvokeRadio.isUpdating = true
                                shortcutInvokeRadio.isUpdating = true
                                autoInvokeRadio.checked = autoInvokeEnabled
                                shortcutInvokeRadio.checked = !autoInvokeEnabled
                                autoInvokeRadio.isUpdating = false
                                shortcutInvokeRadio.isUpdating = false
                            }
                        }
                    }
                }
            }

            // 间距
            Item {
                Layout.preferredHeight: 24
            }

            // 悬浮条快捷功能标题
            Label {
                id: commandListTitleLabel
                text: qsTr("Floating Bar Shortcut Functions")
                Layout.leftMargin: 16
                Layout.topMargin: 0
                Layout.bottomMargin: 0
                color: "#333333"
            }

            // 悬浮条快捷功能列表
            SelectCommandList {
                id: selectCommandList
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 0
                Layout.rightMargin: 0
            }

            // 底部间距
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
        id: settingSelectPageWindowCtrlConnections
        target: (typeof windowCtrl !== "undefined" && windowCtrl !== null) ? windowCtrl : null
        onWinModeChanged: function(winMode) {
            settingSelectPageRet.isSidebarMode = (winMode === 2)
        }
    }

    // 监听划词悬浮条启用状态变化
    Connections {
        id: settingSelectPageGlobalsConnections
        target: (typeof globals !== "undefined" && globals !== null) ? globals : null
        onSigScribeEnabledChanged: function(enabled) {
            if (enableScribeSwitch) {
                enableScribeSwitch.isUpdating = true
                enableScribeSwitch.checked = enabled
                enableScribeSwitch.isUpdating = false
            }
        }
    }
}
