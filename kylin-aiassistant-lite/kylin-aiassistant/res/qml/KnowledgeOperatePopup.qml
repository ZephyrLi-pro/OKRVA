import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import FileListModelProvider 1.0
import "."

Popup{
    id: knowledgeOperatePopup
    width: operateMenuColumn.width
    height: operateMenuColumn.height
    modal: false
    closePolicy: Popup.CloseOnPressOutside
    focus: true

    onVisibleChanged: {
        if(visible){
            knowledgeOperatePopupTextArea.focus = true
            knowledgeOperatePopupTextArea.forceActiveFocus()
        }
    }

    property bool showRenameFlag: false
    property bool isCanOperateFlag: true

    signal moveFile()
    signal copyFile()
    signal renameFile()
    signal deleteFile()

    TextArea {
        id: knowledgeOperatePopupTextArea
        visible: false
        focus: true
        text: "用于获取焦点关闭pop菜单"
        Keys.onShortcutOverride: {
            if (event.key === Qt.Key_Escape) {
                knowledgeOperatePopup.close()
            }
        }

        Keys.onPressed: {
        }
    }

    MouseArea{
        anchors.fill: parent
        hoverEnabled: true

        Column {
            id: operateMenuColumn
            Accessible.name: "kylin-aiassistant_Chat_messageTextMenuColumn"
            anchors.centerIn: parent
            width: 180
            padding: 8
            spacing: 2


            // 移动按钮
            Rectangle {
                id: moveOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                // todo knowledge 0303
                visible: false

                Row {
                    id: moveOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: moveRet
                        width: parent.width - moveOperateRow.leftPadding - moveOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        opacity: isCanOperate ? 1 : 0.5
                        Row {
                            id: moveRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: moveImage
                                source: "qrc:/icon/icon/move.svg"
                                anchors.verticalCenter: parent.verticalCenter

                            }

                            Label {
                                id: moveText
                                text: qsTr("Move")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                            }
                        }

                        MouseArea {
                            id: moveRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            enabled: isCanOperateFlag
                            onEntered: {
                                moveText.color = "#FFFFFF"
                                moveRet.color = "#3489ED"
                                moveImage.source = "qrc:/icon/icon/move-white.svg"
                            }
                            onExited: {
                                moveText.color = "#000000"
                                moveRet.color = "transparent"
                                moveImage.source = "qrc:/icon/icon/move.svg"
                            }
                            onPressed: {
                                moveText.color = "#FFFFFF"
                                moveRet.color = "#2C73C8"
                                moveImage.source = "qrc:/icon/icon/move-white.svg"
                            }
                            onReleased: {
                                moveText.color = "#000000"
                                moveRet.color = "transparent"
                                moveImage.source = "qrc:/icon/icon/move.svg"
                            }
                            onClicked: {
                                knowledgeOperatePopup.close()
                                moveFile()
                            }
                        }
                    }
                }
            }

            // 复制按钮
            Rectangle {
                id: copyOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                // todo knowledge 0303
                visible: false

                Row {
                    id: copyOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: copyRet
                        width: parent.width - copyOperateRow.leftPadding - copyOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        opacity: isCanOperate ? 1 : 0.5
                        Row {
                            id: copyRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: copyImage
                                source: "qrc:/icon/icon/copy.svg"
                                anchors.verticalCenter: parent.verticalCenter

                            }

                            Label {
                                id: copyText
                                text: qsTr("Copy")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                            }
                        }

                        MouseArea {
                            id: copyRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            enabled: isCanOperateFlag
                            onEntered: {
                                copyText.color = "#FFFFFF"
                                copyRet.color = "#3489ED"
                                copyImage.source = "qrc:/icon/icon/copy-white.svg"
                            }
                            onExited: {
                                copyText.color = "#000000"
                                copyRet.color = "transparent"
                                copyImage.source = "qrc:/icon/icon/copy.svg"
                            }
                            onPressed: {
                                copyText.color = "#FFFFFF"
                                copyRet.color = "#2C73C8"
                                copyImage.source = "qrc:/icon/icon/copy-white.svg"
                            }
                            onReleased: {
                                copyText.color = "#000000"
                                copyRet.color = "transparent"
                                copyImage.source = "qrc:/icon/icon/copy.svg"
                            }
                            onClicked: {
                                knowledgeOperatePopup.close()
                                copyFile()
                            }
                        }
                    }
                }
            }

            // 重命名按钮
            Rectangle {
                id: renameOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                visible: showRenameFlag

                Row {
                    id: renameOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: renameRet
                        width: parent.width - renameOperateRow.leftPadding - renameOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        opacity: isCanOperate ? 1 : 0.5
                        Row {
                            id: renameRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: renameImage
                                source: "qrc:/icon/icon/rename.svg"
                                anchors.verticalCenter: parent.verticalCenter

                            }

                            Label {
                                id: renameText
                                text: qsTr("Rename")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                            }
                        }

                        MouseArea {
                            id: renameRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            enabled: isCanOperateFlag
                            onEntered: {
                                renameText.color = "#FFFFFF"
                                renameRet.color = "#3489ED"
                                renameImage.source = "qrc:/icon/icon/rename-white.svg"
                            }
                            onExited: {
                                renameText.color = "#000000"
                                renameRet.color = "transparent"
                                renameImage.source = "qrc:/icon/icon/rename.svg"
                            }
                            onPressed: {
                                renameText.color = "#FFFFFF"
                                renameRet.color = "#2C73C8"
                                renameImage.source = "qrc:/icon/icon/rename-white.svg"
                            }
                            onReleased: {
                                renameText.color = "#000000"
                                renameRet.color = "transparent"
                                renameImage.source = "qrc:/icon/icon/rename.svg"
                            }
                            onClicked: {
                                knowledgeOperatePopup.close()
                                renameFile()
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: lineRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 1
                color: Qt.rgba(0, 0, 0, 0.1)
                // todo knowledge 0303
                visible: false
            }

            // 删除按钮
            Rectangle {
                id: deleteOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"

                Row {
                    id: deleteOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: deleteRet
                        width: parent.width - deleteOperateRow.leftPadding - deleteOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        Row {
                            id: deleteRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: deleteImage
                                source: "qrc:/icon/icon/delete.svg"
                                anchors.verticalCenter: parent.verticalCenter

                            }

                            Label {
                                id: deleteText
                                text: qsTr("Delete")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                            }
                        }

                        MouseArea {
                            id: deleteRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            onEntered: {
                                deleteText.color = "#FFFFFF"
                                deleteRet.color = "#3489ED"
                                deleteImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                            onExited: {
                                deleteText.color = "#000000"
                                deleteRet.color = "transparent"
                                deleteImage.source = "qrc:/icon/icon/delete.svg"
                            }
                            onPressed: {
                                deleteText.color = "#FFFFFF"
                                deleteRet.color = "#2C73C8"
                                deleteImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                            onReleased: {
                                deleteText.color = "#000000"
                                deleteRet.color = "transparent"
                                deleteImage.source = "qrc:/icon/icon/delete.svg"
                            }
                            onClicked: {
                                knowledgeOperatePopup.close()
                                deleteFile()
                            }
                        }
                    }
                }
            }
        }
    }

    background: Rectangle {
        color: "#FFFFFF"  // 背景颜色
        radius: 8  // 圆角
        border.color: Qt.rgba(0/255, 0/255, 0/255, 0.1)  // 边框颜色
        border.width: 1  // 边框宽度
    }
}
