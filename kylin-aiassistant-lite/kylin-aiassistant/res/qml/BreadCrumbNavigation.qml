// BreadCrumbNavigation.qml
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."

Item {
    id: breadCrumbNavigation
    height: 36

    // 公共属性
    property string rootPath: "" // 根路径
    property string currentPath: "" // 当前完整路径
    property variant breadCrumbPath: splitPathToBreadcrumbs(currentPath) // 外部传入的面包屑路径数组
    property bool isWindowMode: root.winMode === 3   // 是否窗口模式，默认侧边栏模式

    // 计算属性：根据模式获取最大宽度
    property int maxTotalWidth: isWindowMode ? 404 : 328
    property int maxButtonWidth: isWindowMode ? 184 : 146

    // 内部状态
    property int visibleStartIndex: 0   // 可见路径的起始索引
    property int hiddenCount: 0         // 隐藏的路径数量
    property int totalContentWidth: 0   // 所有内容的总宽度

    // 信号
    signal crumbClicked(string fullPath)

    // 将完整路径拆分为面包屑数组
    function splitPathToBreadcrumbs(fullPath) {
        if (!fullPath) {
            return [qsTr("All Files")]
        }

        // 确保路径以基础路径开头
        var pathStr = fullPath.toString()
        if (!pathStr.startsWith(rootPath)) {
            console.warn("路径不在基础路径下:", fullPath)
            return [qsTr("All Files")]
        }

        // 去除基础路径部分
        var relativePath = pathStr.substring(rootPath.length)

        // 分割路径
        var parts = relativePath.split('/').filter(function(part) {
            return part !== "" && part !== null && part !== undefined
        })

        // 构建面包屑数组
        var breadcrumbs = [qsTr("All Files")]
        for (var i = 0; i < parts.length; i++) {
            breadcrumbs.push(parts[i])
        }

        return breadcrumbs
    }

    // 根据面包屑索引获取完整路径
    function getFullPathFromCrumbIndex(index) {
        if (index < 0 || index >= breadCrumbPath.length) {
            return rootPath
        }

        if (index === 0) {
            return rootPath
        }

        // 获取对应部分的路径片段
        var pathSegments = []
        for (var i = 1; i <= index; i++) {
            pathSegments.push(breadCrumbPath[i])
        }

        if (pathSegments.length === 0) {
            return rootPath
        }

        return rootPath + "/" + pathSegments.join('/')
    }

    // 计算实际显示的路径数组
    function calculateDisplayPath() {
        if (!breadCrumbPath || breadCrumbPath.length === 0) {
            return []
        }

        // 如果全部显示得下，返回完整路径
        if (totalContentWidth <= maxTotalWidth || breadCrumbPath.length === 1) {
            visibleStartIndex = 0
            hiddenCount = 0
            return breadCrumbPath
        }

        // 需要隐藏前面的路径
        var displayPath = []
        var accumulatedWidth = 0
        var separatorWidth = 0

        // 从最后一项开始往前计算
        for (var i = breadCrumbPath.length - 1; i >= 0; i--) {
            var itemText = breadCrumbPath[i]
            var isLastInDisplay = (i === breadCrumbPath.length - 1)

            // 计算按钮宽度
            var textWidth = calculateTextWidth(itemText)
            // 根据是否最后一项计算宽度
            var estimatedWidth = isLastInDisplay ?
                Math.min(textWidth + 16, maxButtonWidth) : // 最后一项：文本宽度 + 16px
                Math.min(textWidth + 40, maxButtonWidth)   // 非最后一项：文本宽度 + 40px

            separatorWidth = (displayPath.length > 0) ? 24 : 0  // 固定分隔符宽度

            // 如果加上此项后不超过限制，添加到显示列表
            if (accumulatedWidth + estimatedWidth + separatorWidth <= maxTotalWidth) {
                displayPath.unshift(itemText) // 添加到前面
                accumulatedWidth += estimatedWidth + separatorWidth
            } else {
                // 超过限制，设置隐藏部分
                visibleStartIndex = i + 1
                hiddenCount = i + 1

                // 重要：如果只有最后一项且它超宽，仍然要显示它
                if (i === breadCrumbPath.length - 1) {
                    displayPath.unshift(itemText)
                    visibleStartIndex = breadCrumbPath.length - 1
                    hiddenCount = breadCrumbPath.length - 1
                }
                break
            }
        }

        // 如果没有隐藏任何项
        if (visibleStartIndex === 0 && hiddenCount === 0) {
            return breadCrumbPath
        }

        return displayPath
    }

    // 计算文本宽度（用于估算）
    function calculateTextWidth(text) {
        var metrics = Qt.createQmlObject('import QtQuick 2.12; TextMetrics{}',
                                        breadCrumbNavigation, "dynamicMetrics")
        metrics.text = text
        var textWidth = metrics.width
        metrics.destroy()

        // 返回纯文本宽度
        return textWidth
    }

    // 计算单个按钮宽度
    function calculateButtonWidth(text, isLast) {
        var textWidth = calculateTextWidth(text)
        if (isLast) {
            // 最后一项：文本宽度 + 16px（左右边距各8px）
            return Math.min(textWidth + 16, maxButtonWidth)
        } else {
            // 非最后一项：文本宽度 + 40px（左右边距各8px + 图标16px + 图标与文本间距8px）
            return Math.min(textWidth + 40, maxButtonWidth)
        }
    }

    // 更新总宽度
    function updateTotalWidth() {
        if (!breadCrumbPath || breadCrumbPath.length === 0) {
            totalContentWidth = 0
            return
        }

        var width = 0
        for (var i = 0; i < breadCrumbPath.length; i++) {
            var isLast = (i === breadCrumbPath.length - 1)
            var buttonWidth = calculateButtonWidth(breadCrumbPath[i], isLast)
            width += buttonWidth

            if (i < breadCrumbPath.length - 1) {
                width += 24  // 固定分隔符宽度
            }
        }
        totalContentWidth = width
    }

    // 当路径改变时更新
    onBreadCrumbPathChanged: {
        updateTotalWidth()
        visibleStartIndex = 0
    }

    // 主容器
    Row {
        id: breadCrumbContainer
        spacing: 0
        anchors.fill: parent

        // 展开按钮（当有隐藏路径时显示）
        Button {
            id: expandButton
            visible: hiddenCount > 0
            width: visible ? 40 : 0
            height: parent.height
            icon.name: hiddenMenu.visible ? "qrc:/icon/icon/knowledge-bread-down.svg" : "qrc:/icon/icon/knowledge-bread-right.svg"
            focusPolicy: Qt.NoFocus

            // 点击弹出菜单
            onClicked: {
                console.log("点击弹出菜单", hiddenMenu.visible, breadCrumbPath.slice(0, hiddenCount))
                if (hiddenMenu.visible) {
                    hiddenMenu.close()
                } else {
                    hiddenMenu.visible = true
                }
            }
        }

        // 显示的面包屑路径
        Repeater {
            id: crumbRepeater
            model: calculateDisplayPath()

            delegate: Item {
                id: delegateItem
                width: buttonLoader.item ? buttonLoader.item.width : 0
                height: breadCrumbContainer.height

                // 加载按钮组件
                Loader {
                    id: buttonLoader
                    anchors.fill: parent
                    sourceComponent: crumbButtonComponent

                    property string buttonText: modelData
                    property int buttonIndex: index
                    property bool isLast: buttonIndex + hiddenCount === breadCrumbPath.length - 1

                    onLoaded: {
                        // 设置按钮文本
                        item.text = buttonText
                        // 设置按钮宽度
                        var buttonWidth = calculateButtonWidth(buttonText, isLast)
                        item.width = buttonWidth
                    }
                }

                // 面包屑按钮组件
                Component {
                    id: crumbButtonComponent

                    Button {
                        id: crumbButton
                        height: parent.height
                        // 宽度在loaded时通过Loader设置
                        width: 0
                        focusPolicy: Qt.NoFocus

                        text: buttonText

                        // 按钮样式
                        background: Rectangle {
                            color: crumbButton.pressed ? "#B9B9B9" : crumbButton.hovered ? "#DCDCDC" : "transparent"
                            radius: 6
                        }

                        contentItem: Item {
                            anchors.fill: parent
                            Row {
                                id: pathRow
                                anchors.centerIn: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8  // 图标和文本间距8px
                                property bool isLast: buttonLoader.isLast

                                Label {
                                    text: crumbButton.text
                                    // 宽度计算：按钮宽度减去边距和图标
                                    width: buttonLoader.isLast ?
                                        Math.min(implicitWidth, maxButtonWidth - 16) :  // 最后一项：按钮宽度-16px
                                        Math.min(implicitWidth, maxButtonWidth - 40)    // 非最后一项：按钮宽度-40px
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: pathRow.isLast
                                    font.family: root.fontFamily
                                    color: Qt.rgba(0, 0, 0, 0.85)
                                }

                                Rectangle {
                                    width: 16
                                    height: 16
                                    color: "transparent"
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: !pathRow.isLast

                                    Image {
                                        anchors.fill: parent
                                        source: "qrc:/icon/icon/knowledge-bread-right.svg"
                                        fillMode: Image.PreserveAspectFit
                                    }
                                }
                            }
                        }

                        // 点击事件
                        onClicked: {
                            // 计算原始路径中的索引
                            var originalIndex = visibleStartIndex + buttonLoader.buttonIndex
                            if (originalIndex < breadCrumbPath.length) {
                                let fullPath = getFullPathFromCrumbIndex(originalIndex)
                                crumbClicked(fullPath)
                            }
                        }

                        // 鼠标悬停显示完整文本
                        ToolTip {
                            visible: crumbButton.hovered
                            text: buttonText
                            delay: Qt.styleHints.mousePressAndHoldInterval
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }

    // 隐藏路径的菜单
    Menu {
        id: hiddenMenu
        width: 200
        y: 40
        MenuItem {
            visible: hiddenCount > 0
            text: breadCrumbPath[0]
            onClicked: {
                crumbClicked(rootPath)
                hiddenMenu.close()
            }
        }
        MenuItem {
            id: hiddenMenuSecondItem
            visible: hiddenCount > 1
            text: breadCrumbPath.length > 1 ? breadCrumbPath[1] : ""
            hoverEnabled: true
            onClicked: {
                let fullPath = getFullPathFromCrumbIndex(1)
                crumbClicked(fullPath)
                hiddenMenu.close()
            }
            // 使用自定义Text组件
            contentItem: Label {
                id: contentItemText
                text: hiddenMenuSecondItem.text
                elide: Text.ElideRight
            }
            ToolTip {
                visible: hiddenMenuSecondItem.hovered && hiddenMenuSecondItem.text !== "" && contentItemText.truncated
                text: hiddenMenuSecondItem.text
                delay: Qt.styleHints.mousePressAndHoldInterval
                property var maxWidth: 400
                property var wrapMode: Text.WrapAnywhere
            }
        }
    }
}
