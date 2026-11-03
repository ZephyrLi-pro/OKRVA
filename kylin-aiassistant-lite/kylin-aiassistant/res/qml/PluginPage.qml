import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: pluginPage
    Accessible.name: "kylin-aiassistant_PluginPage_pluginPage"
    width: parent.width
    height: parent.height
    property int winMode: msgPane.getCurWinMode()
    property bool  showToolTip: false
//    color: "#F5F5F5"
    Connections{
        target: pluginPage
        onWinModeChanged: function(winMode){
            // 页面切换
            if(winMode === 2){
                msgPane.getPluginList()
            }else if(winMode === 3){
                msgPane.getPluginList()
            }
        }
    }

    // 管理插件的数据
    ListModel {
        id: pluginListModel
        Accessible.name: "kylin-aiassistant_PluginPage_pluginListModel"
    }

    Component.onCompleted: {
        if (msgPane === null || msgPane === undefined) {
            //console.log("msgPane is not defined in PluginPage.qml");
        }
        if (msgPane) {
            //console.log("msgPane object found, connecting signal");
            msgPane.getPluginList()
        }
    }

    ColumnLayout {
        anchors.fill: parent

        Rectangle {
            id: pluginPageRet
            Accessible.name: "kylin-aiassistant_PluginPage_pluginPageRet"
            color: "#FFFFFF"
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                id: pluginPageColumn
                Accessible.name: "kylin-aiassistant_PluginPage_pluginPageColumn"
                anchors.fill: parent
                leftPadding: 10
                rightPadding: 10
                topPadding: 8
                bottomPadding: 0

                Rectangle {
                    id: pluginPageRectangle
                    Accessible.name: "kylin-aiassistant_PluginPage_pluginPageRectangle"
                    width: parent.width - pluginPageColumn.leftPadding - pluginPageColumn.rightPadding
                    height: parent.height - pluginPageColumn.bottomPadding
                    color: "#FFFFFF"
                    // radius: 6

                    Column {
                        anchors.fill: parent
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0

                        // 插件列表
                        Rectangle {
                            height: pluginPage.height - pluginPageColumn.bottomPadding
                            width: pluginPage.width - pluginPageColumn.leftPadding - pluginPageColumn.rightPadding
                            color: "#FFFFFF"

                            // 滑动区域
                            ScrollView {
                                anchors.fill: parent
                                visible: pluginListModel.count > 0
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.minimumSize:this.height
                                //ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                                // 插件列表视图
                                ListView {
                                    id: pluginListView
                                    Accessible.name: "kylin-aiassistant_PluginPage_pluginListView"
                                    clip: true
                                    anchors.fill: parent
                                    model: pluginListModel
                                    spacing: 4

                                    // 插件项布局
                                    delegate: Item {
                                        width: parent.width
                                        height: 72

                                        Rectangle {
                                            id: pluginItem
                                            Accessible.name: "kylin-aiassistant_PluginPage_pluginItem"
                                            width: parent.width
                                            height: parent.height
                                            color: "#FFFFFF"
//                                            color: "red"
                                            radius: 6

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onEntered: {
                                                    //parent.color =  Qt.rgba(0/255, 0/255, 0/255, 0.05);
                                                    parent.color =  "#26000000"
                                                }
                                                onExited: {
                                                    parent.color = "#FFFFFF";
                                                }
                                                onPressed: {
                                                    //parent.color = Qt.rgba(0/255, 0/255, 0/255, 0.08);
                                                    parent.color =  "#33000000"
                                                }
                                                onReleased: {
                                                    parent.color = "#FFFFFF";
                                                }
                                                onClicked: {
                                                    // 切换到插件时正在说话，取消语音录制
                                                    // console.log("222222切换插件" + mychat.isCapturingVideo)
                                                    if (mychat.isCapturingVideo === true) {
                                                        msgPane.onCancelAudio();
                                                    }

                                                    console.log("onClicked跳转插件" + model.qmlPath)
                                                    var winMode = msgPane.getCurWinMode()
                                                    console.log("onClicked当前模式:" + winMode)

                                                    // 切换到插件时打断并初始化朗读
                                                    root.onStopReading();
                                                    // 页面切换
                                                    if(winMode === 2){
                                                        // 侧边栏模式页面切换
                                                        root.currentPageNum = 3
                                                        //msgPane.onShowPluginPage(0, model.qmlPath)
                                                         msgPane.onShowPage(3)
                                                    }else if(winMode === 3){
                                                        // 窗口模式页面切换
                                                        //msgPane.onShowWindowPluginPage(0, model.qmlPath)
                                                         msgPane.onShowPage(5)
                                                    }else{
                                                        return
                                                    }
                                                    root.onCloseCommand()
                                                }
                                            }

                                            Column {
                                                anchors.verticalCenter: parent.verticalCenter

                                                Row {
                                                    id:pluginItemImageRow
                                                    Accessible.name: "kylin-aiassistant_PluginPage_pluginItemImageRow"
                                                    spacing: 8
                                                    leftPadding: 10
                                                    rightPadding: 16

                                                    Image {
                                                        id: dialogueImage
                                                        Accessible.name: "kylin-aiassistant_PluginPage_dialogueImage"
                                                        source: model.image
                                                        width: 24
                                                        height: 24
                                                        anchors.verticalCenter: parent.verticalCenter
                                                    }

                                                    Label {
                                                        id: dialogueTitle
                                                        Accessible.name: "kylin-aiassistant_PluginPage_dialogueTitle"
                                                        text: model.pluginName
                                                        color: "#262626"
                                                        wrapMode: Text.NoWrap
                                                        anchors.verticalCenter: parent.verticalCenter
                                                    }

                                                    Item {
                                                        // 弹簧效果
                                                        Layout.fillHeight: true
                                                    }
                                                }

                                                Row {
                                                    id: pluginItemTextRow
                                                    Accessible.name: "kylin-aiassistant_PluginPage_pluginItemTextRow"
                                                    width: parent.width
                                                    spacing: 0
                                                    leftPadding: 10
                                                    rightPadding: 16

                                                    Label {
                                                        id: dialogueDescription
                                                        Accessible.name: "kylin-aiassistant_PluginPage_pluginItemTextRow"
                                                        text: model.pluginDescription
                                                        width: pluginItem.width - pluginItemTextRow.leftPadding - pluginItemTextRow.rightPadding
                                                        color: "#8c262626"
                                                        elide: Text.ElideRight
                                                        wrapMode: Text.NoWrap
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        MouseArea{
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            onEntered: {
                                                                // 当用户名宽度大于组件显示省略号时，悬浮显示提示
                                                                if (dialogueDescriptionMetrics.width > dialogueDescription.width) {
                                                                    dialogueTooltip.visible = true
                                                                } else {
                                                                    dialogueTooltip.visible = false
                                                                }
                                                                //pluginItem.color =  Qt.rgba(0/255, 0/255, 0/255, 0.05)
                                                                pluginItem.color = "#26000000"
                                                            }
                                                            onExited: {
                                                                dialogueTooltip.visible = false
                                                                pluginItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
                                                            }
                                                            onPressed: {
                                                                //pluginItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.08);
                                                                pluginItem.color = "#33000000"
                                                            }
                                                            onReleased: {
                                                                pluginItem.color = "#FFFFFF";
                                                            }
                                                            onClicked: {
                                                                // 切换到插件时正在说话，取消语音录制
                                                                if (mychat.isCapturingVideo === true) {
                                                                    msgPane.onCancelAudio();
                                                                }

                                                                console.log("Description onClicked跳转插件" + model.qmlPath)
                                                                var winMode = msgPane.getCurWinMode()
                                                                console.log("Description onClicked当前模式:" + winMode)
                                                                // 切换到插件时打断并初始化朗读
                                                                root.onStopReading();
                                                                // 页面切换
                                                                if(winMode === 2){
                                                                    // 侧边栏模式页面切换
                                                                     msgPane.onShowPage(3)
                                                                }else if(winMode === 3){
                                                                    // 窗口模式页面切换
                                                                     msgPane.onShowPage(5)
                                                                }else{
                                                                    return
                                                                }
                                                                root.onCloseCommand()
                                                            }
                                                        }
                                                        // 悬浮提示框
                                                        ToolTip {
                                                            id: dialogueTooltip
                                                            Accessible.name: "kylin-aiassistant_PluginPage_dialogueTooltip"
                                                            visible: false   // 默认不显示
                                                            delay: Qt.styleHints.mousePressAndHoldInterval
                                                            text: model.pluginDescription
                                                            property var maxWidth: 400
                                                            property var wrapMode: Text.WrapAnywhere
                                                        }
                                                    }
                                                    Label {
                                                        id: dialogueDescriptionMetrics
                                                        Accessible.name: "kylin-aiassistant_PluginPage_dialogueDescriptionMetrics"
                                                        text: model.pluginDescription
                                                        visible: false
                                                    }
                                                }
                                            }

                                        }
                                    }

                                    Connections
                                    {
                                        target: msgPane

                                        onLoadPluginList:function (list)
                                        {
                                            //console.log("onLoadPluginList" + list);
                                            //console.log(list.length);
                                            pluginListModel.clear();
                                            for(var i = 0; i < list.length; ++i){
                                                var item = JSON.parse(list[i]);
                                                pluginListModel.append(item);
                                            }
                                            //console.log("pluginListModel contents:");
                                            for (var j = 0; j < pluginListModel.count; j++) {
                                                var modelItem = pluginListModel.get(j);
                                                // console.log("Item " + j + ": pluginName = " + modelItem.pluginName + ", pluginDescription = " + modelItem.pluginDescription + ", qmlPath = " + modelItem.qmlPath + ", image = " + modelItem.image);
                                            }
                                        }
                                        onChangeWinModeToChat:function(winMode){
                                            pluginPage.winMode = winMode
                                            //console.log("winMode" + chat.winMode)
                                        }

                                    }

                                }

                            }

                        }


                    }

                }
            }

        }
    }

 }
