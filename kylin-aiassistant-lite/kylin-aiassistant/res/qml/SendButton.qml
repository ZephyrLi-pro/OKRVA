import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * 发送按钮组件
 * 包含正常状态、禁用状态和三态交互（hover, pressed, normal）
 * 状态切换与输入框文字、图片、文档相关
 */
Rectangle {
    id: sendTextButton
    Accessible.name: "kylin-aiassistant_Chat_sendTextButton"
    width: 32
    height: 32
    radius: 6
    color: "transparent"
    
    // 属性：是否启用按钮（根据输入内容判断）
    property bool buttonEnabled: false
    
    // 属性：是否显示按钮（与isReplying相关）
    property bool buttonVisible: true
    
    // 信号：按钮点击事件
    signal clicked()
    
    // 内部状态：当前按钮是否处于激活状态
    property bool isActive: buttonEnabled && buttonVisible
    
    visible: buttonVisible
    
    ToolTip {
        id: sendTextButtonTooltip
        visible: false
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: qsTr("Send")
    }
    
    RowLayout {
        anchors.fill: parent
        Image {
            id: sendButtonImage
            Accessible.name: "kylin-aiassistant_Chat_sendButtonImage"
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            source: "qrc:/html/html/img/send_text_white.svg"
        }
    }
    
    MouseArea {
        id: sendTextButtonMouseArea
        Accessible.name: "kylin-aiassistant_Chat_sendTextButtonMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        visible: buttonVisible
        
        onEntered: {
            sendTextButtonTooltip.visible = true
            if (isActive) {
                sendTextButton.color = "#3489EE" // 鼠标悬停时的颜色
            }
        }
        
        onExited: {
            sendTextButtonTooltip.visible = false
            if (isActive) {
                sendTextButton.color = "#3790FA" // 鼠标离开时恢复到默认颜色
            } else {
                sendTextButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.45) // 禁用状态的颜色
            }
        }
        
        onPressed: {
            sendTextButtonTooltip.visible = false
            if (isActive) {
                sendTextButton.color = "#2C73C8" // 按下时的颜色
            }
        }
        
        onReleased: {
            sendTextButtonTooltip.visible = false
            if (isActive) {
                // 恢复状态
                sendTextButton.color = "#3790FA"
            }
        }
        
        onClicked: {
            if (isActive) {
                sendTextButton.clicked()
            }
        }
    }
    
    // 更新按钮样式的函数
    function updateButtonStyle() {
        if (!buttonVisible || !isActive) {
            sendTextButton.color = Qt.rgba(55/255, 144/255, 250/255, 0.45) // 禁用状态的颜色
            sendButtonImage.source = "qrc:/html/html/img/send_text_white.svg"
        } else {
            sendTextButton.color = "#3790FA"
            sendButtonImage.source = "qrc:/html/html/img/send_text_white.svg"
        }
    }
    
    // 根据状态更新按钮样式
    onIsActiveChanged: {
        updateButtonStyle()
    }
    
    // 当按钮可见性改变时更新状态
    onButtonVisibleChanged: {
        updateButtonStyle()
    }
    
    // 初始化时更新样式
    Component.onCompleted: {
        updateButtonStyle()
    }
}

