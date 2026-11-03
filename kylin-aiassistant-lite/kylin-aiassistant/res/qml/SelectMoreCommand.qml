import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle{
    visible: true
    width: 180
    height: moreCommandflickable.height
    id:secondFlow
    Accessible.name: "kylin-aiassistant_SelectMoreCommand_secondFlow"
    color:"transparent"
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐

    property int activeCommandID: -1
    property string  activeCommand: ""

    function getTruncateCommandLabel(text, maxChars) {
        let totalUnits = 0;
        let truncatedText = "";

        for (let i = 0; i < text.length; i++) {
            let currentChar = text.charAt(i);

            // 判断是否为中文字符
            if (currentChar >= '\u4E00' && currentChar <= '\u9FFF') {
                // 中文字符占用 2 个单位
                totalUnits += 2;
            } else {
                // 英文或其他字符占用 1 个单位
                totalUnits += 1;
            }

            // 如果总单位数超过了允许的最大值，则截断
            if (totalUnits > maxChars * 2) {
                truncatedText += "...";
                break;
            }

            // 追加当前字符
            truncatedText += currentChar;
        }

        return truncatedText;
    }

    Flickable{
        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
        id:moreCommandflickable
        Accessible.name: "kylin-aiassistant_SelectMoreCommand_moreCommandflickable"
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        width:180
        height: Math.min(moreCommandRepeater.implicitHeight, 36*10+2*9)  // 最多显示 10 行
        contentHeight: moreCommandRepeater.implicitHeight
        boundsBehavior: Flickable.StopAtBounds  // 防止过度滚动
        clip: true

        // 自定义垂直滚动条
        ScrollBar.vertical: ScrollBar {
            visible: false
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
        }
        property bool isCommandContentOverflowing: false
        onContentHeightChanged: {
            // 判断内容是否超出容器高度，超出显示滚动条
            isCommandContentOverflowing = contentHeight > height
            if(isCommandContentOverflowing){
                console.log("isCommandContentOverflowing TRUE")
                moreCommandflickable.ScrollBar.vertical.visible = true
                secondFlow.width = 180 + 16
                moreCommandflickable.width = 180 +16
                selectMoreCommand.setWidgetWidth(secondFlow.width);

            }else{
                console.log("isCommandContentOverflowing FALSE")
                moreCommandflickable.ScrollBar.vertical.visible = false
                 secondFlow.width = 180
                moreCommandflickable.width = 180
                selectMoreCommand.setWidgetWidth(secondFlow.width);
            }
            onSetHeight()
        }
        Flow{
            id:moreCommandRepeater
            Accessible.name: "kylin-aiassistant_SelectMoreCommand_moreCommandRepeater"
            width:moreCommandflickable.width
            height: implicitHeight
            clip: true
            spacing: 2
            leftPadding: 8
            rightPadding: 8

            Repeater {
                model: commandTemplateListModel;
                delegate: Rectangle {
                width:180-16
                height:36;
                color:"white"
                visible: index >2 //当前显示异常是因为索引显示的并不是第一个值
                radius: 4
                enabled: isActive
                Accessible.name: "kylin-aiassistant_SelectMoreCommand_Repeater"
                Row {
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    spacing: 8

                    Image {
                        id: moreButtonImage
                        width:16
                        height:16
                        Accessible.name: "kylin-aiassistant_SelectMoreCommand_moreButtonImage"
                        source: iconPath
                        smooth: true //启动平滑缩放
                        opacity: isActive ? 1 : 0.35
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        id: moreButtonText;
                        Accessible.name: "kylin-aiassistant_SelectMoreCommand_moreButtonText"
                        text: getTruncateCommandLabel(name, 8); // 中文不超过 8 个字符，英文不超过 16 个字符
                        color: isActive ? Qt.rgba(0/255, 0/255, 0/255,0.85) : Qt.rgba(0/255, 0/255, 0/255,0.35);
                        anchors.verticalCenter: parent.verticalCenter
                        width: 124
                        elide: Text.ElideRight
                        clip: true
                    }
                    // 悬浮提示框
                    ToolTip {
                        id: moreButtonTextTooltip
                        visible: false   // 默认不显示v
                        delay:Qt.styleHints.mousePressAndHoldInterval
                        text: name
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }
                    // 用于测量文本宽度的 TextMetrics 用于显示长度超出时的计算
                    Label {
                        id: moreButtonTextMetrics
                        Accessible.name: "kylin-aiassistant_Chat_askTextMetrics"
                        text: name
                        visible: false
                    }
                }

                MouseArea {
                        id: moreCommandMouseArea
                        Accessible.name: "kylin-aiassistant_SelectMoreCommand_MouseArea"
                        anchors.fill: parent
                        hoverEnabled: true
                        property bool isEntered: false
                        onEntered: {
                            if (moreButtonTextMetrics.width > moreButtonText.width) {
                                moreButtonTextTooltip.visible = true
                            } else {
                                moreButtonTextTooltip.visible = false
                            }
                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0.15);
                            isEntered = true
                        }
                        onExited: {
                            moreButtonTextTooltip.visible = false
                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0);
                            isEntered = false
                        }
                        onPressed: {
                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0.2)
                        }
                        onPressedChanged: {
                            if(!moreCommandMouseArea.pressed) {
                                parent.color = Qt.rgba(0/255, 0/255, 0/255,0)
                                if(isEntered){
                                    secondFlow.activeCommand = name
                                    secondFlow.activeCommandID = actorId
                                    selectMoreCommand.sendButtonText(secondFlow.activeCommand,secondFlow.activeCommandID)
                                }
                            }
                        }
                    }

                }
                }

          }

        }

    function onSetWidget(width){
        secondFlow.width = width;
    }
    function   onSetHeight(){
        secondFlow.height =  moreCommandflickable.height+16;
        selectMoreCommand.setWidgetHeight(secondFlow.height);
    }
    // 处理按键按下事件
    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            console.log("Escape键被按下")
            event.accepted = true  // 标记事件已处理
        } else if (event.key === Qt.Key_Space) {
            console.log("空格键被按下")
        }
    }
}



