import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import com.kylin.SelectControl 1.0
import "."

Window {
    id: selectPopupIconWindow
    width:30
    height: 36
    color: "transparent"
    // 迷你形态不接受焦点，只有展开成悬浮条后才接受焦点（由 isWindowExpanded 控制）
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint | (acceptsInputFocus ? 0 : Qt.WindowDoesNotAcceptFocus)
    visible: false
    objectName: "SelectScribeWindow"
    opacity: 1.0

    // 兜底隐藏逻辑：点击其它空白区域通常会导致窗口变为非激活态；
    // 不能依赖 hoverArea.onExited（鼠标未经过按钮区域时不会触发）。
    onActiveChanged: {
        if (!active) {
            // 外部点击：关闭菜单 + 隐藏悬浮按钮（朗读中则保持显示，沿用既有约束）
            if (selectMenuWindow.visible) {
                selectMenuWindow.visible = false
            }
            isInside = false
            // 非空闲状态（合成/朗读/暂停）时窗口失焦，中断并允许隐藏
            if (globalReadState !== 0) {
                sigForceStopSynthesizing()
            }
            // 仅在窗口仍然可见时启动隐藏动画（避免与 sigCloseReadingAndSelectWindow 重复处理）
            if (globalReadState === 0 && selectPopupIconWindow.visible) {
                startHideAnimation()
            }
        } else {
            // 重新激活时停止隐藏计时/动画，避免闪烁
            resetOpacity()
        }
    }

    // 每次窗口被显示时播放 200ms 出现动画
    onVisibleChanged: {
        if (visible) {
            startShowAnimation()
            // 迷你形态不抢焦点；只有用户 hover 展开成悬浮条后，在 hoverArea/rightClickArea 里再 enableWindowFocus + forceActiveFocus
            hasMiniBeenHovered = false
            if (!isWindowExpanded && !isShortcutInvokeConfig && !shortcutInvokePinned) {
                miniAutoHideTimer.stop()
                miniAutoHideTimer.start()
            }
        } else {
            miniAutoHideTimer.stop()
        }
    }

    property int activeCommandID: -1
    property string  activeCommand: ""
    property bool isSelectedEmpty: false
    property real opacityValue: 1.0  // 自定义透明度属性
    property bool isWindowExpanded: false
    property bool acceptsInputFocus: false
    // 快捷键唤起模式下保持展开，鼠标移出不收起到仅图标
    property bool shortcutInvokePinned: false
    readonly property bool isShortcutInvokeConfig: typeof globals !== "undefined"
                                                   && globals
                                                   && !globals.getScribeAutoInvoke()
    property bool isReadingError : false
    property int globalReadState: 0
    property bool isGuideCompleted: configManager.isGuideCompleted
    readonly property int barHorizontalPadding: 4
    readonly property int floatingBarHeight: 36

    // 记录鼠标此时是否在划词条里
    property bool isInside: false
    // 记录最后一次点击时的鼠标全局位置
    property point lastClickGlobalPos: Qt.point(0, 0)
    // 记录本次迷你按钮展示期间是否已被 hover（用于 1s 自动隐藏分支）
    property bool hasMiniBeenHovered: false
    // 新增：标记是否需要等待停止完成后发送
    property bool pendingSendAfterStop: false
    // 菜单关闭后，需鼠标重新进入按钮才显示 tooltip
    property bool suppressMenuTooltip: false

    signal sigSendButtonText(string currentCommand, int currentCommandID)
    signal sigResizeRect(int width, int height)
    signal sigsettingButtonLeftClicked()
    signal sigShowToolTip(bool isShow)
    signal sigHideWindowFinished()
    signal sigGetFocus()
    signal sigForceStopSynthesizing()

    onIsInsideChanged: {
        console.log("*****onIsInsideChanged********" + isInside)
    }

    Rectangle {
        id:selectionRectangle
        Accessible.name: "kylin-aiassistant_Select_selectionRectangle"
        color: "white"
        enabled: true
        anchors.top: parent.top
        anchors.left: parent.left
        width: parent.width
        height: floatingBarHeight
        radius: 8
        focus: isWindowExpanded  // 仅展开成悬浮条时可获焦，迷你形态不抢焦点
        opacity: opacityValue
        border.width: 1
        border.color: Qt.rgba(0/255, 0/255, 0/255,0.1)

        // 右键菜单
        SelectMenuWindow {
            id: selectMenuWindow
            visible: false
            x: selectPopupIconWindow.x
            y: selectPopupIconWindow.y + 45
        }


        // 渐变显示动画：出现 200ms
        PropertyAnimation {
            id: showAnimation
            target: selectPopupIconWindow
            property: "opacityValue"
            from: 0.0
            to: 1.0
            duration: 200
            easing.type: Easing.OutQuad
        }

        // 渐变隐藏动画：消失 200ms
        PropertyAnimation {
            id: hideAnimation
            target: selectPopupIconWindow
            property: "opacityValue"
            from: 1.0
            to: 0.0
            duration: 200
            easing.type: Easing.InQuad
            onRunningChanged: {
                if (!running && opacityValue === 0.0) {
                    resetWindowToIconState();
                    selectPopupIconWindow.visible = false;
                    sigHideWindowFinished();
                }
            }
        }

        // 鼠标事件
        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            // 允许事件继续传递给内部控件
            propagateComposedEvents: true

            onEntered: {
                console.log("***onEntered***")
                isInside = true
                hasMiniBeenHovered = true
                miniAutoHideTimer.stop()
                resetOpacity()
                // Hover may expand the bar, but must never take typing focus.
            }
            onExited: {
                console.log("***onExited***")
                isInside = false
                if (shortcutInvokePinned) {
                    return
                }
                if(selectMenuWindow.visible === false) {
                    console.log("11111111111111开始隐藏")
                    startHideTimer.start(2000)  // 持续显示 2s 后再开始消失动画
                }
            }

            // 主要布局
            RowLayout{
                id: mainLayout
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0
                opacity: opacityValue
                onImplicitWidthChanged: {
                    if (isWindowExpanded) {
                        // 统一使用展开宽度，与 expandAllCommands 和 onUpdateRectSize 保持一致
                        var expandedWidth = expandedWindowWidth()
                        selectionRectangle.width = expandedWidth;
                        selectPopupIconWindow.width = expandedWidth;
                    }
                }

                // 菜单图标
                Rectangle {
                    id: menuRect
                    width: 30;
                    height: 28;
                    color: Qt.rgba(255/255, 255/255, 255/255)
                    radius: 4
                    opacity: opacityValue
                    Accessible.name: "kylin-aiassistant_Select_menuRect"
                    property bool showMenuToolTip: false
                    onShowMenuToolTipChanged: updatePopupHeight()

                    Timer {
                        id: menuToolTipDelayTimer
                        interval: Qt.styleHints.mousePressAndHoldInterval
                        repeat: false
                        onTriggered: {
                            menuRect.showMenuToolTip = rightClickArea.containsMouse
                                    && !selectMenuWindow.visible
                                    && !suppressMenuTooltip
                        }
                    }

                    ToolTip {
                        id: menuToolTip
                        visible: selectPopupIconWindow.visible
                                 && menuRect.showMenuToolTip
                                 && rightClickArea.containsMouse
                                 && !selectMenuWindow.visible
                                 && !suppressMenuTooltip
                        delay: 0
                        text: qsTr("Qi Ling menu")
                        x: Math.max(-mainLayout.x - menuRect.x + 1,
                                    Math.min((menuRect.width - implicitWidth) / 2,
                                             selectPopupIconWindow.width - mainLayout.x - menuRect.x - implicitWidth - 1))
                        y: menuRect.height + 8
                    }
                    focus: true
                    Row {
                        anchors.centerIn: parent
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Image {
                            id: m_textSendButton
                            Accessible.name: "kylin-aiassistant_Select_textSendButton"
                            width: 24
                            height: 24
                            source: "qrc:/icon/icon/kylin-ai-assistant-new-s.png"
                            fillMode: Image.PreserveAspectFit
                            visible: true
                        }
                    }
                    MouseArea {
                        id:rightClickArea
                        objectName: "scribeIconMouseArea"
                        Accessible.name: "kylin-aiassistant_Select_rightClickArea"
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.RightButton |Qt.LeftButton
                        property bool isEntered: false
                        propagateComposedEvents: true
                        onEntered: {
                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0.15);
                            isEntered = true
                            hasMiniBeenHovered = true
                            miniAutoHideTimer.stop()
                            resetOpacity()
                            menuToolTipDelayTimer.restart()
                            expandAllCommands()
                        }
                        onExited: {
                            parent.color = Qt.rgba(255/255, 255/255, 255/255);
                            isEntered = false
                            menuToolTipDelayTimer.stop()
                            menuRect.showMenuToolTip = false
                            suppressMenuTooltip = false
                        }
                        onPressed: {
                            requestExplicitFocus()
                            menuToolTipDelayTimer.stop()
                            menuRect.showMenuToolTip = false
                            if(mouse.button == Qt.RightButton || mouse.button == Qt.LeftButton) {
                                if(selectMenuWindow.visible === true) {
                                    selectMenuWindow.visible = false
                                }
                                else {
                                    selectMenuWindow.visible = true
                                    suppressMenuTooltip = true
                                    resetOpacity()

                                    // 记录菜单显示时的鼠标全局位置
                                    let cursorPos = selectControl.getGlobalCursorPosition()
                                    lastClickGlobalPos = Qt.point(cursorPos.x, cursorPos.y)
                                    console.log("菜单显示，记录鼠标位置:", selectPopupIconWindow.lastClickGlobalPos)
                                }
                            }
                        }
                    }
                }

                // 指令列表
                Row {
                    id:selectCommandFlow
                    Accessible.name: "kylin-aiassistant_Select_selectCommandFlow"
                    height: 28
                    width:selectRepeater.width
                    Layout.rightMargin: 0
                    visible: false
                    Repeater {
                        id:selectRepeater
                        Accessible.name: "kylin-aiassistant_Select_selectRepeater"
                        visible: true
                        model: selectCommandListModel;

                        delegate: Rectangle {
                            width:selectRect.visible ?  selectRect.width+2 : selectReadButton.width+2
                            height: 28
                            id:selectDelegateRectangle
                            visible: true
                            enabled: isGuideCompleted && !IntroductionViewState.isIntroductionVisible && !isSelectedEmpty && isActive
                            Accessible.name: "kylin-aiassistant_Select_Rectangle"
                            color: "white"
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                id:selectButtonLayout
                                Accessible.name: "kylin-aiassistant_Select_selectButtonLayout"
                                Layout.leftMargin: 4
                                Layout.rightMargin: 4
                                Rectangle{
                                    id:selectRect
                                    Accessible.name: "kylin-aiassistant_Select_selectRect"
                                    width: textLayout.implicitWidth
                                    visible:index !== 3
                                    enabled: selectDelegateRectangle.enabled
                                    height:28;
                                    color: "white"
                                    radius: 4
                                    Row{
                                        id: textLayout
                                        leftPadding: 4
                                        rightPadding: 4
                                        anchors.centerIn: parent;
                                        Accessible.name: "kylin-aiassistant_Select_textLayout"
                                        Item{
                                            id: selectButtonIconItem
                                            width: 28
                                            height: 28
                                            opacity: (isSelectedEmpty || !selectDelegateRectangle.enabled) ? 0.3 : 1.0
                                            anchors.verticalCenter: parent.verticalCenter
                                            Image {
                                                id: selectButtonIcon
                                                Accessible.name: "kylin-aiassistant_Select_commandImage"
                                                source: iconPath
                                                anchors.centerIn: parent
                                                opacity: (isActive && selectDelegateRectangle.enabled) ? 1 : 0.35
                                            }
                                        }
                                        Label {
                                            id: selectButtonText;
                                            Accessible.name: "kylin-aiassistant_Select_selectButtonText"
                                            text: name
                                            color:{
                                                if(!selectDelegateRectangle.enabled) {
                                                    return Qt.rgba(0/255,0/255,0/255,0.35)
                                                }
                                                if(isSelectedEmpty) {
                                                    return Qt.rgba(0/255,0/255,0/255,0.35)
                                                }
                                                if(!isActive) {
                                                    return Qt.rgba(0/255,0/255,0/255,0.35)
                                                }
                                                return Qt.rgba(0/255, 0/255, 0/255,0.85)
                                            }
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                    MouseArea {
                                        id: selectMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        property bool isEntered: false
                                        propagateComposedEvents: true
                                        onEntered: {
                                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0.15)
                                            isEntered = true
                                            resetOpacity()
                                        }
                                        onExited: {
                                            parent.color = "white"
                                            isEntered = false
                                        }
                                        onPressed: {
                                            parent.color = Qt.rgba(0/255, 0/255, 0/255,0.2)
                                        }
                                        onClicked: {
                                            console.log("[DEBUG-SELECT] 按钮点击 actorId:", actorId, "name:", name)
                                            activeCommandID = actorId
                                            startSendQuestion()
                                        }
                                    }
                                }
                                // 朗读控件
                                SelectReadButton{
                                    id:selectReadButton
                                    iconPath: iconPath
                                    visible:index === 3
                                    enabled: selectDelegateRectangle.enabled
                                    currentState: 0
                                    selectText: name
                                    onErrorOccurred: function(message, is_end, index, errorCode, model) {
                                        console.log("SelectReadButton error",message, is_end, index, errorCode, model)
                                        selectReadButton.setState(selectReadButton.stateIdle)
                                        selectReadButton.isStarting = false
                                        // 窗口已关闭时不再转发错误到主界面，避免在服务连接断开后操作已关闭的上下文
                                        if (!selectPopupIconWindow.visible) {
                                            return
                                        }
                                        msgPane.enterFromWordSelection()
                                        windowCtrl.showMainWindow()
                                        msgPane.showWindowPage(1)
                                        msgPane.onAudioRecvMsg(message, is_end, index, errorCode, model)
                                    }
                                    onInterrupted: {
                                        console.log("selectReadButton interrupted, index:")
                                    }
                                    onSigStartRead: {
                                        let msg = selectControl.getCurSelectMsg()
                                        console.log("当前待朗读剪切板内容:",msg)
                                        selectReadButton.isStarting = true
                                        readerControl.interruptAllVoiceUsing();
                                        selectReadButton.setState(selectReadButton.stateSynthesizing)
                                        let startFlag = readerControl.startReadingCurrentText(msg)
                                        if (!startFlag) {
                                            selectReadButton.isStarting = false
                                            selectReadButton.setState(selectReadButton.stateIdle)
                                        }
                                    }
                                    onSigPauseRead: {
                                        readerControl.pauseReading()
                                        selectReadButton.setState(selectReadButton.statePaused)
                                    }
                                    onSigContinueRead: {
                                        readerControl.continueReading()
                                        selectReadButton.setState(selectReadButton.stateReading)
                                    }
                                    onSigStopSynthesizing: {
                                        console.log("停止语音合成")
                                        selectReadButton.isStarting = false
                                        readerControl.stopReading()
                                        readerControl.clearWaitingReadingList()
                                        readerControl.clearReadingPlayerData()
                                        selectReadButton.setState(selectReadButton.stateIdle)
                                    }
                                    onSigResetOpacity:  {
                                        resetOpacity()
                                    }
                                    onCurrentStateChanged: {
                                        selectPopupIconWindow.globalReadState = selectReadButton.currentState;
                                    }
                                    Connections {
                                        target: selectPopupIconWindow
                                        enabled: index === 3
                                        onSigForceStopSynthesizing: function() {
                                            if (selectReadButton.currentState !== selectReadButton.stateIdle) {
                                                console.log("窗口失焦，停止语音合成/朗读，当前状态:", selectReadButton.currentState)
                                                selectReadButton.isStarting = false
                                                readerControl.stopReading()
                                                readerControl.clearWaitingReadingList()
                                                readerControl.clearReadingPlayerData()
                                                selectReadButton.setState(selectReadButton.stateIdle)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Timer {
            id: miniAutoHideTimer
            interval: 3000
            repeat: false
            running: false
            onTriggered: {
                // 分支一：迷你按钮出现后 1s 内未 hover，则自动隐藏（快捷键唤起展开态除外）
                if (!isWindowExpanded && !hasMiniBeenHovered && selectMenuWindow.visible === false
                        && !shortcutInvokePinned) {
                    startHideAnimation()
                }
            }
        }

        Timer {
            id: startHideTimer
            repeat: false
            running: false
            onTriggered: {
                startHideAnimation()
            }
        }

        // 处理按键按下事件
        Keys.onPressed: {
            console.log("**************键盘事件**************")
            if (event.key === Qt.Key_Escape) {
                console.log("Escape键被按下")

                // 如果菜单当前可见，则优先关闭菜单
                if (selectMenuWindow.visible) {
                    selectMenuWindow.closingByEsc = true
                    selectMenuWindow.close()
                    event.accepted = true
                    return
                }

                // 否则关闭整个悬浮条
                if(selectPopupIconWindow.globalReadState !== 0) {
                    readerControl.stopReading()
                    readerControl.clearWaitingReadingList()
                    readerControl.clearReadingPlayerData()
                }
                resetWindowToIconState();
                selectPopupIconWindow.visible = false;
                event.accepted = true
            } else if (event.key === Qt.Key_Space) {
                console.log("空格键被按下")
            }
        }

        // onShortcutOverride 仅用于拦截 Esc，防止 tooltip 先消费该按键；
        // 实际关闭逻辑统一由 Keys.onPressed 处理，避免同一次按键被执行两次。
        Keys.onShortcutOverride: {
            if (event.key === Qt.Key_Escape) {
                event.accepted = true
            }
        }

    }

    // 监听朗读状态变化：当朗读结束且鼠标不在悬浮条内时，主动启动隐藏定时器
    onGlobalReadStateChanged: {
        if (globalReadState === 0 && !isInside && !shortcutInvokePinned) {
            console.log("当朗读结束且鼠标不在悬浮条内时，开始隐藏")
            startHideTimer.start(2000);  // 持续显示 2s 后再开始消失
        }
    }

    // 提取发送逻辑为独立函数：改为走“指令模板 + commandUuid”新体系
    function doSendQuestion() {
        console.log("[DEBUG-SELECT] doSendQuestion 进入, activeCommandID:", activeCommandID)
        // 关于窗口打开时，不跳转到对话界面，不关闭悬浮条，直接激活关于窗口（触发 WM 抖动效果）
        if (mainWnd.isAboutWindowOpen()) {
            mainWnd.activateAboutWindow()
            return
        }

        // 划词进入助手时切换到快问快答主页
        msgPane.enterFromWordSelection()
        // 如果语音输入正在进行，先停止语音输入
        msgPane.stopVoiceInputIfActive()
        let msg = selectControl.getCurSelectMsg()
        console.log("[DEBUG-SELECT] 剪切板内容:", msg)

        // 将旧的 actorId 映射到指令模板 UUID
        var commandUuid = ""
        var commandName = ""
        if (activeCommandID === 501) {              // 总结
            commandUuid = "550e8400-e29b-41d4-a716-44665544000b"
            commandName = qsTr("Summarize")
        } else if (activeCommandID === 502) {       // 解释
            commandUuid = "550e8400-e29b-41d4-a716-446655440009"
            commandName = qsTr("Explain")
        } else if (activeCommandID === 503) {       // 翻译
            commandUuid = "550e8400-e29b-41d4-a716-44665544000c"
            commandName = qsTr("Translate")
        } else if (activeCommandID === 505) {       // 问问
            commandUuid = "550e8400-e29b-41d4-a716-44665544000a"
            commandName = qsTr("Ask")
        }

        console.log("[DEBUG-SELECT] 映射结果 commandUuid:", commandUuid, "commandName:", commandName)

        if (commandUuid === "") {
            console.log("[DEBUG-SELECT] 未找到匹配的指令 UUID，直接返回")
            return
        }

        // 先通知主界面应用对应指令（在输入框上方指令标签中保持选中状态）
        console.log("[DEBUG-SELECT] >>> 调用 applyCommandFromSelection:", commandUuid)
        msgPane.applyCommandFromSelection(commandUuid)
        console.log("[DEBUG-SELECT] <<< applyCommandFromSelection 返回")

        // 构造消息头，走新指令体系（commandUuid）
        let jsonObject = {
            msgHead: commandName,
            promptId: -1,
            promptIconPath: "",
            imagePath: "",
            imageSrc: "",
            commandUuid: commandUuid
        };
        let jsonMsgHead = JSON.stringify(jsonObject)

        console.log("[DEBUG-SELECT] >>> 调用 onStartReplyingToChat")
        msgPane.onStartReplyingToChat()
        console.log("[DEBUG-SELECT] >>> 调用 onSendMsg")
        msgPane.onSendMsg(jsonMsgHead, msg)
        msgPane.moveScrollBar()

        // 关闭所有活动的弹窗
        console.log("[DEBUG-SELECT] >>> 调用 closeAllPopupWindows")
        selectControl.closeAllPopupWindows()

        console.log("[DEBUG-SELECT] >>> 调用 showMainWindow + showWindowPage(1)")
        windowCtrl.showMainWindow()
        msgPane.showWindowPage(1)
        resetWindowToIconState()
        selectPopupIconWindow.visible = false
    }

    // 修改后的启动发送问题函数：如果正在回答，先停止并等待停止完成
    function startSendQuestion() {
        console.log("[DEBUG-SELECT] startSendQuestion 进入, activeCommandID:", activeCommandID)
        // 检查当前是否正在回答（通过 msgPane 提供的接口）
        if (msgPane.getIsReplying()) {
            // 先停止
            msgPane.onStopReply();
            pendingSendAfterStop = true;  // 设置等待标志
        } else {
            doSendQuestion();
        }
    }

    // 监听 msgPane 的 replyEnd 信号，当停止完成后发送新消息
    Connections {
        target: msgPane
        onReplyEnd: {
            if (pendingSendAfterStop) {
                msgPane.updateStateWithChat();
                pendingSendAfterStop = false;
                // 使用 Qt.callLater 延迟执行，确保 Chat.qml 的 onReplyEnd 先完成
                // 将上一条消息更新为"已停止回答"，然后再发送新问题
                //Qt.callLater(doSendQuestion);
                doSendQuestionTimer.running = true
            }
        }
    }

    Timer {
        id: doSendQuestionTimer
        interval: 1000
        repeat: false   // 设置定时器只执行一次
        running: false   // 设置定时器启动
        onTriggered: {
            doSendQuestion()
        }
    }

    // 快捷键唤起：直接展开悬浮条并保持展开态
    function showExpandedForShortcutInvoke() {
        shortcutInvokePinned = true
        hasMiniBeenHovered = true
        miniAutoHideTimer.stop()
        startHideTimer.stop()
        resetOpacity()
        expandAllCommands()
        Qt.callLater(function() {
            onUpdateRectSize()
            requestExplicitFocus()
        })
    }

    function clearShortcutInvokePinned() {
        shortcutInvokePinned = false
    }

    // 展开全部指令 - 鼠标悬停图标时调用
    function expandAllCommands() {
        if (!isWindowExpanded) {
            selectCommandFlow.visible = true;
            isWindowExpanded = true;
            // 窗口宽度比内容宽度左右各多 4px，确保边缘按钮不贴边。
            var contentWidth = expandedWindowWidth();
            selectionRectangle.width = contentWidth;
            selectPopupIconWindow.width = contentWidth;
            console.log("✅ 弹窗展开：显示全部指令按钮，当前宽度:", contentWidth);
        }
    }

    // 启动隐藏动画
    function startHideAnimation() {
        resetOpacity()
        console.log("开始启动隐藏划词动画:",selectPopupIconWindow.visible,opacityValue)
        if (selectPopupIconWindow.globalReadState !== 0 && selectPopupIconWindow.globalReadState !== 3) {
            console.log("当前朗读正在进行（合成/朗读），无需隐藏")
            return;
        } else {
            console.log("当前朗读为空闲/暂停状态，开始启动隐藏划词")
        }
        hideAnimation.start()
    }

    // 启动显示动画（出现 200ms）
    function startShowAnimation() {
        hideAnimation.stop()
        opacityValue = 0.0
        showAnimation.start()
    }

    // 窗口复位
    function resetWindowToIconState() {
        acceptsInputFocus = false;
        selectCommandFlow.visible = false;
        menuToolTipDelayTimer.stop();
        menuRect.showMenuToolTip = false;
        selectPopupIconWindow.width = 30;
        selectPopupIconWindow.height = floatingBarHeight;
        selectionRectangle.width = 30;
        selectionRectangle.height = floatingBarHeight;
        opacityValue = 1.0;
        isWindowExpanded = false;
        shortcutInvokePinned = false;
        hasMiniBeenHovered = false;
        miniAutoHideTimer.stop();
        startHideTimer.stop();
    }

    // 复位并显示窗口
    function showWindowWithReset() {
        resetWindowToIconState();
        selectPopupIconWindow.visible = true;
        opacityValue = 1.0;
        console.log("✅ 弹窗二次显示 → 初始为纯图标状态");
    }

    function expandedWindowWidth() {
        var contentWidth = Math.max(mainLayout.implicitWidth, mainLayout.width)
        return Math.ceil(contentWidth + barHorizontalPadding * 2)
    }

    function popupWindowHeight() {
        return floatingBarHeight + (menuRect.showMenuToolTip ? 4 + menuToolTip.implicitHeight + 2 : 0)
    }

    function updatePopupHeight() {
        selectionRectangle.height = floatingBarHeight
        selectPopupIconWindow.height = popupWindowHeight()
    }

    // 更新窗口大小
    function requestExplicitFocus() {
        acceptsInputFocus = true
        selectControl.enableWindowFocus()
        selectionRectangle.forceActiveFocus()
    }

    function prepareAutomaticSelection() {
        resetWindowToIconState()
        selectControl.restoreWindowNoFocus()
        resetOpacity()
        onUpdateRectSize()
        if (visible && !isShortcutInvokeConfig) miniAutoHideTimer.restart()
    }

    function onUpdateRectSize() {
        // 未展开时强制使用初始小按钮尺寸，避免布局计算异常导致宽度错误
        if (!isWindowExpanded) {
            selectionRectangle.width = 30;
            selectPopupIconWindow.width = 30;
            updatePopupHeight()
        } else {
            // 展开时使用与 expandAllCommands 一致的宽度计算
            var expandedWidth = expandedWindowWidth()
            selectionRectangle.width = expandedWidth;
            selectPopupIconWindow.width = expandedWidth;
            updatePopupHeight()
        }
    }

    // 重置透明度
    function resetOpacity() {
        opacityValue = 1.0;
        hideAnimation.stop();
        startHideTimer.stop();
    }

    function getTruncateCommandLabel(text, maxChars) {
        let totalUnits = 0;
        let truncatedText = "";

        for (let i = 0; i < text.length; i++) {
            let currentChar = text.charAt(i);
            if (currentChar >= '\u4E00' && currentChar <= '\u9FFF') {
                totalUnits += 2;
            } else {
                totalUnits += 1;
            }
            if (totalUnits > maxChars * 2) {
                truncatedText += "...";
                break;
            }
            truncatedText += currentChar;
        }
        return truncatedText;
    }

    Connections {
        target: selectMenuWindow
        onSigCloseSelectWindow: function() {
            console.log("***********onSigCloseSelectWindow***********isInside" + isInside)
            if (isInside) {
                return;
            }

            // 记录菜单显示时的鼠标全局位置
            let cursorPos = selectControl.getGlobalCursorPosition()
            console.log("当前鼠标位置:", cursorPos)
            if (lastClickGlobalPos === cursorPos) {
                console.log("点击位置不变，认为还在划词条中")
                return;
            }

            console.log("***********onSigCloseSelectWindow**********关闭")
            resetWindowToIconState();
            selectPopupIconWindow.visible = false;
        }
        onSigMenuClosedByEsc: function() {
            if (selectPopupIconWindow.visible && isWindowExpanded) {
                requestExplicitFocus()
            }
        }
    }

    Connections {
        target: selectControl
        onSigCloseReadingAndSelectWindow: function() {
            console.log("onsigCloseReadingAndSelectWindow")
            if(selectPopupIconWindow.globalReadState !== 0) {
                sigForceStopSynthesizing()
            }
            resetWindowToIconState();
            selectPopupIconWindow.visible = false;
        }
    }
}
