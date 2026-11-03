import QtQuick 2.12
import QtQuick.Controls 2.12
import ButtonState 1.0
import "."

Rectangle {
    id: deepthinkingRectangle
    Accessible.name: "kylin-aiassistant_Chat_deepthinkingRectangle"
    width: deepthinkingLabel.visible ? Math.min(iconRowLayout.implicitWidth + 16, 108) : 32
    implicitWidth: width
    height: 32
    radius: 6
    visible: !ButtonState.isTaskMode

    ToolTip {
        text: qsTr("Deep thinking")
        visible: deepthinkingRectangleMouseArea.containsMouse && (!deepthinkingLabel.visible || deepthinkingLabel.truncated)
        delay: Qt.styleHints.mousePressAndHoldInterval
    }

    states: [
        // 状态1：当前模型不支持深度思考
        State {
            name: "1"
            when: !ButtonState.deepThinkingEnabled
            PropertyChanges {
                target: deepthinkingRectangle
                color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
            PropertyChanges {
                target: ButtonState
                isDeepThinking: false
            }
            PropertyChanges {
                target: deepthinkingImage
                source: "qrc:/icon/icon/think_disabled.svg"
            }
            PropertyChanges {
                target: deepthinkingLabel
                color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
            }
        },
        // 状态2：当前模型支持深度思考且不可切换
        State {
            name: "2"
            when: ButtonState.deepThinkingEnabled && !ButtonState.deepThinkingSwitched
            PropertyChanges {
                target: deepthinkingRectangle
                color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
            }
            PropertyChanges {
                target: ButtonState
                isDeepThinking: true
            }
            PropertyChanges {
                target: deepthinkingImage
                source: "qrc:/icon/icon/think_active_disabled.svg"
            }
            PropertyChanges {
                target: deepthinkingLabel
                color: Qt.rgba(55/255, 144/255, 250/255, 0.45)
            }
        },
        // 状态3：深度思考按钮被选中，标志当前是深度思考模式
        State {
            name: "3"
            when: ButtonState.deepThinkingEnabled && ButtonState.deepThinkingSwitched && ButtonState.deepThinkingisActive
            PropertyChanges {
                target: deepthinkingRectangle
                color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
            }
            PropertyChanges {
                target: ButtonState
                isDeepThinking: true
            }
            PropertyChanges {
                target: deepthinkingImage
                source: "qrc:/icon/icon/think_active.svg"
            }
            PropertyChanges {
                target: deepthinkingLabel
                color: Qt.rgba(55/255, 144/255, 250/255, 1)
            }
        },
        // 状态4：深度思考按钮未被选中，标志当前没有开启深度思考
        State {
            name: "4"
            when: ButtonState.deepThinkingEnabled && ButtonState.deepThinkingSwitched && !ButtonState.deepThinkingisActive
            PropertyChanges {
                target: deepthinkingRectangle
                color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
            PropertyChanges {
                target: ButtonState
                isDeepThinking: false
            }
            PropertyChanges {
                target: deepthinkingImage
                source: "qrc:/icon/icon/think.svg"
            }
            PropertyChanges {
                target: deepthinkingLabel
                color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
            }
        }
    ]

    Row {
        id: iconRowLayout
        anchors.centerIn: parent
        spacing: 4
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        // 深度思考图标
        Rectangle {
            id: deepthinkingImageRectangle
            width: 16
            height: 16
            color: "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Image {
                id: deepthinkingImage

            }
        }

        // 深度思考标签
        Label {
            id: deepthinkingLabel
            width: Math.min(deepthinkingLabel.implicitWidth, 72)
            font.pointSize: globals.fontsize
            text: qsTr("Deep thinking")
            font.family: root.fontFamily
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            visible: ! (root.winMode === 2)
        }
    }

    MouseArea {
        id: deepthinkingRectangleMouseArea
        Accessible.name: "kylin-aiassistant_Chat_deepthinkingRectangleMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            if (ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.15)
                deepthinkingImage.source = "qrc:/icon/icon/think_hover.svg"
            } else if (!ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
            }
        }
        onExited: {
            if (ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                deepthinkingImage.source = "qrc:/icon/icon/think_active.svg"
            } else if (!ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
        }
        onPressed: {
            if (ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
                deepthinkingImage.source = "qrc:/icon/icon/think_click.svg"

            } else if (!ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.08)
            }
        }
        onReleased: {
            if (ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                deepthinkingImage.source = "qrc:/icon/icon/think_active.svg"
            } else if (!ButtonState.deepThinkingisActive && ButtonState.deepThinkingSwitched) {
                deepthinkingRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
        }
        onClicked: {
            if (ButtonState.deepThinkingSwitched) {
                ButtonState.deepThinkingisActive = !ButtonState.deepThinkingisActive
                ButtonState.isDeepThinking = ButtonState.deepThinkingisActive
                msgPane.onSendDataCollect(
                    ButtonState.deepThinkingisActive ? "enable_deep_thinking_click_event" : "disable_deep_thinking_click_event",
                    "chatPage"
                )
            }
        }
    }

    Component.onCompleted: {
        if (ButtonState.isCurrentModelDeleted === true) {
            ButtonState.deepThinkingEnabled = false
        }
    }

    Connections {
        target: ButtonState

        onIsCurrentModelDeletedChanged: function() {
            if (ButtonState.isCurrentModelDeleted === true) {
                ButtonState.deepThinkingEnabled = false
            }
        }
    }
}
