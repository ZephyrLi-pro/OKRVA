import QtQuick 2.12
import QtQuick.Controls 2.12

/**
 * 推荐问题行组件
 * 将推荐问题以横向排列的方式展示，支持点击交互
 */
Row {
    id: root

    // 外部属性：问题模型数据
    property var questionModel: null
    property int maxWidth: 0
    property bool btnEnabled: true
    
    height: 32
    spacing: 8
    clip: true
    opacity: btnEnabled ? 1.0 : 0.4

    // 信号：当问题项被点击时发出，传递问题文本和索引
    signal questionClicked(string question, var questionIndex)

    Repeater {
        id: suggestedQuestionRepeater
        visible: true
        model: root.questionModel
        clip: true
        
        delegate: Rectangle {
            id: suggestedQuestionItem
            width: questionItemRow.width
            height: 34
            color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
            radius: 6
            property bool isHovered: false
            
            Row {
                id: questionItemRow
                height: parent.height
                spacing: 8
                leftPadding: 8
                rightPadding: 8

                Label {
                    id: suggestedQuestionText
                    width: Math.min(implicitWidth, maxWidth - 24 - 16) //最大宽度减去间距和其他组件
                    anchors.verticalCenter: parent.verticalCenter
                    text: model.question || ""
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    property bool isToolTipVisible: implicitWidth > maxWidth - 24 - 16
                    font.pointSize: globals.fontsize - 2
                }

                ToolTip {
                    id: suggestedQuestionTextButtonToolTip
                    delay: Qt.styleHints.mousePressAndHoldInterval
                    visible: false // 手动控制tooltip显示
                    text: suggestedQuestionText.text
                    property var wrapMode: Text.WrapAnywhere
                }

                Image {
                    height: 16
                    width: 16
                    source: "qrc:/icon/icon/arrow-icon.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }

            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                
                onEntered: {
                    if (!btnEnabled) {
                        return
                    }

                    if (suggestedQuestionText.isToolTipVisible){
                        suggestedQuestionTextButtonToolTip.visible = true
                    }

                    suggestedQuestionItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
                    suggestedQuestionItem.isHovered = true
                }
                
                onExited: {
                    if (!btnEnabled) {
                        return
                    }

                    if (suggestedQuestionText.isToolTipVisible){
                        suggestedQuestionTextButtonToolTip.visible = false
                    }

                    suggestedQuestionItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
                    suggestedQuestionItem.isHovered = false
                }
                
                onPressed: {
                    if (!btnEnabled) {
                        return
                    }

                    if (suggestedQuestionText.isToolTipVisible){
                        suggestedQuestionTextButtonToolTip.visible = true
                    }

                    suggestedQuestionItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.08)
                }
                
                onReleased: {
                    if (!btnEnabled) {
                        return
                    }

                    if (suggestedQuestionText.isToolTipVisible){
                        suggestedQuestionTextButtonToolTip.visible = false
                    }

                    suggestedQuestionItem.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
                }
                
                onClicked: {
                    if (!btnEnabled) {
                        return
                    }

                    if (suggestedQuestionText.isToolTipVisible){
                        suggestedQuestionTextButtonToolTip.visible = true
                    }

                    // 发出信号，将点击事件传递给外部处理
                    root.questionClicked(model.question || "", model.questionIndex || undefined)
                }
            }
        }
    }
}



