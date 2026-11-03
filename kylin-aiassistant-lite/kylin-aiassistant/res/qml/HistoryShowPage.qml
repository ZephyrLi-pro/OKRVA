import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Rectangle {
    id: historyShowPageRet
    visible: true
    color: "#FFFFFF"

    property int chooseIndex: 1  // 1全部 or 2收藏
    property bool selectAllButtonChecked: false  // 全选按钮是否全选
    property bool isEditFlag: false
    property string showPageSearchText: ""
    property int editCount: 0
    property int checkedCollectedCount: 0 // 选中的已收藏的数量
    property int padingSize: 24
    property int collectedCount: 0 // 收藏历史对话的条数
    property string noDialogueTextStr: "" // 无历史对话,无搜索结果提示语
    property int topHeight: 0 // 页面全部元素加起来的高度
    property int delegateHeight: 0  // 列表每一项的元素高度
    property int editUnCollectedCount: 0 // 要操作收藏列表的未收藏数量

    signal openHistoryPageBySessionIdAndChatType(string sessionId, int chatType, int modeType)

    onSelectAllButtonCheckedChanged:{
        changeAllCheckboxStatus()
    }

    onIsEditFlagChanged: {
        changeAllCheckboxStatus()
    }

    // 存储和管理历史会话的数据
    ListModel {
        id: dialogueListModel
    }

    ListModel {
        id: tempListModel
    }

    ListModel {
        id: filteredListModel
    }

    // 初始化历史对话页面
    function initHistoryShowPage(){
        chooseIndex = 1
        isEditFlag = false
        selectAllButtonChecked = false
        showPageSearchText = ""
        initSearchTextArea()
        msgPane.getDialogueList()
        topHeight = (historyTitleArea.height + (root.winMode === 2 ? searchTextAreaSideMode.height : 0) +
                     (sumCountTipRet.visible ? sumCountTipRet.implicitHeight + 10 : 0) +
                      historyOperateArea.height + 10 +
                     (historyListView.isContentOverflowing ? historyListView.height : historyShowPageRet.delegateHeight * filteredListModel.count))
    }

    // 初始化搜索框
    function initSearchTextArea(){
        // 恢复窗口模式搜索框
        historyTitleArea.initSearchArea()
        // 恢复侧边模式搜索框
        searchTextAreaSideMode.initSearchTextAreaRet()
    }

    // 取消全选
    function cancelAllChoose(){
        historyShowPageRet.selectAllButtonChecked = false
        // 使用 setProperty 方法更新 model，确保所有项目（包括未渲染的）都被更新
        for (var i = 0; i < filteredListModel.count; ++i) {
            if(filteredListModel.get(i).dateDelegateText === ""){
                filteredListModel.setProperty(i, "checked", false)
            }
        }
        // 更新统计数量
        updateEditCount()
    }
    
    // 更新选中项目的统计数量（遍历所有项目，包括未渲染的虚拟化项目）
    function updateEditCount(){
        var totalCount = 0
        var collectedCountSum = 0
        for (var i = 0; i < filteredListModel.count; ++i) {
            var item = filteredListModel.get(i);
            // 跳过日期分隔项
            if(item.dateDelegateText !== ""){
                continue
            }
            if(item.checked === true){
                totalCount++
                if(item.isCollect === 1){
                    collectedCountSum++
                }
            }
        }
        editCount = totalCount
        editUnCollectedCount = totalCount - collectedCountSum < 0 ? 0 : totalCount - collectedCountSum
        // 确保编辑标志正确
        if(totalCount > 0){
            isEditFlag = true
        }else{
            isEditFlag = false
        }
    }

    // 根据选中的checkbox更新全选按钮的状态
    function changeAllCheckboxStatus(){
        if(isEditFlag){
            if(selectAllButtonChecked){
                historyOperateArea.changeSelectAllCheckBoxImageSoucre("qrc:/icon/icon/sidebar/CheckBox-default.svg")
            }else{
                historyOperateArea.changeSelectAllCheckBoxImageSoucre("qrc:/icon/icon/part-check.svg")
            }
        }else{
            historyOperateArea.changeSelectAllCheckBoxImageSoucre("qrc:/icon/icon/sidebar/noCheck-default.svg")
        }
    }

    // 日期分类判断函数
    function getDateCategory(dateString) {
        // 解析输入日期字符串（支持"yyyy/MM/dd hh:mm"格式）
        const dateParts = dateString.split(' ')[0].split('/');
        if (dateParts.length !== 3) {
            console.error("无效的日期格式，请使用'yyyy/MM/dd hh:mm'");
            return "未知";
        }

        // 构建日期对象（仅关注年月日，忽略时分秒）
        const year = parseInt(dateParts[0]);
        const month = parseInt(dateParts[1]) - 1; // 月份从0开始
        const day = parseInt(dateParts[2]);
        const targetDate = new Date(year, month, day);

        // 获取今天的日期（重置时分秒为0）
        const today = new Date();
        today.setHours(0, 0, 0, 0);

        // 计算目标日期与今天的天数差（毫秒转换为天）
        const timeDiff = today.getTime() - targetDate.getTime();
        const dayDiff = Math.floor(timeDiff / (1000 * 60 * 60 * 24));

        // 根据天数差进行分类
        if (dayDiff <= 0) {
            return 1//"今天"
        } else if (dayDiff >= 1 && dayDiff <= 7) {
            return 2//"最近7天"
        } else if (dayDiff >= 8 && dayDiff <= 30) {
            return 3//"最近30天"
        } else if (dayDiff >= 31 && dayDiff <= 365) {
            return 4//"最近一年"
        } else {
            return 5//"更多"
        }
    }

    // 过滤函数，用于过滤历史记录列表
    function filterList(searchText) {
        //console.log("搜索历史输入的内容:"+searchText)
        filteredListModel.clear()
        tempListModel.clear()
        let collectCount = 0
        var dateTypeMap = new Map();
        dateTypeMap.set(1, 0)
        dateTypeMap.set(2, 0)
        dateTypeMap.set(3, 0)
        dateTypeMap.set(4, 0)
        dateTypeMap.set(5, 0)
        for (var i = 0; i < dialogueListModel.count; ++i) {
            var originalItem = dialogueListModel.get(i);

            if(originalItem.isCollect === 1){
                collectCount++
            }

            var newItem = deepCopy(originalItem);

            var msg = ""
            if(newItem.msgHead !== undefined && newItem.msgHead !== ""){
                msg = newItem.msgHead + "：" + newItem.message
            }else{
                msg = newItem.message
            }
            if(chooseIndex === 2 && newItem.isCollect !== 1){
                continue
            }

            if(searchText === null || searchText === ""){
                newItem.message = msg

                var num = getDateCategory(newItem.operateTime)
                newItem.dateType = num
                dateTypeMap.set(num, 1)

                tempListModel.append(newItem);
            }else{
                if(newItem.message.includes(searchText) || newItem.msgHead.includes(searchText) || msg.includes(searchText)){
                    var escapedSearchText = escapeRegExp(searchText);
                    let reg = new RegExp(escapedSearchText, "g")

                    msg = msg.replace(reg, "<b>" + searchText + "</b>")
                    newItem.message = msg

                    var num = getDateCategory(newItem.operateTime)
                    newItem.dateType = num
                    dateTypeMap.set(num, 1)

                    tempListModel.append(newItem);
                }
            }
        }

        collectedCount = collectCount

        if(dateTypeMap.get(1) === 1){
            let item = {
                "message": "",
                "msgHead": "",
                "isCollect": 0,
                "chatType": 0,
                "operateTime": "",
                "dateType": 0,
                "checked": true,
                "dateDelegateText": qsTr("Today")
            }
            filteredListModel.append(item)
            for(var i = 0; i < tempListModel.count; i++){
                if(tempListModel.get(i).dateType === 1){
                    filteredListModel.append(tempListModel.get(i))
                }
            }
        }

        if(dateTypeMap.get(2) === 1){
            let item = {
                "message": "",
                "msgHead": "",
                "isCollect": 0,
                "chatType": 0,
                "operateTime": "",
                "dateType": 0,
                "checked": true,
                "dateDelegateText": qsTr("The last 7 days")
            }
            filteredListModel.append(item)
            for(var i = 0; i < tempListModel.count; i++){
                if(tempListModel.get(i).dateType === 2){
                    filteredListModel.append(tempListModel.get(i))
                }
            }
        }

        if(dateTypeMap.get(3) === 1){
            let item = {
                "message": "",
                "msgHead": "",
                "isCollect": 0,
                "chatType": 0,
                "operateTime": "",
                "dateType": 0,
                "checked": true,
                "dateDelegateText": qsTr("The last 30 days")
            }
            filteredListModel.append(item)
            for(var i = 0; i < tempListModel.count; i++){
                if(tempListModel.get(i).dateType === 3){
                    filteredListModel.append(tempListModel.get(i))
                }
            }
        }

        if(dateTypeMap.get(4) === 1){
            let item = {
                "message": "",
                "msgHead": "",
                "isCollect": 0,
                "chatType": 0,
                "operateTime": "",
                "dateType": 0,
                "checked": true,
                "dateDelegateText": qsTr("The past year")
            }
            filteredListModel.append(item)
            for(var i = 0; i < tempListModel.count; i++){
                if(tempListModel.get(i).dateType === 4){
                    filteredListModel.append(tempListModel.get(i))
                }
            }
        }

        if(dateTypeMap.get(5) === 1){
            let item = {
                "message": "",
                "msgHead": "",
                "isCollect": 0,
                "chatType": 0,
                "operateTime": "",
                "dateType": 0,
                "checked": true,
                "dateDelegateText": qsTr("Earlier")
            }
            filteredListModel.append(item)
            for(var i = 0; i < tempListModel.count; i++){
                if(tempListModel.get(i).dateType === 5){
                    filteredListModel.append(tempListModel.get(i))
                }
            }
        }

        // 搜索结果提示语
        if(dialogueListModel.count === 0){
            noDialogueTextStr = qsTr("No chat history yet.")
        }else{
            if(chooseIndex === 1){
                if(filteredListModel.count === 0){
                    noDialogueTextStr = qsTr("No search yet.")
                }
            }else{
                if(collectedCount === 0){
                    noDialogueTextStr = qsTr("No chat history yet.")
                }else{
                    noDialogueTextStr = qsTr("No search yet.")
                }
            }
        }
    }

    // 正则特殊字符转义函数：将特殊字符转为字面量
    function escapeRegExp(str) {
        // 匹配所有正则特殊字符（. * + ? ^ $ { } ( ) | [ ] \），并在前面加反斜杠转义
        return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    // 深拷贝工具函数（处理复杂类型）
    function deepCopy(obj) {
        if (obj === null || typeof obj !== "object") {
            return obj; // 基本类型直接返回
        }
        var copy = {};
        for (var prop in obj) {
            copy[prop] = deepCopy(obj[prop]); // 递归复制子属性
        }
        return copy;
    }

    // 批量修改收藏状态
    function batchCancelCollected(selectedOptions, operateType){
        var isCollect = (operateType === 1 ? 1 : 0)
        for(var j = 0; j < selectedOptions.length; j++){
            for(var i = 0; i < dialogueListModel.count; i++){
                if(dialogueListModel.get(i).sessionID === selectedOptions[j]){
                    dialogueListModel.setProperty(i, "isCollect", isCollect)
                }
            }
        }

        msgPane.updateIsCollectBySessionIdList(selectedOptions, isCollect)
        filterList(historyShowPageRet.showPageSearchText)

        historyOperateArea.changeSelectAllCheckBox(false)
        isEditFlag = false
        changeAllCheckboxStatus()
    }

    ColumnLayout{
        id: historyShowPageColumnLayout
        width: Math.min(historyShowPageRet.width, root.maxWidth)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        //Layout.alignment: Qt.AlignCenter

        spacing: 0

        // 全部，收藏，搜索区域
        HistoryTitleArea{
            id: historyTitleArea
            Layout.fillWidth: true
            titleChooseIndex: chooseIndex
            bottomPadding: 10
            onInitHistoryOperatePage: function(){
                selectAllButtonChecked = false
                isEditFlag = false
            }
        }

        // 搜索输入框
        SearchTextArea{
            id: searchTextAreaSideMode
            searchTextAreaRetHeight: 40
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            visible: root.winMode === 2
            onSearchTextAreaTextChanged: function(changeText){
                historyShowPageRet.showPageSearchText = changeText
                filterList(changeText)
            }
        }

        // 数量提示tip 大于等于490条时显示，可关闭
        Rectangle{
            id: sumCountTipRet
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 10
            implicitHeight: sumCountTipRow.implicitHeight + 16
            visible: dialogueListModel.count >= 490 && chooseIndex === 1
            color: Qt.rgba(255/255, 125/255, 0/255, 0.1)
            radius: 8
            Row{
                id: sumCountTipRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 8
                rightPadding: 8
                Image {
                    id: sumCountTipImage
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/yellow-warning.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: sumCountTipLabel
                    width: sumCountTipRow.width - sumCountTipImage.width - closeCountTipImageRet.width -
                                   sumCountTipRow.leftPadding - sumCountTipRow.rightPadding
                    leftPadding: 8
                    wrapMode: Text.WordWrap
                    text: qsTr("History (") + dialogueListModel.count + qsTr("/500) limit nearly reached; earliest conversations will be auto-overwritten.")
                    color: "#FF7D00"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle{
                    id: closeCountTipImageRet
                    width: 36
                    height: 36
                    anchors.verticalCenter: parent.verticalCenter
                    color: "transparent"
                    radius: 8
                    Image {
                        id: closeCountTipImage
                        width: 10
                        height: 10
                        source: "qrc:/icon/icon/yellow-close.svg"
                        anchors.centerIn: parent
                    }
                    // 悬浮提示框
                    ToolTip {
                        id: closeTooltip
                        visible: closeCountTipImageRetMouseArea.containsMouse
                        delay: Qt.styleHints.mousePressAndHoldInterval
                        text: qsTr("Close")
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }
                    MouseArea{
                        id: closeCountTipImageRetMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            closeCountTipImageRet.color = "#F27700"
                            closeCountTipImage.source = "qrc:/icon/icon/white-close.svg"
                        }
                        onExited: {
                            closeCountTipImageRet.color = "transparent"
                            closeCountTipImage.source = "qrc:/icon/icon/yellow-close.svg"
                        }
                        onPressed: {
                            closeCountTipImageRet.color = "#CC6400"
                            closeCountTipImage.source = "qrc:/icon/icon/white-close.svg"
                        }
                        onReleased: {
                            closeCountTipImageRet.color = "transparent"
                            closeCountTipImage.source = "qrc:/icon/icon/yellow-close.svg"
                        }
                        onClicked: {
                            sumCountTipRet.visible = false
                        }
                    }
                }
            }
        }

        // 历史对话标题操作选择，取消，删除区域
        HistoryOperateArea{
            id: historyOperateArea
            Layout.fillWidth: true
            Layout.bottomMargin: 10
            operateChooseIndex: chooseIndex
            totelOperateCollectedCount: collectedCount
            maxOperateCollectedCount: 50
            editOperateCollectedCount: editUnCollectedCount
            checkedCollectEditOperatorCount: checkedCollectedCount
            editOperatorCount: editCount
            onBatchCancelCollect: function(selectedOptions, operateType){
                batchCancelCollected(selectedOptions, operateType)
            }
        }

        // 记录区域
        ScrollView {
            id: historyScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: filteredListModel.count !== 0
            ScrollBar.horizontal.visible : false
            ScrollBar.vertical.visible : false
            //Layout.bottomMargin: 8  // 下边距为8px

            ListView {
                id: historyListView
                Accessible.name: "kylin-aiassistant_historyListView"
                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                boundsMovement: Flickable.StopAtBounds // 防止过度滚动
                cacheBuffer: 5000  // 设置缓存的控件数量，避免滚动时刷新
                model: filteredListModel
                spacing: 0

                property bool isContentOverflowing: false // 内容是否超出容器高度

                Connections {
                    target: filteredListModel
                    onRowsInserted: {
                        // 当有新项目插入时，可以在这里处理
                        historyOperateArea.changeSelectAllCheckBox(false)
                    }
                }
                // 监听model的rowsRemoved信号
                Connections {
                    target: filteredListModel
                    onRowsRemoved: {
                        // 当有项目被移除时，可以在这里处理
                        historyOperateArea.changeSelectAllCheckBox(false)
                    }
                }
                onContentHeightChanged: {

                    // 判断内容是否超出容器高度，超出显示滚动条
                    isContentOverflowing = contentHeight > height
                    if(isContentOverflowing){
                        historyScrollView.ScrollBar.vertical.visible = true
                    }else{
                        historyScrollView.ScrollBar.vertical.visible = false
                    }
                    topHeight = (historyTitleArea.height + (root.winMode === 2 ? searchTextAreaSideMode.height : 0) +
                                 (sumCountTipRet.visible ? sumCountTipRet.implicitHeight + 10 : 0) +
                                  historyOperateArea.height + 10 +
                                 (isContentOverflowing ? height : historyShowPageRet.delegateHeight * filteredListModel.count))
                }
                onHeightChanged: {

                    // 判断内容是否超出容器高度，超出显示滚动条
                    isContentOverflowing = contentHeight > height
                    if(isContentOverflowing){
                        historyScrollView.ScrollBar.vertical.visible = true
                    }else{
                        historyScrollView.ScrollBar.vertical.visible = false
                    }
                    topHeight = (historyTitleArea.height + (root.winMode === 2 ? searchTextAreaSideMode.height : 0) +
                                 (sumCountTipRet.visible ? sumCountTipRet.implicitHeight + 10 : 0) +
                                 historyOperateArea.height + 10 +
                                 (isContentOverflowing ? height : historyShowPageRet.delegateHeight * filteredListModel.count))
                }

                delegate: HistoryItemDelegate {
                    id: historyItemDelegate
                    width: historyListView.width
                    msgHead: model.msgHead || ""
                    message: model.message || ""
                    isCollect: model.isCollect
                    isEdit: isEditFlag
                    currentIndex: model.index
                    chatType: model.chatType
                    modeType: model.modeType !== undefined ? model.modeType : 0
                    operateTime: model.operateTime
                    dateDelegateText: model.dateDelegateText
                    sessionId: model.sessionID || ""
                    totelCollectedCount: collectedCount
                    maxCollectedCount: 50
                    checkedStatus: model.checked
                    isReplying: model.isReplying !== undefined ? model.isReplying : false
                    hasUnread: model.hasUnread !== undefined ? model.hasUnread : false
                    onChangeEditFlag: function(flag){
                        isEditFlag = flag
                        changeAllCheckboxStatus()
                    }
                    onChangeEditCount:function(count){
                        editCount = count
                        var collectedCountSum = 0
                        for(var j = 0; j < filteredListModel.count ; j++){
                            if(filteredListModel.get(j).checked === true && filteredListModel.get(j).isCollect === 1){
                                collectedCountSum ++
                            }
                        }
                        editUnCollectedCount = editCount - collectedCountSum < 0 ? 0 : editCount - collectedCountSum
                    }
                    onChangeCheckedCollectCount: function(count){
                        checkedCollectedCount = count
                    }
                    onChangeSelectAllCheckBoxChecked: function(flag){
                        //selectAllCheckBox.checked = flag
                        historyOperateArea.changeSelectAllCheckBox(flag)
                    }
                    onOpenHistoryBySessionId: function(sessionId, chatType, modeType){
                        console.log("onOpenHistoryBySessionId==",sessionId,"---------",chatType)
                        openHistoryPageBySessionIdAndChatType(sessionId, chatType, modeType)
                    }

                    onChangeIsCollect: function(index){
                        var isCollectNum = 0
                        var currentIsCollect = filteredListModel.get(index).isCollect
                        var currentSessionID = filteredListModel.get(index).sessionID
                        if(currentIsCollect === 0){
                            // 全部页面只更新当前项，用 setProperty 避免整表刷新导致列表跳动
                            filteredListModel.setProperty(index, "isCollect", 1)
                            isCollectNum = 1
                        }else{
                            filteredListModel.setProperty(index, "isCollect", 0)
                            isCollectNum = 0

                            if(historyShowPageRet.chooseIndex === 2){
                                filteredListModel.remove(index)
                            }
                        }

                        msgPane.updateIsCollectBySessionId(currentSessionID, isCollectNum)

                        var collectCount = 0
                        for (var i = 0; i < filteredListModel.count; ++i) {
                            if(filteredListModel.get(i).dateDelegateText !== ""){
                                continue
                            }

                            if(filteredListModel.get(i).checked === true){
                                if(filteredListModel.get(i).isCollect === 1){
                                    collectCount++
                                }
                            }
                        }
                        checkedCollectedCount = collectCount

                        // 用于更新原始数据的收藏状态，不去查询数据库
                        for(var i = 0; i < dialogueListModel.count ; i++){
                            if(currentSessionID === dialogueListModel.get(i).sessionID){
                                if(currentIsCollect === 0){
                                    dialogueListModel.setProperty(i, "isCollect", 1)
                                }else{
                                    dialogueListModel.setProperty(i, "isCollect", 0)
                                }
                                break
                            }
                        }

                        var collectedCountSum = 0
                        for(var j = 0; j < dialogueListModel.count ; j++){
                            if(dialogueListModel.get(j).isCollect === 1){
                                collectedCountSum ++
                            }
                        }
                        collectedCount = collectedCountSum

                        // 全部页面数据不需要刷新列表，不调用 filterList，避免列表重建导致滚动位置跳动
                        // 收藏页面取消收藏已通过 remove(index) 更新，也无需再 filterList
                        // 仅当需要根据搜索条件重新筛选时才调用 filterList（此处不适用）
                    }
                    onChangeItemDelegateHeight: function(itemDelegateHeight){
                        historyShowPageRet.delegateHeight = itemDelegateHeight
                    }
                }
            }
        }

        // 底部padding区域
        Rectangle{
            id: paddindRet
            Layout.fillWidth: true
            height: maxTipLabel.height + 8
            color: "transparent"
            visible: filteredListModel.count > 0
        }

        // 无记录区域
        Rectangle {
            id: noModelRet
            color: "#FFFFFF"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: dialogueListModel.count === 0 || filteredListModel.count === 0

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
                            text: noDialogueTextStr
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#2626268C"
                        }
                    }
                }
            }
        }

        Connections{
            target: msgPane
            onLoadDialogueList: function (list){
                dialogueListModel.clear();
                for(var i = 0; i < list.length; ++i){
                    try{
                        var item = JSON.parse(list[i]);
                        item["dateType"] = 1;
                        item["dateDelegateText"] = "";
                        dialogueListModel.append(item);
                    }catch(e){
                        console.log("历史对话记录加载异常第" + i + "条,详情为:" + list[i])
                    }
                }
                // 初始化过滤空
                filterList("");
            }

            // 删除页面数据
            onDeleteDialogueList:function (deleteList){
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
                    filteredListModel.setProperty(i, "checked", false)
                }

                //删除数据后，取消，删除按钮隐藏
                isEditFlag = false

                filterList(historyShowPageRet.showPageSearchText)
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

            // 外部导入的定时任务（kybot cron）执行结果落库后，刷新历史列表以显示新项
            onScheduledSessionCreated: function(sessionUuid) {
                msgPane.getDialogueList()
            }
        }
    }

    Label{
        id: maxTipLabel
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: chooseIndex === 1 ? "- " + qsTr("We will keep the last 500 historical conversations.") + " -" :
                                  "- " + qsTr("Up to 50 historical dialogues can be collected.") + " -"
        wrapMode: Text.WordWrap
        font.pointSize: globals.fontsize - 2
        font.family: root.fontFamily
        visible: filteredListModel.count > 0
        color: Qt.rgba(158/255, 158/255, 158/255, 1)
        topPadding: 8
        bottomPadding: 0
        anchors.top: parent.top
        anchors.topMargin: topHeight
    }
}
