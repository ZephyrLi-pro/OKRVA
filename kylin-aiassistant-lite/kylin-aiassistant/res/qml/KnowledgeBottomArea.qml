import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Row{
    id: knowledgeBottomArea
    Layout.fillWidth: true
    height: 36
    leftPadding: paddingSize
    rightPadding: paddingSize
    spacing: 20

    property string spaceStatus: ""
    property int spaceUsedPercent: 0
    property int paddingSize: 24

    // 进度条区域
    ProgressBar{
        id: progressBar
        progressBarWidth: knowledgeBottomArea.width - volumeLabelRet.width - knowledgeBottomArea.leftPadding - knowledgeBottomArea.rightPadding -
                          knowledgeBottomArea.spacing
        progressBarHeight: 8
        anchors.verticalCenter: parent.verticalCenter
        progress: knowledgeBottomArea.spaceUsedPercent
    }

    // 文字显示区域
    Rectangle{
        id: volumeLabelRet
        width: volumeLabelLabel.implicitWidth
        height: knowledgeBottomArea.height
        Label{
            id: volumeLabelLabel
            text: spaceStatus
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
        }
    }
}
