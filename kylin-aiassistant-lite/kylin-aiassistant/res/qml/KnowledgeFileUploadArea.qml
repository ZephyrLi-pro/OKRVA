import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Row{
    id: knowledgeItemAreaNoFileRow
    property int paddingSize: 24
    leftPadding: paddingSize
    rightPadding: paddingSize

    signal requestToUploadDocument()

    property int listmodeCount: 0
    property bool isKbEmpty: true

    // 知识库内容为空
    KnowledgeUploadPage{
        id: knowledgeUploadPage
        width: knowledgeItemAreaNoFileRow.width - knowledgeItemAreaNoFileRow.leftPadding - knowledgeItemAreaNoFileRow.rightPadding
        height:  knowledgeItemAreaNoFileRow.height
        visible: isKbEmpty && listmodeCount === 0
    }

    // 知识库内容不为空，文件夹内容为空
    Rectangle{
        id: knowledgeItemAreaFolderNoFileRet
        width: knowledgeItemAreaNoFileRow.width - knowledgeItemAreaNoFileRow.leftPadding - knowledgeItemAreaNoFileRow.rightPadding
        height:  knowledgeItemAreaNoFileRow.height
        color: Qt.rgba(246/255, 246/255, 246/255, 1)//"#F6F6F6"
        //border.width: 1
        //border.color: Qt.rgba(0, 0, 0, 0.1)
        radius: 6
        visible: listmodeCount === 0 && !isKbEmpty
        Column{
            id: knowledgeItemAreaFolderNoFileColumn
            width: parent.width
            anchors.centerIn: parent
            Image{
                anchors.horizontalCenter: parent.horizontalCenter
                source: "qrc:/icon/icon/folder-grey.svg"
            }
            Label{
                text: qsTr("Current folder is empty")
                color: Qt.rgba(0, 0, 0, 0.35)
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                wrapMode: Text.WrapAnywhere
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
