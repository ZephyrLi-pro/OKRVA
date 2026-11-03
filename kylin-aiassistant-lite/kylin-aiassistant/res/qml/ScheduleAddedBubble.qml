import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * 日程已添加提示气泡组件
 * 在用户点击添加日程按钮后显示，提示用户日程已添加成功
 */
Rectangle {
    id: scheduleAddedBubble
    Accessible.name: "kylin-aiassistant_Chat_scheduleAddedBubble"
    width: root.winMode === 2 ? parentWidth : Math.min(root.maxWidth, (promptTextMertics.width + iconCircle.width + headerRow.spacing + headerRow.leftPadding + headerRow.rightPadding))
    height: contentColumn.height
    color: "transparent"

    property int parentWidth: 0
    
    signal viewNowClicked(string date)  // 立即查看按钮点击信号，传递日期参数
    
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
            
            // 绿色圆形图标（左上角）
            Image {
                id: iconCircle
                width: 16
                height: 20
                source: "qrc:/icon/icon/green_checkmark.svg"
                fillMode: Image.PreserveAspectFit
            }
            
            // 提示文字
            TextArea {
                id: promptText
                width: parent.width - iconCircle.width - headerRow.spacing - headerRow.leftPadding - headerRow.rightPadding
                text: qsTr("The to-do item has been added to your schedule. You can view, modify, or delete this schedule in the calendar plugin.")
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
                text: qsTr("The to-do item has been added to your schedule. You can view, modify, or delete this schedule in the calendar plugin.")
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
            
            // 立即查看按钮（蓝色，在右侧）
            Rectangle {
                id: viewNowButton
                width: 88
                height: 32
                radius: 6
                color: viewNowButtonMouseArea.containsMouse 
                       ? (viewNowButtonMouseArea.pressed ? "#2C73C8" : "#3489ED") 
                       : "#3790FA"
                
                Label {
                    anchors.centerIn: parent
                    text: qsTr("View Now")
                    color: "#FFFFFF"
                }
                
                MouseArea {
                    id: viewNowButtonMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: {
                        scheduleAddedBubble.viewNowClicked("")
                    }
                }
            }
        }
    }
}

