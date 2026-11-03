import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import ButtonState 1.0
import "."

Rectangle {
    id: modeSwitchRectangle
    implicitWidth: modeButton.width
    width: implicitWidth
    height: 32
    color: "transparent"

    property bool isWindowActive: Window.window ? Window.window.active : true

    readonly property string _taskModeText: qsTr("Agent")
    readonly property string _chatModeText: qsTr("Quick Chat")
    readonly property string _taskModeDesc: qsTr("Skilled at invoking system capabilities to execute complex tasks")
    readonly property string _chatModeDesc: qsTr("Quick Q&A and simple system invocation")

    signal modeChanged(int chatMode, string modeName)
    signal showTip(string tipImageSource, string tipText)

    property bool isReplying: false

    onIsWindowActiveChanged: {
        if (!isWindowActive && dropdownPopup.visible) {
            dropdownPopup.visible = false
        }
    }

    Button {
        id: modeButton
        width: Math.min(iconRow.implicitWidth + 16, 306)
        height: 32
        hoverEnabled: true
        ToolTip.visible: hovered && (!modeButtonName.visible || modeButtonName.truncated)
        ToolTip.text: ButtonState.isTaskMode ? _taskModeText : _chatModeText

        property bool wasPopupVisibleBeforeClick: false

        contentItem: Item {
            anchors.fill: parent
            Row {
                id: iconRow
                spacing: 4
                anchors.centerIn: parent

                Rectangle {
                    width: 16
                    height: 16
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        anchors.fill: parent
                        source: ButtonState.isTaskMode
                                ? "qrc:/icon/icon/Claw.svg"
                                : "qrc:/icon/icon/chatMode.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Label {
                    id: modeButtonName
                    width: Math.min(implicitWidth, 120)
                    anchors.verticalCenter: parent.verticalCenter
                    text: ButtonState.isTaskMode ? _taskModeText : _chatModeText
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    color: "#262626"
                    visible: root.winMode === 3
                }

                Rectangle {
                    width: 16
                    height: 16
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        anchors.fill: parent
                        source: "qrc:/icon/icon/arrow-down.svg"
                        rotation: dropdownPopup.visible ? 180 : 0
                    }
                }
            }
        }

        background: Rectangle {
            anchors.fill: parent
            color: modeButton.pressed ? Qt.rgba(0, 0, 0, 0.08) :
                                         modeButton.hovered ? Qt.rgba(0, 0, 0, 0.05) :
                                                               Qt.rgba(0, 0, 0, 0.03)
            radius: 8
        }

        onClicked: {
            if (!wasPopupVisibleBeforeClick) {
                dropdownPopup.visible = true
            } else {
                dropdownPopup.visible = false
            }
        }

        onPressed: {
            wasPopupVisibleBeforeClick = dropdownPopup.visible
        }
    }


    Popup {
        id: dropdownPopup
        width: Math.max(180, Math.min(306, _popupContentWidth))

        TextMetrics {
            id: _textMetric
            font.family: root.fontFamily
            font.pointSize: globals.fontsize
        }

        readonly property real _popupContentWidth: {
            _textMetric.text = _taskModeText
            var w1 = _textMetric.width
            _textMetric.text = _chatModeText
            var w2 = _textMetric.width
            return Math.max(w1, w2) + 112
        }

        height: Math.min(menuColumn.implicitHeight, 160)
        x: 0
        y: -height - 8
        visible: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        clip: false
        topPadding: 0
        bottomPadding: 0

        property int currentIndex: -1

        onOpened: {
            currentIndex = ButtonState.isTaskMode ? 0 : 1
            dropdownScrollview.forceActiveFocus()
        }

        background: Rectangle {
            anchors.fill: parent
            color: "#FFFFFF"
            radius: 8
            border.color: Qt.rgba(0, 0, 0, 0.1)
            border.width: 1
        }

        contentItem: Column {
            id: mainColumn
            spacing: 0
            width: parent.width

            ScrollView {
                id: dropdownScrollview
                Accessible.name: "kylin-aiassistant_Chat_modeDropdownScrollview"
                width: parent.width
                height: Math.min(menuColumn.implicitHeight, 158)
                clip: true

                ScrollBar.horizontal.visible: false
                ScrollBar.vertical.policy: contentHeight > dropdownPopup.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                ScrollBar.vertical.width: ScrollBar.vertical.hovered || ScrollBar.vertical.pressed ? 12 : 8

                contentWidth: menuColumn.width
                contentHeight: menuColumn.implicitHeight

                Keys.onPressed: {
                    if (event.key === Qt.Key_Up) {
                        if (dropdownPopup.currentIndex > 0) {
                            dropdownPopup.currentIndex--
                        } else {
                            dropdownPopup.currentIndex = modeRepeater.count - 1
                        }
                        event.accepted = true
                    }
                    if (event.key === Qt.Key_Down) {
                        if (dropdownPopup.currentIndex < modeRepeater.count - 1) {
                            dropdownPopup.currentIndex++
                        } else {
                            dropdownPopup.currentIndex = 0
                        }
                        event.accepted = true
                    }
                }

                Column {
                    id: menuColumn
                    width: dropdownScrollview.width - (dropdownScrollview.ScrollBar.vertical.visible ? 16 : 0)
                    spacing: 2
                    topPadding: 7
                    bottomPadding: 7

                    Repeater {
                        id: modeRepeater
                        model: ListModel {
                            id: modeListModel
                            ListElement { modeName: qsTr("Agent"); modeType: 1; icon: "qrc:/icon/icon/Claw.svg"; iconWhite: "qrc:/icon/icon/claw-white.svg" }
                            ListElement { modeName: qsTr("Quick Chat"); modeType: 0; icon: "qrc:/icon/icon/chatMode.svg"; iconWhite: "qrc:/icon/icon/chatMode-white.svg" }
                        }

                        delegate: Rectangle {
                            id: modeDelegateRectangle
                            width: menuColumn.width
                            height: 36
                            radius: 6

                            property bool isKeyboardHighlighted: dropdownPopup.currentIndex === index
                            property bool isSelected: (modeType === 1 && ButtonState.isTaskMode) ||
                                                      (modeType === 0 && !ButtonState.isTaskMode)

                            Connections {
                                target: modeDelegateRectangle
                                onIsKeyboardHighlightedChanged: function(){
                                    if (isKeyboardHighlighted) {
                                        modeDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                        modeLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                        modeIcon.source = iconWhite
                                        modeCheckedIcon.source = "qrc:/icon/icon/object-select-symbolic_white.svg"
                                    } else {
                                        modeDelegateRectangle.color = "transparent"
                                        modeLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        modeIcon.source = icon
                                        modeCheckedIcon.source = "qrc:/icon/icon/object-select-symbolic.svg"
                                    }
                                }
                            }

                            RowLayout {
                                z: 1
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                // 选中状态的勾选图标
                                Rectangle {
                                    id: modeCheckedIconRect
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    color: "transparent"
                                    opacity: isSelected ? 1 : 0

                                    Image {
                                        id: modeCheckedIcon
                                        anchors.fill: parent
                                        source: "qrc:/icon/icon/object-select-symbolic.svg"
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }

                                // 模式图标
                                Rectangle {
                                    id: modeIconRect
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    color: "transparent"

                                    Image {
                                        id: modeIcon
                                        anchors.fill: parent
                                        source: icon
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }

                                // 模式名称
                                Label {
                                    id: modeLabel
                                    Layout.fillWidth: true
                                    text: modeName
                                    font.pointSize: globals.fontsize
                                    font.family: root.fontFamily
                                    color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                }

                                // 帮助图标
                                Rectangle {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    color: "transparent"
                                    opacity: descIconMouseArea.containsMouse ? 1.0 : (isKeyboardHighlighted ? 0.75 : 0.35)

                                    Image {
                                        anchors.fill: parent
                                        source: (descIconMouseArea.containsMouse || isKeyboardHighlighted) ? "qrc:/icon/icon/write/help-about-symbolic-highlight.svg" : "qrc:/icon/icon/write/help-about-symbolic.svg"
                                        fillMode: Image.PreserveAspectFit
                                    }

                                     ToolTip {
                                         id: descIconTooltip
                                         visible: descIconMouseArea.containsMouse
                                        text: modeType === 1 ? _taskModeDesc : _chatModeDesc
                                        delay: 500
                                        timeout: 5000
                                    }

                                    MouseArea {
                                        id: descIconMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: mouse.accepted = true

                                        onEntered: {
                                            dropdownPopup.currentIndex = index
                                            modeDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            modeLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            modeIcon.source = iconWhite
                                            modeCheckedIcon.source = "qrc:/icon/icon/object-select-symbolic_white.svg"
                                        }
                                        onExited: {
                                            modeDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            modeLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            modeIcon.source = iconWhite
                                            modeCheckedIcon.source = "qrc:/icon/icon/object-select-symbolic_white.svg"
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: {
                                    dropdownPopup.currentIndex = index
                                }
                                onExited: {
                                    dropdownPopup.currentIndex = -1
                                }
                                onPressed: {
                                    modeDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                }
                                onReleased: {
                                    modeDelegateRectangle.color = "transparent"
                                }
                                onClicked: {
                                    if (isReplying) {
                                        showTip("qrc:/icon/icon/newBuiltTips.svg", qsTr("Current session is in progress, please try again after completion."))
                                        dropdownPopup.visible = false
                                        return
                                    }
                                    var currentModeIsTask = ButtonState.isTaskMode
                                    if ((modeType === 1 && currentModeIsTask) || (modeType === 0 && !currentModeIsTask)) {
                                        dropdownPopup.visible = false
                                        return
                                    }
                                    ButtonState.isTaskMode = (modeType === 1)
                                    ButtonState.chatMode = modeType
                                    modeChanged(modeType, modeName)
                                    dropdownPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 50 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 50 }
        }
    }

}
