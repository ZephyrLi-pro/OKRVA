import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

Rectangle {
    id: settingSideBarItemDelegateRet
    color: pageNum === currentIndex ? "#3790FA" :"transparent"
    radius: 8

    property string settingBarSource: ""
    property string settingBarWhiteSource: ""
    property string settingBarText: ""
    property int pageNum: 1
    property int currentIndex: 1

    onPageNumChanged: {
        settingSideBarItemDelegateRet.color = pageNum === currentIndex ? "#3790FA" :"transparent"
        settingBarImage.source = pageNum === currentIndex ? settingBarWhiteSource : settingBarSource
        settingBarLabel.color = pageNum === currentIndex ? "white" : "black"
    }

    signal changePageNum(int index)

    Row{
        anchors.fill: parent
        leftPadding: 10
        Image {
            id: settingBarImage
            source: pageNum === currentIndex ? settingBarWhiteSource : settingBarSource
            anchors.verticalCenter: parent.verticalCenter
        }

        Label{
            id: settingBarLabel
            text: settingBarText
            leftPadding: 10
            anchors.verticalCenter: parent.verticalCenter
            color: pageNum === currentIndex ? "white" : "black"
        }
    }
    MouseArea{
        id: recorderMouseArea
        Accessible.name: "kylin-aiassistant_HistoryPage_recorderMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            if(pageNum !== currentIndex){
                settingSideBarItemDelegateRet.color = "#3790FA"
                settingBarImage.source = settingBarWhiteSource
                settingBarLabel.color = "white"
            }
        }
        onExited: {
            if(pageNum !== currentIndex){
                settingSideBarItemDelegateRet.color = "transparent"
                settingBarImage.source = settingBarSource
                settingBarLabel.color = "black"
            }
        }
        onPressed: {
            if(pageNum !== currentIndex){
                settingSideBarItemDelegateRet.color = "#3790FA"
                settingBarImage.source = settingBarWhiteSource
                settingBarLabel.color = "white"
            }
        }
        onReleased: {
            if(pageNum !== currentIndex){
                settingSideBarItemDelegateRet.color = "#3790FA"
                settingBarImage.source = settingBarWhiteSource
                settingBarLabel.color = "white"
            }
        }

        onClicked: {
            if(pageNum !== currentIndex){
                changePageNum(currentIndex)
            }
        }
    }
}
