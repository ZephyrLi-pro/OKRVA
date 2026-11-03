import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Rectangle{
    id: knowledgeItemAreaNoFileRet
    color: bgColor
    //border.width: 1
    //border.color: Qt.rgba(0, 0, 0, 0.1)
    radius: 6

    property string bgColor: "#F6F6F6"

    z: 1
    MouseArea {
        id: knowledgeItemAreaNoFileRetMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            knowledgeItemAreaNoFileRet.color = Qt.rgba(242/255, 242/255, 242/255, 1)
        }
        onExited: {
            knowledgeItemAreaNoFileRet.color = Qt.rgba(246/255, 246/255, 246/255, 1)
        }
        onPressed: {
            knowledgeItemAreaNoFileRet.color = Qt.rgba(238/255, 238/255, 238/255, 1)
        }
        onReleased: {
            knowledgeItemAreaNoFileRet.color = Qt.rgba(246/255, 246/255, 246/255, 1)
        }
        onClicked: {
            knowledgeItemAreaNoFileRow.requestToUploadDocument()
        }
    }

    Column{
        id: knowledgeItemAreaNoFileColumn
        width: parent.width
        anchors.centerIn: parent
        Image{
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/icon/icon/add-file.svg"
        }
        Label{
            text: qsTr("Click or drag files here to upload")
            color: Qt.rgba(0, 0, 0, 1)
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: globals.fontsize
            font.family: root.fontFamily
        }
        Label{
            id: formatLabel
            text: globals.isPdfSupported ? 
                qsTr("Supported formats (plain text): txt, doc, docx, pdf, wps, pptx, ppt, ofd;") :
                qsTr("Supported formats (plain text): txt, doc, docx, wps, pptx, ppt, ofd;")
            color: Qt.rgba(0, 0, 0, 0.35)
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: globals.fontsize - 2
            font.family: root.fontFamily
        }
        Label{
            text: qsTr("Space capacity: no more than 2 GB;")
            color: Qt.rgba(0, 0, 0, 0.35)
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: globals.fontsize - 2
            font.family: root.fontFamily
        }
        Label{
            text: qsTr("File count: no more than 100;")
            color: Qt.rgba(0, 0, 0, 0.35)
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: globals.fontsize - 2
            font.family: root.fontFamily
        }
        Label{
            text: qsTr("File size: single file no more than 200 MB")
            color: Qt.rgba(0, 0, 0, 0.35)
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            wrapMode: Text.WrapAnywhere
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: globals.fontsize - 2
            font.family: root.fontFamily
        }

    }
}
