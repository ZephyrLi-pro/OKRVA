import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."

Rectangle {
    id: root
    height: 36
    radius: 6
    property string text: ""
    property string icon: ""
    property bool hasSubMenu: false

    signal clicked()

    RowLayout {
        id: qucikMenuItemRow
        anchors.fill: parent
        Layout.alignment: Qt.AlignVCenter
        spacing: 8

        Rectangle{
            width: 0
            height: parent.height
            color: "transparent"
        }

        Image {
            id: qucikMenuItemIcon
            height: 28
            width: 28
            source: icon
            visible: icon !== ""
        }

        Label {
            id: quickMenuItemText
            text: root.text
            Layout.fillWidth: true
        }

        Image{
            height: 28
            width: 28
            source:"qrc:/icon/icon/knowledge-bread-right.svg"
            visible: hasSubMenu
            Layout.rightMargin: 8
        }
    }

    MouseArea{
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            quickMenuItemText.color = "#FFFFFF"
            parent.color = "#3489ED";
        }
        onExited: {
            quickMenuItemText.color = "#000000"
            parent.color = "transparent"
        }
        onPressed: {
            quickMenuItemText.color = "#FFFFFF"
            parent.color = "#2C73C8"
        }
        onReleased: {
            quickMenuItemText.color = "#000000"
            parent.color = "transparent"
        }
        onClicked: {
            root.clicked()
        }
    }
}
