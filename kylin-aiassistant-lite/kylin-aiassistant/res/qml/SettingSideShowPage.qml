import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 设置页面侧边模式显示区域
Rectangle {
    id: settingSideShowPageRet
    visible: true
    color: "#FFFFFF"

    property int currentPageNum: 0

    signal changeTransparentSettingSideShowPage(var flag)
    signal settingSubPageNumChanged(int pageNum)  // 通知子页面号变化

    onCurrentPageNumChanged: {
        if (currentPageNum !== settingSideBar.settingPageNum) {
            settingSideBar.settingPageNum = currentPageNum
        }
        // 通知主窗口子页面号变化（只在显示内容页时通知，即 pageNum > 0）
        if (currentPageNum > 0) {
            settingSubPageNumChanged(currentPageNum)
            // 从窗口模式切换到侧边栏时，currentPageNum 由外部设置，需直接显示到对应的子页面内容（问答指令等内层）
            settingSideBar.visible = false
            settingContentBar.visible = true
            changeSettingSidePage()
        }
    }

    function settingSidePageBack(){
        currentPageNum = 0
        settingSideBar.settingPageNum = 0
        root.lastSettingSubPageNum = 0
        settingSideBar.visible = true
        settingContentBar.visible = false
    }

    signal changeSettingSidePage()

    RowLayout{
        anchors.fill: parent
        spacing: 0
        SettingSideBar{
            id: settingSideBar
            Layout.fillWidth: true
            Layout.fillHeight: true
            settingPageNum: 0
            onChangSettingPageNum: function(pageNum){
                currentPageNum = pageNum
                settingSideBar.visible = false
                settingContentBar.visible = true
                changeSettingSidePage()
            }
            visible: true
        }

        SettingContentBar{
            id: settingContentBar
            Layout.fillWidth: true
            Layout.fillHeight: true
            pageNum: currentPageNum
            visible: false
            onChangeTransparentContentBar: function(flag){
                changeTransparentSettingSideShowPage(flag)
            }
        }
    }
}
