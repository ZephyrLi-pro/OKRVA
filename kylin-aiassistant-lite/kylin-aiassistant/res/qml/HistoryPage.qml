import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.labs.folderlistmodel 2.12
import ButtonState 1.0
import "."

Rectangle {
    id: historyPage
    Accessible.name: "kylin-aiassistant_HistoryPage_historyPage"
    width: parent.width
    height: parent.height
//    color: "#FFFFFF"
    color: "green"
    property int winMode: msgPane.getCurWinMode()
    property int currentSessionId: msgPane.getCurrentSessionId()
    property bool selectAllButtonChecked: false
    property int showPageNum: msgPane.getShowPage()

    // 存储和管理历史会话的数据
    ListModel {
        id: dialogueListModel
        Accessible.name: "kylin-aiassistant_HistoryPage_historyPage"
    }
    ListModel {
        id: filteredListModel
        Accessible.name: "kylin-aiassistant_HistoryPage_historyPage"
    }
//    ListModel {
//        id: selectAllButtonChecked
//        ListElement { boolValue: false }
//    }

    // 过滤函数，用于过滤历史记录列表
    function filterList(searchText) {
        //console.log("搜索历史输入的内容:"+searchText)
        filteredListModel.clear();
        for (var i = 0; i < dialogueListModel.count; ++i) {
            var item = dialogueListModel.get(i);
            if(searchText === null || searchText === ""){
                filteredListModel.append(item);
            }else{
                var msg = item.msgHead + "：" + item.message
                if(item.message.includes(searchText) || item.msgHead.includes(searchText) || msg.includes(searchText)){
                    filteredListModel.append(item);
                }
            }
        }
    }

    // 判断文本前十个字符是否为汉字，并显示前十个汉字,超出显示省略号
    function getElidedText(inputText) {
        var winMode = historyPage.winMode
        var trimText = trimStringAtEnd(inputText)
        trimText = replaceString(trimText)
        trimText = trimText.replace(/<\/?.+?\/?>/g,'');
        trimText = trimText.replace(/&quot;/g,'"');
        //侧边栏模式
        if(winMode === 2){
            return trimText
        }
        //窗口模式
        else if (winMode === 3){
            if(trimText !== null && trimText.length > 10){

            }else{
                return trimText
            }
            var chineseRegex = /[\u4e00-\u9fa5]/g;  // 汉字的 Unicode 范围
            var firstTenChars = trimText.slice(0, 12);  // 获取前12个字符
            var chineseCount = 0;

            // 计算前十个字符中有多少个是汉字
            for (var i = 0; i < firstTenChars.length; i++) {
                if (firstTenChars[i].match(chineseRegex)) {
                    chineseCount++;
                }
            }

            // 如果前十个字符都是汉字，则显示前十个汉字并加省略号
            if (chineseCount === firstTenChars.length) {
                return firstTenChars + '...';
            } else {
                return trimText;  // 如果前十个字符不是全部汉字，显示完整文本
            }
        }
    }
    // 定义函数 使用正则表达式去除末尾的空格和换行符
    function trimStringAtEnd(str) {
        var myText = str
        myText = myText.replace(/^[\s\n\r\t]+/, '')  // 使用正则表达式去除开头的空白字符
        myText = myText.replace(/[\s\n\r\t]*$/, '')  // 使用正则表达式去除末尾的空格和换行符
        return myText
   }
    // 定义函数 使用正则表达式替换文本内换行，制表符/t
    function replaceString(str) {
        var myText = str
        myText = myText.replace(/[\n]+/g, ' ')
        myText = myText.replace(/[\t]+/g, ' ')
        myText = myText.replace(/[\s]+/g, ' ')
        myText = myText.replace(/[\n]/g, ' ')
        myText = myText.replace(/[\t]/g, ' ')
        return myText
   }
    Connections {
        target: historyPage
        onSelectAllButtonCheckedChanged: function(){
            if(historyPage.selectAllButtonChecked){
                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                deleteButtonText.color = "#F53F3F"
            }else{
                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
            }
        }
    }

    Component.onCompleted: {
        if (msgPane === null || msgPane === undefined) {
        }
        if (msgPane) {
            //初始化加一个延迟加载，解决获取不到高度的问题
            timer.running = true
        }
    }
    // 用于解决初始化获取不到高度的问题
    Timer {
        id: timer
        Accessible.name: "kylin-aiassistant_HistoryPage_timer"
        interval: 10 // 10毫秒后执行
        running: false
        repeat: false
        onTriggered: {
            msgPane.getDialogueList()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        visible: dialogueListModel.count > 0
        //visible: true
        Rectangle {
            color: "#ffffff"
//            color: "yellow"
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                id: historyPageColumn
                Accessible.name: "kylin-aiassistant_HistoryPage_historyPageColumn"
                anchors.fill: parent
                leftPadding: 10
                rightPadding: 10
                bottomPadding: 0

                Rectangle {
                    id: historyPageRectangle
                    Accessible.name: "kylin-aiassistant_HistoryPage_historyPageRectangle"
                    width: parent.width - historyPageColumn.leftPadding - historyPageColumn.rightPadding
                    height: parent.height - historyPageColumn.bottomPadding
                    color: "#ffffff"
                    // radius: 6

                    Column {
                        id: historyPageRectangleColumn
                        Accessible.name: "kylin-aiassistant_HistoryPage_historyPageRectangleColumn"
                        anchors.fill: parent
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0

                        // 搜索历史框
                        Rectangle {
                            id: historyPageSearchRectangle
                            Accessible.name: "kylin-aiassistant_HistoryPage_historyPageSearchRectangle"
                            width: parent.width
                            height: 38
                            color: "#FFFFFF"
                            radius: 6

                            Column {
                                anchors.fill: parent
                                leftPadding: 0
                                rightPadding: 0
                                topPadding: 0

                                Rectangle{
                                    id: searchLineRet
                                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRet"
                                    width: parent.width
                                    height: 38
                                    radius: 4
                                    //color: "#ffffff"
                                    color: "#1a000000"
                                    //border.color: "#3790FA"
                                    border.color: "#1a000000"
                                    border.width: 2
                                    Row{
                                        id: searchLineRow
                                        Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRow"
                                        spacing: 0
                                        width: parent.width
                                        height: parent.height - searchLineRet.border.width*2
                                        anchors.centerIn: parent
                                        visible: false
                                        Rectangle{
                                            id: searchLineRowRet
                                            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRowRet"
                                            width: 5
                                            height: parent.height
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: "transparent"
                                        }
                                        Image{
                                            id: searchLineImg
                                            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineImg"
                                            width: 16
                                            height: 16
                                            source: "qrc:/icon/icon/sidebar/search.svg"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        TextField {
                                            id: searchLineEdit
                                            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineEdit"
                                            placeholderText: qsTr("Search history")
                                            width: parent.width - searchLineRowRet.width - searchLineImg.width - searchLineRow.spacing - searchLineRet.border.width*2 - (closeImgRet.visible ? closeImgRet.width : 0)
                                            height: parent.height
                                            selectByMouse: true
                                            verticalAlignment: Text.AlignVCenter
                                            horizontalAlignment: Text.AlignLeft
                                            background: Rectangle {
                                                id: searchLineEditRet
                                                Accessible.name: "kylin-aiassistant_HistoryPage_searchLineEditRet"
                                                radius: 4
                                                color: searchLineEdit.focus ? "#ffffff" : "#0d262626"
                                            }
                                            onFocusChanged: {
                                            }
                                            onTextChanged: filterList(searchLineEdit.text)
                                        }
                                        Rectangle{
                                            id: closeImgRet
                                            Accessible.name: "kylin-aiassistant_HistoryPage_closeImgRet"
                                            width: 20
                                            height: parent.height
                                            anchors.verticalCenter: parent.verticalCenter
                                            visible: searchLineEdit.text !== ""
                                            color: "transparent"
                                            Image{
                                                id: closeImg
                                                Accessible.name: "kylin-aiassistant_HistoryPage_closeImg"
                                                width: 16
                                                height: 16
                                                source: "qrc:/icon/icon/close-black.svg"
                                                anchors.centerIn: parent
                                            }
                                            MouseArea{
                                                id: closeImgRetMouseArea
                                                Accessible.name: "kylin-aiassistant_HistoryPage_closeImgRetMouseArea"
                                                anchors.fill: parent
                                                propagateComposedEvents: true
                                                onEntered: {

                                                }
                                                onExited: {

                                                }
                                                onPressed: {

                                                }
                                                onReleased: {

                                                }
                                                onClicked: {
                                                    searchLineEdit.text = ""
                                                }
                                            }
                                        }
                                    }
                                    RowLayout{
                                        id: searchLineHidden
                                        Accessible.name: "kylin-aiassistant_HistoryPage_searchLineHidden"
                                        spacing: 0
                                        width: parent.width
                                        height: parent.height - 2
//                                        anchors.centerIn: parent
                                        visible: true
                                        Rectangle{
                                            width: searchLineImgHidden.width +　searchLineTextHidden.width + searchLineRowHidden.spacing
                                            height: parent.height
                                            color: "transparent"
                                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                            Row{
                                                id: searchLineRowHidden
                                                Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRowHidden"
                                                width: parent.width
                                                height: parent.height
                                                anchors.centerIn: parent
                                                spacing: 8
                                                Image{
                                                    id: searchLineImgHidden
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineImgHidden"
                                                    width: 16
                                                    height: 16
                                                    source: "qrc:/icon/icon/sidebar/search.svg"
                                                    sourceSize: Qt.size(16, 16)
                                                    smooth: true
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                                Label {
                                                    id: searchLineTextHidden
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineTextHidden"
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: qsTr("Search history")
                                                    color: Qt.rgba(0/255, 0/255, 0/255, 0.55)
                                                }
                                            }
                                        }
                                    }
                                    MouseArea {
                                        id: mouseArea
                                        Accessible.name: "kylin-aiassistant_HistoryPage_mouseArea"
                                        anchors.fill: parent
                                        propagateComposedEvents: true
                                        onClicked: {
                                            // 隐藏上层遮罩层和MouseArea，显示底层真实输入框
                                            searchLineEdit.cursorVisible = true
                                            searchLineEdit.cursorPosition = 0
                                            searchLineEdit.focus = true
                                            searchLineEdit.forceActiveFocus()
                                            searchLineHidden.visible = false
                                            mouseArea.visible = false
                                            searchLineRet.border.color = "#3790FA"
                                            searchLineRet.color = "#ffffff"
                                            searchLineRow.visible = true
                                            root.onCloseCommand()
                                        }
                                    }
                                }
                            }
                        }
                        // 历史会话列表
                        Rectangle {
                            id: historyPageListRet
                            Accessible.name: "kylin-aiassistant_HistoryPage_historyPageListRet"
                            height: historyPage.height - historyPageColumn.bottomPadding - historyPageSearchRectangle.height - bottomButtonLayout.height - historyPageRectangleColumn.topPadding
                            //width: historyPage.width - historyPageColumn.leftPadding - historyPageColumn.rightPadding
                            width: parent.width + 10
                            color: "transparent"
//                            color: "green"
                            property int isEdit: 0
                            // 滑动区域
                            ScrollView {
                                id: historyPageScrollView
                                Accessible.name: "kylin-aiassistant_HistoryPage_historyPageScrollView"
                                anchors.fill: parent
                                visible: dialogueListModel.count > 0
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                //ScrollBar.vertical.minimumSize:this.height
                                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                                // 历史会话列表视图
                                ListView {
                                    id: dialogueListView
                                    Accessible.name: "kylin-aiassistant_HistoryPage_dialogueListView"
                                    clip: true
                                    //防止过度滚动
                                    boundsMovement: Flickable.StopAtBounds
                                    anchors.fill: parent
                                    anchors.top: parent.top
                                    anchors.topMargin: 8
                                    model: filteredListModel
                                    property bool isContentOverflowing: false
                                    Connections {
                                        target: filteredListModel
                                        onRowsInserted: {
                                            // 当有新项目插入时，可以在这里处理
                                            selectAllCheckBox.checked = false
                                        }
                                    }

                                    // 监听model的rowsRemoved信号
                                    Connections {
                                        target: filteredListModel
                                        onRowsRemoved: {
                                            // 当有项目被移除时，可以在这里处理
                                            selectAllCheckBox.checked = false
                                        }
                                    }
                                    onHeightChanged: {
                                        isContentOverflowing = contentHeight > height
                                        if(isContentOverflowing){
                                            historyPageScrollView.ScrollBar.vertical.policy = ScrollBar.AlwaysOn
                                        }else{
                                            historyPageScrollView.ScrollBar.vertical.policy = ScrollBar.AlwaysOff
                                        }
                                    }
                                    onContentHeightChanged: {
                                        // 判断内容是否超出容器高度，超出显示滚动条
                                        isContentOverflowing = contentHeight > height
                                        if(isContentOverflowing){
                                            historyPageScrollView.ScrollBar.vertical.policy = ScrollBar.AlwaysOn
                                        }else{
                                            historyPageScrollView.ScrollBar.vertical.policy = ScrollBar.AlwaysOff
                                        }
                                    }
                                    // 历史会话项布局
                                    delegate: Item {
                                        id: dialogueListViewItem
                                        Accessible.name: "kylin-aiassistant_HistoryPage_dialogueListViewItem"
                                        width: historyPageListRet.width - 10
                                        height: dialogueTitleMetrics.implicitHeight + 10
                                        property int spacingRetBool: 0
                                        property int deleteDialogueImageRetBool: 0
                                        Rectangle{
                                            id: recorderRet
                                            Accessible.name: "kylin-aiassistant_HistoryPage_recorderRet"
                                            width: parent.width
                                            height: dialogueTitleMetrics.implicitHeight + 10
                                            radius: 4
                                            color: "transparent"
                                            Row{
                                                id: recorderRowItem
                                                Accessible.name: "kylin-aiassistant_HistoryPage_recorderRowItem"
                                                width: parent.width
                                                height: dialogueTitleMetrics.implicitHeight + 10
                                                spacing: 0
                                                Rectangle {
                                                    id: recorder
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_recorder"
    //                                                width: parent.width - recorderRowItem.spacing - (spacingRet.visible ? spacingRet.width : 0)
    //                                                             - (deleteDialogueImageRet.visible ? deleteDialogueImageRet.width : 0)
                                                    width: parent.width - recorderRowItem.spacing - (historyPageListRet.isEdit === 0 ? (spacingRet.width + deleteDialogueImageRet.width) : 0)
                                                           //(historyPageListRet.isEdit === 0 ? ((dialogueListViewItem.spacingRetBool === 1 ? spacingRet.width : 0 ) + (dialogueListViewItem.deleteDialogueImageRetBool === 1 ? deleteDialogueImageRet.width : 0 )) : 0)

                                                    height: dialogueTitleMetrics.implicitHeight + 10
                                                    radius: 4
                                                    color: "transparent"
                                                    MouseArea {
                                                        id: recorderMouseArea
                                                        Accessible.name: "kylin-aiassistant_HistoryPage_recorderMouseArea"
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        onEntered: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "#26000000";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                //deleteDialogueImageRet.visible = true
                                                                //spacingRet.visible = true
                                                                dialogueListViewItem.spacingRetBool = 1
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 1
                                                                deleteDialogueImage.visible = true
                                                                deleteDialogueImageRet.color = "#26000000"
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    recorderRet.color = "transparent"
                                                                }
                                                            }
                                                        }
                                                        onExited: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "transparent";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                //deleteDialogueImageRet.visible = false
                                                                //spacingRet.visible = false
                                                                dialogueListViewItem.spacingRetBool = 0
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 0
                                                                deleteDialogueImage.visible = false
                                                                deleteDialogueImageRet.color = "transparent"
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    recorderRet.color = "#16000000"
                                                                }
                                                            }
                                                        }
                                                        onPressed: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "#33000000";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_white.svg"
                                                            }
                                                        }
                                                        onReleased: {
                                                            if(!selectDialogueRectangle.visible){
                                                                //recorder.color = "transparent";
                                                                recorder.color = "#1a000000";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                            }
                                                        }

                                                        onClicked: {
                                                            // 点击恢复历史对话
                                                            //msgPane.onStopReply();
                                                            //mychat.isReplying = false;
                                                            if(!selectDialogueRectangle.visible){
                                                                var currentSessionId = msgPane.getCurrentSessionId()
                                                                if(String(model.sessionID) === String(currentSessionId)){
                                                                    //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
                                                                    //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
                                                                    if(historyPage.winMode === 2){
                                                                        msgPane.onShowPage(1)
                                                                        return
                                                                    }else if(historyPage.winMode === 3){
                                                                        if(historyPage.showPageNum === 5 || historyPage.showPageNum === 6 || historyPage.showPageNum === 7){
                                                                            msgPane.onShowPage(4)
                                                                        }
                                                                        return
                                                                    }else{
                                                                        return
                                                                    }
                                                                }
                                                                if(!ButtonState.isTaskMode && mychat.isReplying){
                                                                    root.showCommonTip("qrc:/icon/icon/newBuiltTips.svg", qsTr("Generating answer, please try again after completion"))
                                                                    return
                                                                }else{
                                                                    //msgPane.onStopReply();
                                                                    //mychat.isReplying = false;
                                                                    //console.log("跳转会话" + model.sessionID)
                                                                    var winMode = msgPane.getCurWinMode()
                                                                    //console.log("当前模式:" + winMode)
                                                                    //在切换到主界面前根据当前是否在运行会议助手进行会议助手后台进度条显示
                                                                    //msgPane.sigSetMeetingBackBar(msgPane.isMeetingRunning)
                                                                    if(winMode === 2){
                                                                        // 侧边栏模式页面切换
                                                                        msgPane.onShowPage(1)
                                                                    }else if(winMode === 3){
                                                                        // 窗口模式页面切换
                                                                        msgPane.onShowPage(4)
                                                                    }
                                                                    // 用于触发点击跳转对话时默认选中当前
                                                                    if(parseInt(model.sessionID) === historyPage.currentSessionId){
                                                                        return
                                                                    }else{
                                                                        model.currentSessionId = true
                                                                        for (var i = 0; i < filteredListModel.count; ++i) {
                                                                            if(historyPage.currentSessionId === parseInt(filteredListModel.get(i).sessionID)){
                                                                                filteredListModel.get(i).currentSessionId = false
                                                                                break
                                                                            }
                                                                        }
                                                                    }
                                                                    // 恢复session逻辑待编写
                                                                    //msgPane.closeSession()
                                                                    model.isReplying = false
                                                                    model.hasUnread = false
                                                                    // 模式同步
                                                                    var effectiveModeType = msgPane.getSessionModeType(model.sessionID)
                                                                    if (effectiveModeType === undefined || effectiveModeType === null) {
                                                                        effectiveModeType = model.modeType
                                                                    }
                                                                    if (effectiveModeType === 1 && !ButtonState.isTaskMode) {
                                                                        ButtonState.isTaskMode = true
                                                                        ButtonState.chatMode = 1
                                                                        mychat.initTaskModeUI()
                                                                    } else if (effectiveModeType !== 1 && ButtonState.isTaskMode) {
                                                                        ButtonState.isTaskMode = false
                                                                        ButtonState.chatMode = 0
                                                                        if(mychat && mychat.isReplying) {
                                                                            msgPane.deactivateTaskSession()
                                                                        } else {
                                                                            msgPane.closeSession()
                                                                        }
                                                                        mychat.initChatModeUI()
                                                                    }
                                                                    if (mychat && mychat.currentTaskTabIndex !== 0) {
                                                                        mychat.setTaskTab(0, "chat")
                                                                    }
                                                                    msgPane.loadHistoryBySessionId(model.sessionID);
                                                                    // 切换对话时，清空输入框内容
                                                                    root.onClearMessageField()
                                                                    // 切换对话时，初始化朗读
                                                                    root.onInitReadingIndex()
                                                                }
                                                            }
                                                        }
                                                    }

                                                    Row {
                                                        id: recorderRow
                                                        Accessible.name: "kylin-aiassistant_HistoryPage_recorderRow"
                                                        width: parent.width
                                                        height: dialogueTitleMetrics.implicitHeight + 10
                                                        spacing: 8
                                                        leftPadding: 8
                                                        rightPadding: 8
                                                        anchors.verticalCenter: parent.verticalCenter

                                                        // 选择会话按钮
                                                        Rectangle {
                                                            id: selectDialogueRectangle
                                                            Accessible.name: "kylin-aiassistant_HistoryPage_selectDialogueRectangle"
                                                            objectName: "myComponentId"
                                                            width: selectDialogueCheckBox.width// + selectDialogueRow.spacing * 2
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            height: dialogueTitleMetrics.implicitHeight + 10
                                                            color: "transparent"
                                                            visible: {
                                                                if(!editButton.visible){
                                                                    return true
                                                                }else{
                                                                    return false
                                                                }
                                                            }

                                                            Row {
                                                                id: selectDialogueRow
                                                                Accessible.name: "kylin-aiassistant_HistoryPage_selectDialogueRow"
                                                                anchors.centerIn: parent
                                                                spacing: 0

                                                                CheckBox {
                                                                    id: selectDialogueCheckBox
                                                                    Accessible.name: "kylin-aiassistant_HistoryPage_selectDialogueCheckBox"
                                                                    anchors.verticalCenter: parent.verticalCenter
                                                                    width: 16
                                                                    //text: model.sessionID
                                                                    //background: Rectangle {
                                                                        //color: "blue"
                                                                    //}
                                                                    checked: model.checked
                                                                    onCheckedChanged: {
                                                                        filteredListModel.setProperty(index, "checked", checked)
                                                                        if(checked){
                                                                            recorder.color = "#0d262626"
                                                                            deleteButtonText.color = "#F53F3F"
                                                                            //selectDialogueRectangle.color = "#0d262626"
                                                                            // 用于修改单选框全选后，全选按钮状态
                                                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                                                if(filteredListModel.get(i).checked === false){
                                                                                    selectAllCheckBox.checked = false
                                                                                    break;
                                                                                }
                                                                                selectAllCheckBox.checked = true
                                                                            }
                                                                        }else{
                                                                            //是否有记录被选中，有记录被选中，则删除按钮显示红色
                                                                            var chooseFlag = false
                                                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                                                if(filteredListModel.get(i).checked === true){
                                                                                    chooseFlag = true
                                                                                }else{
                                                                                }
                                                                            }
                                                                            if(chooseFlag){
                                                                                deleteButtonText.color = "#F53F3F"
                                                                            }else{
                                                                                deleteButtonText.color = "#4df53f3f"
                                                                            }
                                                                            recorder.color = "transparent"
                                                                            selectDialogueRectangle.color = "transparent"
                                                                            selectAllCheckBox.checked = false
                                                                            historyPage.selectAllButtonChecked = false
                                                                        }
                                                                    }
                                                                    indicator: Item {
                                                                        id: selectDialogueCheckBoxItem
                                                                        Accessible.name: "kylin-aiassistant_HistoryPage_selectDialogueCheckBoxItem"
                                                                        anchors.verticalCenter: parent.verticalCenter
                                                                        width: 16
                                                                        height: 16

                                                                        Image {
                                                                            id: selectDialogueCheckBoxImage
                                                                            Accessible.name: "kylin-aiassistant_HistoryPage_selectDialogueCheckBoxImage"
                                                                            source: selectDialogueCheckBox.checked ? "qrc:/icon/icon/sidebar/CheckBox-default.svg" : "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                                                        }
                                                                    }

                                                                }
                                                            }
                                                        }

//                                                        Image {
//                                                            id: dialogueImage
//                                                            Accessible.name: "kylin-aiassistant_HistoryPage_dialogueImage"
//                                                            width:16
//                                                            height: 16
//                                                            source: "qrc:/icon/icon/sidebar/dialogue_black.svg"
//                                                            anchors.verticalCenter: parent.verticalCenter
//                                                        }
                                                        Rectangle {
                                                            id: dialogueTypeIconRectangle
                                                            width: 20
                                                            height: 20
                                                            color: "transparent"
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            Image {
                                                                id: dialogueTypeIcon
                                                                anchors.centerIn: parent
                                                                width: 16; height: 16
                                                                visible: !(model.isReplying !== undefined && model.isReplying)
                                                                source: {
                                                                    if (model.modeType === 1) {
                                                                        if (model.currentSessionId) {
                                                                            return "qrc:/icon/icon/Claw-white.svg"
                                                                        } else {
                                                                            return "qrc:/icon/icon/Claw.svg"
                                                                        }
                                                                    } else {
                                                                        if (model.currentSessionId) {
                                                                            return "qrc:/icon/icon/chat-white.svg"
                                                                        } else {
                                                                            return "qrc:/icon/icon/chat-black.svg"
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            Image {
                                                                id: dialogueTypeLoadingIcon
                                                                anchors.centerIn: parent
                                                                width: 16; height: 16
                                                                source: "qrc:/icon/ukui--ondevice-loading.svg"
                                                                visible: model.isReplying !== undefined && model.isReplying
                                                            }
                                                            RotationAnimation {
                                                                target: dialogueTypeLoadingIcon
                                                                from: 0
                                                                to: 360
                                                                duration: 1000
                                                                loops: Animation.Infinite
                                                                easing.type: Easing.Linear
                                                                running: dialogueTypeLoadingIcon.visible
                                                            }
                                                            Rectangle {
                                                                width: 8; height: 8; radius: 4
                                                                color: "red"
                                                                visible: (model.hasUnread !== undefined && model.hasUnread) && !(model.isReplying !== undefined && model.isReplying)
                                                                anchors.top: dialogueTypeIcon.top
                                                                anchors.right: dialogueTypeIcon.right
                                                                anchors.topMargin: -4
                                                                anchors.rightMargin: -4
                                                            }
                                                        }
                                                        Label {
                                                            id: dialogueTitle
                                                            Accessible.name: "kylin-aiassistant_HistoryPage_dialogueTitle"
                                                            height: dialogueTitle.implicitHeight
                                                            width: parent.width - (!selectDialogueRectangle.visible ? 0 : selectDialogueRectangle.width)
                                                                   - recorderRow.leftPadding
                                                                   - (dialogueListViewItem.spacingRetBool === 1 ? spacingRet.width : 0 ) - (dialogueListViewItem.deleteDialogueImageRetBool === 1 ? deleteDialogueImageRet.width : 0 )
                                                                   - recorderRow.rightPadding //+ spacingRet.width + deleteDialogueImageRet.width
                                                                   + (historyPageListRet.isEdit === 0 ? (spacingRet.width + deleteDialogueImageRet.width) : 0)
                                                            text: model.msgHead? model.msgHead + "："　+ getElidedText(model.message) : getElidedText(model.message)
                                                            elide: Text.ElideRight
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        Label {
                                                            id: dialogueTitleMetrics
                                                            Accessible.name: "kylin-aiassistant_HistoryPage_dialogueTitleMetrics"
                                                            visible: false
                                                            text: "用于计算汉字高度"
                                                            elide: Text.ElideRight
                                                        }
                                                    }
                                                }
                                                // 用于做spacing区域，可以做悬浮效果
                                                Rectangle {
                                                    id: spacingRet
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_spacingRet"
                                                    width: 4
                                                    height: dialogueTitleMetrics.implicitHeight + 10
                                                    color: "transparent"
                                                    visible: true
                                                    MouseArea {
                                                        id: spacingRetMouseArea
                                                        Accessible.name: "kylin-aiassistant_HistoryPage_spacingRetMouseArea"
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        onEntered: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "#26000000";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                //deleteDialogueImageRet.visible = true
                                                                //spacingRet.visible = true
                                                                dialogueListViewItem.spacingRetBool = 1
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 1
                                                                deleteDialogueImage.visible = true
                                                                deleteDialogueImageRet.color = "#26000000"
                                                            }
                                                            if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                recorderRet.color = "transparent"
                                                            }
                                                        }
                                                        onExited: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "transparent";
                                                                dialogueTitle.color = "#262626";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                //deleteDialogueImageRet.visible = false
                                                                //spacingRet.visible = false
                                                                dialogueListViewItem.spacingRetBool = 0
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 0
                                                                deleteDialogueImage.visible = false
                                                                deleteDialogueImageRet.color = "transparent"
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    recorderRet.color = "#16000000"
                                                                }
                                                            }
                                                        }
                                                        onPressed: {
    //                                                        if(!selectDialogueRectangle.visible){
    //                                                            recorder.color = "#2C73C8";
    //                                                            dialogueTitle.color = "#FFFFFF";
    //                                                            dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_white.svg"
    //                                                        }
                                                        }
                                                        onReleased: {
    //                                                        if(!selectDialogueRectangle.visible){
    //                                                            recorder.color = "transparent";
    //                                                            dialogueTitle.color = "#262626";
    //                                                            dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
    //                                                        }
                                                        }


                                                    }
                                                }

                                                Rectangle {
                                                    id: deleteDialogueImageRet
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_deleteDialogueImageRet"
                                                    width: 36
                                                    height: dialogueTitleMetrics.implicitHeight + 10
                                                    radius: 4
                                                    visible: true
                                                    //color: "#0d262626"
                                                    color: "transparent"
                                                    //color:"blue"
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    Image {
                                                        id: deleteDialogueImage
                                                        Accessible.name: "kylin-aiassistant_HistoryPage_deleteDialogueImage"
                                                        visible: false
                                                        source: "qrc:/icon/icon/delete.svg"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        anchors.horizontalCenter: parent.horizontalCenter
                                                    }
                                                    // 不显示，用于动态修改取消删除后的样式
                                                    TextArea{
                                                        visible: false
                                                        text: model.onReleasedFlag

                                                        background:Item {
                                                        }

                                                        onTextChanged: {
                                                            if(model.onReleasedFlag === true){
                                                            }else{
                                                                //recorder.color = "transparent";
                                                                //用于触发默认选中当前对话
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    model.currentSessionId = true
                                                                    recorderRet.color = "#16000000";
                                                                    recorder.color = "transparent";
                                                                }else{
                                                                    recorder.color = "transparent";
                                                                }

                                                                dialogueTitle.color = "#262626";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                deleteDialogueImageRet.color = "transparent"
                                                                //deleteDialogueImageRet.visible = false
                                                                //spacingRet.visible = false
                                                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                                                                dialogueListViewItem.spacingRetBool = 0
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 0
                                                                deleteDialogueImage.visible = false
                                                            }
                                                        }
                                                    }
                                                    // 不显示，//用于触发默认选中当前对话
                                                    TextArea{
                                                        visible: false
                                                        text: model.currentSessionId

                                                        background:Item {
                                                        }

                                                        onTextChanged: {
                                                            if(model.currentSessionId === true){
                                                                model.currentSessionId = true
                                                                recorderRet.color = "#16000000"
                                                            }else{
                                                                model.currentSessionId = false
                                                                recorderRet.color = "transparent"
                                                            }
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: deleteDialogueImageRetMouseArea
                                                        Accessible.name: "kylin-aiassistant_HistoryPage_deleteDialogueImageRetMouseArea"
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        //propagateComposedEvents: true
                                                        onEntered: {
                                                            if(!selectDialogueRectangle.visible){
                                                                recorder.color = "#26000000";
                                                                dialogueTitle.color = "#000000";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                //deleteDialogueImageRet.visible = true
                                                                deleteDialogueImageRet.color = "#E93C3C"
                                                                //spacingRet.visible = true
                                                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                                                                dialogueListViewItem.spacingRetBool = 1
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 1
                                                                deleteDialogueImage.visible = true
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    recorderRet.color = "transparent";
                                                                }
                                                            }
                                                        }
                                                        onExited: {
                                                            if(!selectDialogueRectangle.visible && model.onReleasedFlag === false){
                                                                recorder.color = "transparent";
                                                                dialogueTitle.color = "#262626";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
                                                                deleteDialogueImageRet.color = "transparent"
                                                                //deleteDialogueImageRet.visible = false
                                                                //spacingRet.visible = false
                                                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                                                                dialogueListViewItem.spacingRetBool = 0
                                                                dialogueListViewItem.deleteDialogueImageRetBool = 0
                                                                deleteDialogueImage.visible = false
                                                                if(historyPage.currentSessionId === parseInt(model.sessionID)){
                                                                    recorderRet.color = "#16000000";
                                                                }
                                                            }
                                                        }
                                                        onPressed: {
                                                            if(!selectDialogueRectangle.visible){
                                                                //recorder.color = "#2C73C8";
                                                                //dialogueTitle.color = "#FFFFFF";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_white.svg"
                                                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                                                                deleteDialogueImageRet.color = "#C43232"
                                                                root.onCloseCommand()
                                                            }
                                                        }
                                                        onReleased: {
                                                            if(!selectDialogueRectangle.visible){
                                                                model.onReleasedFlag = true
                                                                //recorder.color = "transparent";
                                                                //dialogueTitle.color = "#262626";
                                                                //dialogueImage.source = "qrc:/icon/icon/sidebar/dialogue_black.svg"
    //                                                            deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
    //                                                            deleteDialogueImageRet.color = "#0d262626"
                                                            }
                                                        }

                                                        onClicked: {
                                                            if(deleteDialogueImageRet.visible){
                                                                //console.log("deleteButton click")
                                                                var selectedOptions = [];
                                                                // 获取选中的历史记录
                                                                selectedOptions.push(model.sessionID);
                                                                // 没有选中的历史记录返回,不做操作
                                                                if(selectedOptions.length < 1){
                                                                    return
                                                                }
                                                                //console.log("Selected options:", JSON.stringify(selectedOptions));
                                                                // 调用后端删除数据库内记录接口
                                                                msgPane.initDeleteLayout(selectedOptions, 1)
                                                                //console.log("Selected option:", selectedOptions)
                                                            }
                                                        }
                                                    }
                                                }

                                            }

                                        }
                                    }

                                    Connections
                                    {
                                        target: msgPane
                                        onLoadDialogueList: function (list)
                                        {
                                            dialogueListModel.clear();
                                            for(var i = 0; i < list.length; ++i){
                                                try{
                                                    var item = JSON.parse(list[i]);
                                                    item["checked"] = false;
                                                    item["onReleasedFlag"] = false
                                                    item["currentSessionId"] = false
                                                    dialogueListModel.append(item);
                                                }catch(e){
                                                    console.log("历史对话记录加载异常第" + i + "条,详情为:" + list[i])
                                                }
                                            }
                                            // 初始化过滤空
                                            filterList("");
                                            // 强制隐藏并显示 ListView 来刷新
                                            dialogueListView.visible = false;
                                            dialogueListView.visible = true;

                                        }

                                        onSessionReplyStatusChanged: function(sessionId, isReplying, hasUnread) {
                                            var found = false
                                            for (var i = 0; i < dialogueListModel.count; ++i) {
                                                if (dialogueListModel.get(i).sessionID === sessionId) {
                                                    found = true
                                                    dialogueListModel.setProperty(i, "isReplying", isReplying)
                                                    dialogueListModel.setProperty(i, "hasUnread", hasUnread ? 1 : 0)
                                                }
                                            }
                                            for (var j = 0; j < filteredListModel.count; ++j) {
                                                if (filteredListModel.get(j).sessionID === sessionId) {
                                                    found = true
                                                    filteredListModel.setProperty(j, "isReplying", isReplying)
                                                    filteredListModel.setProperty(j, "hasUnread", hasUnread ? 1 : 0)
                                                }
                                            }
                                            if (!found) {
                                                msgPane.getDialogueList()
                                            }
                                        }

                                        // 删除页面数据
                                        onDeleteDialogueList:function (deleteList){
                                            //console.log("deleteList:" + deleteList);
                                            // 删除逻辑
                                            for(var k = 0; k < deleteList.length; k++){
                                                for (var j = 0; j < filteredListModel.count; j++) {
                                                    if (filteredListModel.get(j).sessionID === deleteList[k]) {
                                                        filteredListModel.remove(j)
                                                        break
                                                    }
                                                }
                                                for (var l = 0; l < dialogueListModel.count; l++) {
                                                    if (dialogueListModel.get(l).sessionID === deleteList[k]) {
                                                        dialogueListModel.remove(l)
                                                        break
                                                    }
                                                }
                                            }
                                            // 取消删除后的选中状态
                                            for (var i = 0; i < filteredListModel.count; i++) {
                                                filteredListModel.get(i).checked = false
                                            }
                                            //判断如果全部删除后，则恢复编辑按钮，全选按钮等初始状态
                                            if(dialogueListModel.count === 0){
                                                selectAllButton.visible = false
                                                cancelButton.visible = false
                                                deleteButton.visible = false
                                                editButton.visible = true
                                                springRet.visible = false
                                                historyPageListRet.isEdit = 0
                                                selectAllCheckBox.checked = false
                                            }
                                            //删除数据后，删除按钮展示黑色
                                            deleteButtonText.color = "#4df53f3f"
                                        }
                                        onModifyDialogueListFlag:function (sessionID){
                                            for (var j = 0; j < filteredListModel.count; j++) {
                                                if (filteredListModel.get(j).sessionID === sessionID) {
                                                    var item = filteredListModel.get(j);
                                                    item.onReleasedFlag = false;
                                                    //dialogueListView.model.set(j, item); // 更新模型
                                                    //filteredListModel.get(j).onReleasedFlag = true
                                                    break
                                                }
                                            }
                                        }
                                        onChangeWinModeToChat:function(winMode){
                                            historyPage.winMode = winMode
                                            timer.running = true
                                        }
                                        // 更新SessionId
                                        onChangeSessionId:function(sessionId){
                                            historyPage.currentSessionId = sessionId
                                        }

                                        onShowPage:function (num){
                                            historyPage.showPageNum = num
                                        }

                                        // 窗口模式下新建对话取消默认对话的选中
                                        onUpdateDialogueList:function() {
                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                if(filteredListModel.get(i).currentSessionId){
                                                    filteredListModel.get(i).currentSessionId = false
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }


                        Rectangle{
                            id: bottomButtonRet
                            Accessible.name: "kylin-aiassistant_HistoryPage_bottomButtonLayout"
                            width: parent.width// - historyPageColumn.leftPadding - historyPageColumn.rightPadding
                            height: 36
                            // 底部按钮
                            Row{
                                id: bottomButtonLayout
                                Accessible.name: "kylin-aiassistant_HistoryPage_bottomButtonLayout"
                                width: parent.width// - historyPageColumn.leftPadding - historyPageColumn.rightPadding
                                height: 36
                                rightPadding: 6
                                leftPadding: 6
                                spacing: 6
                                // 编辑按钮
                                Rectangle {
                                    id: editButton
                                    Accessible.name: "kylin-aiassistant_HistoryPage_editButton"
                                    width: editButtonImage.implicitWidth + editButtonText.implicitWidth + editButtonRow.spacing
                                    height: 36
                                    color: "transparent"
                                    visible: true
                                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                    Row {
                                        id: editButtonRow
                                        Accessible.name: "kylin-aiassistant_HistoryPage_editButtonRow"
                                        anchors.centerIn: parent
                                        spacing: 8
                                        width: parent.width
                                        height: parent.height
                                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                        Image {
                                            id: editButtonImage
                                            Accessible.name: "kylin-aiassistant_HistoryPage_editButtonImage"
                                            width: 14
                                            height: 14
                                            source: "qrc:/icon/icon/view-list-symbolic.svg"
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Label {
                                            id: editButtonText
                                            Accessible.name: "kylin-aiassistant_HistoryPage_editButtonText"
                                            text: qsTr("Choose")
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        // 按钮三态
                                        onEntered: {
                                            editButtonText.color = "#3489EE";
                                            editButtonImage.source = "qrc:/icon/icon/view-list-symbolic-hover.svg"
                                        }
                                        onExited: {
                                            editButtonText.color = "#d9000000";
                                            editButtonImage.source = "qrc:/icon/icon/view-list-symbolic.svg"
                                        }
                                        onPressed: {
                                            editButtonText.color = "#2C73C8";
                                            editButtonImage.source = "qrc:/icon/icon/view-list-symbolic-click.svg"
                                        }
                                        onReleased: {
                                            editButtonText.color = "#d9000000";
                                            editButtonImage.source = "qrc:/icon/icon/view-list-symbolic.svg"
                                        }
                                        onClicked: {
                                            //console.log("editButton click")
                                            //selectAllButtonChecked.set(0, { "boolValue": false })
                                            historyPage.selectAllButtonChecked = false
                                            editButton.visible = false
                                            selectAllButton.visible = true
                                            cancelButton.visible = true
                                            deleteButton.visible = true
                                            springRet.visible = true
                                            historyPageListRet.isEdit = 1

                                            //清除当前活动对话的背景色
                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                if(filteredListModel.get(i).currentSessionId === true){
                                                    filteredListModel.get(i).currentSessionId = false
                                                    break
                                                }
                                            }
                                            root.onCloseCommand()
                                        }
                                    }
                                }


                                // 全选按钮
                                Rectangle {
                                    id: selectAllButton
                                    Accessible.name: "kylin-aiassistant_HistoryPage_selectAllButton"
                                    width: selectAllRow.width//selectAllCheckBox.implicitWidth + selectAllRow.spacing * 2
                                    height: 36
                                    color: "transparent"
                                    visible: false

                                    Row {
                                        id: selectAllRow
                                        Accessible.name: "kylin-aiassistant_HistoryPage_selectAllRow"
                                        anchors.centerIn: parent
                                        spacing: 6

                                        CheckBox {
                                            id: selectAllCheckBox
                                            Accessible.name: "kylin-aiassistant_HistoryPage_selectAllCheckBox"
                                            checked: historyPage.selectAllButtonChecked
                                            Label {
                                                id:selectAllCheckBoxLabel
                                                Accessible.name: "kylin-aiassistant_HistoryPage_selectAllCheckBoxLabel"
                                                anchors.left: selectAllCheckBox.indicator.right
                                                anchors.leftMargin: 8
                                                anchors.verticalCenter: selectAllCheckBox.verticalCenter
                                                width: (winMode === 2 ? selectAllCheckBoxLabelMetrics.width : 34)
                                                text: qsTr("Select all")
                                                wrapMode: Text.NoWrap
                                                elide: Text.ElideRight
                                                color: "black"// 设置文本颜色
                                            }
                                            // 悬浮提示框
                                            ToolTip {
                                                id: selectAllCheckBoxLabelTooltip
                                                Accessible.name: "kylin-aiassistant_Chat_selectAllCheckBoxLabelTooltip"
                                                visible: false   // 默认不显示
                                                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                                                text: qsTr("Select all")
                                            }
                                            // 用于测量文本宽度的 TextMetrics 用于显示长度超出时的计算
                                            Label {
                                                id: selectAllCheckBoxLabelMetrics
                                                Accessible.name: "kylin-aiassistant_Chat_selectAllCheckBoxLabelMetrics"
                                                text: qsTr("Select all")
                                                visible: false
                                            }
                                            onCheckedChanged: historyPage.selectAllButtonChecked = checked//selectAllButtonChecked.set(0, { "boolValue": checked })//selectAllButtonChecked.setProperty(index, "boolValue", checked)
                                            indicator: Item {
                                                id: selectAllCheckBoxItem
                                                Accessible.name: "kylin-aiassistant_HistoryPage_selectAllCheckBoxItem"
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 16
                                                height: 16

                                                Image {
                                                    id: selectAllCheckBoxImage
                                                    Accessible.name: "kylin-aiassistant_HistoryPage_selectAllCheckBoxImage"
                                                    source: selectAllCheckBox.checked ? "qrc:/icon/icon/sidebar/CheckBox-default.svg" : "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                                }
                                            }
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        // 按钮三态
                                        onEntered: {
                                            // 当用户名宽度大于组件显示省略号时，悬浮显示提示
                                            if (selectAllCheckBoxLabelMetrics.width > selectAllCheckBoxLabel.width) {
                                                selectAllCheckBoxLabelTooltip.visible = true
                                            } else {
                                                selectAllCheckBoxLabelTooltip.visible = false
                                            }
                                            //selectAllCheckBoxLabel.color = "#3489EE";
                                            if(selectAllCheckBox.checked){
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-hover.svg";
                                            }else{
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-hover.svg";
                                            }
                                        }
                                        onExited: {
                                            selectAllCheckBoxLabelTooltip.visible = false
                                            //selectAllCheckBoxLabel.color = "#d9000000";
                                            console.log("selectAllCheckBox.checked----------",selectAllCheckBox.checked)
                                            if(selectAllCheckBox.checked){
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                                            }else{
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                                            }
                                        }
                                        onPressed: {
                                            //selectAllCheckBoxLabel.color = "#2C73C8";
                                            if(selectAllCheckBox.checked){
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg";
                                            }else{
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-click.svg";
                                            }
                                        }
                                        onReleased: {
                                            //selectAllCheckBoxLabel.color = "#d9000000";
                                            if(selectAllCheckBox.checked){
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                                            }else{
                                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                                            }
                                        }
                                        onClicked: {
                                            //console.log("selectAllButton click")
                                            // 用于判断历史记录复选框是否是全选状态 true:没有全部选中  false:全部选中
                                            var checkedFlag = false
                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                var itemModel = filteredListModel.get(i);
                                                if(!itemModel.checked){
                                                    checkedFlag = true
                                                    break
                                                }
                                            }
                                            // 若没有全部选中则全选;若已经全选,点击按钮则全不选
                                            for (var j = 0; j < filteredListModel.count; ++j) {
                                                var item = filteredListModel.get(j);
                                                if(checkedFlag){
                                                    item.checked = true;
                                                }else{
                                                    item.checked = false;
                                                    deleteButtonText.color = "#4df53f3f"
                                                }
                                            }
                                            // 修改全选按钮勾选状态
                                            if(checkedFlag){
                                                //selectAllButtonChecked.set(0, { "boolValue": true })
                                                historyPage.selectAllButtonChecked = true
                                                selectAllCheckBox.checked = true
                                            }else{
                                                //selectAllButtonChecked.set(0, { "boolValue": false })
                                                historyPage.selectAllButtonChecked = false
                                                selectAllCheckBox.checked = false
                                            }
                                        }
                                    }
                                }


                                // 底部中间弹簧
                                Rectangle {
                                    id: springRet
                                    height: 36
                                    width: bottomButtonRet.width - selectAllButton.width - cancelButton.width - deleteButton.width
                                           - bottomButtonLayout.spacing * 3 - bottomButtonLayout.rightPadding - bottomButtonLayout.leftPadding
                                    color: "transparent"
                                    visible: false
                                }

                                // 取消按钮
                                Rectangle {
                                    id: cancelButton
                                    Accessible.name: "kylin-aiassistant_HistoryPage_cancelButton"
                                    width: cancelButtonText.width// + 2
                                    height: 36
                                    color: "transparent"
                                    visible: false
                                    Row {
                                        id: cancelButtonRow
                                        Accessible.name: "kylin-aiassistant_HistoryPage_cancelButtonRow"
                                        anchors.centerIn: parent
                                        Label {
                                            id: cancelButtonText
                                            Accessible.name: "kylin-aiassistant_HistoryPage_cancelButtonText"
                                            width: (winMode === 2 ? cancelButtonTextMetrics.width : (cancelButtonTextMetrics.width >= 34 ? 34 : cancelButtonTextMetrics.width))
                                            text: qsTr("Cancel")
                                            wrapMode: Text.NoWrap
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        // 悬浮提示框
                                        ToolTip {
                                            id: cancelButtonTextTooltip
                                            Accessible.name: "kylin-aiassistant_Chat_cancelButtonTextTooltip"
                                            visible: false   // 默认不显示
                                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                                            text: qsTr("Cancel")
                                        }
                                        // 用于测量文本宽度的 TextMetrics 用于显示长度超出时的计算
                                        Label {
                                            id: cancelButtonTextMetrics
                                            Accessible.name: "kylin-aiassistant_Chat_cancelButtonTextMetrics"
                                            text: qsTr("Cancel")
                                            visible: false
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        // 按钮三态
                                        onEntered: {
                                            // 当用户名宽度大于组件显示省略号时，悬浮显示提示
                                            if (cancelButtonTextMetrics.width > cancelButtonText.width) {
                                                cancelButtonTextTooltip.visible = true
                                            } else {
                                                cancelButtonTextTooltip.visible = false
                                            }
                                            cancelButtonText.color = "#3489EE";
                                        }
                                        onExited: {
                                            cancelButtonTextTooltip.visible = false
                                            cancelButtonText.color = "#d9000000";
                                        }
                                        onPressed: {
                                            cancelButtonText.color = "#2C73C8";
                                        }
                                        onReleased: {
                                            cancelButtonText.color = "#d9000000";
                                        }
                                        onClicked: {
                                            //console.log("cancelButton click")
                                            for (var i = 0; i < filteredListModel.count; ++i) {
                                                var item = filteredListModel.get(i);
                                                item.checked = false;
                                            }
                                            selectAllCheckBox.checked = false
                                            selectAllButton.visible = false
                                            cancelButton.visible = false
                                            deleteButton.visible = false
                                            editButton.visible = true
                                            springRet.visible = false
                                            historyPageListRet.isEdit = 0
                                            deleteButtonText.color = "#4df53f3f"

                                            for (var j = 0; j < filteredListModel.count; ++j) {
                                                if(historyPage.currentSessionId === parseInt(filteredListModel.get(j).sessionID)){
                                                    filteredListModel.get(j).currentSessionId = true
                                                    break
                                                }
                                            }
                                        }
                                    }
                                }

                                // 删除按钮
                                Rectangle {
                                    id: deleteButton
                                    Accessible.name: "kylin-aiassistant_HistoryPage_deleteButton"
                                    width: deleteButtonText.width// + 2
                                    height: 36
                                    color: "transparent"
                                    visible: false
                                    Row {
                                        id: deleteButtonRow
                                        Accessible.name: "kylin-aiassistant_HistoryPage_deleteButtonRow"
                                        anchors.centerIn: parent
                                        Label {
                                            id: deleteButtonText
                                            Accessible.name: "kylin-aiassistant_HistoryPage_deleteButtonText"
                                            width: (winMode === 2 ? deleteButtonTextMetrics.width : (cancelButtonTextMetrics.width >= 34 ? 34 : cancelButtonTextMetrics.width))
                                            text: qsTr("Delete")
                                            wrapMode: Text.NoWrap
                                            color: "#4dF53F3F"
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        // 悬浮提示框
                                        ToolTip {
                                            id: deleteButtonTextTooltip
                                            Accessible.name: "kylin-aiassistant_Chat_deleteButtonTextTooltip"
                                            visible: false   // 默认不显示
                                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                                            text: qsTr("Delete")
                                        }
                                        // 用于测量文本宽度的 TextMetrics 用于显示长度超出时的计算
                                        Label {
                                            id: deleteButtonTextMetrics
                                            Accessible.name: "kylin-aiassistant_Chat_deleteButtonTextMetrics"
                                            text: qsTr("Delete")
                                            visible: false
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        // 按钮三态
                                        onEntered: {
                                            // 当用户名宽度大于组件显示省略号时，悬浮显示提示
                                            if (deleteButtonTextMetrics.width > deleteButtonText.width) {
                                                deleteButtonTextTooltip.visible = true
                                            } else {
                                                deleteButtonTextTooltip.visible = false
                                            }
                                            if(String(deleteButtonText.color) !== String("#4df53f3f")){
                                                deleteButtonText.color = "#E93C3C";
                                            }
                                        }
                                        onExited: {
                                            deleteButtonTextTooltip.visible = false
                                            if(String(deleteButtonText.color) !== String("#4df53f3f")){
                                                deleteButtonText.color = "#F53F3F";
                                            }
                                        }
                                        onPressed: {
                                            if(String(deleteButtonText.color) !== String("#4df53f3f")){
                                                deleteButtonText.color = "#C63432";
                                            }
                                        }
                                        onReleased: {
                                            if(String(deleteButtonText.color) !== String("#4df53f3f")){
                                                deleteButtonText.color = "#F53F3F";
                                            }
                                        }
                                        onClicked: {
                                            //console.log("deleteChooseButton click")
                                            var selectedOptions = [];
                                            // 获取所有选中的历史记录
                                            for (var i = 0; i < filteredListModel.count; i++) {
                                                if (filteredListModel.get(i).checked) {
                                                    selectedOptions.push(filteredListModel.get(i).sessionID);
                                                }
                                            }
                                            // 没有选中的历史记录返回,不做操作
                                            if(selectedOptions.length < 1){
                                                return
                                            }
                                            //console.log("Selected options:", JSON.stringify(selectedOptions));
                                            // 调用后端删除数据库内记录接口
                                            msgPane.initDeleteLayout(selectedOptions, 1)
                                            //console.log("Selected choose options:", selectedOptions)
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

    Rectangle {
        width: historyPage.width
        height: 1
        color: "#0d262626"
        anchors.left: historyPage.left
        anchors.bottom: historyPage.bottom
        anchors.bottomMargin: 36
    }

    ColumnLayout {
        anchors.fill: parent
        visible: dialogueListModel.count === 0

        Rectangle {
            color: "#FFFFFF"
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: parent.height / 2

                Rectangle {
                    id: noDialoguePageRectangle
                    Accessible.name: "kylin-aiassistant_HistoryPage_noDialoguePageRectangle"
                    width: noDialogueImage.width
                    height: noDialogueImage.height + noDialogueText.implicitHeight + noDialoguePageColumn.spacing
                    Layout.alignment: Qt.AlignCenter
                    color: "transparent"

                    Column {
                        id: noDialoguePageColumn
                        Accessible.name: "kylin-aiassistant_HistoryPage_noDialoguePageColumn"
                        anchors.centerIn: parent
                        spacing: 8

                        Image {
                            id: noDialogueImage
                            Accessible.name: "kylin-aiassistant_HistoryPage_noDialogueImage"
                            width: 128
                            height: 128
                            source: "qrc:/icon/icon/sidebar/noDialogue.png"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Label {
                            id: noDialogueText
                            Accessible.name: "kylin-aiassistant_HistoryPage_noDialogueText"
                            text: qsTr("No chat history yet.")
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#2626268C"
                        }
                    }

                }
            }

        }

    }
}
