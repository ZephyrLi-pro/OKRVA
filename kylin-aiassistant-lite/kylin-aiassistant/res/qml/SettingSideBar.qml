import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 设置页面导航区域
Rectangle {
    id: settingSideBarRet
    color: "#FFFFFF"

    property int settingPageNum: 1

    signal changSettingPageNum(int pageNum)

    ListModel {
        id: settingSidebarListModel
        ListElement{source: "qrc:/icon/icon/setting-black.svg"; whiteSource: "qrc:/icon/icon/setting-white.svg"; labelText: qsTr("General")}
        ListElement{source: "qrc:/icon/icon/chat-black.svg"; whiteSource: "qrc:/icon/icon/chat-white.svg"; labelText: qsTr("Q&A Commands")}
        ListElement{source: "qrc:/icon/icon/select-black.svg"; whiteSource: "qrc:/icon/icon/select-white.svg"; labelText: qsTr("Global Word Selection")}
    }

    ColumnLayout{
        id: settingSideBarColumn
        anchors.fill: parent
        Label{
            id: settingTitleLabel
            text: qsTr("Settings")
            leftPadding: 34
            topPadding: 6
            bottomPadding: 6
        }

        ScrollView {
            id: settingSideBarScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.horizontal.visible : false
            ScrollBar.vertical.visible : false
            clip: true
            leftPadding: 24
            rightPadding: 24
            ListView {
                id: settingSideBarListView
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                boundsMovement: Flickable.StopAtBounds
                cacheBuffer: 5000
                model: settingSidebarListModel
                spacing: 2
                delegate: SettingSideBarItemDelegate{
                    width: parent.width
                    height: 36
                    settingBarSource: model.source
                    settingBarWhiteSource: model.whiteSource
                    settingBarText: model.labelText
                    currentIndex: (model.index + 1)
                    pageNum: settingPageNum
                    onChangePageNum: function(index){
                        settingPageNum = index
                        changSettingPageNum(settingPageNum)
                    }
                }
            }
        }
    }
}
