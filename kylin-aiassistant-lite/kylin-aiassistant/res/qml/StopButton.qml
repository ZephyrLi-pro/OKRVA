import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * 停止回答按钮组件
 * 包含三态交互（hover, pressed, normal）
 */
Rectangle {
    id: stopButton
    Accessible.name: "kylin-aiassistant_Chat_stopButton"
    width: 40
    height: 40
    implicitWidth: 40
    implicitHeight: 40
    Layout.minimumWidth: 32
    Layout.minimumHeight: 32
    color: "#3790FA"
    radius: 30
    border.width: 1
    border.color: "#00000000"

    // 属性：是否显示按钮（与isReplying相关）
    property bool buttonVisible: true

    // 信号：按钮点击事件
    signal clicked()

    visible: buttonVisible

    ToolTip {
        text: qsTr("Stop answering")
        visible: stopButtonMouseArea.containsMouse
        delay: Qt.styleHints.mousePressAndHoldInterval
    }

    Item {
        anchors.fill: parent
        anchors.margins: 8

        Image {
            id: stopButtonImage
            Accessible.name: "kylin-aiassistant_Chat_stopButtonImage"
            anchors.centerIn: parent
            source: "qrc:/icon/icon/stop-reply-middle.svg"
        }
    }
    
    MouseArea {
        id: stopButtonMouseArea
        Accessible.name: "kylin-aiassistant_Chat_stopButtonMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        enabled: true
        
        onEntered: {
            stopButton.color = "#3489EE" // 鼠标悬停时的颜色
        }
        
        onExited: {
            stopButton.color = "#3790FA" // 鼠标离开时恢复到默认颜色
        }
        
        onPressed: {
            stopButton.color = "#2C73C8" // 按下时的颜色
        }
        
        onReleased: {
            stopButton.color = "#3790FA"
        }
        
        onClicked: {
            stopButton.clicked()
        }
    }
}


