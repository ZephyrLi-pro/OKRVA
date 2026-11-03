import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

// 添加/编辑指令对话框
Window {
    id: commandDialog
    property bool isSidebarMode: {
        if (typeof windowCtrl !== "undefined" && windowCtrl !== null) {
            return windowCtrl.winMode === 2
        } else if (typeof globals !== "undefined" && globals !== null) {
            return globals.getCurrentWindowMode() === "sidebar"
        }
        return false
    }

    width: isSidebarMode ? 376 : 452
    height: isSidebarMode ? 492 : 556
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    title: isEditMode ? qsTr("Edit Command") : qsTr("Add Command")
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false
    color: "#FFFFFF"

    property bool isEditMode: false
    property string commandUuid: ""
    property string originalName: ""
    property string originalSummary: ""
    property string originalDescription: ""

    signal accepted(string name, string summary, string description)

    function openForAdd() {
        isEditMode = false
        commandUuid = ""
        nameField.text = ""
        nameFieldWindow.text = ""
        summaryField.text = ""
        summaryFieldWindow.text = ""
        descriptionField.text = ""
        descriptionFieldWindow.text = ""
        nameError.visible = false
        summaryError.visible = false
        descriptionError.visible = false
        updateButtonState()
        visible = true
    }

    function openForEdit(uuid, name, summary, description) {
        isEditMode = true
        commandUuid = uuid
        originalName = name
        originalSummary = summary || ""
        originalDescription = description
        nameField.text = name
        nameFieldWindow.text = name
        summaryField.text = summary || ""
        summaryFieldWindow.text = summary || ""
        descriptionField.text = description
        descriptionFieldWindow.text = description
        nameError.visible = false
        summaryError.visible = false
        descriptionError.visible = false
        updateButtonState()
        visible = true
    }

    function showDialog() {
        visible = true
    }

    function updateButtonState() {
        var nameText = isSidebarMode ? nameField.text : nameFieldWindow.text
        var summaryText = isSidebarMode ? summaryField.text : summaryFieldWindow.text
        var descriptionText = isSidebarMode ? descriptionField.text : descriptionFieldWindow.text

        var nameValid = nameText.length > 0 && nameText.length <= 4
        var summaryValid = summaryText.length <= 20
        var descriptionValid = descriptionText.length > 0 && descriptionText.length <= 1000

        if (isEditMode) {
            var hasChanges = nameText !== originalName ||
                           summaryText !== originalSummary ||
                           descriptionText !== originalDescription
            confirmButton.enabled = nameValid && summaryValid && descriptionValid && hasChanges
        } else {
            confirmButton.enabled = nameValid && summaryValid && descriptionValid
        }
    }
    Rectangle{
        anchors.fill: parent
        radius: 12
        color: "transparent"
        focus: true

        Keys.onEscapePressed: {
            commandDialog.visible = false
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 内容区域
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 0

                // 指令名称
                RowLayout {
                    Layout.topMargin: isSidebarMode ? 24 : 16
                    spacing: isSidebarMode ? 8 : 0
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Command Name")
                        color: "#333333"
                        Layout.preferredWidth: isSidebarMode ? 60 : 60
                    }

                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        placeholderText: qsTr("e.g.: Smart Reply")
                        font.pointSize: globals.fontsize
                        selectByMouse: true
                        visible: isSidebarMode
                        property bool isUpdating: false
                        property bool isTrimming: false
                        background: Rectangle {
                            color: nameField.focus ? "#FFFFFF" : "#F5F5F5"
                            border.color: nameField.focus ? "#3790FA" : "transparent"
                            border.width: 2
                            radius: 6
                        }

                        onTextChanged: {
                            if (isTrimming) return

                            if (text.length > 4) {
                                isTrimming = true
                                text = text.substring(0, 4)
                                isTrimming = false
                                nameError.visible = true
                                if (!isSidebarMode && !nameFieldWindow.isUpdating) {
                                    nameFieldWindow.isUpdating = true
                                    nameFieldWindow.text = text
                                    nameFieldWindow.isUpdating = false
                                }
                                updateButtonState()
                                return
                            }

                            if (text.length < 4) {
                                nameError.visible = false
                            }

                            if (!isSidebarMode && !nameFieldWindow.isUpdating) {
                                nameFieldWindow.isUpdating = true
                                nameFieldWindow.text = text
                                nameFieldWindow.isUpdating = false
                            }
                            updateButtonState()
                        }
                    }
                }

                TextField {
                    id: nameFieldWindow
                    Layout.topMargin: 8
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    placeholderText: qsTr("e.g.: Smart Reply")
                    font.pointSize: globals.fontsize
                    selectByMouse: true
                    visible: !isSidebarMode
                    property bool isUpdating: false
                    property bool isTrimming: false
                    text: nameField.text
                    background: Rectangle {
                        color: nameFieldWindow.focus ? "#FFFFFF" : "#F5F5F5"
                        border.color: nameFieldWindow.focus ? "#3790FA" : "transparent"
                        border.width: 2
                        radius: 6
                    }

                    onTextChanged: {
                        if (isTrimming) return

                        if (text.length > 4) {
                            isTrimming = true
                            text = text.substring(0, 4)
                            isTrimming = false
                            nameError.visible = true
                            if (!isUpdating) {
                                isUpdating = true
                                nameField.text = text
                                isUpdating = false
                            }
                            updateButtonState()
                            return
                        }

                        if (text.length < 4) {
                            nameError.visible = false
                        }

                        if (!isUpdating) {
                            isUpdating = true
                            nameField.text = text
                            isUpdating = false
                        }
                        updateButtonState()
                    }
                }

                Label {
                    id: nameError
                    Layout.topMargin: 4
                    Layout.leftMargin: isSidebarMode ? 68 : 0
                    text: qsTr("Length must not exceed 4 characters")
                    font.pointSize: globals.fontsize - 2
                    color: "#F53F3F"
                    visible: false
                }

                // 指令简介
                RowLayout {
                    Layout.topMargin: isSidebarMode ? 24 : 16
                    spacing: isSidebarMode ? 8 : 0
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Command Summary")
                        color: "#333333"
                        Layout.preferredWidth: isSidebarMode ? 60 : 60
                    }

                    TextField {
                        id: summaryField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        placeholderText: qsTr("e.g.: Generate reply based on context")
                        font.pointSize: globals.fontsize
                        selectByMouse: true
                        visible: isSidebarMode
                        property bool isUpdating: false
                        property bool isTrimming: false
                        background: Rectangle {
                            color: summaryField.focus ? "#FFFFFF" : "#F5F5F5"
                            border.color: summaryField.focus ? "#3790FA" : "transparent"
                            border.width: 2
                            radius: 6
                        }

                        onTextChanged: {
                            if (isTrimming) return

                            if (text.length > 20) {
                                isTrimming = true
                                text = text.substring(0, 20)
                                isTrimming = false
                                summaryError.visible = true
                                if (!isSidebarMode && !summaryFieldWindow.isUpdating) {
                                    summaryFieldWindow.isUpdating = true
                                    summaryFieldWindow.text = text
                                    summaryFieldWindow.isUpdating = false
                                }
                                updateButtonState()
                                return
                            }

                            if (text.length < 20) {
                                summaryError.visible = false
                            }

                            if (!isSidebarMode && !summaryFieldWindow.isUpdating) {
                                summaryFieldWindow.isUpdating = true
                                summaryFieldWindow.text = text
                                summaryFieldWindow.isUpdating = false
                            }
                            updateButtonState()
                        }
                    }
                }

                TextField {
                    id: summaryFieldWindow
                    Layout.topMargin: 8
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    placeholderText: qsTr("e.g.: Generate reply based on context")
                    font.pointSize: globals.fontsize
                    selectByMouse: true
                    visible: !isSidebarMode
                    property bool isUpdating: false
                    property bool isTrimming: false
                    text: summaryField.text
                    background: Rectangle {
                        color: summaryFieldWindow.focus ? "#FFFFFF" : "#F5F5F5"
                        border.color: summaryFieldWindow.focus ? "#3790FA" : "transparent"
                        border.width: 2
                        radius: 6
                    }

                    onTextChanged: {
                        if (isTrimming) return

                        if (text.length > 20) {
                            isTrimming = true
                            text = text.substring(0, 20)
                            isTrimming = false
                            summaryError.visible = true
                            if (!isUpdating) {
                                isUpdating = true
                                summaryField.text = text
                                isUpdating = false
                            }
                            updateButtonState()
                            return
                        }

                        if (text.length < 20) {
                            summaryError.visible = false
                        }

                        if (!isUpdating) {
                            isUpdating = true
                            summaryField.text = text
                            isUpdating = false
                        }
                        updateButtonState()
                    }
                }

                Label {
                    id: summaryError
                    Layout.topMargin: 4
                    Layout.leftMargin: isSidebarMode ? 68 : 0
                    text: qsTr("Length must not exceed 20 characters")
                    font.pointSize: globals.fontsize - 2
                    color: "#F53F3F"
                    visible: false
                }

                // 指令描述
                RowLayout {
                    Layout.topMargin: isSidebarMode ? 24 : 16
                    spacing: isSidebarMode ? 8 : 0
                    Layout.fillWidth: true
                    Layout.fillHeight: isSidebarMode

                    Label {
                        text: qsTr("Command Description")
                        color: "#333333"
                        Layout.preferredWidth: isSidebarMode ? 60 : 60
                        Layout.alignment: isSidebarMode ? Qt.AlignTop : Qt.AlignVCenter
                        Layout.topMargin: isSidebarMode ? 8 : 0
                    }

                    // 固定大小的边框容器
                    Rectangle {
                        id: descriptionContainer
                        Layout.fillWidth: true
                        Layout.fillHeight: isSidebarMode
                        Layout.preferredHeight: isSidebarMode ? -1 : 200
                        visible: isSidebarMode
                        color: descriptionField.activeFocus ? "#FFFFFF" : "#F5F5F5"
                        border.color: descriptionField.activeFocus ? "#3790FA" : "transparent"
                        border.width: 2
                        radius: 6

                        Flickable {
                            id: descriptionFlickable
                            anchors.fill: parent
                            anchors.leftMargin: 0
                            anchors.rightMargin: 0
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            clip: true
                            contentWidth: width
                            contentHeight: descriptionField.implicitHeight
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {
                                policy: descriptionFlickable.contentHeight > descriptionFlickable.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                            }

                            TextArea {
                                id: descriptionField
                                width: descriptionFlickable.width
                                height: Math.max(implicitHeight, descriptionFlickable.height)
                                wrapMode: TextArea.Wrap
                                font.pointSize: globals.fontsize
                                selectByMouse: true
                                topPadding: 0
                                bottomPadding: 0
                                property bool isUpdating: false
                                background: Item {}  // 透明背景

                                // 自定义可换行的placeholder
                                Label {
                                    x: descriptionField.leftPadding
                                    y: 0
                                    width: descriptionField.width - descriptionField.leftPadding - descriptionField.rightPadding
                                    text: qsTr("e.g.: You are an intelligent chat assistant, aiming to automatically reply to friends' messages. Use concise, natural language to mimic the user's communication style. I will give you the message from the other party, and you need to judge the context based on the message content, reply to the information in a timely manner, provide practical advice or friendly responses. If you are not sure of the answer, you can politely explain to avoid misleading the user. Make sure the language is friendly, appropriate, and suitable for daily communication.")
                                    font: descriptionField.font
                                    color: "#999999"
                                    wrapMode: Text.Wrap
                                    visible: descriptionField.text.length === 0
                                }

                                onTextChanged: {
                                    if (!isSidebarMode && !descriptionFieldWindow.isUpdating) {
                                        descriptionFieldWindow.isUpdating = true
                                        descriptionFieldWindow.text = text
                                        descriptionFieldWindow.isUpdating = false
                                    }
                                    if (text.length > 1000) {
                                        // 截断文本
                                        var cursorPos = cursorPosition
                                        text = text.substring(0, 1000)
                                        cursorPosition = Math.min(cursorPos, 1000)
                                        descriptionError.visible = true
                                    } else {
                                        descriptionError.visible = false
                                    }
                                    updateButtonState()
                                }

                                onCursorRectangleChanged: {
                                    // 自动滚动使光标保持可见
                                    var cursorY = cursorRectangle.y
                                    var cursorBottom = cursorY + cursorRectangle.height
                                    var flickableHeight = descriptionFlickable.height
                                    var contentY = descriptionFlickable.contentY

                                    if (cursorBottom > contentY + flickableHeight) {
                                        // 光标在可视区域下方，向下滚动
                                        descriptionFlickable.contentY = cursorBottom - flickableHeight
                                    } else if (cursorY < contentY) {
                                        // 光标在可视区域上方，向上滚动
                                        descriptionFlickable.contentY = cursorY
                                    }
                                }
                            }
                        }
                    }
                }

                // 固定大小的边框容器（窗口模式）
                Rectangle {
                    id: descriptionContainerWindow
                    Layout.topMargin: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !isSidebarMode
                    color: descriptionFieldWindow.activeFocus ? "#FFFFFF" : "#F5F5F5"
                    border.color: descriptionFieldWindow.activeFocus ? "#3790FA" : "transparent"
                    border.width: 2
                    radius: 6

                    Flickable {
                        id: descriptionFlickableWindow
                        anchors.fill: parent
                        anchors.leftMargin: 0
                        anchors.rightMargin: 0
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        clip: true
                        contentWidth: width
                        contentHeight: descriptionFieldWindow.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            policy: descriptionFlickableWindow.contentHeight > descriptionFlickableWindow.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        }

                        TextArea {
                            id: descriptionFieldWindow
                            width: descriptionFlickableWindow.width
                            height: Math.max(implicitHeight, descriptionFlickableWindow.height)
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            topPadding: 0
                            bottomPadding: 0
                            font.pointSize: globals.fontsize
                            property bool isUpdating: false
                            text: descriptionField.text
                            background: Item {}  // 透明背景

                            // 自定义可换行的placeholder
                            Label {
                                x: descriptionFieldWindow.leftPadding
                                y: 0
                                width: descriptionFieldWindow.width - descriptionFieldWindow.leftPadding - descriptionFieldWindow.rightPadding
                                text: qsTr("e.g.: You are an intelligent chat assistant, aiming to automatically reply to friends' messages. Use concise, natural language to mimic the user's communication style. I will give you the message from the other party, and you need to judge the context based on the message content, reply to the information in a timely manner, provide practical advice or friendly responses. If you are not sure of the answer, you can politely explain to avoid misleading the user. Make sure the language is friendly, appropriate, and suitable for daily communication.")
                                font: descriptionFieldWindow.font
                                color: "#999999"
                                wrapMode: Text.Wrap
                                visible: descriptionFieldWindow.text.length === 0
                            }

                            onTextChanged: {
                                if (!isUpdating) {
                                    isUpdating = true
                                    descriptionField.text = text
                                    isUpdating = false
                                }
                                if (text.length > 1000) {
                                    // 截断文本
                                    var cursorPos = cursorPosition
                                    text = text.substring(0, 1000)
                                    cursorPosition = Math.min(cursorPos, 1000)
                                    descriptionError.visible = true
                                } else {
                                    descriptionError.visible = false
                                }
                                updateButtonState()
                            }

                            onCursorRectangleChanged: {
                                // 自动滚动使光标保持可见
                                var cursorY = cursorRectangle.y
                                var cursorBottom = cursorY + cursorRectangle.height
                                var flickableHeight = descriptionFlickableWindow.height
                                var contentY = descriptionFlickableWindow.contentY

                                if (cursorBottom > contentY + flickableHeight) {
                                    // 光标在可视区域下方，向下滚动
                                    descriptionFlickableWindow.contentY = cursorBottom - flickableHeight
                                } else if (cursorY < contentY) {
                                    // 光标在可视区域上方，向上滚动
                                    descriptionFlickableWindow.contentY = cursorY
                                }
                            }
                        }
                    }
                }

                Label {
                    id: descriptionError
                    Layout.topMargin: 4
                    Layout.leftMargin: isSidebarMode ? 68 : 0
                    text: qsTr("Length must not exceed 1000 characters")
                    font.pointSize: globals.fontsize - 2
                    color: "#F53F3F"
                    visible: false
                }
            }

            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                Layout.topMargin: isSidebarMode ? 24 : 16
                Layout.bottomMargin: 24
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: cancelButton
                    text: qsTr("Cancel")
                    implicitWidth: 80
                    implicitHeight: 36
                    highlighted: false
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        color: {
                            if (cancelButton.pressed) {
                                return "#B9B9B9"
                            }
                            if (cancelButton.hovered) {
                                return "#DCDCDC"
                            }
                            return "#E6E6E6"
                        }
                        radius: 6
                    }
                    contentItem: Label {
                        text: cancelButton.text
                        color: "#333333"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: commandDialog.close()
                }

                Button {
                    id: confirmButton
                    text: isEditMode ? qsTr("OK") : qsTr("Add")
                    implicitWidth: 80
                    implicitHeight: 36
                    enabled: false
                    opacity: enabled ? 1.0 : 0.45
                    focusPolicy: Qt.NoFocus
                    background: Rectangle {
                        color: "#3790FA"
                        radius: 6
                    }
                    contentItem: Label {
                        text: confirmButton.text
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        var nameText = isSidebarMode ? nameField.text : nameFieldWindow.text
                        var summaryText = isSidebarMode ? summaryField.text : summaryFieldWindow.text
                        var descriptionText = isSidebarMode ? descriptionField.text : descriptionFieldWindow.text
                        accepted(nameText, summaryText, descriptionText)
                        commandDialog.close()
                        // 注意：destroy 会在 accepted 信号处理完成后执行
                    }
                }
            }
        }
    }

    onClosing: function(close) {
        close.accepted = true
        // 关闭时销毁对象
        Qt.callLater(function() {
            if (typeof destroy === "function") {
                destroy()
            }
        })
    }
}
