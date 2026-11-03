import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtQuick.Layouts 1.12


Rectangle {
   id: smallSideBarView
   visible: true
   color: "#F5F5F5"
   //color: "#FFFFFF"
   z: 10

   property int smallCurrentPageNum: 1

   onSmallCurrentPageNumChanged: {
       chatRet.color = smallCurrentPageNum === 1 ? "#3790FA" : "transparent"
       speechRet.color = smallCurrentPageNum === 2 ? "#3790FA" : "transparent"
       //mettingRet.color = smallCurrentPageNum === 4 ? "#3790FA" : "transparent"
       knowledgeRet.color = smallCurrentPageNum === 5 ? "#3790FA" : "transparent"
       historyRet.color = smallCurrentPageNum === 6 ? "#3790FA" : "transparent"
       settingRet.color = smallCurrentPageNum === 7 ? "#3790FA" : "transparent"
       chatRetIcon.source = smallCurrentPageNum === 1 ? "qrc:/icon/icon/chat-white.svg" : "qrc:/icon/icon/chat-black.svg"
       speechRetIcon.source = smallCurrentPageNum === 2 ? "qrc:/icon/icon/speech-white.svg" : "qrc:/icon/icon/speech-black.svg"
       //mettingRetIcon.source = smallCurrentPageNum === 4 ? "qrc:/icon/icon/metting-white.svg" : "qrc:/icon/icon/metting-black.svg"
       knowledgeRetIcon.source = smallCurrentPageNum === 5 ? "qrc:/icon/icon/knowledge-white.svg" : "qrc:/icon/icon/knowledge-black.svg"
       historyRetIcon.source = smallCurrentPageNum === 6 ? "qrc:/icon/icon/history-white.svg" : "qrc:/icon/icon/history-black.svg"
       settingRetIcon.source = smallCurrentPageNum === 7 ? "qrc:/icon/icon/setting-white.svg" : "qrc:/icon/icon/setting-black.svg"
   }

   Column {
       id: smallSideBarColumn
       anchors.fill: parent
       spacing: 8

       // 问答页面
       Rectangle{
           id: chatRet
           Accessible.name: "kylin-aiassistant_smallSideBarView_chatRet"
           width: 36
           height: 36
           color: smallCurrentPageNum === 1 ? "#3790FA" : "transparent"
           radius: 6
           anchors.horizontalCenter: parent.horizontalCenter
           ToolTip {
               delay: Qt.styleHints.mousePressAndHoldInterval
               visible: chatRetMouseArea.containsMouse
               property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
               text: qsTr("Chat")
               property var wrapMode: Text.WrapAnywhere
           }
           MouseArea{
               id: chatRetMouseArea
               anchors.fill: parent
               hoverEnabled: true
               onEntered: {
                   if(smallCurrentPageNum !== 1){
                       chatRet.color = Qt.rgba(0, 0, 0, 0.05)
                   }
               }
               onExited: {
                   if(smallCurrentPageNum !== 1){
                       chatRet.color = "transparent"
                   }
               }
               onPressed: {
                   if(smallCurrentPageNum !== 1){
                       chatRet.color = "#3790FA"
                       chatRetIcon.source = "qrc:/icon/icon/chat-white.svg"
                   }
               }
               onReleased: {
                   if(smallCurrentPageNum !== 1){

                   }
               }
               onClicked: {
                   root.changeSettingPageByModeOrPageNum = 2
                   root.onChangeWindowPage(1)
               }
           }

           RowLayout {
               anchors.fill: parent
               Image {
                   id: chatRetIcon
                   Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                   source: smallCurrentPageNum === 1 ? "qrc:/icon/icon/chat-white.svg" : "qrc:/icon/icon/chat-black.svg"
               }
           }
       }

       // 语音通话页面
       Rectangle{
           id: speechRet
           Accessible.name: "kylin-aiassistant_smallSideBarView_historyRet"
           width: 36
           height: 36
           // 3月份版本移除语音通话
           visible: false
           color: smallCurrentPageNum === 2 ? "#3790FA" : "transparent"
           radius: 6
           anchors.horizontalCenter: parent.horizontalCenter
           ToolTip {
               delay: Qt.styleHints.mousePressAndHoldInterval
               visible: speechRetMouseArea.containsMouse
               property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
               text: qsTr("Voice call")
               property var wrapMode: Text.WrapAnywhere
           }
           MouseArea{
               id: speechRetMouseArea
               anchors.fill: parent
               hoverEnabled: true
               onEntered: {
                   if(smallCurrentPageNum !== 2){
                       speechRet.color = Qt.rgba(0, 0, 0, 0.05)
                   }
               }
               onExited: {
                   if(smallCurrentPageNum !== 2){
                       speechRet.color = "transparent"
                   }
               }
               onPressed: {
                   if(smallCurrentPageNum !== 2){
                       speechRet.color = "#3790FA"
                       speechRetIcon.source = "qrc:/icon/icon/speech-white.svg"
                   }
               }
               onReleased: {
                   if(smallCurrentPageNum !== 2){

                   }
               }
               onClicked: {
                   root.changeSettingPageByModeOrPageNum = 2
                   root.onChangeWindowPage(2)
               }
           }
           RowLayout {
               anchors.fill: parent
               Image {
                   id: speechRetIcon
                   Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                   source: smallCurrentPageNum === 2 ? "qrc:/icon/icon/speech-white.svg" : "qrc:/icon/icon/speech-black.svg"
               }
           }
       }

       Rectangle{
           id: elasticitySideBarRet
           width: 36
           // 固定高度项：问答 + 历史 + 设置（+ 知识库可见时再加一项）
           height: smallSideBarView.height - 36 * 3 - smallSideBarColumn.spacing * 4 - (knowledgeRet.visible ? (36 + 4) : 0)
           // height: smallSideBarView.height - 36 * 6 - smallSideBarColumn.spacing * 7
           color: "transparent"
           anchors.horizontalCenter: parent.horizontalCenter
       }

       // 知识库管理页面
       Rectangle{
           id: knowledgeRet
           Accessible.name: "kylin-aiassistant_smallSideBarView_knowledgeRet"
           width: 36
           height: 36
           color: smallCurrentPageNum === 5 ? "#3790FA" : "transparent"
           radius: 6
           anchors.horizontalCenter: parent.horizontalCenter
           //enabled: globals.isKnowledgeBaseSupported
           opacity: globals.isKnowledgeBaseSupported ? 1 : 0.3
           ToolTip {
               delay: Qt.styleHints.mousePressAndHoldInterval
               visible: knowledgeRetMouseArea.containsMouse
               property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
               text: qsTr("Knowledge")
               property var wrapMode: Text.WrapAnywhere
           }
           MouseArea{
               id: knowledgeRetMouseArea
               anchors.fill: parent
               hoverEnabled: true
               onEntered: {
                   if(!globals.isKnowledgeBaseSupported){
                       return
                   }
                   if(smallCurrentPageNum !== 5){
                       knowledgeRet.color = Qt.rgba(0, 0, 0, 0.05)
                   }
               }
               onExited: {
                   if(!globals.isKnowledgeBaseSupported){
                       return
                   }
                   if(smallCurrentPageNum !== 5){
                       knowledgeRet.color = "transparent"
                   }
               }
               onPressed: {
                   if(!globals.isKnowledgeBaseSupported){
                       return
                   }
                   if(smallCurrentPageNum !== 5){
                       knowledgeRet.color = "#3790FA"
                       knowledgeRetIcon.source = "qrc:/icon/icon/knowledge-white.svg"
                   }
               }
               onReleased: {
                   if(!globals.isKnowledgeBaseSupported){
                       return
                   }
                   if(smallCurrentPageNum !== 5){

                   }
               }
               onClicked: {
                   if(!globals.isKnowledgeBaseSupported){
                       return
                   }
                   root.changeSettingPageByModeOrPageNum = 2
                   root.onChangeWindowPage(5)
               }
           }
           RowLayout {
               anchors.fill: parent
               Image {
                   id: knowledgeRetIcon
                   Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                   source: smallCurrentPageNum === 5 ? "qrc:/icon/icon/knowledge-white.svg" : "qrc:/icon/icon/knowledge-black.svg"
               }
           }
       }

       // 历史会话页面
       Rectangle{
           id: historyRet
           Accessible.name: "kylin-aiassistant_smallSideBarView_historyRet"
           width: 36
           height: 36
           color: smallCurrentPageNum === 6 ? "#3790FA" : "transparent"
           radius: 6
           anchors.horizontalCenter: parent.horizontalCenter
           ToolTip {
               delay: Qt.styleHints.mousePressAndHoldInterval
               visible: historyRetMouseArea.containsMouse
               property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
               text: qsTr("History")
               property var wrapMode: Text.WrapAnywhere
           }
           MouseArea{
               id: historyRetMouseArea
               anchors.fill: parent
               hoverEnabled: true
               onEntered: {
                   if(smallCurrentPageNum !== 6){
                       historyRet.color = Qt.rgba(0, 0, 0, 0.05)
                   }
               }
               onExited: {
                   if(smallCurrentPageNum !== 6){
                       historyRet.color = "transparent"
                   }
               }
               onPressed: {
                   if(smallCurrentPageNum !== 6){
                       historyRet.color = "#3790FA"
                       historyRetIcon.source = "qrc:/icon/icon/history-white.svg"
                   }
               }
               onReleased: {
                   if(smallCurrentPageNum !== 6){

                   }
               }
               onClicked: {
                   root.changeSettingPageByModeOrPageNum = 2
                   root.onChangeWindowPage(6)
               }
           }
           RowLayout {
               anchors.fill: parent
               Image {
                   id: historyRetIcon
                   Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                   source: smallCurrentPageNum === 6 ? "qrc:/icon/icon/history-white.svg" : "qrc:/icon/icon/history-black.svg"
               }
           }
       }

       // 设置页面
       Rectangle{
           id: settingRet
           Accessible.name: "kylin-aiassistant_smallSideBarView_settingRet"
           width: 36
           height: 36
           color: smallCurrentPageNum === 7 ? "#3790FA" : "transparent"
           radius: 6
           anchors.horizontalCenter: parent.horizontalCenter
           ToolTip {
               delay: Qt.styleHints.mousePressAndHoldInterval
               visible: settingRetMouseArea.containsMouse
               property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
               text: qsTr("Setting")
               property var wrapMode: Text.WrapAnywhere
           }
           MouseArea{
               id: settingRetMouseArea
               anchors.fill: parent
               hoverEnabled: true
               onEntered: {
                   if(smallCurrentPageNum !== 7){
                       settingRet.color = Qt.rgba(0, 0, 0, 0.05)
                   }
               }
               onExited: {
                   if(smallCurrentPageNum !== 7){
                       settingRet.color = "transparent"
                   }
               }
               onPressed: {
                   if(smallCurrentPageNum !== 7){
                       settingRet.color = "#3790FA"
                       settingRetIcon.source = "qrc:/icon/icon/setting-white.svg"
                   }
               }
               onReleased: {
                   if(smallCurrentPageNum !== 7){

                   }
               }
               onClicked: {
                   root.changeSettingPageByModeOrPageNum = 2
                   root.onChangeWindowPage(7)
               }
           }
           RowLayout {
               anchors.fill: parent
               Image {
                   id: settingRetIcon
                   Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                   source: smallCurrentPageNum === 7 ? "qrc:/icon/icon/setting-white.svg" : "qrc:/icon/icon/setting-black.svg"
               }
           }
       }
   }
   Connections{
       target: msgPane
   }
}
