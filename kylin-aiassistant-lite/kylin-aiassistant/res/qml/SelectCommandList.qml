import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

Rectangle {
    id: selectCommandRectangle
    Accessible.name: "kylin-aiassistant_SelectCommandList_selectCommandRectangle"
    visible: true
    width: parent ? parent.width : 496
    height: listView.contentHeight
    color: "transparent"
    antialiasing: true

    property int maxHeight: 332

    Component {
        id: listDelegate

        Item {
            id: delegateItem
            Accessible.name: "kylin-aiassistant_SelectCommandList_delegateItem"
            width: listItem.width
            height: listItem.height
            antialiasing: true

            Rectangle {
                id: listItem
                Accessible.name: "kylin-aiassistant_SelectCommandList_listItem"
                anchors.verticalCenter: parent.verticalCenter
                color: "#F6F6F6"
                width: listView.width
                height: 60
                // 第一个项目上圆角，最后一个项目下圆角，中间项目无圆角
                radius: 6
                antialiasing: true
                clip: true

                // 遮盖底部圆角（第一个项目：只保留上圆角）
                Rectangle {
                    visible: index === 0 && listView.count > 1
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 6
                    color: parent.color
                }

                // 遮盖顶部圆角（最后一个项目：只保留下圆角）
                Rectangle {
                    visible: index === listView.count - 1 && listView.count > 1
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 6
                    color: parent.color
                }

                // 遮盖所有圆角（中间项目：无圆角）
                Rectangle {
                    visible: index > 0 && index < listView.count - 1
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 6
                    color: parent.color
                }
                Rectangle {
                    visible: index > 0 && index < listView.count - 1
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 6
                    color: parent.color
                }

                Row {
                    id: listItemRow
                    Accessible.name: "kylin-aiassistant_SelectCommandList_listItemRow"
                    anchors.fill: parent
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    anchors.leftMargin: 16
                    anchors.rightMargin: 4
                    leftPadding: 0
                    rightPadding: 0
                    spacing: 8

                    // 指令图标
                    Image {
                        id: commandIcon
                        Accessible.name: "kylin-aiassistant_SelectCommandList_commandIcon"
                        source: model.iconPath
                        sourceSize.width: 16
                        sourceSize.height: 16
                        smooth: true
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // 指令名称
                    Label {
                        id: commandNameText;
                        Accessible.name: "kylin-aiassistant_SelectCommandList_commandNameText"
                        text: model.name
                        color: Qt.rgba(0/255, 0/255, 0/255, 0.85);
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // 指令描述
                    Label {
                        id: commandDescText;
                        Accessible.name: "kylin-aiassistant_SelectCommandList_commandDescText"
                        text: model.description
                        font.pointSize: globals.fontsize - 2
                        font.family: root.fontFamily
                        color: Qt.rgba(0, 0, 0, 0.35);
                        width: listItem.width - listItemRow.anchors.leftMargin - listItemRow.anchors.rightMargin
                               - commandIcon.width - commandNameText.implicitWidth - 2 * listItemRow.spacing
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    ListView {
        id: listView
        Accessible.name: "kylin-aiassistant_SelectCommandList_listView"
        anchors.fill: parent
        model: selectCommandListModel
        delegate: listDelegate
        spacing: 1
        clip: true
        interactive: false
    }
}
