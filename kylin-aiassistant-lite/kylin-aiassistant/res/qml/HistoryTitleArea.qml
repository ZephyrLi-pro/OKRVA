import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

// 全部，收藏，搜索区域
Row{
    id: titleRow
    topPadding: 24
    leftPadding: 24
    rightPadding: 24
    height: 70
    spacing: 10

    property int titleChooseIndex: 1  // 1全部 or 2收藏

    function initSearchArea(){
        searchTextArea.initSearchTextAreaRet()
    }

    signal initHistoryOperatePage() // 初始化历史对话操作栏

    onTitleChooseIndexChanged: {
        if(chooseIndex === 1){
            allLabel.color = "#FFFFFF"
            allRet.color = "#3790FA"
            collectLabel.color = "#000000"
            collectRet.color = "transparent"
            initSearchArea()
        }else{
            allLabel.color = "#000000"
            allRet.color = "transparent"
            collectLabel.color = "#FFFFFF"
            collectRet.color = "#3790FA"
            initSearchArea()
        }
    }

    // 全部按钮
    Rectangle{
        id: allRet
        width: 72
        height: 36
        radius: 6
        color: "#3790FA"
        Label{
            id: allLabel
            text: qsTr("All")
            color: (chooseIndex === 1 ? "#FFFFFF" : "#000000")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        MouseArea{
            id: allRetMousrArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                if(chooseIndex !== 1){
                    allRet.color = Qt.rgba(0, 0, 0, 0.05)
                }
            }
            onExited: {
                if(chooseIndex !== 1){
                    allRet.color = "transparent"
                }
            }
            onPressed: {
                if(chooseIndex !== 1){
                    allRet.color = "#3790FA"
                    allLabel.color = "white"
                }
            }
            onReleased: {
                if(chooseIndex !== 1){

                }
            }
            onClicked: {
                if(chooseIndex !== 1){
                    chooseIndex = 1
                    collectRet.color = "transparent"
                    collectLabel.color = "black"
                    isEditFlag = false
                    selectAllButtonChecked = false
                    filterList(showPageSearchText)
                }
            }
        }
    }

    //收藏
    Rectangle{
        id: collectRet
        width: 72
        height: 36
        radius: 6
        color: "transparent"//"#3790FA"
        Label{
            id: collectLabel
            text: qsTr("Collect")
            //color: (chooseIndex === 2 ? "#FFFFFF" : "#000000")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.centerIn: parent
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
        }
        MouseArea{
            id: collectRetMousrArea
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                if(chooseIndex !== 2){
                    collectRet.color = Qt.rgba(0, 0, 0, 0.05)
                }
            }
            onExited: {
                if(chooseIndex !== 2){
                    collectRet.color = "transparent"
                }
            }
            onPressed: {
                if(chooseIndex !== 2){
                    collectRet.color = "#3790FA"
                    collectLabel.color = "white"
                }
            }
            onReleased: {
                if(chooseIndex !== 2){

                }
            }
            onClicked: {
                if(chooseIndex !== 2){
                    chooseIndex = 2
                    allRet.color = "transparent"
                    allLabel.color = "black"
                    isEditFlag = false
                    selectAllButtonChecked = false
                    filterList(showPageSearchText)
                }
            }
        }
    }

    // 弹簧
    Rectangle{
        id: elasticityRet
        width: parent.width - allRet.width - collectRet.width - searchTextArea.width - titleRow.spacing * 3 -
               titleRow.leftPadding - titleRow.rightPadding
        height: 36
        color: "transparent"
    }

    // 搜索输入框
    SearchTextArea{
        id: searchTextArea
        width: 240
        searchTextAreaRetHeight: 36
        visible: root.winMode === 3
        onSearchTextAreaTextChanged: function(changeText){
            historyShowPageRet.showPageSearchText = changeText
            filterList(changeText)
            initHistoryOperatePage()
        }
    }
}
