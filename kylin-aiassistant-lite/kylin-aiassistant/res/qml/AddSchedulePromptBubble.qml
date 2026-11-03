import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * 添加日程提示气泡组件
 * 在生成待办完成后显示，询问用户是否要将待办添加到日程中
 */
Rectangle {
    id: addSchedulePromptBubble
    Accessible.name: "kylin-aiassistant_Chat_addSchedulePromptBubble"
    width: root.winMode === 2 ? parentWidth : Math.min(root.maxWidth, (promptTextMertics.width + iconCircle.width + headerRow.spacing + headerRow.leftPadding + headerRow.rightPadding))
    height: contentColumn.height
    color: "transparent"

    property int parentWidth: 0
    property bool isCanClick: true
    
    signal addScheduleClicked()  // 添加日程按钮点击信号，传递日期参数
    signal skipClicked()  // 暂不添加按钮点击信号
    
    Column {
        id: contentColumn
        width: parent.width
        spacing: 12
        
        Row {
            id: headerRow
            width: parent.width
            spacing: 12
            topPadding: 10
            leftPadding: 16
            rightPadding: 16
            
            // 蓝色圆形图标（左上角）
            // 绿色圆形图标（左上角）
            Image {
                id: iconCircle
                width: 16
                height: 20
                source: "qrc:/icon/icon/blue_question.svg"
                fillMode: Image.PreserveAspectFit
            }
            
            // 提示文字
            TextArea {
                id: promptText
                width: parent.width - iconCircle.width - headerRow.spacing - headerRow.leftPadding - headerRow.rightPadding
                text: qsTr("I've organized the to-do items for you. Would you like to add them to your schedule?")
                font.pointSize: globals.fontsize
                wrapMode: Text.Wrap
                padding: 0
                cursorVisible: false   // 隐藏光标
                readOnly: true
                selectByMouse: true
                selectionColor: sentByUser ? Qt.rgba(00/255, 00/255, 00/255, 0.2) : "#3790FA"  // 自定义选中文本的背景颜色
                background:Item {
                }
            }

            // 提示文字
            TextArea {
                id: promptTextMertics
                text: qsTr("I've organized the to-do items for you. Would you like to add them to your schedule?")
                font.pointSize: globals.fontsize
                padding: 0
                background:Item {
                }
                visible: false
            }
        }
        
        // 按钮区域（右下方）
        Row {
            id: buttonRow
            width: parent.width
            layoutDirection: Qt.RightToLeft
            spacing: 8
            leftPadding: 16
            rightPadding: 16
            bottomPadding: 16
            
            // 添加日程按钮（蓝色，在右侧）
            Rectangle {
                id: addScheduleButton
                width: 88
                height: 32
                radius: 6
                enabled: isCanClick
                opacity: isCanClick ? 1 : 0.5
                color: isCanClick && addScheduleButtonMouseArea.containsMouse 
                       ? (addScheduleButtonMouseArea.pressed ? "#2C73C8" : "#3489ED") 
                       : "#3790FA"
                
                Label {
                    id: addScheduleButtonText
                    anchors.centerIn: parent
                    text: qsTr("Add to Schedule")
                    color: "#FFFFFF"
                    elide: Text.ElideRight
                    clip: true
                }
                
                MouseArea {
                    id: addScheduleButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: isCanClick
                    enabled: isCanClick
                    cursorShape: isCanClick ? Qt.PointingHandCursor : Qt.ArrowCursor
                    
                    onClicked: {
                        if (isCanClick) {
                            addSchedulePromptBubble.addScheduleClicked()
                        }
                    }
                }
            }
            
            // 暂不添加按钮（灰色，在左侧）
            Rectangle {
                id: skipButton
                width: 88
                height: 32
                radius: 6
                color: isCanClick && skipButtonMouseArea.containsMouse ? "#E5E5E5" : "#F0F0F0"
                enabled: isCanClick
                opacity: isCanClick ? 1 : 0.5
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Skip")
                    color: "#000000"
                }
                
                MouseArea {
                    id: skipButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: isCanClick
                    enabled: isCanClick
                    cursorShape: isCanClick ? Qt.PointingHandCursor : Qt.ArrowCursor
                    
                    onClicked: {
                        if (isCanClick) {
                            addSchedulePromptBubble.skipClicked()
                        }
                    }
                }
            }
        }
    }
}
