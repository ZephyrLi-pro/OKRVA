import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import ButtonState 1.0
import "."

Rectangle {
    id: intelligentModeRectangle
    implicitWidth: modelButton.width
    width: implicitWidth
    height: 32
    color: "transparent"
    visible: !ButtonState.isTaskMode

    // 暴露给外部的属性
    property bool isEditorPageShow: false
    property bool hasSharedModel: false  // 是否有共享模型
    property bool hasLocalModel: false   // 是否有本地模型

    // 窗口是否失焦
    property bool isWindowActive: Window.window ? Window.window.active : true

    // 信号定义
    signal modelChanged(string modelName, bool isSupportsThinking, bool isSupportsWebSearch, bool isSupportsThinkingToggle)
    signal dropdownOpened()

    // 窗口失焦后模型切换列表关闭
    onIsWindowActiveChanged: {
        if (!isWindowActive && dropdownPopup.visible) {
            console.log("dropdownPopup closed")
            dropdownPopup.visible = false
        }
    }

    // 切换模型按钮
    Button {
        id: modelButton
        width: Math.min(iconRow.implicitWidth + 16, 264)
        height: 32
        hoverEnabled: true
        focusPolicy: Qt.NoFocus

        ToolTip {
            visible: modelButton.hovered && (!modelButtonName.visible || modelButtonName.truncated)
            text: modelButtonName.text
        }

        // 记录在点击模型切换按钮前下拉菜单的的状态,避免关闭后立刻打开
        property bool wasPopupVisibleBeforeClick: false       

        contentItem: Item {
            anchors.fill: parent
            Row {
                id: iconRow
                spacing: 4
                anchors.centerIn: parent

                // 模型图标
                Rectangle {
                    width: 16
                    height: 16
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        id: modelButtonIcon
                        anchors.fill: parent
                        source: ButtonState.modelButtonIcon
                        opacity: ButtonState.isCurrentModelDeleted ?  0.4 : 1.0
                        fillMode: Image.PreserveAspectFit
                    }
                }

                // 按钮名称
                Label {
                    id: modelButtonName
                    width: Math.min(implicitWidth, 208)
                    anchors.verticalCenter: parent.verticalCenter
                    text: ButtonState.currentModel
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    color: ButtonState.isCurrentModelDeleted ? Qt.rgba(0.4, 0.4, 0.4, 1) : "#262626"  // 置灰或正常颜色
                    visible: root.winMode === 3 && !isEditorPageShow
                }

                // 箭头图标
                Rectangle {
                    width: 16
                    height: 16
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        id: arrowIcon
                        anchors.fill: parent
                        source: "qrc:/icon/icon/arrow-down.svg"
                        rotation: dropdownPopup.visible ? 180 : 0
                    }
                }
            }
        }

        background: Rectangle {
            anchors.fill: parent
            color: modelButton.pressed ? Qt.rgba(0, 0, 0, 0.08) :
                                         modelButton.hovered ? Qt.rgba(0, 0, 0, 0.05) :
                                                               Qt.rgba(0, 0, 0, 0.03)
            radius: 8
        }

        onClicked: {
            if (!wasPopupVisibleBeforeClick) {
                ButtonState.isCurrentModelDeleted = !msgPane.isCurrentModelAvailable()
                console.log("获取可用文本模型列表")
                dropdownOpened()

                // 获取模型列表后检查是否有共享模型和本地模型
                checkModelTypes()
                dropdownPopup.visible = true
            } else {
                console.log("模型切换下拉菜单关闭")
                dropdownPopup.visible = false
            }
        }

        onPressed: {
            wasPopupVisibleBeforeClick = dropdownPopup.visible
        }  

        Connections {
            target: dropdownPopup
            onVisibleChanged: function() {
                if (!dropdownPopup.visible) {
                    console.log("模型切换下拉菜单关闭")
                }
            }
        }

        // 监听 msgPane 的信号，在即将发送消息时更新 System Default 按钮状态
        Connections {
            target: msgPane
            function onSigAboutToSendMessage() {
                updateIntelligentModeButtonState()
            }
        }    
    }

    // 检查模型类型：是否有共享模型和本地模型
    function checkModelTypes() {
        hasSharedModel = false
        hasLocalModel = false

        for (var i = 0; i < modelItemModel.rowCount(); i++) {
            // 使用modelItemModel的get方法获取数据
            var item = modelItemModel.get(i)
            if (item.isSharedModel) {
                hasSharedModel = true
            }
            if (item.modelType === "local" && !item.isSharedModel) {
                hasLocalModel = true
            }
        }

        console.log("hasSharedModel:", hasSharedModel, "hasLocalModel:", hasLocalModel)
    }

    // 更新 System Default 的按钮状态
    function updateIntelligentModeButtonState() {
        console.log("  ButtonState.currentModel:", ButtonState.currentModel)

        // 检查当前模型是否是 System Default
        if (ButtonState.currentModel === qsTr("System Default")) {
            console.log("  Current model is System Default, refreshing model list and updating button states")

            // 刷新模型列表
            msgPane.initNLPModelList()

            // 查找 System Default 的模型项并更新按钮状态
            for (var i = 0; i < modelItemModel.rowCount(); i++) {
                var item = modelItemModel.get(i)
                if (item.modelName === qsTr("System Default") && item.modelType === "intelligent") {
                    console.log("  Found System Default item, updating button states:")
                    console.log("    isSupportsThinking:", item.isSupportsThinking)
                    console.log("    isSupportsThinkingToggle:", item.isSupportsThinkingToggle)
                    console.log("    isSupportsWebSearch:", item.isSupportsWebSearch)

                    // 更新 ButtonState 中的按钮状态
                    ButtonState.deepThinkingSwitched = item.isSupportsThinkingToggle
                    ButtonState.deepThinkingEnabled = item.isSupportsThinking
                    if (ButtonState.storageWebSearchEnabled === null) {
                        ButtonState.webSearchEnabled = item.isSupportsWebSearch
                    }
                    ButtonState.modelButtonIcon = item.iconPath
                    break
                }
            }
        } else {
            console.log("  Current model is not System Default, skipping update")
        }
    }

    // 下拉菜单
    Popup {
        id: dropdownPopup
        width: dropdownScrollview.ScrollBar.vertical.visible ? 306 : 290
        height: Math.min(menuColumn.implicitHeight, 378)
        x: 0
        y: -height - 8
        visible: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        clip: false
        topPadding: 1
        bottomPadding: 1
        leftPadding: 8
        rightPadding: 8

        property int currentIndex: -1

        onOpened: {
            currentIndex = -1
            dropdownScrollview.forceActiveFocus()
        }

        contentItem: Column {
            id: mainColumn
            spacing: 0
            width: parent.width

            // 模型列表区域（可滚动）
            ScrollView {
                id: dropdownScrollview
                Accessible.name: "kylin-aiassistant_Chat_dropdownScrollview"
                width: parent.width
                height: Math.min(menuColumn.implicitHeight, 376)
                clip: true

                ScrollBar.horizontal.visible: false
                ScrollBar.vertical.policy: contentHeight > dropdownPopup.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                ScrollBar.vertical.width: ScrollBar.vertical.hovered || ScrollBar.vertical.pressed ? 12 : 8

                contentWidth: menuColumn.width
                contentHeight: menuColumn.implicitHeight

                // 快捷键控制
                Keys.onPressed: {
                    // 上键：选中上一项
                    if (event.key === Qt.Key_Up) {
                        if (dropdownPopup.currentIndex > 0) {
                            dropdownPopup.currentIndex--
                        } else {
                            dropdownPopup.currentIndex = getAllVisibleItems().length - 1
                        }
                        scrollToCurrentIndex()
                        event.accepted = true
                    }
                    // 下键/Tab：选中下一项
                    if (event.key === Qt.Key_Down || event.key === Qt.Key_Tab) {
                        var items = getAllVisibleItems()
                        if (dropdownPopup.currentIndex < items.length - 1) {
                            dropdownPopup.currentIndex++
                        } else {
                            dropdownPopup.currentIndex = 0
                        }
                        scrollToCurrentIndex()
                        event.accepted = true
                    }
                    // Home/PageUp：选中第一项
                    if (event.key === Qt.Key_Home || event.key === Qt.Key_PageUp) {
                        dropdownPopup.currentIndex = 0
                        scrollToCurrentIndex()
                        event.accepted = true
                    }
                    // End/PageDown：选中最后一项
                    if (event.key === Qt.Key_End || event.key === Qt.Key_PageDown) {
                        dropdownPopup.currentIndex = getAllVisibleItems().length - 1
                        scrollToCurrentIndex()
                        event.accepted = true
                    }
                }

                // 获取所有可见的模型项
                function getAllVisibleItems() {
                    var items = []

                    // 收集智能调度模型项
                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                        var item = intelligentModelRepeater.itemAt(i)
                        if (item && item.visible) {
                            items.push(item)
                        }
                    }

                    // 收集云端模型项
                    for (i = 0; i < cloudModelRepeater.count; i++) {
                        item = cloudModelRepeater.itemAt(i)
                        if (item && item.visible) {
                            items.push(item)
                        }
                    }

                    // 收集本地模型项
                    for (i = 0; i < localModelRepeater.count; i++) {
                        item = localModelRepeater.itemAt(i)
                        if (item && item.visible) {
                            items.push(item)
                        }
                    }

                    // 收集自选模型项
                    for (i = 0; i < optionalModelRepeater.count; i++) {
                        item = optionalModelRepeater.itemAt(i)
                        if (item && item.visible) {
                            items.push(item)
                        }
                    }

                    // 收集共享模型项
                    for (i = 0; i < sharedModelRepeater.count; i++) {
                        item = sharedModelRepeater.itemAt(i)
                        if (item && item.visible) {
                            items.push(item)
                        }
                    }

                    return items
                }

                // 根据索引获取可见项
                function getVisibleItemAt(index) {
                    var items = getAllVisibleItems()
                    if (index >= 0 && index < items.length) {
                        return items[index]
                    }
                    return null
                }

                // 滚动到当前选中项
                function scrollToCurrentIndex() {
                    var currentItem = getVisibleItemAt(dropdownPopup.currentIndex)
                    if (!currentItem) return

                    var itemPos = currentItem.mapToItem(menuColumn, 0, 0)
                    var itemTopY = itemPos.y
                    var itemBottomY = itemTopY + currentItem.height

                    if (itemTopY < dropdownScrollview.contentItem.contentY) {
                        dropdownScrollview.contentItem.contentY = itemTopY
                    } else if (itemBottomY > dropdownScrollview.contentItem.contentY + dropdownScrollview.height) {
                        dropdownScrollview.contentItem.contentY = itemBottomY - dropdownScrollview.height
                    }
                }

                Column {
                    id: menuColumn
                    Accessible.name: "kylin-aiassistant_Chat_menuColumn"
                    width: dropdownScrollview.width - (dropdownScrollview.ScrollBar.vertical.visible ? 16 : 0)
                    spacing: 2
                    topPadding: 7
                    bottomPadding: 7

                    // 智能调度模型分类
                    Column {
                        id: intelligentModelColumn
                        width: parent.width - 16
                        spacing: 2
                        visible: intelligentModelRepeater.count > 0

                        // 智能调度模型列表
                        Repeater {
                            id: intelligentModelRepeater
                            model: modelItemModel

                            delegate: Rectangle {
                                id: intelligentModelDelegateRectangle
                                width: 274
                                height: 36
                                radius: 6
                                visible: modelType === "intelligent"

                                // 标志当前item高亮
                                property bool isKeyboardHighlighted: dropdownPopup.currentIndex === getGlobalIndex()

                                // 计算当前项在全局可见项列表中的索引
                                function getGlobalIndex() {
                                    var globalIndex = 0

                                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                                        if (i < index) {
                                            var item = intelligentModelRepeater.itemAt(i)
                                            if (item && item.visible) globalIndex++
                                        } else {
                                            break
                                        }
                                    }
                                    return globalIndex
                                }

                                // 设置选中态与非选中态
                                Connections {
                                    target: intelligentModelDelegateRectangle

                                    onIsKeyboardHighlightedChanged: function() {
                                        if (isKeyboardHighlighted === true) {
                                            intelligentModelDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            intelligentModelLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            intelligentModelIcon.source = iconPathWhite
                                            intelligentModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic_white.svg" : ""
                                        } else {
                                            intelligentModelDelegateRectangle.color = "transparent"
                                            intelligentModelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            intelligentModelIcon.source = iconPath
                                            intelligentModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic.svg" : ""
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // 选中状态的勾选图标
                                    Rectangle {
                                        id: intelligentModelCheckedIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"
                                        opacity: ButtonState.selectedModel === modelName ? 1 : 0

                                        Image {
                                            id: intelligentModelCheckedIcon
                                            anchors.fill: parent
                                            source: "qrc:/icon/icon/object-select-symbolic.svg"
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型图标
                                    Rectangle {
                                        id: intelligentModelIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"

                                        Image {
                                            id: intelligentModelIcon
                                            anchors.fill: parent
                                            source: iconPath
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型名称
                                    Label {
                                        id: intelligentModelLabel
                                        text: modelName
                                        color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width - 16 - 8 - 16 - 8 - 8 - 8
                                    }

                                    // 初始化按钮状态
                                    Component.onCompleted: {
                                        if(ButtonState.selectedModel === modelName && modelType === "intelligent") {
                                            ButtonState.deepThinkingSwitched = isSupportsThinkingToggle
                                            ButtonState.deepThinkingEnabled = isSupportsThinking
                                            if (ButtonState.storageWebSearchEnabled === null) {
                                                ButtonState.webSearchEnabled = isSupportsWebSearch
                                            }
                                            ButtonState.modelButtonIcon = iconPath
                                        }
                                    }
                                }

                                MouseArea {
                                    id: intelligentMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true

                                    onEntered: {
                                        dropdownPopup.currentIndex = getGlobalIndex()
                                    }

                                    onExited: {
                                        dropdownPopup.currentIndex = -1
                                    }

                                    onPressed: {
                                        intelligentModelDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                    }

                                    onReleased: {
                                        intelligentModelDelegateRectangle.color = "transparent"
                                    }

                                    onClicked: {
                                        ButtonState.selectedModel = modelName
                                        ButtonState.currentModel = modelName
                                        ButtonState.isCurrentModelDeleted = false
                                        ButtonState.modelButtonIcon = iconPath
                                        modelChanged(modelName, isSupportsThinking, isSupportsWebSearch, isSupportsThinkingToggle)
                                        dropdownPopup.visible = false
                                        msgPane.onSendDataCollectWithProps("switch_intelligent_model_click_event", "modelSwitchPage", {"modelName": modelName})
                                    }
                                }

                                // 模型名称过长显示省略号且鼠标悬浮时显示ToolTip
                                ToolTip {
                                    id: intelligentModelToolTip
                                    visible: intelligentMouseArea.containsMouse && intelligentModelLabel.truncated
                                    text: modelName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var wrapMode: Text.NoWrap
                                }
                            }
                        }
                    }

                    // 云端模型分类
                    Column {
                        id: cloudModelColumn
                        width: parent.width - 16
                        spacing: 2
                        visible: cloudModelRepeater.count > 0

                        // 分类标题
                        Label {
                            text: qsTr("Cloud Model")
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
                            font.pointSize: globals.fontsize - 2
                            leftPadding: 32
                        }

                        // 云端模型列表
                        Repeater {
                            id: cloudModelRepeater
                            model: modelItemModel

                            delegate: Rectangle {
                                id: cloudModelDelegateRectangle
                                width: 274
                                height: 36
                                radius: 6
                                visible: modelType === "cloud"

                                property bool isKeyboardHighlighted: dropdownPopup.currentIndex === getGlobalIndex()

                                function getGlobalIndex() {
                                    var globalIndex = 0
                                    // 先计算前面所有分类的可见项数量
                                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                                        var item = intelligentModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }
                                    // 再计算当前分类中当前项之前的可见项数量
                                    for (i = 0; i < cloudModelRepeater.count; i++) {
                                        if (i < index) {
                                            item = cloudModelRepeater.itemAt(i)
                                            if (item && item.visible) globalIndex++
                                        } else {
                                            break
                                        }
                                    }
                                    return globalIndex
                                }

                                Connections {
                                    target: cloudModelDelegateRectangle

                                    onIsKeyboardHighlightedChanged: function() {
                                        if (isKeyboardHighlighted === true) {
                                            cloudModelDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            cloudModelLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            cloudModelIcon.source = iconPathWhite
                                            cloudModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic_white.svg" : ""
                                        } else {
                                            cloudModelDelegateRectangle.color = "transparent"
                                            cloudModelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            cloudModelIcon.source = iconPath
                                            cloudModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic.svg" : ""
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // 选中状态的勾选图标
                                    Rectangle {
                                        id: cloudModelCheckedIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"
                                        opacity: ButtonState.selectedModel === modelName ? 1 : 0

                                        Image {
                                            id: cloudModelCheckedIcon
                                            anchors.fill: parent
                                            source: "qrc:/icon/icon/object-select-symbolic.svg"
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型图标
                                    Rectangle {
                                        id: cloudModelIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"

                                        Image {
                                            id: cloudModelIcon
                                            anchors.fill: parent
                                            source: iconPath
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型名称
                                    Label {
                                        id: cloudModelLabel
                                        text: modelName
                                        color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width - 16 - 8 - 16 - 8 - 8 - 8
                                    }

                                    // 初始化按钮状态
                                    Component.onCompleted: {
                                        if(ButtonState.selectedModel === modelName && modelType === "cloud") {
                                            ButtonState.deepThinkingSwitched = isSupportsThinkingToggle
                                            ButtonState.deepThinkingEnabled = isSupportsThinking
                                            if (ButtonState.storageWebSearchEnabled === null) {
                                                ButtonState.webSearchEnabled = isSupportsWebSearch
                                            }
                                            ButtonState.modelButtonIcon = iconPath
                                        }
                                    }
                                }

                                MouseArea {
                                    id: cloudMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true

                                    onEntered: {
                                        dropdownPopup.currentIndex = getGlobalIndex()
                                    }

                                    onExited: {
                                        dropdownPopup.currentIndex = -1
                                    }

                                    onPressed: {
                                        cloudModelDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                    }

                                    onReleased: {
                                        cloudModelDelegateRectangle.color = "transparent"
                                    }

                                    onClicked: {
                                        ButtonState.selectedModel = modelName
                                        ButtonState.currentModel = modelName
                                        ButtonState.isCurrentModelDeleted = false
                                        ButtonState.modelButtonIcon = iconPath
                                        modelChanged(modelName, isSupportsThinking, isSupportsWebSearch, isSupportsThinkingToggle)
                                        dropdownPopup.visible = false
                                        msgPane.onSendDataCollectWithProps("switch_cloud_model_click_event", "modelSwitchPage", {"modelName": modelName})
                                    }
                                }

                                // 模型名称过长显示省略号且鼠标悬浮时显示ToolTip
                                ToolTip {
                                    id: cloudModelToolTip
                                    visible: cloudMouseArea.containsMouse && cloudModelLabel.truncated
                                    text: modelName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var wrapMode: Text.NoWrap
                                }
                            }
                        }
                    }

                    // 本地模型分类 - 修改：根据是否有共享模型决定是否显示
                    Column {
                        id: localModelColumn
                        width: parent.width - 16
                        spacing: 2
                        visible: !hasSharedModel && localModelRepeater.count > 0  // 有共享模型时不显示本地模型

                        // 分类标题
                        Label {
                            text: qsTr("Local Model")
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
                            font.pointSize: globals.fontsize - 2
                            leftPadding: 32
                            visible: globals.isLocalModelSupported
                        }

                        // 本地模型列表
                        Repeater {
                            id: localModelRepeater
                            model: modelItemModel

                            delegate: Rectangle {
                                id: localModelDelegateRectangle
                                width: 274
                                height: 36
                                radius: 6
                                visible: modelType === "local" && !isSharedModel   // 只显示非共享的本地模型

                                property bool isKeyboardHighlighted: dropdownPopup.currentIndex === getGlobalIndex()

                                function getGlobalIndex() {
                                    var globalIndex = 0

                                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                                        var item = intelligentModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < cloudModelRepeater.count; i++) {
                                        item = cloudModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < localModelRepeater.count; i++) {
                                        if (i < index) {
                                            item = localModelRepeater.itemAt(i)
                                            if (item && item.visible) globalIndex++
                                        } else {
                                            break
                                        }
                                    }
                                    return globalIndex
                                }

                                Connections {
                                    target: localModelDelegateRectangle

                                    onIsKeyboardHighlightedChanged: function() {
                                        if (isKeyboardHighlighted === true && isModelEnabled) {  // 只有可用的模型才高亮
                                            localModelDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            localModelLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            localModelIcon.source = iconPathWhite
                                            localModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic_white.svg" : ""
                                        } else {
                                            localModelDelegateRectangle.color = "transparent"
                                            localModelLabel.color = isModelEnabled ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.35)
                                            localModelIcon.source = iconPath
                                            localModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic.svg" : ""
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // 选中状态的勾选图标
                                    Rectangle {
                                        id: localModelCheckedIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"
                                        opacity: ButtonState.selectedModel === modelName ? 1 : 0

                                        Image {
                                            id: localModelCheckedIcon
                                            anchors.fill: parent
                                            source: "qrc:/icon/icon/object-select-symbolic.svg"
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型图标
                                    Rectangle {
                                        id: localModelIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"

                                        Image {
                                            id: localModelIcon
                                            anchors.fill: parent
                                            source: iconPath
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型名称
                                    Label {
                                        id: localModelLabel
                                        text: modelName
                                        color: isModelEnabled ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.35)  // 根据是否可用设置颜色
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width - 16 - 8 - 16 - 8 - 8 - 8
                                    }

                                    // 初始化按钮状态
                                    Component.onCompleted: {
                                        if(ButtonState.selectedModel === modelName && modelType === "local") {
                                            ButtonState.deepThinkingSwitched = isSupportsThinkingToggle
                                            ButtonState.deepThinkingEnabled = isSupportsThinking
                                            if (ButtonState.storageWebSearchEnabled === null) {
                                                ButtonState.webSearchEnabled = isSupportsWebSearch
                                            }
                                            ButtonState.modelButtonIcon = iconPath
                                        }
                                    }
                                }

                                MouseArea {
                                    id: localMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: isModelEnabled  // 根据是否可用设置点击状态

                                    onEntered: {
                                        if (isModelEnabled) {  // 只有可用的模型才有悬停效果
                                            dropdownPopup.currentIndex = getGlobalIndex()
                                        }
                                    }

                                    onExited: {
                                        dropdownPopup.currentIndex = -1
                                    }

                                    onPressed: {
                                        if (isModelEnabled) {  // 只有可用的模型才有按下效果
                                            localModelDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                        }
                                    }

                                    onReleased: {
                                        if (isModelEnabled) {  // 只有可用的模型才有释放效果
                                            localModelDelegateRectangle.color = "transparent"
                                        }
                                    }

                                    onClicked: {
                                        if (isModelEnabled) {  // 只有可用的模型才能点击
                                            ButtonState.selectedModel = modelName
                                            ButtonState.currentModel = modelName
                                            ButtonState.isCurrentModelDeleted = false
                                            ButtonState.modelButtonIcon = iconPath
                                            modelChanged(modelName, isSupportsThinking, isSupportsWebSearch, isSupportsThinkingToggle)
                                            dropdownPopup.visible = false
                                            msgPane.onSendDataCollectWithProps("switch_local_model_click_event", "modelSwitchPage", {"modelName": modelName})
                                        }
                                    }
                                }

                                // 模型名称过长显示省略号且鼠标悬浮时显示ToolTip
                                ToolTip {
                                    id: localModelToolTip
                                    visible: localMouseArea.containsMouse && localModelLabel.truncated
                                    text: modelName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var wrapMode: Text.NoWrap
                                }
                            }
                        }
                    }

                    // 自选模型分类
                    Column {
                        id: optionalModelColumn
                        width: parent.width -16
                        spacing: 2
                        visible: optionalModelRepeater.count > 0

                        // 分类标题
                        Label {
                            text: qsTr("Optional Model")
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
                            font.pointSize: globals.fontsize - 2
                            leftPadding: 32
                        }

                        // 自选模型列表
                        Repeater {
                            id: optionalModelRepeater
                            model: modelItemModel

                            delegate: Rectangle {
                                id: optionalModelDelegateRectangle
                                width: 274
                                height: 36
                                radius: 6
                                visible: modelType === "optional"
                                readonly property string defaultOptionalIcon: "qrc:/html/icon/custom-model.svg"
                                readonly property string defaultOptionalIconWhite: "qrc:/html/icon/custom-model-white.svg"

                                property bool isKeyboardHighlighted: dropdownPopup.currentIndex === getGlobalIndex()

                                function getGlobalIndex() {
                                    var globalIndex = 0

                                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                                        var item = intelligentModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < cloudModelRepeater.count; i++) {
                                        item = cloudModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < localModelRepeater.count; i++) {
                                        item = localModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < optionalModelRepeater.count; i++) {
                                        if (i < index) {
                                            item = optionalModelRepeater.itemAt(i)
                                            if (item && item.visible) globalIndex++
                                        } else {
                                            break
                                        }
                                    }
                                    return globalIndex
                                }

                                Connections {
                                    target: optionalModelDelegateRectangle

                                    onIsKeyboardHighlightedChanged: function() {
                                        if (isKeyboardHighlighted === true) {
                                            optionalModelDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            optionalModelLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            optionalModelIcon.source = defaultOptionalIconWhite
                                            optionalModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic_white.svg" : ""
                                        } else {
                                            optionalModelDelegateRectangle.color = "transparent"
                                            optionalModelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            optionalModelIcon.source = defaultOptionalIcon
                                            optionalModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic.svg" : ""
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // 选中状态的勾选图标
                                    Rectangle {
                                        id: optionalModelCheckedIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"
                                        opacity: ButtonState.selectedModel === modelName ? 1 : 0

                                        Image {
                                            id: optionalModelCheckedIcon
                                            anchors.fill: parent
                                            source: "qrc:/icon/icon/object-select-symbolic.svg"
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型图标
                                    Rectangle {
                                        id: optionalModelIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"

                                        Image {
                                            id: optionalModelIcon
                                            anchors.fill: parent
                                            source: defaultOptionalIcon
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型名称
                                    Label {
                                        id: optionalModelLabel
                                        text: modelName
                                        color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width - 16 - 8 - 16 - 8 - 8 - 8
                                    }

                                    // 初始化按钮状态
                                    Component.onCompleted: {
                                        if(ButtonState.selectedModel === modelName && modelType === "optional") {
                                            ButtonState.deepThinkingSwitched = isSupportsThinkingToggle
                                            ButtonState.deepThinkingEnabled = isSupportsThinking
                                            if (ButtonState.storageWebSearchEnabled === null) {
                                                ButtonState.webSearchEnabled = isSupportsWebSearch
                                            }
                                            ButtonState.modelButtonIcon = defaultOptionalIcon
                                        }
                                    }
                                }

                                MouseArea {
                                    id: optionalMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true

                                    onEntered: {
                                        dropdownPopup.currentIndex = getGlobalIndex()
                                    }

                                    onExited: {
                                        dropdownPopup.currentIndex = -1
                                    }

                                    onPressed: {
                                        optionalModelDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                    }

                                    onReleased: {
                                        optionalModelDelegateRectangle.color = "transparent"
                                    }

                                    onClicked: {
                                        ButtonState.selectedModel = modelName
                                        ButtonState.currentModel = modelName
                                        ButtonState.isCurrentModelDeleted = false
                                        ButtonState.modelButtonIcon = defaultOptionalIcon
                                        modelChanged(modelName, isSupportsThinking, isSupportsWebSearch, isSupportsThinkingToggle)
                                        dropdownPopup.visible = false
                                        msgPane.onSendDataCollectWithProps("switch_optional_model_click_event", "modelSwitchPage", {"modelName": modelName})
                                    }
                                }

                                // 模型名称过长显示省略号且鼠标悬浮时显示ToolTip
                                ToolTip {
                                    id: optionalModelToolTip
                                    visible: optionalMouseArea.containsMouse && optionalModelLabel.truncated
                                    text: modelName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var wrapMode: Text.NoWrap
                                }
                            }
                        }
                    }

                    // 算力共享模型分类
                    Column {
                        id: sharedModelColumn
                        width: parent.width - 16
                        spacing: 2
                        visible: hasSharedModel && sharedModelRepeater.count > 0  // 有共享模型时才显示

                        // 分类标题
                        Label {
                            text: qsTr("Compute Sharing Model")
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.35)
                            font.pointSize: globals.fontsize - 2
                            leftPadding: 32
                        }

                        // 共享模型列表
                        Repeater {
                            id: sharedModelRepeater
                            model: modelItemModel

                            delegate: Rectangle {
                                id: sharedModelDelegateRectangle
                                width: 274
                                height: 36
                                radius: 6
                                visible: isSharedModel || modelType === "shared"  // 显示共享模型

                                property bool isKeyboardHighlighted: dropdownPopup.currentIndex === getGlobalIndex()

                                function getGlobalIndex() {
                                    var globalIndex = 0

                                    for (var i = 0; i < intelligentModelRepeater.count; i++) {
                                        var item = intelligentModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < cloudModelRepeater.count; i++) {
                                        item = cloudModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < localModelRepeater.count; i++) {
                                        item = localModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < optionalModelRepeater.count; i++) {
                                        item = optionalModelRepeater.itemAt(i)
                                        if (item && item.visible) globalIndex++
                                    }

                                    for (i = 0; i < sharedModelRepeater.count; i++) {
                                        if (i < index) {
                                            item = sharedModelRepeater.itemAt(i)
                                            if (item && item.visible) globalIndex++
                                        } else {
                                            break
                                        }
                                    }
                                    return globalIndex
                                }

                                Connections {
                                    target: sharedModelDelegateRectangle

                                    onIsKeyboardHighlightedChanged: function() {
                                        if (isKeyboardHighlighted === true) {
                                            sharedModelDelegateRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 1)
                                            sharedModelLabel.color = Qt.rgba(255/255, 255/255, 255/255, 1)
                                            sharedModelIcon.source = iconPathWhite
                                            sharedModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic_white.svg" : ""
                                        } else {
                                            sharedModelDelegateRectangle.color = "transparent"
                                            sharedModelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            sharedModelIcon.source = iconPath
                                            sharedModelCheckedIcon.source =
                                            ButtonState.selectedModel === modelName ? "qrc:/icon/icon/object-select-symbolic.svg" : ""
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // 选中状态的勾选图标
                                    Rectangle {
                                        id: sharedModelCheckedIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"
                                        opacity: ButtonState.selectedModel === modelName ? 1 : 0

                                        Image {
                                            id: sharedModelCheckedIcon
                                            anchors.fill: parent
                                            source: "qrc:/icon/icon/object-select-symbolic.svg"
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型图标
                                    Rectangle {
                                        id: sharedModelIconRect
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        color: "transparent"

                                        Image {
                                            id: sharedModelIcon
                                            anchors.fill: parent
                                            source: iconPath
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }

                                    // 模型名称
                                    Label {
                                        id: sharedModelLabel
                                        text: modelName
                                        color: Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width - 16 - 8 - 16 - 8 - 8 - 8
                                    }

                                    // 初始化按钮状态
                                    Component.onCompleted: {
                                        if(ButtonState.selectedModel === modelName && (isSharedModel || modelType === "shared")) {
                                            ButtonState.deepThinkingSwitched = isSupportsThinkingToggle
                                            ButtonState.deepThinkingEnabled = isSupportsThinking
                                            if (ButtonState.storageWebSearchEnabled === null) {
                                                ButtonState.webSearchEnabled = isSupportsWebSearch
                                            }
                                            ButtonState.modelButtonIcon = iconPath
                                        }
                                    }
                                }

                                MouseArea {
                                    id: sharedMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true

                                    onEntered: {
                                        dropdownPopup.currentIndex = getGlobalIndex()
                                    }

                                    onExited: {
                                        dropdownPopup.currentIndex = -1
                                    }

                                    onPressed: {
                                        sharedModelDelegateRectangle.color = Qt.rgba(44/255, 115/255, 200/255, 1)
                                    }

                                    onReleased: {
                                        sharedModelDelegateRectangle.color = "transparent"
                                    }

                                    onClicked: {
                                        ButtonState.selectedModel = modelName
                                        ButtonState.currentModel = modelName
                                        ButtonState.isCurrentModelDeleted = false
                                        ButtonState.modelButtonIcon = iconPath
                                        modelChanged(modelName, isSupportsThinking, isSupportsWebSearch, isSupportsThinkingToggle)
                                        dropdownPopup.visible = false
                                        msgPane.onSendDataCollectWithProps("switch_shared_model_click_event", "modelSwitchPage", {"modelName": modelName})
                                    }
                                }

                                // 模型名称过长显示省略号且鼠标悬浮时显示ToolTip
                                ToolTip {
                                    id: sharedModelToolTip
                                    visible: sharedMouseArea.containsMouse && sharedModelLabel.truncated
                                    text: modelName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var wrapMode: Text.NoWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        background: Rectangle {
            anchors.fill: parent
            color: "#FFFFFF"
            radius: 8
            border.color: Qt.rgba(0, 0, 0, 0.1)
            border.width: 1
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 50 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 50 }
        }
    }

    // 断开算力共享时的处理（需要在外部调用）
    function disconnectSharedModel() {
        // 检查当前选中的是否是共享模型
        for (var i = 0; i < modelItemModel.rowCount(); i++) {
            var item = modelItemModel.get(i)
            if (item.modelName === ButtonState.currentModel &&
                (item.isSharedModel || item.modelType === "shared")) {
                // 切换到智能调度模型
                for (var j = 0; j < modelItemModel.rowCount(); j++) {
                    var intelligentItem = modelItemModel.get(j)
                    if (intelligentItem.modelType === "intelligent") {
                        ButtonState.selectedModel = intelligentItem.modelName
                        ButtonState.currentModel = intelligentItem.modelName
                        ButtonState.modelButtonIcon = intelligentItem.iconPath
                        modelChanged(intelligentItem.modelName,
                                   intelligentItem.isSupportsThinking,
                                   intelligentItem.isSupportsWebSearch,
                                   intelligentItem.isSupportsThinkingToggle)
                        break
                    }
                }
                break
            }
        }
    }
}
