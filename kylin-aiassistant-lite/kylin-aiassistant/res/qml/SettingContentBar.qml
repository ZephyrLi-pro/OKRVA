import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 设置页面右侧内容区域
Rectangle {
    id: settingContentBarRet
    color:  "#FFFFFF"

    property int pageNum: 1
    property int maxWidth: 992

    signal changeTransparentContentBar(var flag)

    onPageNumChanged: {
        onChangeWindowPage(pageNum)
    }

    // 窗口模式切换页面
    function onChangeWindowPage(pageNum){
        if(pageNum === 1){
            stackView.clear()
            stackView.push(settingNormalPage)
        }else if(pageNum === 2){
            stackView.clear()
            stackView.push(settingChatCommandPage)
        }else if(pageNum === 3){
            stackView.clear()
            // todo knowledge 0303
            stackView.push(settingSelectPage)
        }else if(pageNum === 5){
            stackView.clear()
            stackView.push(settingSelectPage)
        }
    }

    RowLayout{
        //anchors.fill: parent
        width: Math.min(settingContentBarRet.width, maxWidth)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter

        spacing: 0

        // 设置页面右侧内容展示区域
        StackView{
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: settingNormalPage // 设置初始页面
            visible: true
        }

        SettingNormalPage{
            id: settingNormalPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: false
        }

        SettingChatCommandPage{
            id: settingChatCommandPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: false
            onChangeTransparentCommandPage: function(flag){
                changeTransparentContentBar(flag)
            }
        }

        SettingSelectPage{
            id: settingSelectPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: false
        }

    }

}
