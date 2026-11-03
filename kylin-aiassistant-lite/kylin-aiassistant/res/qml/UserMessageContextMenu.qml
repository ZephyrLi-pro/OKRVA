import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12

import "."

/**
 * 用户消息右键菜单组件
 * 包含复制和朗读功能，可在普通对话页面、语音通话页面使用
 */
Popup {
    id: userMessageContextMenu
    width: menuColumn.width
    height: menuColumn.height
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // 属性定义
    property bool sentByUser: false  // 是否是用户发送的消息
    property bool showRead: true     // 是否显示朗读按钮（false表示显示停止朗读按钮）
    property string selectedText: "" // 选中的文本
    property string fullText: ""     // 完整的文本内容
    property int messageIndex: -1    // 消息索引,当前按钮所在的索引

    // 是否为图片右键菜单
    property bool isImage: false
    // 图片地址（仅图片菜单使用）
    property string imageSrc: ""

    // 存储当前显示状态（仅在菜单打开时从 showRead 同步，避免点击后菜单未关时切换显示）
    property bool currentShowRead: true


    // 按钮内部记录当前的朗读索引
    property int chatReadingIndex: -1


    property bool isStarting: false  // 是否正在启动朗读


    // 仅在显隐变化时更新currentShowRead
    onVisibleChanged: {
        currentShowRead = showRead
    }

    // 信号定义
    signal copyText(string text)         // 复制文本信号
    signal copySuccess()                 // 复制成功信号
    signal pauseRead()                   // 暂停朗读信号

    signal startReadText(string text)   //开始朗读
    signal stopReadText()               //停止朗读
    signal errorOccurred(string message,bool is_end,int  index, int errorCode, string model) // 错误信号
    signal updateReadState(int index, bool showRead)  // 更新朗读状态信号

    // 图片相关信号（图片菜单使用）
    signal downloadImage(string imageSrc)   // 下载图片
    signal copyImage(string imageSrc)       // 复制图片
    signal previewImage(string imageSrc)    // 预览图片


    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        Column {
            id: menuColumn
            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_menuColumn"
            anchors.centerIn: parent
            width: 180
            height: isImage
                    ? (downloadImageButton.height + copyImageButton.height + previewImageButton.height)
                    : (copyButton.height
                       + (readButton.visible ? readButton.height : 0)
                       + (pauseReadButton.visible ? pauseReadButton.height : 0))
            padding: 0

            // 文本 - 复制按钮
            Rectangle {
                id: copyButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButton"
                width: menuColumn.width
                height: (readButton.visible || pauseReadButton.visible) ? 44 : 52
                radius: 8
                color: "transparent"
                visible: !isImage

                Row {
                    id: copyButtonRow
                    Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButtonRow"
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 8
                    bottomPadding: (readButton.visible || pauseReadButton.visible) ? 0 : 8

                    Rectangle {
                        id: copyButtonRet
                        Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButtonRet"
                        width: copyButton.width - copyButtonRow.leftPadding - copyButtonRow.rightPadding
                        height: copyButton.height - copyButtonRow.topPadding - copyButtonRow.bottomPadding
                        radius: 6
                        color: "transparent"

                        Row {
                            id: copyButtonTextRow
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButtonTextRow"
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: copyButtonText
                                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButtonText"
                                text: qsTr("Copy")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: copyButtonMouseArea
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyButtonMouseArea"
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true

                            onEntered: {
                                copyButtonText.color = "#FFFFFF"
                                copyButtonRet.color = "#3489ED"
                            }
                            onExited: {
                                copyButtonText.color = "#000000"
                                copyButtonRet.color = "transparent"
                            }
                            onPressed: {
                                copyButtonText.color = "#FFFFFF"
                                copyButtonRet.color = "#2C73C8"
                            }
                            onReleased: {
                                copyButtonText.color = "#000000"
                                copyButtonRet.color = "transparent"
                            }
                            onClicked: {
                                var textToCopy = selectedText.length > 0 ? selectedText : fullText
                                copyText(textToCopy)
                                copySuccess()
                                userMessageContextMenu.close()
                            }
                        }
                    }
                }
            }

            // 文本 - 朗读按钮（仅用户发送的消息显示）
            Rectangle {
                id: readButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButton"
                width: menuColumn.width
                height: 46
                radius: 8
                color: "transparent"
                visible: !isImage && sentByUser && currentShowRead

                Row {
                    id: readButtonRow
                    Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButtonRow"
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 2
                    bottomPadding: 8

                    Rectangle {
                        id: readButtonRet
                        Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButtonRet"
                        width: readButton.width - readButtonRow.leftPadding - readButtonRow.rightPadding
                        height: readButton.height - readButtonRow.topPadding - readButtonRow.bottomPadding
                        radius: 6
                        color: "transparent"

                        Row {
                            id: readButtonTextRow
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButtonTextRow"
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: readButtonText
                                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButtonText"
                                text: qsTr("Read")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: readButtonMouseArea
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_readButtonMouseArea"
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true

                            onEntered: {
                                readButtonText.color = "#FFFFFF"
                                readButtonRet.color = "#3489ED"
                            }
                            onExited: {
                                readButtonText.color = "#000000"
                                readButtonRet.color = "transparent"
                            }
                            onPressed: {
                                readButtonText.color = "#FFFFFF"
                                readButtonRet.color = "#2C73C8"
                            }
                            onReleased: {
                                readButtonText.color = "#000000"
                                readButtonRet.color = "transparent"
                            }
                            onClicked: {
                                // 先隐藏菜单，再执行朗读逻辑（避免菜单上先变成“停止朗读”再消失）
                                userMessageContextMenu.close()

                                if (messageIndex >= 0) {
                                    console.log("右键菜单启动朗读，将朗读状态更新为false:",messageIndex)
                                    updateReadState(messageIndex, false)
                                }

                                // 获取要朗读的文本
                                var textToRead = selectedText.length > 0 ? selectedText : fullText
                                // 去除HTML标签
                                textToRead = textToRead.replace(/<\/?.+?\/?>/g, '')
                                // 替换HTML实体
                                textToRead = textToRead.replace(/&quot;/g, "\"")
                                userMessageContextMenu.startReadText(textToRead)
                            }
                        }
                    }
                }
            }

            // 文本 - 停止朗读按钮（仅用户发送的消息且正在朗读时显示）
            Rectangle {
                id: pauseReadButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButton"
                width: menuColumn.width
                height: 46
                radius: 8
                color: "transparent"
                visible: !isImage && sentByUser && !currentShowRead

                Row {
                    id: pauseReadButtonRow
                    Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButtonRow"
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 2
                    bottomPadding: 8

                    Rectangle {
                        id: pauseReadButtonRet
                        Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButtonRet"
                        width: pauseReadButton.width - pauseReadButtonRow.leftPadding - pauseReadButtonRow.rightPadding
                        height: pauseReadButton.height - pauseReadButtonRow.topPadding - pauseReadButtonRow.bottomPadding
                        radius: 6
                        color: "transparent"

                        Row {
                            id: pauseReadButtonTextRow
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButtonTextRow"
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: pauseReadButtonText
                                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButtonText"
                                text: qsTr("Stop Read")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: pauseReadButtonMouseArea
                            Accessible.name: "kylin-aiassistant_UserMessageContextMenu_pauseReadButtonMouseArea"
                            anchors.fill: parent
                            propagateComposedEvents: true
                            hoverEnabled: true

                            onEntered: {
                                pauseReadButtonText.color = "#FFFFFF"
                                pauseReadButtonRet.color = "#3489ED"
                            }
                            onExited: {
                                pauseReadButtonText.color = "#000000"
                                pauseReadButtonRet.color = "transparent"
                            }
                            onPressed: {
                                pauseReadButtonText.color = "#FFFFFF"
                                pauseReadButtonRet.color = "#2C73C8"
                            }
                            onReleased: {
                                pauseReadButtonText.color = "#000000"
                                pauseReadButtonRet.color = "transparent"
                            }
                            onClicked: {
                                // 先关闭菜单，由onVisibleChanged更新，避免关闭前选项已切换
                                userMessageContextMenu.close()
                                if (messageIndex >= 0) {
                                    userMessageContextMenu.updateReadState(messageIndex, true)
                                }
                                userMessageContextMenu.stopReadText()
                            }


                        }

                    }
                }
            }

            // 朗读信号槽 -主要响应朗读错误信息和朗读结束
            Connections {
                target: readerControl

                // 监听朗读开始信号
                onSigPlayingStart: function() {
                    // 空闲状态不处理此信号
                    if(currentShowRead===true && userMessageContextMenu.isStarting === false) {
                        return
                    }
                    console.log("开始朗读，清除isstarting标记",userMessageContextMenu.isStarting)
                    userMessageContextMenu.isStarting = false  // 清除启动标志
                    if (!userMessageContextMenu.visible) {
                        currentShowRead = false
                    }
                }
                // 监听朗读结束信号
                onSigReadEnd: function(index) {
                    // 空闲状态不响应朗读结束信号
                    if(currentShowRead === true) {
                        return
                    }
                    // 非空闲状态的按钮，检查是否是本次启动的索引
                    else {
                        // 正在启动中进行判断处理
                        if(userMessageContextMenu.isStarting) {
                            if(messageIndex === userMessageContextMenu.chatReadingIndex) {
                                // 如果是本次触发的索引，则不进行重置
                                console.log("正在右键菜单的启动中，忽略本次自身发出的的结束信号")
                                return;
                            } else {
                                // 如果不是本次触发的索引，则证明是在进行其他的朗读，将当前界面进行重置
                                userMessageContextMenu.updateReadState(messageIndex, true)
                                if (!userMessageContextMenu.visible) {
                                    currentShowRead = true
                                }
                            }

                        }
                        // 如果不是本次触发的索引，则证明是在进行其他的朗读，将当前界面进行重置
                        userMessageContextMenu.updateReadState(messageIndex, true)
                        if (!userMessageContextMenu.visible) {
                            currentShowRead = true
                        }
                    }
                    console.log("当前右键菜单朗读结束,待更新索引为：",messageIndex)
                }
                // 监听回复信号，朗读主要返回错误信号
                onReply: function(message, is_end, index, errorCode, model){
                    console.log("errorReply::::::::::",index,errorCode,currentShowRead)

                    userMessageContextMenu.errorOccurred(message, is_end, index, errorCode, model)
                    if (!userMessageContextMenu.visible) {
                        currentShowRead = true
                    }
                    // 更新 model 状态，下次打开菜单时正确显示朗读
                    if (messageIndex >= 0) {
                        userMessageContextMenu.updateReadState(messageIndex, true)
                    }
                }
            }



            // 图片 - 下载按钮
            Rectangle {
                id: downloadImageButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_downloadImageButton"
                width: menuColumn.width
                height: downloadImageRet.height + downloadImageButtonRow.topPadding + downloadImageButtonRow.bottomPadding
                radius: 8
                color: "transparent"
                visible: isImage

                Row {
                    id: downloadImageButtonRow
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 8
                    bottomPadding: 0

                    Rectangle {
                        id: downloadImageRet
                        width: downloadImageButton.width - downloadImageButtonRow.leftPadding - downloadImageButtonRow.rightPadding
                        height: 36
                        radius: 6
                        color: "transparent"

                        Row {
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: downloadImageText
                                text: qsTr("Download")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true

                            onEntered: {
                                downloadImageText.color = "#FFFFFF"
                                downloadImageRet.color = "#3489ED"
                            }
                            onExited: {
                                downloadImageText.color = "#000000"
                                downloadImageRet.color = "transparent"
                            }
                            onPressed: {
                                downloadImageText.color = "#FFFFFF"
                                downloadImageRet.color = "#2C73C8"
                            }
                            onReleased: {
                                downloadImageText.color = "#000000"
                                downloadImageRet.color = "transparent"
                            }
                            onClicked: {
                                downloadImage(imageSrc)
                                userMessageContextMenu.close()
                            }
                        }
                    }
                }
            }

            // 图片 - 复制按钮
            Rectangle {
                id: copyImageButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_copyImageButton"
                width: menuColumn.width
                height: copyImageRet.height + copyImageButtonRow.topPadding + copyImageButtonRow.bottomPadding
                radius: 8
                color: "transparent"
                visible: isImage

                Row {
                    id: copyImageButtonRow
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 2
                    bottomPadding: 0

                    Rectangle {
                        id: copyImageRet
                        width: copyImageButton.width - copyImageButtonRow.leftPadding - copyImageButtonRow.rightPadding
                        height: 36
                        radius: 6
                        color: "transparent"

                        Row {
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: copyImageText
                                text: qsTr("Copy")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true

                            onEntered: {
                                copyImageText.color = "#FFFFFF"
                                copyImageRet.color = "#3489ED"
                            }
                            onExited: {
                                copyImageText.color = "#000000"
                                copyImageRet.color = "transparent"
                            }
                            onPressed: {
                                copyImageText.color = "#FFFFFF"
                                copyImageRet.color = "#2C73C8"
                            }
                            onReleased: {
                                copyImageText.color = "#000000"
                                copyImageRet.color = "transparent"
                            }
                            onClicked: {
                                copyImage(imageSrc)
                                userMessageContextMenu.close()
                                copySuccess()
                            }
                        }
                    }
                }
            }

            // 图片 - 预览按钮
            Rectangle {
                id: previewImageButton
                Accessible.name: "kylin-aiassistant_UserMessageContextMenu_previewImageButton"
                width: menuColumn.width
                height: previewImageRet.height + previewImageButtonRow.topPadding + previewImageButtonRow.bottomPadding
                radius: 8
                color: "transparent"
                visible: isImage

                Row {
                    id: previewImageButtonRow
                    anchors.fill: parent
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 2
                    bottomPadding: 8

                    Rectangle {
                        id: previewImageRet
                        width: previewImageButton.width - copyImageButtonRow.leftPadding - copyImageButtonRow.rightPadding
                        height: 36
                        radius: 6
                        color: "transparent"

                        Row {
                            anchors.fill: parent
                            leftPadding: 8
                            rightPadding: 8

                            Label {
                                id: previewImageText
                                text: qsTr("Preview")
                                color: "black"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true

                            onEntered: {
                                previewImageText.color = "#FFFFFF"
                                previewImageRet.color = "#3489ED"
                            }
                            onExited: {
                                previewImageText.color = "#000000"
                                previewImageRet.color = "transparent"
                            }
                            onPressed: {
                                previewImageText.color = "#FFFFFF"
                                previewImageRet.color = "#2C73C8"
                            }
                            onReleased: {
                                previewImageText.color = "#000000"
                                previewImageRet.color = "transparent"
                            }
                            onClicked: {
                                previewImage(imageSrc)
                                userMessageContextMenu.close()
                            }
                        }
                    }
                }
            }
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: 8
        border.color: Qt.rgba(0/255, 0/255, 0/255, 0.1)
        border.width: 1
    }
}
