import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12

import "."

// 模块化的联网搜索信息来源组件
// 可以在问答和写作回答中复用
Column {
    id: webSearchSourcesArea
    Accessible.name: "kylin-aiassistant_WebSearchSourcesArea"

    property string webSearchSources: ""  // JSON字符串格式的搜索来源数据
    property bool isEnd: false            // 是否生成结束
    property bool sentByUser: false       // 是否用户发送
    property bool isShowWebSearchSources: false  // 是否应该显示信息来源（由数据模型控制）
    property int parentWidth: 0            // 父容器宽度
    property int customLeftPadding: 16     // 自定义左边距，默认16px

    signal expandedChanged(bool expanded)  // 展开状态变化信号

    visible: !sentByUser && isEnd && isShowWebSearchSources && webSearchSources !== undefined && webSearchSources !== ""
    width: parentWidth
    spacing: 6
    topPadding: 10
    bottomPadding: 10
    leftPadding: customLeftPadding
    rightPadding: 16

    // 信息来源按钮行
    Row {
        id: webSearchSourcesButtonRow
        width: parent.width - parent.leftPadding - parent.rightPadding
        leftPadding: 0
        rightPadding: 0
        bottomPadding: webSearchSourcesButton.isExpanded ? 0 : 8  // 未展开时距离下面8px，展开时不需要间距

        //信息来源按钮
        Rectangle {
            id: webSearchSourcesButton
            Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourcesButton"
            width: 88  // 固定宽度 88px
            height: 32  // 固定高度 32px
            color: "#FFFFFF"
            radius: 6
            property bool isExpanded: false

            Row {
                id: buttonRow
                spacing: 4
                anchors.centerIn: parent

                Label {
                   id: webSearchSourcesButtonText
                   text: qsTr("Sources of information")
                   color: Qt.rgba(0, 0, 0, 0.6)  // 灰色文字
                   anchors.verticalCenter: parent.verticalCenter
                   font.pointSize: globals.fontsize

                   width: 64
                   elide: Text.ElideRight // 超出显示省略号
                   wrapMode: Text.NoWrap // 不换行
                   horizontalAlignment: Text.AlignHCenter
                }

                // 箭头图标在文字右边
                Image {
                    id: webSearchSourcesArrowImage
                    Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourcesArrowImage"
                    source: webSearchSourcesButton.isExpanded ? "qrc:/icon/icon/arrow-up.svg" : "qrc:/icon/icon/arrow-down.svg"
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea{
                id: webSearchSourcesButtonMouseArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onEntered: {
                    webSearchSourcesButtonText.color = Qt.rgba(0, 0, 0, 0.8)
                    // 控制ToolTip的显示
                    if (webSearchSourcesButtonText.truncated) {
                        webSearchSourcesButtonTooltip.visible = true
                    }
                }
                onExited: {
                    webSearchSourcesButtonText.color = Qt.rgba(0, 0, 0, 0.6)
                    webSearchSourcesButtonTooltip.visible = false
                }
                onPressed: {
                    webSearchSourcesButtonText.color = Qt.rgba(0, 0, 0, 0.9)
                    webSearchSourcesButtonTooltip.visible = false
                }
                onClicked: {
                    // 切换箭头状态
                    webSearchSourcesButton.isExpanded = !webSearchSourcesButton.isExpanded
                    console.log("Web search sources button clicked, isExpanded:", webSearchSourcesButton.isExpanded)
                    // 切换展开区域的显示状态
                    webSearchSourcesExpandedArea.visible = webSearchSourcesButton.isExpanded
                    // 发送展开状态变化信号
                    webSearchSourcesArea.expandedChanged(webSearchSourcesButton.isExpanded)
                }
                onReleased: {
                    webSearchSourcesButtonText.color = Qt.rgba(0, 0, 0, 0.8)
                }
                // 添加按钮的tooltip
                ToolTip {
                    id: webSearchSourcesButtonTooltip
                    visible: false  // 默认不显示
                    delay: 500
                    timeout: 3000
                    text: qsTr("sources of information")
                }
            }
        }
    }

    // 信息来源展开区域
    Column {
        id: webSearchSourcesExpandedArea
        Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourcesExpandedArea"
        visible: webSearchSourcesButton.isExpanded
        width: parent.width - parent.leftPadding - parent.rightPadding
        leftPadding: 0
        rightPadding: 0
        topPadding: 4     // 与按钮的间距
        bottomPadding: 8

        // 白色背景框
        Rectangle {
            width: parent.width
            height: Math.min(webSearchSourcesExpandedListView.contentHeight, 28 * 5)  // 最多显示5条，每条28px，超过5条时固定高度
            color: "#FFFFFF"
            radius: 6
            border.width: 0  // 去掉边框
            anchors.horizontalCenter: parent.horizontalCenter  // 居中对齐

            // 信息来源列表
            Flickable {
                id: webSearchSourcesExpandedScrollView
                Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourcesExpandedScrollView"
                anchors.fill: parent
                anchors.margins: 0  // 去掉margin
                clip: true
                contentWidth: webSearchSourcesExpandedListView.width
                contentHeight: webSearchSourcesExpandedListView.contentHeight
                boundsBehavior: Flickable.StopAtBounds

                // 自定义垂直滚动条
                ScrollBar.vertical: ScrollBar {
                    policy: webSearchSourcesExpandedScrollView.contentHeight > webSearchSourcesExpandedScrollView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    visible: webSearchSourcesExpandedScrollView.contentHeight > webSearchSourcesExpandedScrollView.height
                    width: 10
                }

                ListView {
                    id: webSearchSourcesExpandedListView
                    Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourcesExpandedListView"
                    height: contentHeight
                    width: webSearchSourcesExpandedScrollView.width - (webSearchSourcesExpandedScrollView.ScrollBar.vertical.visible ? 16 : 0)
                    clip: true
                    interactive: false
                    boundsMovement: Flickable.StopAtBounds
                    spacing: 0  // 去掉item间距
                    model: ListModel{
                        id: webSearchSourcesExpandedListModel
                    }

                    Label {
                        visible: false
                        text: webSearchSourcesArea.webSearchSources
                        onTextChanged: {
                            console.log("webSearchSourcesArea.webSearchSources===========", webSearchSourcesArea.webSearchSources)
                            if(webSearchSourcesArea.webSearchSources === "" || webSearchSourcesArea.webSearchSources === undefined){
                                webSearchSourcesExpandedListModel.clear()
                                return
                            }
                            var webSearchSources = JSON.parse(webSearchSourcesArea.webSearchSources)
                            if(webSearchSources.webSearchSources !== undefined){
                                webSearchSourcesExpandedListModel.clear()
                                for (var i = 0; i < webSearchSources.webSearchSources.length; i++) {
                                    var item = webSearchSources.webSearchSources[i]
                                    // 去除标题开头的Markdown引用符>，后端可能返回">标题"格式
                                    var rawTitle = item.title || ""
                                    var cleanTitle = rawTitle.replace(/^\s*>\s*/, "")
                                    // 删除后端返回的index字段，避免覆盖ListView内置的index属性
                                    var cleanItem = {
                                        "title": cleanTitle,
                                        "url": item.url || ""
                                    }
                                    webSearchSourcesExpandedListModel.insert(i, cleanItem)
                                }
                                console.log("webSearchSourcesExpandedListModel.count===========", webSearchSourcesExpandedListModel.count)
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: webSearchSourceExpandedItem
                        Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourceExpandedItem"
                        width: webSearchSourcesExpandedListView.width
                        height: 28  // 进一步减少高度到28px
                        color: "transparent"
                        border.width: 0  // 去掉边框

                        Row {
                            anchors.fill: parent
                            leftPadding: 6  // 进一步减少padding
                            rightPadding: 6
                            spacing: 0  // 减少spacing

                            // 序号
                            Rectangle {
                                id: numberRect
                                width: 16  // 进一步减少宽度
                                height: 16
                                color: "transparent"
                                anchors.verticalCenter: parent.verticalCenter

                                Label {
                                    anchors.centerIn: parent
                                    id: numberLabel
                                    text: (index + 1) + "."  // 序号从1开始
                                    color: Qt.rgba(0, 0, 0, 0.6)
                                    font.pointSize: globals.fontsize - 3  // 进一步减小字体
                                }
                            }

                            // 文档信息
                            Column {
                                id: docExpandedInfoColumn
                                width: parent.width - numberRect.width - parent.leftPadding - parent.rightPadding - parent.spacing
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 0  // 去掉spacing

                                Label {
                                    id: docExpandedTitleLabel
                                    width: parent.width
                                    text: {
                                        var title = model.title !== undefined ? model.title : (model.url !== undefined ? model.url : "")
                                        return (title || "")
                                    }
                                    color: Qt.rgba(0, 0, 0, 0.6)  // 常态灰色
                                    elide: Text.ElideRight
                                    font.pointSize: globals.fontsize - 2  // 进一步减小字体
                                    wrapMode: Text.NoWrap
                                }
                            }
                        }

                        MouseArea {
                            id: webSearchSourceExpandedItemMouseArea
                            Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourceExpandedItemMouseArea"
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            // 定义三态颜色常量
                            property color defaultColor: Qt.rgba(0, 0, 0, 0.6)  // 默认态：灰字
                            property color hoverColor: "#3790FA"                // 悬浮态：蓝字
                            property color pressedColor: "#2B7CE6"              // 点击态：深蓝字

                            onEntered: {
                                // 序号和文字都变蓝色
                                numberLabel.color = hoverColor
                                docExpandedTitleLabel.color = hoverColor
                                // 手动显示tooltip
                                webSearchSourceExpandedItemTooltip.visible = true
                            }
                            onExited: {
                                // 恢复默认颜色
                                numberLabel.color = defaultColor
                                docExpandedTitleLabel.color = defaultColor
                                // 隐藏tooltip
                                webSearchSourceExpandedItemTooltip.visible = false
                            }
                            onPressed: {
                                // 点击时变深蓝色
                                numberLabel.color = pressedColor
                                docExpandedTitleLabel.color = pressedColor
                                // 隐藏tooltip
                                webSearchSourceExpandedItemTooltip.visible = false
                            }
                            onReleased: {
                                // 释放时恢复悬浮色
                                numberLabel.color = hoverColor
                                docExpandedTitleLabel.color = hoverColor
                            }
                            onClicked: {
                                // 打开链接
                                var url = model.url !== undefined ? model.url : ""
                                if (url !== "") {
                                    Qt.openUrlExternally(url)
                                }
                                // 隐藏tooltip
                                webSearchSourceExpandedItemTooltip.visible = false
                            }
                        }

                        // 悬浮提示框 - 使用系统主题背景 + 富文本内容
                        ToolTip {
                            id: webSearchSourceExpandedItemTooltip
                            Accessible.name: "kylin-aiassistant_WebSearchSourcesArea_webSearchSourceExpandedItemTooltip"
                            visible: false  // 手动控制tooltip显示
                            delay: Qt.styleHints.mousePressAndHoldInterval
                            timeout: 5000
                            text: formatText(myText)
                            property var textFormat: Text.RichText
                            property var wrapMode: Text.WrapAnywhere
                            property var maxWidth: 320

                            // 原始文本：标题 + 换行 + URL
                            property string myText: {
                                var title = model.title !== undefined ? model.title : (model.url || "")
                                var url = model.url !== undefined ? model.url : ""

                                // 标题限制约2行（约80字符）
                                if (title.length > 70) {
                                    title = title.substring(0, 67) + "..."
                                }
                                // URL限制约3行（约120字符）
                                if (url.length > 105) {
                                    url = url.substring(0, 102) + "..."
                                }

                                // 用\n连接标题和URL
                                if (url !== "") {
                                    return title + "\n" + url
                                }
                                return title
                            }

                            // 格式化文本：第一行加粗，\n替换为<br>
                            function formatText(text) {
                                var lines = text.split('\n')
                                if (lines.length > 0) {
                                    // 第一行（标题）加粗
                                    lines[0] = "<b>" + lines[0] + "</b>"
                                }
                                // 用<br>连接各行
                                return lines.join('<br>')
                            }
                        }
                    }
                }
            }
        }
    }
}
