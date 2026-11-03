import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import "."

Window {
    id: renameWindow
    width: 376
    height: 176
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    title: " "
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false

    property string lineEditText: ""

    signal confirmed(string fileName)
    signal canceled()
    signal dialogClosed()

    function showDialog() {
        visible = true;
        renameLineEdit.forceActiveFocus();
    }

    function hideDialog() {
        visible = false;
        dialogClosed();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 0

        Label {
            id: renametext
            Accessible.name: "kylin-aiassistant_Knowledge_renameWindow_renametext"
            Layout.preferredHeight: 20
            text: "文件重命名"
            Layout.alignment: Qt.AlignLeft
        }

        Item { Layout.preferredHeight: 9 }

        TextField {
            id: renameLineEdit
            Accessible.name: "kylin-aiassistant_Knowledge_renameWindow_renameLineEdit"
            Layout.preferredWidth: 328
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            text: lineEditText
            maximumLength: 100
        }

        Item { Layout.preferredHeight: 1 }

        Label {
            id: lenthLimittext
            Accessible.name: "kylin-aiassistant_Knowledge_renameWindow_lenthLimittext"
            Layout.preferredWidth: 122
            Layout.preferredHeight: 17
            Layout.fillWidth: true
            text: "长度不可超过 100 个字"
            color: "#F53F3F"
            font.pointSize: globals.fontsize - 2
            opacity: renameLineEdit.text.length >= 100
        }

        Item { Layout.preferredHeight: 6 }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            Button {
                id: cancelBtn
                Accessible.name: "kylin-aiassistant_Knowledge_renameWindow_cancelBtn"
                text: qsTr("Cancel")
                Layout.preferredWidth: 80
                Layout.preferredHeight: 36
                focusPolicy: Qt.NoFocus
                onClicked: {
                    canceled();
                    hideDialog();
                }
            }

            Button {
                id: confirmBtn
                Accessible.name: "kylin-aiassistant_Knowledge_renameWindow_confirmBtn"
                text: qsTr("Confirm")
                Layout.preferredWidth: 80
                Layout.preferredHeight: 36
                highlighted: true
                focusPolicy: Qt.NoFocus
                onClicked: {
                    confirmed(renameLineEdit.text);
                    hideDialog();
                }
            }
        }
    }
}
