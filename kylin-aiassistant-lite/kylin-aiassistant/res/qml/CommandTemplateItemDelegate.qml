import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// 指令模板列表项组件
Rectangle {
    id: commandItemDelegate
    height: 60
    color: "transparent"
    clip: true

    property int commandType: 1  // 1: ReadOnly, 2: ReadWrite, 3: NoAccess
    property int actorId: 0
    property string name: ""
    property string content: ""
    property string description: ""
    property string iconPath: ""
    property bool isFirst: false
    property bool isLast: false

    signal changeTransparentCommandItemDelegate(var flag)

    // 背景实现：使用圆角矩形 + 遮罩矩形实现部分圆角
    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: "#F6F6F6"
        radius: (isFirst || isLast) ? 6 : 0
        border.color: "transparent"

        // 遮盖下方圆角（第一条需要）
        Rectangle {
            visible: isFirst
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 6
            color: parent.color
        }

        // 遮盖上方圆角（最后一条需要）
        Rectangle {
            visible: isLast
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 6
            color: parent.color
        }
    }

    signal editClicked()
    signal deleteClicked()
    signal moveUp()
    signal moveDown()



    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        // 图标
        Image {
            id: iconImage
            source: iconPath || "qrc:/icon/icon/setting/prompt.svg"
            sourceSize.width: 16
            sourceSize.height: 16
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
        }

        // 名称和描述
        RowLayout {
            id: textRow
            Layout.fillWidth: true
            spacing: 8

            Label {
                id: nameLabel
                text: name
                color: "#333333"
                elide: Text.ElideRight
            }

            Label {
                id: descLabel
                text: description
                color: "#999999"
                Layout.fillWidth: true
                elide: Text.ElideRight

                MouseArea {
                    id: descMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                ToolTip {
                    visible: descMouseArea.containsMouse && descLabel.truncated && description !== ""
                    text: description
                    delay: 500
                }
            }
        }

        // 操作按钮区域
        RowLayout {
            id: actionRow
            spacing: 4
            Layout.rightMargin: 4

            // 更多选项按钮（...）
            Rectangle {
                id: moreButton
                width: 32
                height: 32
                radius: 4
                color: "transparent"
                visible: commandType === 2  // 只有自定义指令显示

                // Tooltip
                ToolTip {
                    visible: moreMouseArea.containsMouse
                    text: qsTr("More")
                    delay: 200
                }

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/html/icon/setting/view-more-horizontal.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    width: 16
                    height: 16
                }

                MouseArea {
                    id: moreMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        moreButton.color = Qt.rgba(0, 0, 0, 0.15)
                    }
                    onExited: {
                        if(moreMenu.visible){
                            moreButton.color = Qt.rgba(0, 0, 0, 0.15)
                        }else{
                            moreButton.color = "transparent"
                        }
                    }
                    onPressed: {
                        moreButton.color = Qt.rgba(0, 0, 0, 0.15)
                    }
                    onReleased: {
                        if(moreMenu.visible){
                            moreButton.color = Qt.rgba(0, 0, 0, 0.15)
                        }else{
                            moreButton.color = "transparent"
                        }
                    }
                    onClicked: {
                        moreMenu.x = moreButton.x
                        //moreMenu.y = moreButton.y + moreButton.height

                        var windowPos = moreMouseArea.mapToItem(commandItemDelegate, mouse.x, mouse.y);

                        let posY = windowPos.y

                        var windowPosRoot  = mapToItem(null, 0, 0)
                        if(windowPosRoot.y + moreMenu.implicitHeight + posY > root.height){
                            posY = posY - moreMenu.implicitHeight
                        }
                        // 保证菜单不会显示不下
                        moreMenu.y = posY

                        moreMenu.popup()
                    }
                }

                // 更多菜单
                MyMenu {
                    id: moreMenu
                    width: 180
                    height: 90
                    padding: 8
                    onVisibleChanged: {
                        changeTransparentCommandItemDelegate(visible)
                        moreButton.color = moreMenu.visible ? Qt.rgba(0, 0, 0, 0.15) : "transparent"
                    }

                    MenuItem {
                        width: 164
                        height: 36
                        text: qsTr("Edit")
                        icon.source: "qrc:/icon/icon/edit.svg"

                        background: Rectangle {
                            color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                            radius: 6
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                        }

                        onTriggered: {
                            commandItemDelegate.editClicked()
                        }
                    }

                    MenuItem {
                        width: 164
                        height: 2
                        enabled: false
                    }

                    MenuItem {
                        width: 164
                        height: 36
                        text: qsTr("Delete")
                        icon.source: "qrc:/html/icon/setting/delete-black.svg"

                        background: Rectangle {
                            color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
                            radius: 6
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                        }

                        onTriggered: {
                            commandItemDelegate.deleteClicked()
                        }
                    }
                }
            }

            // 分割线
            Rectangle {
                id: dividingline
                width: 1
                height: 16
                visible: moreButton.visible

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/html/icon/setting/dividing-line-vertical.svg"
                    sourceSize.width: 1
                    sourceSize.height: 16
                    width: 1
                    height: 16
                }
            }

            // 上移按钮
            Rectangle {
                id: upButton
                width: 32
                height: 32
                radius: 4
                color: upMouseArea.containsMouse ?  Qt.rgba(0, 0, 0, 0.15) : "transparent"
                enabled: !isFirst

                Image {
                    anchors.centerIn: parent
                    source: isFirst ? "qrc:/icon/icon/setting/go-up-disable.svg" : "qrc:/icon/icon/setting/go-up-symbolic.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    width: 16
                    height: 16
                }

                MouseArea {
                    id: upMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        commandItemDelegate.moveUp()
                    }
                }

                ToolTip {
                    visible: upMouseArea.containsMouse && !upMouseArea.pressed
                    text: qsTr("Move Up")
                    delay: 500
                }
            }

            // 下移按钮
            Rectangle {
                id: downButton
                width: 32
                height: 32
                radius: 4
                color: downMouseArea.containsMouse ? Qt.rgba(0, 0, 0, 0.15) : "transparent"
                enabled: !isLast

                Image {
                    anchors.centerIn: parent
                    source: isLast ? "qrc:/icon/icon/setting/go-down-disable.svg" : "qrc:/icon/icon/setting/go-down-symbolic.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    width: 16
                    height: 16
                }

                MouseArea {
                    id: downMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        commandItemDelegate.moveDown()
                    }
                }

                ToolTip {
                    visible: downMouseArea.containsMouse && !downMouseArea.pressed
                    text: qsTr("Move Down")
                    delay: 500
                }
            }
        }
    }
}

