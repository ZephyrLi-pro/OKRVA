import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

Item {
    id: dialogueListViewItem
    height: itemHeight

    property int padingSize: 24
    property int itemHeight: dialogueTitleMetrics.implicitHeight > 36 ? dialogueTitleMetrics.implicitHeight + 10 : 36
    property bool isEdit: false
    property int currentIndex: 0
    property int chatType: 0
    property int modeType: 0
    property string operateTime: ""
    property string dateDelegateText: ""
    property string msgHead: ""
    property string message: ""
    property int isCollect: 0
    property string sessionId: ""
    property int totelCollectedCount: 0
    property int maxCollectedCount: 0
    property bool checkedStatus: false
    property bool isReplying: false
    property bool hasUnread: false

    signal changeEditFlag(bool flag) // 更新取消，删除按钮的显示
    signal changeIsCollect(int currentIndex)
    signal changeEditCount(int count)
    signal changeSelectAllCheckBoxChecked(bool flag)
    signal openHistoryBySessionId(string sessionId, int chatType, int modeType)
    signal changeItemDelegateHeight(var itemHeight)
    signal changeCheckedCollectCount(int count)

    property bool isHovered: false

    onHeightChanged: {
        changeItemDelegateHeight(height)
    }

    onCheckedStatusChanged: {
        if(checkedStatus){
            deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
        }else{
            deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
        }
    }

    function getElidedText(inputText) {
        var trimText = trimStringAtEnd(inputText)
        trimText = replaceString(trimText)
        //trimText = trimText.replace(/<\/?.+?\/?>/g,'');
        trimText = trimText.replace(/&quot;/g,'"');
        return trimText
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

    // 今天，最近七天，最近30天显示区域
    Rectangle{
        id: dateDelegateRet
        width: parent.width
        height: itemHeight
        anchors.right:  parent.right
        anchors.rightMargin: padingSize
        anchors.left: parent.left
        anchors.leftMargin: padingSize + 8
        visible: dateDelegateText !== ""
        Label{
            id: dateDelegateLabel
            height: dateDelegateLabel.implicitHeight
            width: dateDelegateLabel.implicitWidth
            text: dateDelegateText
            color: Qt.rgba(0, 0, 0, 0.6)
            anchors.verticalCenter: parent.verticalCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // 记录列表显示区域
    Rectangle{
        id: recorderRet
        width: parent.width
        height: itemHeight
        radius: 4
        color: "transparent"
        anchors.right:  parent.right
        anchors.rightMargin: padingSize
        anchors.left: parent.left
        anchors.leftMargin: padingSize
        visible: dateDelegateText === ""
        Row{
            id: recorderRowItem
            Accessible.name: "kylin-aiassistant_HistoryPage_recorderRowItem"
            width: parent.width
            height: itemHeight
            spacing: 0
            Rectangle {
                id: recorder
                width: parent.width - recorderRowItem.spacing
                height: itemHeight
                radius: 4
                color: "transparent"
                MouseArea {
                    id: recorderMouseArea
                    Accessible.name: "kylin-aiassistant_HistoryPage_recorderMouseArea"
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        isHovered = true
                        deleteDialogueImageRet.visible = true
                        collectImageRet.visible = true
                        if(checkedStatus){
                            return
                        }
                        recorderRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                    }
                    onExited: {
                        isHovered = false
                        deleteDialogueImageRet.visible = false
                        collectImageRet.visible = false
                        if(checkedStatus){
                            return
                        }
                        recorderRet.color = "transparent";
                    }
                    onPressed: {
                        if(checkedStatus){
                            return
                        }
                        recorderRet.color = "#33000000"
                    }
                    onReleased: {
                        if(checkedStatus){
                            return
                        }
                        recorderRet.color = "#1a000000"
                    }
                    onClicked: {
                        if(checkedStatus){
                            return
                        }
                        openHistoryBySessionId(sessionId, chatType, modeType)
                    }
                }

                Row {
                    id: recorderRow
                    width: parent.width - padingSize * 2
                    height: itemHeight
                    spacing: 8
                    leftPadding: 8
                    //rightPadding: 8
                    anchors.verticalCenter: parent.verticalCenter

                    // 选择会话CheckBox按钮
                    Rectangle {
                        id: selectDialogueRectangle
                        objectName: "myComponentId"
                        width: selectDialogueCheckBox.width
                        anchors.verticalCenter: parent.verticalCenter
                        height: selectDialogueCheckBox.height
                        color: "transparent"
                        visible: true

                        MouseArea{
                            id: selectDialogueRectangleMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                            acceptedButtons: Qt.RightButton | Qt.LeftButton
                            z: 1
                            onEntered: {
                                isHovered = true
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-hover.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-hover.svg"
                                }
                                recorderRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                                deleteDialogueImageRet.visible = true
                                collectImageRet.visible = true
                            }
                            onExited: {
                                isHovered = false
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                }
                                recorderRet.color = "transparent";
                                deleteDialogueImageRet.visible = false
                                collectImageRet.visible = false
                            }
                            onPressed: {
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-click.svg"
                                }
                                mouse.accepted = false
                            }
                            onReleased: {
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                                }
                                mouse.accepted = false
                            }
                            onClicked: {
                                mouse.accepted = false
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
                                height: 16
                                checked: model.checked
                                onCheckedChanged: {
                                    filteredListModel.setProperty(index, "checked", checked)

                                    if(dateDelegateText !== ""){
                                        return
                                    }

                                    if(checked){
                                        recorder.color = Qt.rgba(55/255, 140/255, 250/255, 1)
                                        selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                                        dialogueTitle.color = "#FFFFFF"
                                        // 用于修改单选框全选后，全选按钮状态,计数
                                        for (var i = 0; i < filteredListModel.count; ++i) {
                                            if(filteredListModel.get(i).dateDelegateText !== ""){
                                                continue
                                            }

                                            if(filteredListModel.get(i).checked === false){
                                                changeSelectAllCheckBoxChecked(false)
                                                //selectAllCheckBox.checked = false
                                                break;
                                            }
                                            changeSelectAllCheckBoxChecked(true)
                                            //selectAllCheckBox.checked = true
                                        }
                                        changeEditFlag(true)

                                        var count = 0
                                        var collectCount = 0
                                        for (var i = 0; i < filteredListModel.count; ++i) {
                                            if(filteredListModel.get(i).dateDelegateText !== ""){
                                                continue
                                            }

                                            if(filteredListModel.get(i).checked === true){
                                                count++
                                                if(filteredListModel.get(i).isCollect === 1){
                                                    collectCount++
                                                }
                                            }
                                        }
                                        changeEditCount(count)
                                        changeCheckedCollectCount(collectCount)
                                    }else{
                                        //是否有记录被选中，有记录被选中，则删除按钮显示红色
                                        var chooseFlag = false
                                        var count = 0
                                        var collectCount = 0
                                        for (var i = 0; i < filteredListModel.count; ++i) {
                                            if(filteredListModel.get(i).dateDelegateText !== ""){
                                                continue
                                            }
                                            if(filteredListModel.get(i).checked === true){
                                                chooseFlag = true
                                                count++
                                                if(filteredListModel.get(i).isCollect === 1){
                                                    collectCount++
                                                }
                                            }else{
                                            }
                                        }
                                        if(chooseFlag){
                                            changeEditFlag(true)
                                        }else{
                                            changeEditFlag(false)
                                        }
                                        changeEditCount(count)
                                        changeCheckedCollectCount(collectCount)
                                        selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                        recorder.color = "transparent"
                                        selectDialogueRectangle.color = "transparent"
                                        dialogueTitle.color = "#000000"
                                        changeSelectAllCheckBoxChecked(false)
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

                    // 对话类型图标
                    Rectangle {
                        id: dialogueTypeIconRectangle
                        objectName: "myComponentId"
                        width: 20
                        height: itemHeight
                        color: "transparent"
                        visible: true
                        Image{
                            id: dialogueTypeIcon
                            anchors.centerIn: parent
                            visible: !isReplying
                            source: {
                                if(modeType === 1){
                                    if(checkedStatus){
                                        return "qrc:/icon/icon/claw-white.svg"
                                    }else{
                                        return "qrc:/icon/icon/Claw.svg"
                                    }
                                }else if(chatType === 0){
                                    if(checkedStatus){
                                        return "qrc:/icon/icon/chat-white.svg"
                                    }else{
                                        return "qrc:/icon/icon/chat-black.svg"
                                    }
                                }else if(chatType === 1){
                                    if(checkedStatus){
                                        return "qrc:/icon/icon/chat-white.svg"
                                    }else{
                                        return "qrc:/icon/icon/chat-black.svg"
                                    }
                                }else if(chatType === 2){
                                    if(checkedStatus){
                                        return "qrc:/icon/icon/speech-white.svg"
                                    }else{
                                        return "qrc:/icon/icon/speech-black.svg"
                                    }
                                }else if(chatType === 4){
                                    if(checkedStatus){
                                        return "qrc:/icon/icon/chat-white.svg"
                                    }else{
                                        return "qrc:/icon/icon/chat-black.svg"
                                    }
                                }
                            }
                        }

                        Image {
                            id: dialogueTypeLoadingIcon
                            anchors.centerIn: parent
                            source: checkedStatus ? "qrc:/icon/ukui--ondevice-loading.svg" : "qrc:/icon/ukui--ondevice-loading-black.svg"
                            visible: isReplying
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

                        Image {
                            id: unreadDot
                            width: 4; height: 4
                            source: "qrc:/icon/icon/red_dot.svg"
                            visible: hasUnread && !isReplying
                            anchors.top: dialogueTypeIcon.top
                            anchors.right: dialogueTypeIcon.right
                            anchors.topMargin: 0
                            anchors.rightMargin: 0
                        }
                    }

                    Label {
                        id: dialogueTitle
                        height: dialogueTitle.implicitHeight
                        width: parent.width - (!selectDialogueRectangle.visible ? 0 : selectDialogueRectangle.width)
                               - (dialogueTypeIconRectangle.visible ? dialogueTypeIconRectangle.width : 0)
                               - recorderRow.leftPadding - recorderRow.rightPadding
                               - (dateLabel.visible ? dateLabel.width : 0)
                               - (collectImageRet.visible ? (collectImageRet.width) : 0)
                               - (deleteDialogueImageRet.visible ? (deleteDialogueImageRet.width) : 0)
                               + 10
                        text: getElidedText(message)
                        color: checkedStatus ? "#FFFFFF" : "#000000"
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        maximumLineCount: 1
                        // 悬浮提示框
                        ToolTip {
                            id: dialogueTitleTooltip
                            visible: dialogueTitleMouseArea.containsMouse &&
                                     dialogueTitleMertics.width > dialogueTitle.width
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: getElidedText(message)
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                        MouseArea {
                            id: dialogueTitleMouseArea
                            Accessible.name: "kylin-aiassistant_HistoryPage_recorderMouseArea"
                            anchors.fill: parent
                            hoverEnabled: true
                            //enabled: !isEdit
                            onEntered: {
                                isHovered = true
                                deleteDialogueImageRet.visible = true
                                collectImageRet.visible = true
                                if(checkedStatus){
                                    return
                                }
                                recorderRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onExited: {
                                isHovered = false
                                deleteDialogueImageRet.visible = false
                                collectImageRet.visible = false
                                if(checkedStatus){
                                    return
                                }
                                recorderRet.color = "transparent";
                            }
                            onPressed: {
                                if(checkedStatus){
                                    return
                                }
                                recorderRet.color = "#33000000"
                            }
                            onReleased: {
                                if(checkedStatus){
                                    return
                                }
                                recorderRet.color = "#1a000000"
                            }

                            onClicked: {
                                if(checkedStatus){
                                    return
                                }
                                openHistoryBySessionId(sessionId, chatType, modeType)
                            }
                        }
                    }

                    // 用于计算宽度，不显示
                    Label {
                        id: dialogueTitleMertics
                        height: dialogueTitle.implicitHeight
                        text: getElidedText(message)
                        anchors.verticalCenter: parent.verticalCenter
                        visible: false
                        maximumLineCount: 1
                    }

                    Label{
                        id: dateLabel
                        height: dialogueTitle.implicitHeight
                        //width: dateLabel.implicitWidth
                        text: operateTime
                        color: checkedStatus ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.35)
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        visible: root.winMode === 3 && !isHovered
                    }

                    Label {
                        id: dialogueTitleMetrics
                        visible: false
                        text: "用于计算汉字高度"
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    id: collectImageRet
                    width: 26
                    height: 26
                    radius: 4
                    color: "transparent"
                    visible: false
                    anchors.right: parent.right
                    anchors.rightMargin: marginLength
                    anchors.verticalCenter: parent.verticalCenter
                    property int marginLength: 40
                    opacity: ((totelCollectedCount >= maxCollectedCount && isCollect === 0) ? 0.5 : 1)
                    Image {
                        id: collectImage
                        visible: true
                        source: isCollect === 0 ? checkedStatus ? "qrc:/icon/icon/collect-white.svg" : "qrc:/icon/icon/collect-button.svg" : "qrc:/icon/icon/saved-button.svg"
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    // 悬浮提示框
                    ToolTip {
                        id: collectImageTooltip
                        visible: isCollect === 0 && totelCollectedCount < maxCollectedCount && collectImageRetMouseArea.containsMouse   // 默认不显示
                        delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                        text: qsTr("Collect")
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }
                    // 悬浮提示框
                    ToolTip {
                        id: unCollectImageTooltip
                        visible: isCollect !== 0 && collectImageRetMouseArea.containsMouse  // 默认不显示
                        delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                        text: qsTr("Unsave")
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }

                    // 悬浮提示框
                    ToolTip {
                        id: cannotCollectImageTooltip
                        visible: isCollect === 0 && totelCollectedCount >= maxCollectedCount && collectImageRetMouseArea.containsMouse   // 默认不显示
                        delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                        text: qsTr("The number of collections cannot exceed 50")
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }

                    MouseArea {
                        id: collectImageRetMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            isHovered = true
                            collectImageRet.visible = true
                            deleteDialogueImageRet.visible = true
                            recorderRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            if(totelCollectedCount >= maxCollectedCount && isCollect === 0){
                                return
                            }
                            if(isCollect === 1){
                                collectImage.source = "qrc:/icon/icon/collected-button-hover.svg"
                            }
                            collectImageRet.color = "#26000000"
                        }
                        onExited: {
                            isHovered = false
                            collectImageRet.visible = false
                            deleteDialogueImageRet.visible = false
                            recorderRet.color = "transparent"
                            if(totelCollectedCount >= maxCollectedCount && isCollect === 0){
                                return
                            }
                            if(isCollect === 1){
                                collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                            }
                            collectImageRet.color = "transparent"
                        }
                        onPressed: {
                            if(totelCollectedCount >= maxCollectedCount && isCollect === 0){
                                return
                            }
                            if(isCollect === 1){
                                collectImage.source = "qrc:/icon/icon/collected-button.svg"
                            }
                            collectImageRet.color = "#33000000"
                        }
                        onReleased: {
                            if(totelCollectedCount >= maxCollectedCount && isCollect === 0){
                                return
                            }
                            if(isCollect === 1){
                                if(checkedStatus){
                                    collectImage.source = "qrc:/icon/icon/collect-white.svg"
                                }else{
                                    collectImage.source = "qrc:/icon/icon/collect-button.svg"
                                }
                            }else{
                                collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                            }

                            collectImageRet.color = "transparent"
                        }
                        onClicked: {
                            if(totelCollectedCount >= maxCollectedCount && isCollect === 0){
                                return
                            }
                            changeIsCollect(currentIndex)
                        }
                    }
                }

                Rectangle {
                    id: deleteDialogueImageRet
                    width: 26
                    height: 26
                    radius: 4
                    color: "transparent"
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: false
                    Image {
                        id: deleteDialogueImage
                        source: checkedStatus ? "qrc:/icon/icon/delete-click.svg" : "qrc:/icon/icon/delete.svg"
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    // 悬浮提示框
                    ToolTip {
                        id: deleteDialogueImageTooltip
                        visible: false   // 默认不显示
                        delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                        text: qsTr("Delete")
                        property var maxWidth: 400
                        property var wrapMode: Text.WrapAnywhere
                    }

                    MouseArea {
                        id: deleteDialogueImageRetMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            isHovered = true
                            deleteDialogueImageRet.visible = true
                            collectImageRet.visible = true
                            recorderRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            if(checkedStatus){
                                deleteDialogueImageRet.color = "#26000000"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }else{
                                deleteDialogueImageRet.color = "#E93C3C"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                            deleteDialogueImageTooltip.visible = true
                        }
                        onExited: {
                            isHovered = false
                            deleteDialogueImageRet.visible = false
                            collectImageRet.visible = false
                            recorderRet.color = "transparent"
                            if(checkedStatus){
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }else{
                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                            }
                            deleteDialogueImageRet.color = "transparent"
                            deleteDialogueImageTooltip.visible = false
                        }
                        onPressed: {
                            deleteDialogueImageRet.color = "#33000000"
                            deleteDialogueImageTooltip.visible = false
                            if(checkedStatus){

                            }else{
                                deleteDialogueImageRet.color = "#C63432"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                        }
                        onReleased: {
                            deleteDialogueImageRet.color = "transparent"
                            deleteDialogueImageTooltip.visible = false
                            if(checkedStatus){

                            }else{
                                deleteDialogueImageRet.color = "transparent"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                        }

                        onClicked: {
                            //console.log("deleteButton click")
                            var selectedOptions = [];
                            // 获取选中的历史记录
                            selectedOptions.push(model.sessionID);
                            // 没有选中的历史记录返回,不做操作
                            if(selectedOptions.length < 1){
                                return
                            }
                            console.log("Selected options:", JSON.stringify(selectedOptions));
                            let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
                            if (dialogComponent.status === Component.Ready) {
                                let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
                                dialog.selectedOptions = selectedOptions
                                dialog.questionText = qsTr("Delete is irreversible. Continue?")
                                dialog.infoText = ""
                                dialog.confirmButtonText = qsTr("Delete")
                                dialog.cancelButtonText = qsTr("Cancel")
                                dialog.highlightConfirmButton = true
                                dialog.highlightCancelButton = false
                                dialog.showNoRemindCheckBox = false
                                if (dialog) {
                                    dialog.confirmed.connect(function(options, noRemindChecked) {
                                       msgPane.deleteSessionList(selectedOptions)
                                       dialog.destroy();
                                    });

                                    dialog.canceled.connect(function() {
                                       dialog.destroy();
                                    });

                                    dialog.showDialog();
                                } else {
                                    console.error("创建弹窗失败：", dialogComponent.errorString());
                                }
                            } else {
                                console.error("加载弹窗组件失败：", dialogComponent.errorString());
                            }
                        }
                    }
                }
            }
        }
    }
}
