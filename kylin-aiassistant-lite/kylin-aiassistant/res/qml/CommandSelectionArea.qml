import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
// arm设备不支持QtGraphicalEffects，import本身会导致GL状态异常，展开区域显示空白
// import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 指令选择区域控件
// 一行展示不下时显示 "..."，点击后以相同样式展开未展示的指令（不显示说明文案）
Column {
    id: commandSelectionArea
    Accessible.name: "kylin-aiassistant_CommandSelectionArea"
    spacing: 8

    // 属性接口
    property bool areaVisible: true  // 整个区域的可见性
    property int showMoreThreshold: 2  // 显示"更多"按钮的阈值，同时也是默认显示的最大指令数量
    property int moreButtonWidth: 20   // "..." 按钮预留宽度
    property int itemSpacing: 8        // 按钮之间的间距
    property int sidePadding: 16       // 左右边距
    property int separatorWidth: 1     // 分隔符宽度
    property int oneLineHeight: 24
    property int commandListMaxHeight: 100

    // 动态计算可显示的命令数量
    property int dynamicShowCount: 1
    property bool isReplyingFlag: false

    // 根据实际按钮宽度计算可显示的命令数量
    function calculateVisibleCount() {
        if (typeof commandInstructionModel === "undefined" || !commandInstructionModel) {
            return 0
        }

        var totalCommands = commandInstructionModel.count ? commandInstructionModel.count() : 0
        if (totalCommands === 0) return 0

        var ellipsisWidth = (moreCommandButton && moreCommandButton.width > 0) ? moreCommandButton.width : moreButtonWidth
        // 可用宽度 = 容器宽度 - 左右边距 - "..."按钮宽度 - 间距
        var availableWidth = defaultCommandArea.width - sidePadding * 2 - ellipsisWidth - itemSpacing
        if (availableWidth <= 0) return 1

        var usedWidth = 0
        var count = 0

        // 遍历每个命令，累加实际宽度
        for (var i = 0; i < totalCommands; i++) {
            var item = defaultCommandRepeater.itemAt(i)
            // 每个条目的宽度 = 按钮宽度 + 分隔符宽度 + 间距
            var buttonWidth = 60  // 默认估算值
            if (item && item.children && item.children[0]) {
                buttonWidth = item.children[0].width  // 获取内部Rectangle的宽度
            }
            var itemTotalWidth = buttonWidth + separatorWidth + itemSpacing * 2

            if (usedWidth + itemTotalWidth <= availableWidth) {
                usedWidth += itemTotalWidth
                count++
            } else {
                break
            }
        }

        return Math.max(1, count)
    }

    Timer {
        id: fontSizeChangeTimer
        interval: 1
        repeat: false
        onTriggered: {
            dynamicShowCount = calculateVisibleCount()
        }
    }

    // 延迟计算器
    Timer {
        id: recalculateTimer
        interval: 50
        repeat: false
        onTriggered: {
            dynamicShowCount = calculateVisibleCount()
        }
    }

    // 初始化延迟计算器（等待 Repeater 完成创建子项）
    Timer {
        id: initialCalculateTimer
        interval: 200
        repeat: false
        onTriggered: {
            dynamicShowCount = calculateVisibleCount()
        }
    }

    // 页面切换时自动收起展开的指令列表
    onVisibleChanged: {
        if (!visible) {
            allCommandsContainer.visible = false
        }
    }

    onAreaVisibleChanged: {
        if (!areaVisible) {
            allCommandsContainer.visible = false
        }
    }

    // 当容器宽度变化时重新计算
    onWidthChanged: {
        recalculateTimer.restart()
    }

    // 监听 defaultCommandArea 的宽度变化（窗口模式下更可靠）
    Connections {
        target: defaultCommandArea
        onWidthChanged: function() {
            recalculateTimer.restart()
        }
    }

    // 组件完成加载后计算（使用较长延迟确保子项创建完成）
    Component.onCompleted: {
        initialCalculateTimer.restart()
    }

    // 信号：当指令被选中时触发
    signal commandSelected(string uuid, string name, string description, string iconPath)

    // 设置页与问答页保持一致的图标映射与状态资源选择
    function getCommandIconBasePath(uuid) {
        // 返回不带后缀的 base path（不含 .svg / -hover.svg 等）
        if (uuid === "550e8400-e29b-41d4-a716-446655440001") {
            return "qrc:/icon/icon/setting/todo-generate"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440002") {
            return "qrc:/icon/icon/setting/system-QA"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440003") {
            return "qrc:/icon/icon/setting/text-expend"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440004") {
            return "qrc:/icon/icon/setting/text-polish"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440005") {
            return "qrc:/icon/icon/setting/translate"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440006") {
            return "qrc:/icon/icon/setting/weekly-report"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440007") {
            return "qrc:/icon/icon/setting/text-proofread"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440008") {
            return "qrc:/icon/icon/setting/summary"
        } else if (uuid === "550e8400-e29b-41d4-a716-446655440009") {
            return "qrc:/icon/icon/setting/summary"
        } else if (uuid === "550e8400-e29b-41d4-a716-44665544000a") {
            return "qrc:/icon/icon/setting/summary"
        } else if (uuid === "550e8400-e29b-41d4-a716-44665544000b") {
            return "qrc:/icon/icon/setting/summary"
        } else if (uuid === "550e8400-e29b-41d4-a716-44665544000c") {
            return "qrc:/icon/icon/setting/summary"
        }
        return "qrc:/icon/icon/setting/prompt"
    }

    function getCommandIconSource(uuid, hovered, pressed) {
        var base = getCommandIconBasePath(uuid)
        // 少数图标提供 hover/click 资源，其他图标用 highlight 资源即可
        var hasHoverClick = (base === "qrc:/icon/icon/setting/todo-generate")
                        || (base === "qrc:/icon/icon/setting/system-QA")
                        || (base === "qrc:/icon/icon/setting/prompt")
        if (hasHoverClick) {
            if (pressed) return base + "-click.svg"
            if (hovered) return base + "-hover.svg"
            return base + ".svg"
        }
        if (hovered || pressed) return base + "-highlight.svg"
        return base + ".svg"
    }

    function getCommandIconSourceActive(uuid, active) {
        var base = getCommandIconBasePath(uuid)
        if (active) {
            // 高亮态在蓝色底上使用白色版本保证对比度
            // 这些图标都有对应的 -white.svg 版本
            return base + "-white.svg"
        }
        return base + ".svg"
    }

    function getCommandIconPathForSelected(uuid) {
        // 选中后显示在输入框标签的图标，按设置页使用基础图标即可
        return getCommandIconBasePath(uuid) + ".svg"
    }

    // 暴露展开状态给外部使用
    property bool isCommandsExpanded: allCommandsContainer.visible

    // 收起指令滚动区的函数
    function collapseCommands() {
        allCommandsContainer.visible = false
    }

    // 展开指令滚动区的函数
    function expandCommands() {
        if (!areaVisible)
            return
        allCommandsContainer.visible = true
    }

    // 获取所有指令的数量（commandInstructionModel 是全局上下文属性）
    function getAllActiveCommandsCount() {
        if (typeof commandInstructionModel === "undefined" || !commandInstructionModel) {
            return 0
        }
        // 确保返回有效的数量
        var count = commandInstructionModel.count || 0
        return count
    }

    // 检查是否有隐藏的指令（即"更多"按钮是否应该可见）
    function hasHiddenCommands() {
        if (typeof commandInstructionModel === "undefined" || !commandInstructionModel) {
            return false
        }
        var totalCount = commandInstructionModel.count ? commandInstructionModel.count() : 0
        return totalCount > dynamicShowCount
    }

    // 默认指令区域
    Rectangle {
        id: defaultCommandArea
        Accessible.name: "kylin-aiassistant_CommandSelectionArea_defaultCommandArea"
        width: parent.width
        height: oneLineHeight
        visible: areaVisible
        color: "transparent"

        Row {
            id: bottomCommandRow
            spacing: 8
            height: 24
            //anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter

            // 默认显示的指令（直接使用 commandInstructionModel，通过 visible 过滤）
            Repeater {
                id: defaultCommandRepeater
                model: typeof commandInstructionModel !== "undefined" ? commandInstructionModel : null

                // 当 Repeater 完成创建所有子项后重新计算
                onItemAdded: {
                    recalculateTimer.restart()
                }

                delegate: Row {
                    id: defaultCommandItemContainer
                    spacing: 8
                    visible: model && index < dynamicShowCount

                    Rectangle {
                        id: defaultCommandItem
                        Accessible.name: "kylin-aiassistant_CommandSelectionArea_defaultCommandItem"
                        height: 24
                        width: defaultCommandItemRow.width
                        color: "transparent"
                        radius: 4

                        visible: model && index < dynamicShowCount
                        ToolTip {
                            visible: defaultCommandMouseArea.containsMouse && model && model.name && (model.name.length > 8)
                            text: model ? model.name : ""
                            delay: Qt.styleHints.mousePressAndHoldInterval
                        }
                        enabled: !isReplyingFlag
                        opacity: !isReplyingFlag ? 1 : 0.5

                        Row {
                            id: defaultCommandItemRow
                            height: 24
                            spacing: 4

                            Image {
                                id: defaultCommandItemImage
                                source: getCommandIconSource(
                                    model ? (model.uuid || "") : "",
                                    defaultCommandMouseArea.containsMouse,
                                    defaultCommandMouseArea.pressed
                                )
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                            }

                            Label {
                                id: defaultCommandItemText
                                text: model && model.name ? model.name : ""
                                anchors.verticalCenter: parent.verticalCenter
                                // 字体颜色根据状态切换
                                color: defaultCommandMouseArea.pressed
                                    ? Qt.rgba(55/255, 144/255, 250/255, 0.95)
                                    : (defaultCommandMouseArea.containsMouse
                                        ? Qt.rgba(55/255, 144/255, 250/255, 0.8)
                                        : Qt.rgba(0, 0, 0, 0.85))
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                onFontChanged: {
                                    fontSizeChangeTimer.running = true
                                }
                            }
                        }

                        MouseArea {
                            id: defaultCommandMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (model) {
                                    var iconPath = getCommandIconPathForSelected(model.uuid || "")
                                    commandSelectionArea.commandSelected(
                                        model.uuid || "",
                                        model.name || "",
                                        model.description || "",
                                        iconPath
                                    )
                                    allCommandsContainer.visible = false
                                }
                            }
                        }
                    }

                    // 竖线分隔符
                    Rectangle {
                        id: separatorLine
                        width: 1
                        height: 16
                        color: Qt.rgba(0, 0, 0, 0.1)
                        anchors.verticalCenter: parent.verticalCenter
                        visible: index < dynamicShowCount - 1 || (typeof commandInstructionModel !== "undefined" && commandInstructionModel && commandInstructionModel.count() > dynamicShowCount)
                    }
                }
            }

            // 溢出按钮：一行放不下时显示 "..."
            Rectangle {
                id: moreCommandButton
                Accessible.name: "kylin-aiassistant_CommandSelectionArea_moreCommandButton"
                height: 24
                width: moreCommandButtonText.width
                color: "transparent"
                visible: (typeof commandInstructionModel !== "undefined" && commandInstructionModel && commandInstructionModel.count() > dynamicShowCount)

                property bool isExpanded: allCommandsContainer.visible
                property bool hasHiddenCommands: (typeof commandInstructionModel !== "undefined" && commandInstructionModel && commandInstructionModel.count() > dynamicShowCount)

                MouseArea {
                    id: moreCommandMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        allCommandsContainer.visible = !allCommandsContainer.visible
                    }
                }

                Label {
                    id: moreCommandButtonText
                    text: "..."
                    anchors.verticalCenter: parent.verticalCenter
                    color: moreCommandMouseArea.pressed
                        ? Qt.rgba(55/255, 144/255, 250/255, 0.95)
                        : (moreCommandMouseArea.containsMouse
                            ? Qt.rgba(55/255, 144/255, 250/255, 0.8)
                            : Qt.rgba(0, 0, 0, 0.85))
                }
            }
        }
    }

    // 未展示指令（第二行起，样式与第一行一致，不含说明文案）
    Rectangle {
        id: allCommandsContainer
        Accessible.name: "kylin-aiassistant_CommandSelectionArea_allCommandsContainer"
        width: parent.width
        height: visible ? Math.min(commandListMaxHeight, overflowFlick.contentHeight) : 0
        visible: false
        color: "transparent"
        clip: true

        Behavior on height {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }

        Flickable {
            id: overflowFlick
            anchors.fill: parent
            contentWidth: width
            contentHeight: overflowFlow.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {
                policy: overflowFlick.contentHeight > overflowFlick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            Flow {
                id: overflowFlow
                x: bottomCommandRow.x
                width: Math.max(0, allCommandsContainer.width - bottomCommandRow.x * 2)
                spacing: 8

                Repeater {
                    id: allCommandsRepeater
                    model: typeof commandInstructionModel !== "undefined" ? commandInstructionModel : null

                    delegate: Row {
                        id: overflowCommandItemContainer
                        spacing: 8
                        // Qt 5.12 的 Flow 仍会排布 visible:false 的项，需同时置零宽高
                        visible: model !== null && index >= dynamicShowCount
                        width: visible ? implicitWidth : 0
                        height: visible ? 24 : 0

                        Rectangle {
                            id: overflowCommandItem
                            Accessible.name: "kylin-aiassistant_CommandSelectionArea_allCommandItem"
                            height: 24
                            width: overflowCommandItemRow.width
                            color: "transparent"
                            radius: 4
                            enabled: !isReplyingFlag
                            opacity: !isReplyingFlag ? 1 : 0.5

                            ToolTip {
                                visible: overflowCommandMouseArea.containsMouse && model && model.name && (model.name.length > 8)
                                text: model ? model.name : ""
                                delay: Qt.styleHints.mousePressAndHoldInterval
                            }

                            Row {
                                id: overflowCommandItemRow
                                height: 24
                                spacing: 4

                                Image {
                                    source: getCommandIconSource(
                                        model ? (model.uuid || "") : "",
                                        overflowCommandMouseArea.containsMouse,
                                        overflowCommandMouseArea.pressed
                                    )
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16
                                }

                                Label {
                                    text: model && model.name ? model.name : ""
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: overflowCommandMouseArea.pressed
                                        ? Qt.rgba(55/255, 144/255, 250/255, 0.95)
                                        : (overflowCommandMouseArea.containsMouse
                                            ? Qt.rgba(55/255, 144/255, 250/255, 0.8)
                                            : Qt.rgba(0, 0, 0, 0.85))
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }

                            MouseArea {
                                id: overflowCommandMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (model) {
                                        var iconPath = getCommandIconPathForSelected(model.uuid || "")
                                        commandSelectionArea.commandSelected(
                                            model.uuid || "",
                                            model.name || "",
                                            model.description || "",
                                            iconPath
                                        )
                                        allCommandsContainer.visible = false
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 16
                            color: Qt.rgba(0, 0, 0, 0.1)
                            anchors.verticalCenter: parent.verticalCenter
                            visible: typeof commandInstructionModel !== "undefined"
                                     && commandInstructionModel
                                     && index < commandInstructionModel.count() - 1
                        }
                    }
                }
            }
        }
    }
}

