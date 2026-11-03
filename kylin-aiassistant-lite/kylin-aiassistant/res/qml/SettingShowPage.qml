import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 设置页面显示区域
Rectangle {
    id: settingShowPageRet
    visible: true
    color: "#FFFFFF"

    property int currentPageNum: 1

    signal changeTransparentSettingShowPage(var flag)
    signal settingSubPageNumChanged(int pageNum)  // 通知子页面号变化

    onCurrentPageNumChanged: {
        if (currentPageNum !== settingSideBar.settingPageNum) {
            settingSideBar.settingPageNum = currentPageNum
        }
        // 通知主窗口子页面号变化
        settingSubPageNumChanged(currentPageNum)
    }

    RowLayout{
        anchors.fill: parent
        spacing: 0
        SettingSideBar{
            id: settingSideBar
            width: 232
            Layout.fillHeight: true
            settingPageNum: 1
            onChangSettingPageNum: function(pageNum){
                currentPageNum = pageNum
            }
        }

        SettingContentBar{
            id: settingContentBar
            Layout.fillWidth: true
            Layout.fillHeight: true
            pageNum: currentPageNum
            onChangeTransparentContentBar: function(flag){
                changeTransparentSettingShowPage(flag)
            }
        }
    }
}
