import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."
import ButtonState 1.0

ApplicationWindow {
    id: root
    objectName: "mainWindow"
    visible: false
    color: "#FFFFFF"
    width: windowCtrl.width
    height:  windowCtrl.height
    x: windowCtrl.x
    y: windowCtrl.y
    title: qsTr("kylin-aiassistant")
    // 仅窗口模式启用拖动/贴边半屏，侧边栏模式不可拖动
    property bool useStyleWindowManager: windowCtrl.winMode === 3

    // 窗口大小限制：根据 winMode 动态设置
    // winMode = 2 (侧边栏模式)：固定大小，不可拉伸
    // winMode = 3 (窗口模式)：windowCtrl 的尺寸作为最小尺寸
    minimumWidth: windowCtrl.width
    minimumHeight: windowCtrl.height
    maximumWidth: winMode === 2 ? windowCtrl.width : (winMode === 3 ? 16777215 : 16777215)
    maximumHeight: winMode === 2 ? windowCtrl.height : (winMode === 3 ? 16777215 : 16777215)

    // 页面编号
    enum ShowPage {
        CHAT = 1,
        WRITING = 2,
        SPEECH = 3,
        METTING = 4,
        KNOWLEDGE = 5,
        HISTORY = 6,
        SETTING = 7
    }

    property bool isSupportDocumentQA: true
    property bool isSupportImageProcessing: true
    property bool isSupportHtml2Docx: msgPane.isSupportHtml2Docx();
    property int fileCounts: 0
    property bool isSidebarClose: msgPane.getIsSidebarClose()
    property int currentPageNum: 8 // 默认设为8，解决点击对话页面闪烁的问题
    property int winMode: windowCtrl.winMode
    property int maxWidth: 1040 // 全屏时页面最大区域，水平居中显示
    property bool isSettingSubpageFlag: false
    property int lastSettingSubPageNum: 0  // 记录设置页面的子页面号，用于切换回来后重置
    property int taskSessionCount: msgPane.getTaskSessionCount()
    property string commonTipsSources: ""
    property string commonTipsText: ""
    property int smallSideBarWidth: 56
    property bool aboutPageVisible: false  // 控制关于弹窗显示
    // 启动页遮罩是否可见
    property bool showStartupMask: true
    property bool pendingStartupPageSwitch: true

    // 组件引用：因为组件在 Loader 中，需要通过属性暴露给外部访问
    property var mychatRef: null
    property var introductionViewRef: null
    property var historyShowPageRef: null
    property var knowledgeShowPageRef: null
    property var settingShowPageRef: null
    property var settingSideShowPageRef: null
    property string fontFamily: Qt.application.font.family
    property int changeSettingPageByModeOrPageNum: 0 // 切换设置页面是通过窗口侧边切换/通过侧边导航栏切换 0：默认值  1：通过窗口侧边切换 2：通过侧边导航栏切换

    Enums {id: enums}

    // 用于判断当前页面是否点击了外部区域
    onActiveChanged: {
        if (!active) {
            console.log("窗口点击了外面 / 失去焦点")
            if (knowledgeShowPageRef) {
                knowledgeShowPageRef.parentWindowFocusChanged(false)
            }
            if (mychatRef) {
                mychatRef.stopVoiceInputIfActive()
            }
        }
    }

    onWinModeChanged: {
        changeSettingPageByModeOrPageNum = 1
        if (winMode === 2) {
            resetAgentTabToChatIfSidebar()
        }
        // 从窗口模式切换到侧边栏模式时，保存窗口模式下的设置子页面号
        if(winMode === 2 && settingShowPageRef && settingShowPageRef.currentPageNum > 0){
            lastSettingSubPageNum = settingShowPageRef.currentPageNum
        }
        // 从侧边栏模式切换到窗口模式时，如果之前有保存的设置子页面号，则恢复
        // 如果之前没有保存（lastSettingSubPageNum === 0），则保持默认值1
    }

    onCommonTipsTextChanged: {
        if(commonTipsText !== ""){
            commonTips.visible = true
            copyTipCloseTimer.running = true
        }
    }

    onShowStartupMaskChanged: {
        if (!showStartupMask) {
            showStartupPage()
        }
    }

    // 处理窗口关闭事件，根据配置决定是隐藏窗口还是真正关闭
    onClosing: function(close) {
        // 获取配置：是否隐藏到托盘
        var isHideToTray = false
        if (typeof globals !== "undefined" && globals !== null) {
            isHideToTray = globals.getCloseWindowMode()
        }

        if (isHideToTray) {
            // 隐藏窗口而不是关闭
            close.accepted = false
            root.visible = false
        } else {
            // 真正关闭窗口
            // 先通过 D-Bus 通知语音通话进程自行退出，防止其退出流程与主进程交叉造成卡死
            msgPane.stopVoiceChat()
            // 停止朗读（立即中断，参考"三"菜单退出的效果）
            if (mychatRef) {
                mychatRef.stopReadingIfActive()
            }
            close.accepted = true
        }
    }

    function applyWindowGeometry(mode) {
        var isSidebar = (mode === 2 || Number(mode) === 2)
        minimumWidth = windowCtrl.width
        minimumHeight = windowCtrl.height
        maximumWidth = isSidebar ? windowCtrl.width : 16777215
        maximumHeight = isSidebar ? windowCtrl.height : 16777215
        root.x = windowCtrl.x
        root.y = windowCtrl.y
        root.width = windowCtrl.width
        root.height = windowCtrl.height
    }

    Component.onCompleted: {
        windowCtrl.setWindow(root)
        windowCtrl.clearWindowStyle(root)
        
        // 使用 X11 原生 API 移除标题栏 (Linux X11 环境)
        if (x11WindowHelper && x11WindowHelper.isX11Environment()) {
            x11WindowHelper.removeTitleBar(root)
            console.log("X11: Title bar removed using native X11 API")
        }
        
        windowCtrl.initScreenWathcer()
        // Qt 5.12: Qt.callLater() 不支持直接传参，使用闭包包装
        Qt.callLater(function() {
            applyWindowGeometry(root.winMode)
        })
        // 监听 aboutToQuit 信号
        Qt.application.aboutToQuit.connect(function() {
            console.log("[SHUTDOWN] qml aboutToQuit begin")
            if (mychatRef) {
                console.log("[SHUTDOWN] qml shutdownTaskWebEngine...")
                mychatRef.shutdownTaskWebEngine()
                console.log("[SHUTDOWN] qml stopReadingIfActive...")
                mychatRef.stopReadingIfActive()
            } else {
                console.log("[SHUTDOWN] qml mychatRef is null")
            }
            if(mychatRef && !mychatRef.isReplying){
                console.log("[SHUTDOWN] qml aboutToQuit end (not replying, skip onReplyEnd)")
                return
            }
            console.log("[SHUTDOWN] qml msgPane.onReplyEnd(true)...")
            msgPane.onReplyEnd(true);
            console.log("[SHUTDOWN] qml aboutToQuit end")
        });
    }

    function checkAndShowIntroduction() {
        var isFirstInstall = msgPane.getConfigValue("is-first-install")
        var isFirstInstall50 = msgPane.getConfigValue("is-first-install-50")
        console.log("checkAndShowIntroduction - isFirstInstall:", isFirstInstall, "isFirstInstall50:", isFirstInstall50)
        if (isFirstInstall === "true" || isFirstInstall === true || isFirstInstall50 === "true" || isFirstInstall50 === true) {
            onChangeWindowPage(0)
        } else {
            onChangeWindowPage(1)
        }
    }

    function showStartupPage() {
        if (showStartupMask || !pendingStartupPageSwitch || !mainContentLoader.active || !introductionViewRef) {
            return
        }
        pendingStartupPageSwitch = false
        checkAndShowIntroduction()
        Qt.callLater(function() {
            windowCtrl.checkUnProcessFile()
        })
    }

    // 清空之前对话函数
    function onClearListModel(){
        if (mychatRef) {
            mychatRef.handleClearListModel()
        }
    }

    // 清空问答输入框内容
    function onClearMessageField(){
        if (mychatRef) {
            mychatRef.handleClearMessageField()
        }
    }

    //  初始化朗读
    function onInitReadingIndex(){
        if (mychatRef) {
            mychatRef.handleInitReadingIndex()
        }
    }

    // 切换到会议助手打断并初始化朗读
    function onStopReading(){
        if (mychatRef) {
            mychatRef.handleStopReading()
        }
    }

    function onCloseCommand(){
        if (mychatRef) {
            mychatRef.closeCommand()
        }
    }


    // 通用提示
    function showCommonTip(tipImageSource, tipText) {
        commonTipsText = tipText
        commonTipsSources = tipImageSource
        //commonTips.visible = true
        //copyTipCloseTimer.running = true
    }

    // 侧边栏模式下智能体仅支持对话页，窗口顶栏的通道/技能/任务/模型 Tab 需切回对话
    function resetAgentTabToChatIfSidebar() {
        if (winMode !== 2 || !ButtonState.isTaskMode || !mychatRef) {
            return
        }
        if (mychatRef.currentTaskTabIndex !== 0) {
            mychatRef.setTaskTab(0, "chat")
        }
    }

    // 窗口模式切换页面
    function onChangeWindowPage(pageNum){
        // currentPageNum 统一在此处更新
        console.log("----onChangeWindowPage----" + pageNum)

        // 如果切换到非问答页面，停止问答页面的语音输入
        if (pageNum !== 1 && mychatRef) {
            mychatRef.stopVoiceInputIfActive()
            mychatRef.stopReadingIfActive()
        }
        if (pageNum === 0) {
            stackView.clear()
            if (introductionViewRef) {
                stackView.push(introductionViewRef)
            }
        } else if(pageNum === 1){
            if(currentPageNum === 1){
                resetAgentTabToChatIfSidebar()
                return
            }
            stackView.clear()

            if (mychatRef) {
                mychatRef.moveScrollBar()
                Qt.callLater(function() {
                    stackView.push(mychatRef)
                    mychatRef.setFocusToTextArea()
                })
            }
        }else if(pageNum === 5){
            stackView.clear()
            if (knowledgeShowPageRef) {
                stackView.push(knowledgeShowPageRef)
            }
        }else if(pageNum === 6){
            if(currentPageNum === 6){
                return
            }
            stackView.clear()
            if (historyShowPageRef) {
                stackView.push(historyShowPageRef)
                historyShowPageRef.initHistoryShowPage()
            }
        }else if(pageNum === 7){
            if(winMode === 2){
                if (settingSideShowPageRef) {
                    if(changeSettingPageByModeOrPageNum === 2){
                        if(currentPageNum === 7){
                            return
                        }
                    }
                    stackView.clear()
                    // 如果是从窗口模式切换过来的，且之前有保存的设置子页面号，则恢复它
                    if(changeSettingPageByModeOrPageNum === 1 && lastSettingSubPageNum > 0){
                        settingSideShowPageRef.currentPageNum = lastSettingSubPageNum
                        root.isSettingSubpageFlag = true
                    } else {
                        settingSideShowPageRef.settingSidePageBack()
                        root.isSettingSubpageFlag = false
                    }
                    stackView.push(settingSideShowPageRef)
                }
            }else{
                if (settingShowPageRef) {
                    if(changeSettingPageByModeOrPageNum === 1){
                        if(lastSettingSubPageNum === 0){
                            lastSettingSubPageNum = 1
                        }
                        settingShowPageRef.currentPageNum = lastSettingSubPageNum
                    }else if(changeSettingPageByModeOrPageNum === 2){
                        if(currentPageNum === 7){
                            return
                        }
                        settingShowPageRef.currentPageNum = 1
                    }else{
                        console.log("changeSettingPageByModeOrPageNum = 0")
                    }
                    stackView.clear()
                    stackView.push(settingShowPageRef)
                }
            }
        }

        console.log("before currentPageNum" + currentPageNum)
        currentPageNum = pageNum
    }

    function openHistoryPage(sessionId, chatType, modeType){
        if(chatType === 0 ){
            var effectiveModeType = msgPane.getSessionModeType(sessionId)
            if (effectiveModeType === undefined || effectiveModeType === null) {
                effectiveModeType = modeType
            }
            // 快问快答：检查是否为当前会话
            if (effectiveModeType !== 1) {
                var currentSessionId = msgPane.getCurrentSessionId()
                if(String(sessionId) === String(currentSessionId)){
                    onChangeWindowPage(1)
                    return
                }
            }

            // 快问快答（非智能体）回答中不允许切换到其他历史会话
            if (!ButtonState.isTaskMode && mychatRef && mychatRef.isReplying){
                showCommonTip("qrc:/icon/icon/newBuiltTips.svg",qsTr("Generating answer, please try again after completion"))
                return
            }

            // 模式同步
            if (effectiveModeType === 1 && !ButtonState.isTaskMode) {
                // 快问快答 → 智能体历史
                ButtonState.isTaskMode = true
                ButtonState.chatMode = 1
                mychatRef.initTaskModeUI()
            } else if (effectiveModeType !== 1 && ButtonState.isTaskMode) {
                ButtonState.isTaskMode = false
                ButtonState.chatMode = 0
                if(mychatRef && mychatRef.isReplying) {
                    msgPane.deactivateTaskSession()
                } else {
                    msgPane.closeSession()
                }
                mychatRef.initChatModeUI()
            }

            if (mychatRef && mychatRef.currentTaskTabIndex !== 0) {
                mychatRef.setTaskTab(0, "chat")
            }

            //console.log("跳转会话" + model.sessionID)
            //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
            //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
            onChangeWindowPage(1)
            // 清空之前对话
            root.onClearListModel()
            // 新建对话时，初始化朗读
            root.onInitReadingIndex()
            // 清空session
            msgPane.loadHistoryBySessionId(sessionId);
            // 切换对话时，清空输入框内容
            root.onClearMessageField()

            // 快问快答：恢复历史对话的文件引用（智能体无此需求）
            if (effectiveModeType !== 1) {
                var documentJson = msgPane.extractDocumentReferenceFromHistory(sessionId)
                if (documentJson && documentJson !== "" && mychatRef) {
                    mychatRef.restoreDocumentReferenceFromJson(documentJson)
                }
            }
        }
    }

    function newConversationFunction(page) {
        // 当历史记录超出500时，弹出删除弹窗
        var histroyCount = msgPane.getHistoryCount()
        var isShowHistoryMaxTip = msgPane.getIsShowHistoryMaxTip()
        if(histroyCount >= 500 && isShowHistoryMaxTip){
            let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
            if (dialogComponent.status === Component.Ready) {
                let dialog = dialogComponent.createObject(root); // ?????????
                dialog.selectedOptions = []
                dialog.questionText = qsTr("The number of conversations has reached the maximum limit")
                dialog.infoText = qsTr("Historical conversations have reached the storage limit of 500. New conversations will replace the earliest records. Do you want to continue creating new conversations.")
                dialog.noRemindText = qsTr("No remind")
                dialog.showNoRemindCheckBox = true
                dialog.confirmButtonText = qsTr("Create")
                dialog.cancelButtonText = qsTr("Cancel")
                dialog.highlightConfirmButton = true
                dialog.highlightCancelButton = false
                if (dialog) {
                    dialog.confirmed.connect(function(options, noRemindChecked) {
                        if(noRemindChecked){
                            msgPane.setIsShowHistoryMaxTip(false)
                        }
                        msgPane.deleteTheEarliestHistory()
                        newConversation(page)
                        dialog.destroy();
                    });

                    dialog.canceled.connect(function() {
                        dialog.destroy();
                        return
                    });

                    dialog.showDialog();
                } else {
                    console.error("创建弹窗失败：", dialogComponent.errorString());
                }
            }
        }else{
            newConversation(page)
        }
    }

    // 划词进入助手时强制切换到快问快答主页
    function switchToQuickChatFromWordSelection() {
        if (mychatRef && mychatRef.currentTaskTabIndex !== 0) {
            mychatRef.setTaskTab(0, "chat")
        }
        if (ButtonState.isTaskMode) {
            ButtonState.isTaskMode = false
            ButtonState.chatMode = 0
            if (mychatRef && mychatRef.isReplying) {
                msgPane.deactivateTaskSession()
            } else {
                msgPane.closeSession()
            }
            if (mychatRef) {
                mychatRef.initChatModeUI()
            }
        }
    }

    function newConversation(page){
        console.log("page=======", page)
        if (page === 1) {
            if (ButtonState.isTaskMode) {
                // 智能体：允许在回答过程中新建会话
                if (mychatRef && mychatRef.isReplying) {
                    msgPane.deactivateTaskSession()
                } else {
                    msgPane.closeSession()
                }
                // 切换到智能体聊天tab
                if (mychatRef && mychatRef.currentTaskTabIndex !== 0) {
                    mychatRef.setTaskTab(0, "chat")
                }
                root.onChangeWindowPage(1)
                // 停止语音输入
                if (mychatRef) {
                    mychatRef.stopVoiceInputIfActive()
                    mychatRef.stopReadingIfActive()
                }
                // 清空之前对话
                root.onClearListModel()
                // 重置前端回答状态
                if (mychatRef) {
                    mychatRef.isReplying = false
                }
                // 新建对话时，初始化朗读
                root.onInitReadingIndex()
                // 切换对话时，清空输入框内容
                root.onClearMessageField()
            } else {
                // 快问快答：回答中不允许新建会话
                if (mychatRef && mychatRef.isReplying) {
                    showCommonTip("qrc:/icon/icon/newBuiltTips.svg",qsTr("Generating answer, please try again after completion"))
                } else {
                    //msgPane.onStopSpeak();//停止朗读
                    //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
                    //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
                    // 开启新对话时停止当前朗读
                    if (mychatRef) {
                        mychatRef.stopVoiceInputIfActive()
                        mychatRef.stopReadingIfActive()
                    }
                    root.onChangeWindowPage(1)
                    // 清空之前对话
                    root.onClearListModel()
                    // 新建对话时，初始化朗读
                    root.onInitReadingIndex()
                    // 清空session
                    msgPane.closeSession()
                    // 切换对话时，清空输入框内容
                    root.onClearMessageField()
                }
            }
        }
    }

    function openSettingPage(pageNum) {
        if(winMode === 2){
            if (settingSideShowPageRef) {
                settingSideShowPageRef.currentPageNum = pageNum
                // 记录设置页面的子页面号
                lastSettingSubPageNum = pageNum
            }
        }else{
            if (settingShowPageRef) {
                settingShowPageRef.currentPageNum = pageNum
                // 记录设置页面的子页面号
                lastSettingSubPageNum = pageNum
            }
        }
    }

    function getFoldersBySuffixCheck(pathArr) {
        const folderPaths = [];

        // 空值保护
        if (!pathArr || pathArr.length === 0) {
            return folderPaths;
        }

        // 遍历每个路径
        pathArr.forEach(path => {
            // 1. 清理路径：移除file://前缀、末尾的/，统一处理
            let cleanPath = path.replace("file://", "").trim();
            cleanPath = cleanPath.replace(/\/+$/, ""); // 移除末尾所有/

            // 2. 核心判断：是否包含有效的文件后缀（.xxx格式）
            const lastDotIndex = cleanPath.lastIndexOf(".");
            // 有效后缀条件：
            // - 存在.
            // - .不是最后一个字符（排除如"文件名."的情况）
            // - .后面至少有1个字符（扩展名）
            const hasValidExtension = lastDotIndex > 0 && lastDotIndex < cleanPath.length - 1;

            // 3. 无有效后缀 → 判定为文件夹
            if (!hasValidExtension) {
                folderPaths.push(path); // 保留原始路径格式
            }
        });

        return folderPaths;
    }


    function changeTrasnparent(flag){
        transparentRet.visible = flag
    }

    // 启动页组件：在 stackView 中优先展示应用图标
    Component {
        id: startupPageComponent

        Rectangle {
            color: "#FFFFFF"
            anchors.fill: parent

            Image {
                id: startupIcon
                source: "qrc:/icon/icon/kylin-ai-assistant-new.png"
                anchors.centerIn: parent
                width: 120
                height: 120
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
                // 设置 sourceSize 确保 SVG 按正确尺寸渲染，避免模糊
                // 考虑设备像素比，在高 DPI 屏幕上也能清晰显示
                sourceSize.width: Math.round(width * Screen.devicePixelRatio)
                sourceSize.height: Math.round(height * Screen.devicePixelRatio)
            }

            // 图片显示一段时间后，加载主内容
            Timer {
                interval: 50    // 图片停留 50ms 左右
                running: true
                repeat: false
                onTriggered: {
                    // 激活主内容 Loader（延迟创建 802-955 行的组件）
                    if (mainContentLoader && !mainContentLoader.active) {
                        mainContentLoader.active = true
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部标题栏
        TitleBar {
            id: titleBar
            height: 40
            Layout.fillWidth: true
            isSidebarCloseFlag: isSidebarClose
            pageNum: currentPageNum
            taskSessionCount: root.taskSessionCount
            isSettingSubpage: isSettingSubpageFlag
            isShowIntroduction: introductionViewRef ? introductionViewRef.visible : false
            z: 10
            onChangeIsSidebarClose: function(flag){
                mychatRef.changeSideBarMove()
                root.isSidebarClose = flag
                msgPane.setIsSidebarClose(flag)
            }
            onNewConversation: function(){
                newConversationFunction(root.currentPageNum)
            }
            onChangeWindowMode: function(){
                resetAgentTabToChatIfSidebar()
                var targetPage = currentPageNum
                if (winMode === 2 && ButtonState.isTaskMode && mychatRef && mychatRef.currentTaskTabIndex !== 0) {
                    targetPage = 1
                }
                onChangeWindowPage(targetPage)
            }
            onBtnClick: function(){
                if (settingSideShowPageRef) {
                    settingSideShowPageRef.settingSidePageBack()
                }
                root.isSettingSubpageFlag = false
            }
            onShowSetting: function(){
                onChangeWindowPage(7)
            }
        }

        RowLayout{
            id: mainLayout
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 0
            // 让RowLayout内的所有子项整体居中
            Layout.alignment: Qt.AlignCenter

            // 侧边栏收起展示区域
            SmallSideBar{
                id: smallSideBarView
                width: smallSideBarWidth
                Layout.fillHeight: true
                visible: isSidebarClose && !IntroductionViewState.isIntroductionVisible
                smallCurrentPageNum: currentPageNum
            }

            // 页面展示区域
            StackView{
                id: stackView
                Accessible.name: "kylin-aiassistant_main_stackView"
                Layout.fillWidth: true
                Layout.fillHeight: true
                // 启动时先显示空白页面，真实内容在主内容 Loader 加载完成后再根据业务逻辑 push 进来
                initialItem: Item { }
                visible: true
            }

            Connections{
                target: msgPane
                // 侧边栏模式切换插件页面
                onShowPluginPage:function (num, pluginName){

                }

                // 窗口模式切换页面
                onShowWindowPage:function (num){
                    console.log("onShowWindowPage: 切换到页面", num)
                    root.onChangeWindowPage(num)
                }

                onSigEnterFromWordSelection: function() {
                    root.switchToQuickChatFromWordSelection()
                }

                onSigActivateAndSwitchToSession: function(sessionId) {
                    windowCtrl.showMainWindow()
                    if (mychatRef && mychatRef.currentTaskTabIndex !== 0) {
                        mychatRef.setTaskTab(0, "chat")
                    }
                    root.onChangeWindowPage(1)
                    var modeType = msgPane.getSessionModeType(sessionId)
                    if (modeType === 1 && !ButtonState.isTaskMode) {
                        ButtonState.isTaskMode = true
                        ButtonState.chatMode = 1
                        mychatRef.initTaskModeUI()
                    } else if (modeType !== 1 && ButtonState.isTaskMode) {
                        ButtonState.isTaskMode = false
                        ButtonState.chatMode = 0
                        mychatRef.initChatModeUI()
                    }
                    root.onClearListModel()
                    root.onInitReadingIndex()
                    root.onClearMessageField()
                }

                // 窗口模式切换插件页面
                onShowWindowPluginPage:function (num, pluginName){
                }

                onSigNlpModelInitCompleted:function () {
                    // 25w 文档不再经过向量化处理，由助手提取文档内容，所有架构都支持临时文件问答
                    isSupportDocumentQA = true;
                    isSupportImageProcessing = msgPane.isSupportImageProcessing();
                    globals.sendLog("isSupportImageProcessing" + isSupportImageProcessing)
                    //mychat.checkCurrentModelStatus()
                }

                onChatInitCompleted: function () {
                    // 初始化成功，关闭长计时器
                    if (maxLoaderTimer.running === true) {
                        maxLoaderTimer.stop()
                    }

                    // SDK初始化完成后设置模型
                    console.log("SDK初始化完成后设置模型")
                    msgPane.initNLPModel()

                    // 如果初始化成功时间小于短计时器，则等待
                    if (minLoaderTimer.running === true) {
                        return
                    }

                    // 如果初始化成功时间大于短计时器，则关闭遮罩
                    showStartupMask = false
                }

                onShowTipMessage:function (tipMesasge){
                    commonTipsText = tipMesasge
                    commonTipsSources = "qrc:/html/html/img/tips.svg"
//                    commonTips.visible = true
//                    copyTipCloseTimer.running = true
                }

                onSigClearTextArea:function () {
                    root.onClearMessageField()
                }

                onCloseOrOpenSideBar:function (isSidebarClose) {
                    root.isSidebarClose = isSidebarClose
                    if(isSidebarClose){
                        smallSideBarView.visible = true
                    }else{
                        smallSideBarView.visible = false
                    }
                }

                onChangeWinModeToChat:function(winMode){
                    root.winMode = winMode
                }

                onSigShowSettingsPage: function() {
                    root.onChangeWindowPage(7)
                }

                // 删除当前对话的model ，并初始化页面
                onClearCurrentPage:function(pageNum){
                    console.log("onClearCurrentPage====", pageNum)
                }

                onTaskSessionCountChanged: function(count) {
                    root.taskSessionCount = count
                }
            }

            // 监听 windowCtrl 的窗口模式变化，更新窗口大小和位置
            Connections {
                target: windowCtrl

                onWinModeChanged: function (mode)  {
                    console.log("onWinModeChanged")
                    if (mode === 2 || Number(mode) === 2) {
                        resetAgentTabToChatIfSidebar()
                    }
                    // Qt 5.12: Qt.callLater() 不支持直接传参，使用闭包包装
                    Qt.callLater(function() {
                        applyWindowGeometry(mode)
                    })
                }

                onScreenChanged: function() {
                    // Qt 5.12: Qt.callLater() 不支持直接传参，使用闭包包装
                    Qt.callLater(function() {
                        applyWindowGeometry(root.winMode)
                    })
                }
            }
        }
    }

    LoadImage{
        id: startupMask
        visible: showStartupMask
        // 图片显示一段时间后，加载主内容
        Timer {
            id: minLoaderTimer
            interval: 50    // 图片停留 50ms 左右
            running: true
            repeat: false
            onTriggered: {
                // 激活主内容 Loader（延迟创建主内容组件）
                if (mainContentLoader && !mainContentLoader.active) {
                    mainContentLoader.active = true
                }
            }
        }

        Timer {
            id: maxLoaderTimer
            interval: 30000    // 30s后强制隐藏动画
            running: true
            repeat: false
            onTriggered: {
                // 激活主内容 Loader（延迟创建主内容组件）
                if (startupMask.visible === true){
                    showStartupMask = false
                }
            }
        }
    }

    DropArea{
        anchors.fill: parent
        // 根据PDF支持情况动态构建文件格式列表
        property var fileFormats: globals.isPdfSupported ?
            [ "txt", "doc", "docx", "pdf", "wps", "pptx", "ppt", "ofd" ] :
            [ "txt", "doc", "docx", "wps", "pptx", "ppt", "ofd" ]
        property var imageFormats : [ "bmp", "jpe", "jpeg", "jpg", "pbm", "pgm", "png",  "ppm", "tiff", "webp" ]

        // 对话页：快问快答始终可拖入；智能体仅「聊天」TAB 可拖入，通道/技能/任务/模型等 Web 页禁止
        function isChatFileDropTarget() {
            if (root.currentPageNum !== 1)
                return false
            if (!mychatRef)
                return true
            if (ButtonState.isTaskMode && mychatRef.currentTaskTabIndex !== 0)
                return false
            return true
        }

        onEntered: {
            if(!(root.currentPageNum === 1 || root.currentPageNum === 5)){
                drag.accepted = false;
                console.log("Drop failed, root.currentPageNum wrong!")
                return false;
            }

            // 智能体非对话 Web 页（通道/技能/任务/模型等）禁止拖入文件与图片
            if (root.currentPageNum === 1 && !isChatFileDropTarget()) {
                drag.accepted = false;
                console.log("Drop failed, not on agent chat tab!")
                return false;
            }

            // 当选中非"生成待办"的其他指令时，禁止文件拖入
            if(root.currentPageNum === 1 && mychatRef && mychatRef.hasActiveCommand && !mychatRef.isTodoCommand){
                drag.accepted = false;
                console.log("Drop failed, command selected and not todo command!")
                return false;
            }

            root.fileCounts = drag.urls.length
            var urls = drag.urls;
            var filePath = urls[0];
            var fileExtension = filePath.split('.').pop().toLowerCase();

            if(root.currentPageNum === 1){
                var isImageFile = imageFormats.indexOf(fileExtension) !== -1
                var isDocumentFile = fileFormats.indexOf(fileExtension) !== -1
                var imageSupportedForCurrentMode = ButtonState.isTaskMode || isSupportImageProcessing
                if((!isDocumentFile && !isImageFile) || (!isSupportDocumentQA && isDocumentFile) || (!imageSupportedForCurrentMode && isImageFile)){
                    drag.accepted = false;
                    console.log("Drop failed, file type unsupport!")
                    return false;
                }
            }else if(root.currentPageNum === 5){
                if(urls.length > 100){
                    drag.accepted = false
                    return false
                }
                let paths = []
                for(let i = 0; i < urls.length; i++){
                    paths.push(urls[i].replace("file://", ""))
                }
                let isContainDir = msgPane.isContainDir(paths)
                // 判断是否包含文件夹
                if(isContainDir){
                    console.log("Drop failed, has Folder")
                    drag.accepted = false
                    return false
                }

                // 此处获取的多文件列表文件夹被排序在最前面，所以会校验住，所以先不进行全部路径的校验
//                if(fileFormats.indexOf(fileExtension) === -1){
//                    drag.accepted = false;
//                    console.log("Drop failed, file type unsupport!")
//                    return false;
//                }
            }

            drag.accepted = true;
            if(root.currentPageNum === 1){
                if (mychatRef) {
                    mychatRef.showDragShadowFlag = true
                }
            }else if(root.currentPageNum === 5){
                if (knowledgeShowPageRef) {
                    knowledgeShowPageRef.showDragShadowFlag = true
                }
            }

            return true;
        }

        onExited: {
            if(root.currentPageNum === 1){
                if (mychatRef) {
                    mychatRef.showDragShadowFlag = false
                }
            }else if(root.currentPageNum === 5){
                if (knowledgeShowPageRef) {
                    knowledgeShowPageRef.showDragShadowFlag = false
                }
            }
        }

        onDropped: {
            if (root.currentPageNum === 1 && !isChatFileDropTarget()) {
                return
            }

            var urls = drop.urls;
            var filePath = urls[0];
            filePath = filePath.replace("file://", "");
            var fileExtension = filePath.split('.').pop().toLowerCase();
            if(root.currentPageNum === 1){
                if(fileFormats.indexOf(fileExtension) !== -1){
                    msgPane.onShowDocument(filePath);
                    if (mychatRef) {
                        mychatRef.moveScrollBar()
                    }
                }
                else if(imageFormats.indexOf(fileExtension) !== -1){
                    // 25w变动:回答过程中允许拖入图片
                    msgPane.onShowImage(filePath, 1);
                    if (mychatRef) {
                        mychatRef.moveScrollBar()
                    }
                }
                if (mychatRef) {
                    mychatRef.showDragShadowFlag = false
                }
            }else if(root.currentPageNum === 5){
                var filePaths = [];   // 存储替换后的文件路径数组

                // 遍历 urls 数组
                for (let i = 0; i < urls.length; i++) {
                    let originalUrl = urls[i];
                    // 替换 "file://" 前缀（注意：若路径含特殊字符，可加 decodeURIComponent 解码）
                    let filePath = originalUrl.replace("file://", "");
                    // 可选：解码 URL 中的特殊字符（如空格→%20，中文编码等）
                    filePath = decodeURIComponent(filePath);
                    // 将处理后的路径插入新数组
                    filePaths.push(filePath);
                }

                if (knowledgeShowPageRef) {
                    knowledgeShowPageRef.uploadFileUrl = filePaths
                    knowledgeShowPageRef.showDragShadowFlag = false
                }
            }
        }
    }

    // 通用提示tip
    CommonTips{
        id: commonTips
        anchors.fill: parent
        color: "transparent"
        visible: false
        z: 1
        tipImageSource: commonTipsSources
        tipText: commonTipsText
    }

    // 指令展开时覆盖标题栏的遮罩层
    Rectangle {
        id: commandTitleBarOverlay
        x: isSidebarClose ? smallSideBarWidth : 0
        y: 0
        width: parent.width - x
        height: 40  // TitleBar 高度
        color: Qt.rgba(0, 0, 0, 0.2)
        visible: currentPageNum === 1 && mychatRef && mychatRef.isCommandOverlayVisible
        z: 11  // 需要高于 TitleBar 的 z: 10

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            onWheel: {
                wheel.accepted = true
            }
            onClicked: {
                if (mychatRef) {
                    mychatRef.closeCommand()
                }
            }
        }
    }

    // 用于实现popup右键显示时拦截底层的鼠标事件
    Rectangle{
        id: transparentRet
        anchors.fill: parent
        color: "transparent"
        visible: false
        z: 1
        MouseArea{
            id: transparentRetMouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            // 使用 WheelHandler 来忽略滚轮事件
            onWheel: {
                // 阻止事件传播
                wheel.accepted = true
            }
            onClicked: {
                transparentRet.visible = false
            }
        }
    }

    Timer {
        id: copyTipCloseTimer
        interval: 3000
        repeat: false   // 设置定时器只执行一次
        running: false   // 设置定时器启动
        onTriggered: {
            // 定时器触发后执行的代码
            commonTips.visible = false
            commonTipsText = ""
            commonTipsSources = ""
        }
    }

    // 关于弹窗
    AboutWindow {
        id: aboutPage
    }

    // 主内容：延迟加载（启动图片消失后再创建）
    Loader {
        id: mainContentLoader
        active: false
        sourceComponent: Item {
            id: mainContentRoot

            // 新手引导界面
            IntroductionView {
                id: introductionView
                visible: false
                onCloseIntroductionView: {
                    onChangeWindowPage(1)
                    root.isSidebarClose = true
                    msgPane.setIsSidebarClose(true)
                }
                onVisibleChanged: {
                    // 通过QML单例通知全局划词组件
                    console.log("IntroductionViewState.isIntroductionVisible " + IntroductionViewState.isIntroductionVisible)
                    IntroductionViewState.isIntroductionVisible = visible
                }
            }

            Chat{
                id: mychat
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: false
                showDragShadowFlag: false
                onCopySuccess: {
                    console.log("**********copy success********")
                    commonTipsText = qsTr("Copy Successful")
                    commonTipsSources = "qrc:/icon/icon/dialog-success.svg"
//                    commonTips.visible = true
//                    copyTipCloseTimer.running = true
                }
                onTipShow:function(iconPath, message){
                    commonTipsText = message
                    commonTipsSources = iconPath
                }

                onShowChatTransparentRet: function(flag){
                    root.changeTrasnparent(flag)
                }
            }

            HistoryShowPage{
                id: historyShowPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: false
                onOpenHistoryPageBySessionIdAndChatType: function(sessionId, chatType, modeType){
                    openHistoryPage(sessionId, chatType, modeType)
                }
            }

            Loader {
                id: knowledgeShowPageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                active: false
                Component.onCompleted: {
                    if (globals.isKnowledgeBaseSupported) {
                        // 使用异步加载，避免在解析时验证类型
                        Qt.callLater(function() {
                            knowledgeShowPageLoader.source = "qrc:/qml/qml/KnowledgeShowPageWrapper.qml"
                            knowledgeShowPageLoader.active = true
                        })
                    }
                }
                onLoaded: {
                    if (item && item.knowledgeShowPage) {
                        root.knowledgeShowPageRef = item.knowledgeShowPage
                    }
                }
            }


            SettingShowPage{
                id: settingShowPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: false
                onChangeTransparentSettingShowPage: function(flag){
                    root.changeTrasnparent(flag)
                }
                onSettingSubPageNumChanged: function(pageNum){
                    // 记录设置页面的子页面号
                    root.lastSettingSubPageNum = pageNum
                }
            }

            SettingSideShowPage{
                id: settingSideShowPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: false
                onChangeSettingSidePage: function(){
                    root.isSettingSubpageFlag= true
                }
                onChangeTransparentSettingSideShowPage: function(flag){
                    root.changeTrasnparent(flag)
                }
                onSettingSubPageNumChanged: function(pageNum){
                    // 记录设置页面的子页面号
                    root.lastSettingSubPageNum = pageNum
                }
            }

            // 主内容完全创建完成后，再从启动页切换到正常界面
            Component.onCompleted: {
                // 将组件引用赋值给根对象的属性，供外部访问
                root.mychatRef = mychat
                root.introductionViewRef = introductionView
                root.historyShowPageRef = historyShowPage
                // knowledgeShowPageRef 在 Loader 的 onLoaded 回调中设置
                if (knowledgeShowPageLoader.item && knowledgeShowPageLoader.item.knowledgeShowPage) {
                    root.knowledgeShowPageRef = knowledgeShowPageLoader.item.knowledgeShowPage
                }
                root.settingShowPageRef = settingShowPage
                root.settingSideShowPageRef = settingSideShowPage

                if (stackView) {
                    stackView.clear()
                    showStartupPage()
                }

                // 主内容加载完成后，根据SDK初始化状态判断是否隐藏启动遮罩。如果SDK初始化一直失败，30s后会强制隐藏。
                if (globals.getIsChatInitCompleted()) {
                    showStartupMask = false
                    if (maxLoaderTimer.running === true) {
                        maxLoaderTimer.stop()
                    }
                }
            }
        }
    }
}
