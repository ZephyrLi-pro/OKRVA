import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import FileListModelProvider 1.0
import "."

// 侧边模式下知识库收起状态下popup操作菜单
Popup{
    id: knowledgeBreadcrumbOperatePopup
    width: operateMenuColumn.width
    height: operateMenuColumn.height
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property int popupType: 1 // 1: 新建文件夹，添加文件  2: 移动，复制，删除
    property bool showNewFolderFlag: true
    property bool newFileShowFlag: true
    property bool isCanOperateFlag: true

    onNewFileShowFlagChanged: {
        if(!newFileShowFlag){
            addFileImage.source = "qrc:/icon/icon/add-file-click.svg"
            addFileImage.opacity = 0.45
            addFileText.color = Qt.rgba(55/255, 144/255, 250/255, 0.45)
        }else{
            addFileImage.source = "qrc:/icon/icon/add-file-black.svg"
            addFileImage.opacity = 1
            addFileText.color = "#000000"
        }
    }

    signal moveFile()
    signal copyFile()
    signal deleteFile()
    signal newFloder()
    signal addFile()

    background: Rectangle {
        color: "#FFFFFF"  // 背景颜色
        radius: 8  // 圆角
        border.color: Qt.rgba(0/255, 0/255, 0/255, 0.1)  // 边框颜色
        border.width: 1  // 边框宽度
    }

    onVisibleChanged: {
        if(visible){
            knowledgeBreadcrumbOperatePopup.focus = true
            knowledgeBreadcrumbOperatePopup.forceActiveFocus()
        }
    }

    TextArea {
        id: knowledgeBreadcrumbOperatePopupTextArea
        visible: false
        focus: true
        text: "用于获取焦点关闭pop菜单"
        Keys.onShortcutOverride: {
            if (event.key === Qt.Key_Escape) {
                knowledgeBreadcrumbOperatePopup.close()
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
                visible: false//popupType === 2

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
                                moveRet.color = "#DCDCDC"
                            }
                            onExited: {
                                moveRet.color = "transparent"
                            }
                            onPressed: {
                                moveRet.color = "#B9B9B9"
                            }
                            onReleased: {
                                moveRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbOperatePopup.close()
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
                visible: false//popupType === 2
                opacity: isCanOperate ? 1 : 0.5
                Row {
                    id: copyOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: copyRet
                        width: parent.width - copyOperateRow.leftPadding - copyOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
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
                                copyRet.color = "#DCDCDC"
                            }
                            onExited: {
                                copyRet.color = "transparent"
                            }
                            onPressed: {
                                copyRet.color = "#B9B9B9"
                            }
                            onReleased: {
                                copyRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbOperatePopup.close()
                                copyFile()
                            }
                        }
                    }
                }
            }

            // 删除按钮
            Rectangle {
                id: deleteOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                visible: popupType === 2

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
                                deleteRet.color = "#DCDCDC"
                            }
                            onExited: {
                                deleteRet.color = "transparent"
                            }
                            onPressed: {
                                deleteRet.color = "#B9B9B9"
                            }
                            onReleased: {
                                deleteRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbOperatePopup.close()
                                deleteFile()
                            }
                        }
                    }
                }
            }


            // 新建文件夹
            Rectangle {
                id: newFolderOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                // todo knowledge 0303
                visible: false//popupType === 1 && showNewFolderFlag

                Row {
                    id: newFolderOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: newFolderRet
                        width: parent.width - newFolderOperateRow.leftPadding - newFolderOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        Row {
                            id: newFolderRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: newFolderImage
                                source: "qrc:/icon/icon/new-folder-black.svg"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                id: newFolderText
                                text: qsTr("New Folder")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                            }
                        }

                        MouseArea {
                            id: newFolderRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            onEntered: {
                                newFolderRet.color = "#DCDCDC"
                            }
                            onExited: {
                                newFolderRet.color = "transparent"
                            }
                            onPressed: {
                                newFolderRet.color = "#B9B9B9"
                            }
                            onReleased: {
                                newFolderRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbOperatePopup.close()
                                newFloder()
                            }
                        }
                    }
                }
            }

            // 添加文件
            Rectangle {
                id: addFileOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                visible: popupType === 1

                Row {
                    id: addFileOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: addFileRet
                        width: parent.width - addFileOperateRow.leftPadding - addFileOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        ToolTip {
                            id: newFileRetTooltip
                            visible: addFileRetMouseArea.containsMouse && !newFileShowFlag
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: qsTr("The number of files cannot exceed 100")
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                        Row {
                            id: addFileRow
                            anchors.fill: parent
                            leftPadding: 8
                            Image {
                                id: addFileImage
                                source: newFileShowFlag ? "qrc:/icon/icon/add-file-black.svg" : "qrc:/icon/icon/add-file-click.svg"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Label {
                                id: addFileText
                                text: qsTr("Add File")
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                                color: newFileShowFlag ? "#000000" : Qt.rgba(55/255, 144/255, 250/255, 0.45)
                            }
                        }

                        MouseArea {
                            id: addFileRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            onEntered: {
                                if(!newFileShowFlag){
                                    return
                                }
                                addFileRet.color = "#DCDCDC"
                            }
                            onExited: {
                                if(!newFileShowFlag){
                                    return
                                }
                                addFileRet.color = "transparent"
                            }
                            onPressed: {
                                if(!newFileShowFlag){
                                    return
                                }
                                addFileRet.color = "#B9B9B9"
                            }
                            onReleased: {
                                if(!newFileShowFlag){
                                    return
                                }
                                addFileText.color = "#000000"
                                addFileRet.color = "transparent"
                            }
                            onClicked: {
                                if(!newFileShowFlag){
                                    return
                                }
                                knowledgeBreadcrumbOperatePopup.close()
                                addFile()
                            }
                        }
                    }
                }
            }
        }
    }
}
