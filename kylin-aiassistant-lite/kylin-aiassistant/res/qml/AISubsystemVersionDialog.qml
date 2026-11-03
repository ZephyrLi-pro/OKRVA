import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import "."

Window {
    id: aiSubsystemVersionDialog
    width: 452
    height: windowRow.height
    maximumHeight: windowRow.height
    maximumWidth: width
    minimumHeight: windowRow.height
    minimumWidth: width
    title: " "
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false
    color: "#FFFFFF"

    Component.onCompleted: {
        windowCtrl.clearWindowStyle(aiSubsystemVersionDialog)
    }

    signal confirmed()
    signal canceled()

    function showDialog() {
        visible = true
        requestActivate()
        Qt.callLater(function() {
            windowRow.forceActiveFocus()
        })
    }

    function hideDialog() {
        visible = false
    }

    // 打开设置中心的AI子系统页面
    function openUkuiControlCenter() {
        globals.openUkuiControlCenter()
        Qt.quit()
    }

    Row {
        id: windowRow
        topPadding: 0
        leftPadding: 0
        rightPadding: 0
        bottomPadding: 24
        width: 452
        height: mainColumnLayout.height + windowRow.bottomPadding
        focus: true

        Keys.onEscapePressed: {
            canceled()
            Qt.quit()
        }

        Keys.onReturnPressed: {
            if (confirmBtn.visible) {
                confirmBtn.clicked()
            }
        }

        Keys.onEnterPressed: {
            if (confirmBtn.visible) {
                confirmBtn.clicked()
            }
        }

        ColumnLayout {
            id: mainColumnLayout
            width: windowRow.width - windowRow.leftPadding - windowRow.rightPadding
            spacing: 0

            // 标题栏区域
            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                height: 40
                color: "transparent"

                MouseArea {
                    anchors.fill: parent
                    property point startDragPos: Qt.point(0, 0)

                    onPressed: {
                        startDragPos = Qt.point(mouse.x, mouse.y)
                    }

                    onPositionChanged: {
                        var delta = Qt.point(mouse.x - startDragPos.x, mouse.y - startDragPos.y)
                        aiSubsystemVersionDialog.x += delta.x
                        aiSubsystemVersionDialog.y += delta.y
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                    }

                    // 关闭按钮
                    Button {
                        id: closeButton
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 32
                        Layout.minimumHeight: 32
                        Layout.minimumWidth: 32
                        Layout.maximumHeight: 32
                        Layout.maximumWidth: 32
                        Layout.rightMargin: 4
                        icon.name: "window-close-symbolic"
                        focusPolicy: Qt.NoFocus

                        property string buttonType: "windowTitleBarCloseButton"

                        ToolTip {
                            id: closeButtonTooltip
                            text: qsTr("Close")
                            delay: Qt.styleHints.mousePressAndHoldInterval
                        }

                        onHoveredChanged: {
                            if(hovered)
                                closeButtonTooltip.visible = true
                            else
                                closeButtonTooltip.visible = false
                        }

                        onClicked: {
                            canceled()
                            Qt.quit()
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                height: 8
            }

            // 内容区域
            RowLayout {
                id: contentRowLayout
                Layout.fillWidth: true
                Layout.topMargin: 0
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 4

                // 左侧图标
                Image {
                    id: tipImage
                    source: "qrc:/icon/icon/hint.svg"
                    width: 16
                    height: 16
                    sourceSize.width: 16
                    sourceSize.height: 16
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 4
                }

                // 右侧文本区域
                Column {
                    id: tipTextColumn
                    Layout.fillWidth: true
                    spacing: 0

                    // 标题文本
                    Label {
                        id: titleLabelText
                        width: tipTextColumn.width
                        text: qsTr("Current AI Subsystem Version is Low")
                        font.bold: true
                        font.family: root.fontFamily
                        wrapMode: Text.WrapAnywhere
                        color: Qt.rgba(0, 0, 0, 0.85)
                    }

                    // 标题和正文之间的间距
                    Rectangle {
                        width: 1
                        height: 10
                        color: "transparent"
                    }

                    // 正文文本
                    Label {
                        id: infoLabelText
                        width: tipTextColumn.width
                        text: qsTr("The AI subsystem version is detected to be low, causing abnormal Qi Ling functionality. Please go to System Settings > Update, upgrade the system to update the AI subsystem and try again.")
                        color: Qt.rgba(0, 0, 0, 0.6)
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                height: 32
            }

            // 按钮区域
            RowLayout {
                id: bottomButtonRowLayout
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                // 取消按钮
                Button {
                    id: cancelBtn
                    Accessible.name: "kylin-aiassistant_AISubsystemVersionDialog_cancelBtn"
                    text: qsTr("Cancel")
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 36
                    focusPolicy: Qt.NoFocus

                    onClicked: {
                        canceled()
                        Qt.quit()
                    }
                }

                // 立即前往按钮
                Button {
                    id: confirmBtn
                    Accessible.name: "kylin-aiassistant_AISubsystemVersionDialog_confirmBtn"
                    text: qsTr("Go now")
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 36
                    highlighted: true
                    focusPolicy: Qt.NoFocus

                    onClicked: {
                        confirmed()
                        openUkuiControlCenter()
                    }
                }
            }
        }
    }
}
