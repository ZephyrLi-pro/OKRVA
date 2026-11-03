import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."


Rectangle {
    id: commonTip

    property string tipImageSource: ""
    property string tipText: ""

    Rectangle{
        id: commonTipRet
        anchors.centerIn: parent
        width: Math.min(Math.max(112, (askImageRet.width + commonTipTextMetrics.width + 8 + 32)), 424) // 最小112px，最大424px，加上左右padding 32px
        height: 52 // 固定高度52px
        opacity: 1
        color: "#FFFFFF" // Base-Active
        radius: 8 // KRadius-Menu
        border.width: 1
        border.color: Qt.rgba(0/255, 0/255, 0/255, 0.1) // KLine-Window-Inactive

        Row{
            id: commonTipRetRow
            anchors.fill: parent
            anchors.leftMargin: 16 // padding: 16px
            anchors.rightMargin: 16
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 8 // gap: 8px
            layoutDirection: Qt.LeftToRight

            Rectangle{
                id: askImageRet
                width: 16
                height: 16
                color: "transparent"
                anchors.verticalCenter: parent.verticalCenter
                Image {
                    id: commonTipImage
                    source: tipImageSource
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                }
            }
            Label {
                id: commonTipText
                text: tipText
                color: "#000000"
                anchors.verticalCenter: parent.verticalCenter
                width: commonTipRetRow.width - askImageRet.width - commonTipRetRow.spacing
                wrapMode: Text.WrapAnywhere
            }
        }
        // 计算宽度文本，永不展示
        Label {
            id: commonTipTextMetrics
            text: tipText
            visible: false
        }
    }
}
