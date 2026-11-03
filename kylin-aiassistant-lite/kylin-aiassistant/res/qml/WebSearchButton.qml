import QtQuick 2.12
import QtQuick.Controls 2.12
import ButtonState 1.0
import "."

Rectangle {
    id: websearchRectangle
    Accessible.name: "kylin-aiassistant_Chat_websearchRectangle"
    width: websearchLabel.visible ? Math.min(iconRowLayout.implicitWidth + 16, 108) : 32
    implicitWidth: width
    height: 32
    radius: 6
    visible: !ButtonState.isTaskMode

    ToolTip {
        text: qsTr("Web search")
        visible: websearchRectangleMouseArea.containsMouse && (!websearchLabel.visible || websearchLabel.truncated)
        delay: Qt.styleHints.mousePressAndHoldInterval
    }

    states: [
        // 状态1：当前模型不支持联网搜索
        State {
            name: "11"
            when: !ButtonState.webSearchEnabled
            PropertyChanges {
                target: websearchRectangle
                color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
            PropertyChanges {
                target: ButtonState
                isWebSearch: false
            }
            PropertyChanges {
                target: websearchImage
                source: "qrc:/icon/icon/web-search_disabled.svg"
            }
            PropertyChanges {
                target: websearchLabel
                color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
            }
        },
        // 状态2：联网搜索按钮被选中，标志当前是联网搜索模式
        State {
            name: "12"
            when: ButtonState.webSearchEnabled && ButtonState.webSearchisActive
            PropertyChanges {
                target: websearchRectangle
                color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
            }
            PropertyChanges {
                target: ButtonState
                isWebSearch: true
            }
            PropertyChanges {
                target: websearchImage
                source: "qrc:/icon/icon/web-search_active.svg"
            }
            PropertyChanges {
                target: websearchLabel
                color: Qt.rgba(55/255, 144/255, 250/255, 1)
            }
        },
        // 状态3：联网搜索按钮未被选中，标志当前没有开启联网搜索
        State {
            name: "13"
            when: ButtonState.webSearchEnabled && !ButtonState.webSearchisActive
            PropertyChanges {
                target: websearchRectangle
                color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
            PropertyChanges {
                target: ButtonState
                isWebSearch: false
            }
            PropertyChanges {
                target: websearchImage
                source: "qrc:/icon/icon/web-search.svg"
            }
            PropertyChanges {
                target: websearchLabel
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

        // 联网搜索图标
        Rectangle {
            id: websearchImageRectangle
            width: 16
            height: 16
            color: "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Image {
                id: websearchImage

            }
        }

        // 联网搜索标签
        Label {
            id: websearchLabel
            width: Math.min(websearchLabel.implicitWidth, 72)
            font.pointSize: globals.fontsize
            font.family: root.fontFamily
            text: qsTr("Web search")
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            visible: ! (root.winMode === 2)
        }
    }

    MouseArea {
        id: websearchRectangleMouseArea
        Accessible.name: "kylin-aiassistant_Chat_websearchRectangleMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            if (ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.15)
                websearchImage.source = "qrc:/icon/icon/web-search_hover.svg"
            } else if (!ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
            }
        }
        onExited: {
            if (ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                websearchImage.source = "qrc:/icon/icon/web-search_active.svg"
            } else if (!ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
        }
        onPressed: {
            if (ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
                websearchImage.source = "qrc:/icon/icon/web-search_click.svg"
            } else if (!ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.08)
            }
        }
        onReleased: {
            if (ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                websearchImage.source = "qrc:/icon/icon/web-search_active.svg"
            } else if (!ButtonState.webSearchisActive && ButtonState.webSearchEnabled) {
                websearchRectangle.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
            }
        }
        onClicked: {
            if (ButtonState.webSearchEnabled) {
                ButtonState.webSearchisActive = !ButtonState.webSearchisActive
                ButtonState.isWebSearch = ButtonState.webSearchisActive
                msgPane.onSendDataCollect(
                    ButtonState.webSearchisActive ? "enable_web_search_click_event" : "disable_web_search_click_event",
                    "chatPage"
                )
            }
        }
    }

    Component.onCompleted: {
        if (ButtonState.isCurrentModelDeleted === true) {
            ButtonState.webSearchEnabled = false
        }
    }

    Connections {
        target: ButtonState

        onIsCurrentModelDeletedChanged: function() {
            if (ButtonState.isCurrentModelDeleted === true) {
                ButtonState.webSearchEnabled = false
            }
        }
    }
}
