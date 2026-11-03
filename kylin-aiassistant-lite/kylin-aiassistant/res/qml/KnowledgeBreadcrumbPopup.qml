import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import FileListModelProvider 1.0
import "."

// 侧边模式下知识库收起状态下面包屑popup
Popup{
    id: knowledgeBreadcrumbPopup
    width: operateMenuColumn.width
    height: operateMenuColumn.height
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string firstBreadcrumb: ""

    signal openAllFile()
    signal openBreadcrumbPath()

    background: Rectangle {
        color: "#FFFFFF"  // 背景颜色
        radius: 8  // 圆角
        border.color: Qt.rgba(0/255, 0/255, 0/255, 0.1)  // 边框颜色
        border.width: 1  // 边框宽度
    }

    onVisibleChanged: {
        if(visible){
            knowledgeBreadcrumbPopupTextArea.focus = true
            knowledgeBreadcrumbPopupTextArea.forceActiveFocus()
        }
    }

    TextArea {
        id: knowledgeBreadcrumbPopupTextArea
        visible: false
        focus: true
        text: "用于获取焦点关闭pop菜单"
        Keys.onShortcutOverride: {
            if (event.key === Qt.Key_Escape) {
                knowledgeBreadcrumbPopup.close()
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

            // 全部文件
            Rectangle {
                id: allFileOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"

                Row {
                    id: allFileOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: allFileRet
                        width: parent.width - allFileOperateRow.leftPadding - allFileOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        Row {
                            id: allFileRow
                            anchors.fill: parent
                            Label {
                                id: allFileText
                                text: qsTr("All Files")
                                color: "black"
                                width: parent.width - parent.leftPadding
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 8
                                rightPadding: 8
                            }
                        }

                        MouseArea {
                            id: allFileRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            onEntered: {
                                allFileText.color = "#FFFFFF"
                                allFileRet.color = "#3489ED";
                            }
                            onExited: {
                                allFileText.color = "#000000"
                                allFileRet.color = "transparent"
                            }
                            onPressed: {
                                allFileText.color = "#FFFFFF"
                                allFileRet.color = "#2C73C8"
                            }
                            onReleased: {
                                allFileText.color = "#000000"
                                allFileRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbPopup.close()
                                openAllFile()
                            }
                        }
                    }
                }
            }

            // path[0] 第一个文件路径
            Rectangle {
                id: firstBreadcrumbOperateRet
                width: operateMenuColumn.width - operateMenuColumn.padding * 2
                height: 36
                radius: 8
                color: "transparent"
                visible: firstBreadcrumb !== ""

                Row {
                    id: firstBreadcrumbOperateRow
                    anchors.fill: parent
                    Rectangle {
                        id: firstBreadcrumbRet
                        width: parent.width - firstBreadcrumbOperateRow.leftPadding - firstBreadcrumbOperateRow.rightPadding
                        height: parent.height
                        radius: 6
                        color: "transparent"
                        Row {
                            id: firstBreadcrumbRow
                            anchors.fill: parent
                            Label {
                                id: firstBreadcrumbText
                                text: firstBreadcrumb
                                color: "black"
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                leftPadding: 8
                                rightPadding: 8
                            }
                        }

                        Label {
                            id: firstBreadcrumbTextMertics
                            text: firstBreadcrumb
                            color: "black"
                            leftPadding: 8
                            rightPadding: 8
                            visible: false
                        }

                        // 悬浮提示框
                        ToolTip {
                            id: firstBreadcrumbRetTooltip
                            visible: firstBreadcrumbRetMouseArea.containsMouse && firstBreadcrumbTextMertics.width > firstBreadcrumbText.width
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: firstBreadcrumb
                            property var maxWidth: 400
                            property var wrapMode: Text.WordWrap
                        }

                        MouseArea {
                            id: firstBreadcrumbRetMouseArea
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true  // 允许悬浮事件
                            onEntered: {
                                firstBreadcrumbText.color = "#FFFFFF"
                                firstBreadcrumbRet.color = "#3489ED";
                            }
                            onExited: {
                                firstBreadcrumbText.color = "#000000"
                                firstBreadcrumbRet.color = "transparent"
                            }
                            onPressed: {
                                firstBreadcrumbText.color = "#FFFFFF"
                                firstBreadcrumbRet.color = "#2C73C8"
                            }
                            onReleased: {
                                firstBreadcrumbText.color = "#000000"
                                firstBreadcrumbRet.color = "transparent"
                            }
                            onClicked: {
                                knowledgeBreadcrumbPopup.close()
                                openBreadcrumbPath()
                            }
                        }
                    }
                }
            }

        }
    }
}
