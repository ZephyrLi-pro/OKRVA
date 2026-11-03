import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12

// 回答复制按钮点击菜单
MyMenu {
    id: copyButtonMenuPopup
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property bool isCopyButtonShowImageFlag: false  // 是否显示复制图片选项（有图片时显示）

    signal copyImage()
    signal copyAsMarkdown()
    signal copyAsPlainText()

    // 复制图片
    MenuItem {
        text: qsTr("Copy Image")
        visible: copyButtonMenuPopup.isCopyButtonShowImageFlag

        background: Rectangle {
            color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
            radius: 6
        }

        contentItem: Label {
            text: parent.text
            color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
        }

        onTriggered: {
            copyButtonMenuPopup.copyImage()
        }
    }

    // 复制为 Markdown
    MenuItem {
        text: qsTr("Copy as Markdown")

        background: Rectangle {
            color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
            radius: 6
        }

        contentItem: Label {
            text: parent.text
            color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
        }
        onTriggered: {
            copyButtonMenuPopup.copyAsMarkdown()
        }
    }

    // 复制为纯文本
    MenuItem {
        text: qsTr("Copy as Plain Text")

        background: Rectangle {
            color: parent.hovered ? (parent.pressed ? "#2C73C8" : "#3790FA") : "transparent"
            radius: 6
        }

        contentItem: Label {
            text: parent.text
            color: parent.hovered ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
        }

        onTriggered: {
            copyButtonMenuPopup.copyAsPlainText()
        }
    }
}
