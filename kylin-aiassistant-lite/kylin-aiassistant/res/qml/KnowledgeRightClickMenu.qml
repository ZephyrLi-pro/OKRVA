import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import "."

Popup{
    id: rightClickMenu
    width: 180
    height: isHideRename ? 133 : 169
    clip: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    property bool isHideRename: false

    signal move()
    signal copy()
    signal rename()
    signal deleted()

    function showDialog() {
        visible = true;
    }

    function hideDialog() {
        visible = false;
    }

    property list<QtObject> menuModel: [
        QtObject {
            property string text: qsTr("Move")
            property string source: "qrc:/icon/icon/move.svg"
            property bool visible: true
            property var triggered: function() { rightClickMenu.move(); }
        },
        QtObject {
            property string text: qsTr("Copy")
            property string source: "qrc:/icon/icon/copy.svg"
            property bool visible: true
            property var triggered: function() { rightClickMenu.copy(); }
        },
        QtObject {
            property string text: qsTr("Rename")
            property string source: "qrc:/icon/icon/rename.svg"
            property bool visible: !rightClickMenu.isHideRename
            property var triggered: function() { rightClickMenu.rename(); }
        },
        QtObject {
            property bool isSeparator: true
            property bool visible: true
        },
        QtObject {
            property string text: qsTr("Delete")
            property string source: "qrc:/icon/icon/delete-blue.svg"
            property bool visible: true
            property var triggered: function() { rightClickMenu.deleted(); }
        }
    ]

    Column {
        id: rightClickMenuLayout
        anchors.fill: parent
        anchors.margins: 8
        spacing: 2

        Repeater {
            model: rightClickMenu.menuModel
            Loader {
                width: parent.width
                sourceComponent: model.isSeparator ? separatorComp : menuItemComp
                visible: model.visible
                property var modelData: model
            }
        }
    }

    Component {
        id: menuItemComp
        Rectangle {
            id: menuItemRet
            Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_menuItemRet"
            width: parent.width
            height: 36
            radius: 6
            color: menuItemMouseArea.containsPress ? "#CCCCCC" :
                   menuItemMouseArea.containsMouse ? "#E6E6E6" : "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Image {
                    id: itemIcon
                    Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_itemIcon"
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: modelData.source
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    id: itemText
                    Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_itemText"
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.text
                    color: Qt.rgba(0, 0, 0, 0.85)
                }
            }

            MouseArea {
                id: menuItemMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    modelData.triggered();
                    rightClickMenu.close();
                }
            }
        }
    }

    // 分割线
    Component {
        id: separatorComp
        Item {
            width: parent.width
            height: 1

            Rectangle {
                id:separatorLine
                Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_separatorLine"
                width: 148
                height: 1
                anchors.horizontalCenter: parent.horizontalCenter
                color: Qt.rgba(0, 0, 0, 0.1)
            }
        }
    }

    background: Rectangle {
        id: backgroundRct
        color: "white"
        radius: 8
    }
}
