import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtQuick.Layouts 1.12


Rectangle {
    id: sideBarView
    visible: true
    color: "#FFFFFF"
    Accessible.name: "kylin-aiassistant_SideBar_sideBarView"
    ColumnLayout {
        anchors.fill: parent

        // 新建对话按钮
        Row {
            id: sidebarRow
            width:parent.width
            leftPadding: 10
            rightPadding: 10
            topPadding: 4
            bottomPadding: 0
            Accessible.name: "kylin-aiassistant_SideBar_sidebarRow"
            Rectangle {
                id: createButtonRec
                width: sideBarView.width - sidebarRow.leftPadding - sidebarRow.rightPadding
                height: 36
                radius: 4
                color: "#3790FA"
                Accessible.name: "kylin-aiassistant_SideBar_Rectangle_1"

                Row {
                    id: createButtonRow
                    anchors.centerIn: parent
                    spacing: 8
                    leftPadding: 8
                    rightPadding: 8
                    Accessible.name: "kylin-aiassistant_SideBar_Row_1"

                    Image {
                        id: createButtonImage
                        width: 16
                        height: 16
                        Accessible.name: "kylin-aiassistant_SideBar_createButtonImage"
                        source: "qrc:/icon/icon/sidebar/create_dialogue.svg"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        id: ctreateButtonText
                        text: qsTr("New conversation")
                        Accessible.name: "kylin-aiassistant_SideBar_ctreateButtonText"
                        color: "#FFFFFF"
                        anchors.verticalCenter: parent.verticalCenter
                        width: createButtonTextMetrics.implicitWidth <= (createButtonRec.width - createButtonImage.width - createButtonRow.spacing - createButtonRow.leftPadding - createButtonRow.rightPadding)
                        ?
                            createButtonTextMetrics.implicitWidth
                        :
                            (createButtonRec.width - createButtonImage.width - createButtonRow.spacing - createButtonRow.leftPadding - createButtonRow.rightPadding)
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                    Label {
                        id: createButtonTextMetrics
                        Accessible.name: "kylin-aiassistant_SideBar_createButtonTextMetrics"
                        text: ctreateButtonText.text
                        visible: false
                    }

                    // 悬浮提示框
                    ToolTip {
                        id: createButtonToolTip
                        Accessible.name: "kylin-aiassistant_SideBar_createButtonToolTip"
                        visible: false // 默认不显示
                        delay: Qt.styleHints.mousePressAndHoldInterval
                        text: ctreateButtonText.text
                        property var maxWidth:  400
                        property var wrapMode: Text.WrapAnywhere
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    Accessible.name: "kylin-aiassistant_SideBar_MouseArea_1"

                    // 按钮三态
                    onEntered: {
                        // 当用户名宽度大于组件显示省略号时，悬浮显示提示
                        if (createButtonTextMetrics.implicitWidth > ctreateButtonText.width) {
                            createButtonToolTip.visible = true
                        } else {
                            createButtonToolTip.visible = false
                        }

                        parent.color = "#3489EE";
                    }
                    onExited: {
                        createButtonToolTip.visible = false
                        parent.color = "#3790FA";
                    }
                    onPressed: {
                        parent.color = "#2C73C8";
                    }
                    onReleased: {
                        parent.color = "#3489EE";
                    }
                    onClicked: {
                        if(mychat.isReplying){
                           msgPane.onContinueDialog();
                        }else{
                            //msgPane.onStopSpeak();//停止朗读
                            //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
                            //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
                            //页面切换
                            var winMode = msgPane.getCurWinMode()
                            console.log("当前模式:" + winMode)
                            if(winMode === 2){
                                // 侧边栏模式页面切换
                                msgPane.onShowPage(1)
                            }else if(winMode === 3){
                                // 窗口模式页面切换
                                msgPane.onShowPage(4)
                                msgPane.updateDialogueListSessionId()
                            }

                            root.onClearListModel()
                            root.onInitReadingIndex()
                            if(mychat && mychat.isReplying) {
                                msgPane.deactivateTaskSession()
                            } else {
                                msgPane.closeSession()
                            }
                            root.onClearMessageField()
                        }
                    }
                }
            }
        }

         // 标签页切换栏
        Rectangle {
            id: changeRet
            Layout.fillWidth: true
            Layout.fillHeight: true
            Accessible.name: "kylin-aiassistant_SideBar_changeRet"
            color: "#FFFFFF"
            property int currentIndex: 0
            Connections {
                target: changeRet
                onCurrentIndexChanged: function(){
                    if (changeRet.currentIndex === 0) {
                        if (stackView.currentItem !== historyPageComponent) {
                            stackView.clear()
                            stackView.push(historyPageComponent)
                        }
                    } else {
                        if (stackView.currentItem !== pluginPageComponent) {
                            stackView.clear()
                            stackView.push(pluginPageComponent)
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                Rectangle {
                    id: tabBarRet
                    width: 200
                    Layout.preferredHeight: 36
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                    Accessible.name: "kylin-aiassistant_SideBar_tabBarRet"
                    RowLayout {
                        anchors.centerIn: parent
                        id: tabBarRow
                        spacing: 0

                        //Chat history
                        Rectangle {
                            color: "#FFFFFF"
                            border.color: "transparent"
                            border.width: 0

                            // 在RowLayout中使用Layout.preferredWidth
                            Layout.preferredWidth: (sideBarView.width - sidebarRow.leftPadding - sidebarRow.rightPadding) / 2
                            Layout.preferredHeight: 36
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                            Label {
                                id: chatHistory
                                text: qsTr("Chat history")
                                color: (changeRet.currentIndex === 0 ? "#3790FA" : "#000000")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent

                                // 设置超出宽度显示...
                                width: parent.width
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }

                            // 下边框
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 2
                                color: (changeRet.currentIndex === 0 ? "#3790FA" : "#0d262626")
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                // 添加悬浮提示
                                ToolTip {
                                    visible: (chatHistory.implicitWidth > chatHistory.width) && containsMouse
                                    text: qsTr("Chat history")
                                    delay: 500
                                }

                                onClicked: {
                                    changeRet.currentIndex = 0
                                    root.onCloseCommand()
                                }
                            }
                        }

                        // AI Plugin Applications
                        Rectangle {
                            color: "#FFFFFF"
                            border.color: "transparent"
                            border.width: 0

                            // ✅ 在RowLayout中使用Layout.preferredWidth
                            Layout.preferredWidth: (sideBarView.width - sidebarRow.leftPadding - sidebarRow.rightPadding) / 2
                            Layout.preferredHeight: 36
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                            Label {
                                id: aiPluginText
                                text: qsTr("AI Plugin Applications")
                                color: (changeRet.currentIndex === 1 ? "#3790FA" : "#000000")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                anchors.centerIn: parent

                                //  超出宽度显示省略号
                                width: parent.width
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }

                            // 下边框
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 2
                                color: (changeRet.currentIndex === 1 ? "#3790FA" : "#0d262626")
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true

                                // 添加悬浮提示
                                ToolTip {
                                    visible: (aiPluginText.implicitWidth > aiPluginText.width) && containsMouse
                                    text: qsTr("AI Plugin Applications")
                                    delay: 500
                                }

                                onEntered: {
                                    console.log("AI Plugin MouseArea entered!")
                                    console.log("Text needs tooltip:", aiPluginText.implicitWidth > aiPluginText.width)
                                }

                                onClicked: {
                                    changeRet.currentIndex = 1
                                    root.onCloseCommand()
                                }
                            }
                        }
                    }

                    // 初始页面设置
                    Component.onCompleted: {
                        if (changeRet.currentIndex === 0) {
                            stackView.push(historyPageComponent)
                        } else {
                            stackView.push(pluginPageComponent)
                        }
                    }

                    // 页面切换时，重新加载历史记录
                    onVisibleChanged: {
                        if(sideBarView.visible){
                            if (changeRet.currentIndex === 0) {
                                if (stackView.currentItem !== historyPageComponent) {
                                    stackView.clear()
                                    stackView.push(historyPageComponent)
                                }
                            } else {
                                if (stackView.currentItem !== pluginPageComponent) {
                                    stackView.clear()
                                    stackView.push(pluginPageComponent)
                                }
                            }
                        }
                    }
                }


                // 堆叠视图
                StackView {
                    id: stackView
                    Accessible.name: "kylin-aiassistant_SideBar_stackView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop

                    // 历史对话页面
                    Component {
                        id: historyPageComponent

                        Loader {
                            source: "HistoryPage.qml"
                        }

                    }

                    // 插件页面
                    Component {
                        id: pluginPageComponent

                        Loader {
                            source: "PluginPage.qml"
                        }
                    }
                }
            }
        }
    }

    function onBackBtnClick(){
        //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
        //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
        var currentWindowMode = msgPane.getCurWinMode()
        //侧边栏模式返回侧边栏模式下的主页
        if(currentWindowMode === 2)
        {
            msgPane.onShowPage(1)
        }
        //窗口模式返回窗口模式下的主页
        else if(currentWindowMode ===3){
            msgPane.onShowPage(4)
        }
    }
}
