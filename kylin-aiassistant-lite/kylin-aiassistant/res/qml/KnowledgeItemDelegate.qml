import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Item {
    id: knowledgeListViewItem
    height: itemHeight

    property int padingSize: 8
    property int itemHeight: knowledgeTitleMetrics.implicitHeight > 36 ? knowledgeTitleMetrics.implicitHeight + 10 : 36//knowledgeTitleMetrics.implicitHeight + 20

    property string fileName:  ""
    property string filePath:  ""
    property var fileIcon: null
    property int fileProgressNum: 10
    property string createTime:  ""
    property string fileSize:  ""
    property int fileStatus: 1 // 上传状态，1：成功，0：失败
    // 文件的上传状态，由后端数据传入
    property string fileStatusStr: ""
    property bool checked: false
    property bool isEdit: false
    property string uuid: ""
    property bool isDir: false
    property int currentIndex: 0
    property int totelCheckedCount: 0
    property bool doubleClickFlag: false
    property bool canOperateFlag: true
    property int itemCreatTimeWidth: 176
    property int itemFileSizeWidth: 96
    property bool itemClosePopupFlag: false

    signal changeEditFlag(bool flag)
    signal changeEditCount(int count)
    signal changeSelectAllCheckBoxChecked(bool flag)
    signal requestToLoadDir()
    signal openDeleteLayout(int type)
    signal moveFileItem(var filePaths)
    signal copyFileItem(var filePaths)
    signal deleteFileItem(var filePaths)
    signal renameFileItem(var filePaths)
    signal changeTransparentKnowledgeItemDelegate(var flag)
    signal changeItemCreatTimeWidth(var itemCreatTimeWidth)
    signal changeItemFileSizeWidth(var itemfileSizeWidth)

    onCheckedChanged: {
        if(checked){
            deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
        }else{
            deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
            recorderRet.isHover = false
        }
    }

    onItemClosePopupFlagChanged: {
        knowledgeOperatePopup.visible = false
    }

    KnowledgeOperatePopup{
        id: knowledgeOperatePopup
        isCanOperateFlag: isCanOperate
        onMoveFile: function(){
            moveFileItem(pushFilePaths())
        }
        onCopyFile: function(){
            copyFileItem(pushFilePaths())
        }
        onRenameFile:  function(){
            renameFileItem(pushFilePaths())
        }
        onDeleteFile: function(){
            deleteFileItem(pushFilePaths())
        }
        onVisibleChanged: {
            changeTransparentKnowledgeItemDelegate(visible)
            if(!visible){
                recorderRet.isHover = false
            }
        }
    }

    function pushFilePaths(){
        var filePaths = []
        for (var i = 0; i < knowledgeListModel.count; ++i) {
            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true){
                filePaths.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 260))
                continue
            }
        }
        if(filePaths.length === 0){
            filePaths.push(knowledgeListViewItem.filePath)
        }
        return filePaths
    }

    function changeItemChecked(){
        for (var i = 0; i < knowledgeListModel.count; ++i) {
            knowledgeListModel.setData(knowledgeListModel.index(i,0), false, 269)
        }
        knowledgeListModel.setData(knowledgeListModel.index(index,0), true, 269)
//        if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){

//        }else{
//            for (var i = 0; i < knowledgeListModel.count; ++i) {
//                knowledgeListModel.setData(knowledgeListModel.index(i,0), false, 269)
//            }
//            knowledgeListModel.setData(knowledgeListModel.index(index,0), true, 269)
//        }
    }

    function changeItemCheckedRightButton(){
        if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){

        }else{
            for (var i = 0; i < knowledgeListModel.count; ++i) {
                knowledgeListModel.setData(knowledgeListModel.index(i,0), false, 269)
            }
            knowledgeListModel.setData(knowledgeListModel.index(index,0), true, 269)
        }
    }

    Timer{
        id: clickTimer
        interval: 1000
        repeat: false   // 设置定时器只执行一次
        running: false   // 设置定时器启动
        onTriggered: {
            recorder.color = "transparent";
            //deleteDialogueImageRet.visible = false
            recorderRet.isHover = false
            deleteDialogueImageTooltip.visible = false
            deleteDialogueImageRet.color = "transparent"
            deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
        }
    }
    Timer{
        // 双击事件定时器
        id: doubleClickTimer
        interval: 300
        repeat: false   // 设置定时器只执行一次
        running: false   // 设置定时器启动
        onTriggered: {
            knowledgeListViewItem.doubleClickFlag = false
        }
    }

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
        property bool isHover: false

        Row{
            id: recorderRowItem
            width: parent.width
            height: itemHeight
            spacing: 0           
            Rectangle {
                id: recorder
                width: parent.width - recorderRowItem.spacing - 4
                height: itemHeight
                radius: 4
                color: "transparent"
                MouseArea {
                    id: recorderMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: true
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
                    propagateComposedEvents: true // 允许事件冒泡给父容器
                    onEntered: {                        
                        if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                            if(totelCheckedCount > 1){
                                recorderRet.isHover = false
                            }else{
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                                recorderRet.isHover = true
                            }
                            return
                        }else{
                            if(totelCheckedCount > 1){
                                recorderRet.isHover = false
                            }else{
                                recorderRet.isHover = true
                            }
                        }

                        recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                    }
                    onExited: {
                        if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                            if(!recorderMouseArea.containsMouse){
                                recorderRet.isHover = false
                            }
                            return
                        }
                        recorderRet.isHover = false

                        if(knowledgeOperatePopup.visible){
                            return
                        }

                        recorder.color = "transparent";
                        deleteDialogueImageRet.color = "transparent"
                        deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                    }
                    onPressed: {
                        if(isEdit){
                            return
                        }
                    }
                    onReleased: {
                        if(isEdit){
                            return
                        }
                    }
                    onClicked: {
                        doubleClickTimer.running = true
                        if(knowledgeListViewItem.doubleClickFlag){
                            return
                        }
                        knowledgeListViewItem.doubleClickFlag = true

                        if(mouse.button === Qt.LeftButton){
                            changeItemChecked()
                        }


                        if(mouse.button === Qt.RightButton){
                            knowledgeOperatePopup.showRenameFlag = true

                            var windowPos = recorderMouseArea.mapToItem(knowledgeListViewItem, mouse.x, mouse.y);

                            let posX = windowPos.x
                            let posY = windowPos.y
                            let isSidebarCloseFlag = root.isSidebarClose
                            let sideBarWidth = root.smallSideBarWidth
                            let rightCanUsedWidth = 0
                            if(isSidebarCloseFlag){
                                rightCanUsedWidth = ((root.width - root.maxWidth - sideBarWidth) / 2)
                            }else{
                                rightCanUsedWidth = ((root.width - root.maxWidth) / 2)
                            }

                            if(root.winMode === 3){
                                if(isSidebarCloseFlag){
                                    if(rightCanUsedWidth > 0){
                                        if(root.width - posX - rightCanUsedWidth < 260){
                                            posX = root.width - 260 - rightCanUsedWidth
                                        }
                                    }else{
                                        if(root.width - posX  < 260){
                                            posX = root.width - 260
                                        }
                                    }
                                }else{
                                    if(rightCanUsedWidth > sideBarWidth / 2){
                                        if(root.width - posX - rightCanUsedWidth + sideBarWidth < 260){
                                            posX = root.width - 260 - rightCanUsedWidth + sideBarWidth
                                        }
                                    }else{
                                        if(root.width - posX + rightCanUsedWidth < 260){
                                            posX = root.width - 260 + rightCanUsedWidth
                                        }
                                    }
                                }
                            }else{
                                if(isSidebarCloseFlag){
                                    if(root.width - posX < 260){
                                        posX = root.width - 260
                                    }
                                }else{
                                    if(root.width - posX + sideBarWidth < 260){
                                        posX = root.width - 260 + sideBarWidth
                                    }
                                }
                            }
                            knowledgeOperatePopup.x = posX

                            // 用于解决列表最下面的项右键popup显示超出屏幕问题
                            var windowPosRoot  = mapToItem(null, 0, 0)
                            // 169为popup弹窗的高度,直接使用knowledgeOperatePopup.height会偶现高度不对的问题，故写死  169 - 72  知识库改动0303
                            if(windowPosRoot.y + (169 - 72) + posY > root.height){
                                posY = posY - (169 - 72)
                            }
                            knowledgeOperatePopup.y = posY

                            changeItemCheckedRightButton()

                            var chooseCount = 0
                            for (var j = 0; j < knowledgeListModel.count; ++j) {
                                if(knowledgeListModel.data(knowledgeListModel.index(j,0), 269) === true){
                                    chooseCount ++
                                    if(chooseCount > 1){
                                        knowledgeOperatePopup.showRenameFlag = false
                                        break;
                                    }
                                    continue;
                                }
                            }
                            knowledgeOperatePopup.open()
                        }
                    }
                    onDoubleClicked: {
                        if(mouse.button === Qt.LeftButton){
                            if(isDir){
                                changeEditCount(0)
                                changeEditFlag(false)
                                requestToLoadDir()
                            }
                        }
                    }
                }

                Row {
                    id: recorderRow
                    width: parent.width - recorderRow.leftPadding
                    height: itemHeight
                    spacing: 0
                    leftPadding: 8
                    anchors.verticalCenter: parent.verticalCenter
                    // 选择会话CheckBox按钮
                    Rectangle {
                        id: selectDialogueRectangle
                        width: selectDialogueCheckBox.width
                        anchors.verticalCenter: parent.verticalCenter
                        height: selectDialogueCheckBox.height//itemHeight
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
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-hover.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-hover.svg"
                                }
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                                        recorderRet.isHover = true
                                    }
                                    return
                                }else{
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                }

                                recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onExited: {
                                if(selectDialogueCheckBox.checked === true){
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                                }else{
                                    selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                }
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(!recorderMouseArea.containsMouse){
                                        recorderRet.isHover = false
                                    }
                                    return
                                }
                                recorderRet.isHover = false

                                if(knowledgeOperatePopup.visible){
                                    return
                                }

                                recorder.color = "transparent";
                                deleteDialogueImageRet.color = "transparent"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
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
                            anchors.centerIn: parent
                            spacing: 0
                            width: 16
                            height: 16
                            CheckBox {
                                id: selectDialogueCheckBox
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                checked: model.checked

                                onCheckedChanged: {
                                    knowledgeListModel.setData(knowledgeListModel.index(index,0), checked, 269)
                                    if(checked){
                                        recorder.color = Qt.rgba(55/255, 140/255, 250/255, 1)
                                        dialogueTitle.color = "white"
                                        creatTimeLabel.color = "white"
                                        fileSizeLabel.color = "white"
                                        changeEditFlag(true)
                                        // 用于修改单选框全选后，全选按钮状态,计数
                                        for (var i = 0; i < knowledgeListModel.count; ++i) {
                                            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === false){
                                                changeSelectAllCheckBoxChecked(false)
                                                break;
                                            }
                                            changeSelectAllCheckBoxChecked(true)
                                        }
                                        var count = 0
                                        for (var i = 0; i < knowledgeListModel.count; ++i) {
                                            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true){
                                                count++
                                            }
                                        }
                                        changeEditCount(count)
                                        selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                                    }else{
                                        var chooseFlag = false
                                        var count = 0
                                        for (var i = 0; i < knowledgeListModel.count; ++i) {
                                            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true){
                                                chooseFlag = true
                                                count++
                                            }else{
                                            }
                                        }
                                        if(chooseFlag){
                                            changeEditFlag(true)
                                        }else{
                                            changeEditFlag(false)
                                        }
                                        changeEditCount(count)
                                        changeSelectAllCheckBoxChecked(false)
                                        recorder.color = "transparent"
                                        dialogueTitle.color = "#000000"
                                        creatTimeLabel.color = Qt.rgba(0, 0, 0, 0.35)
                                        fileSizeLabel.color = Qt.rgba(0, 0, 0, 0.35)
                                        selectDialogueRectangle.color = "transparent"
                                        selectDialogueCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                    }
                                }

                                indicator: Item {
                                    id: selectDialogueCheckBoxItem
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16

                                    Image {
                                        id: selectDialogueCheckBoxImage
                                        source: selectDialogueCheckBox.checked ? "qrc:/icon/icon/sidebar/CheckBox-default.svg" : "qrc:/icon/icon/sidebar/noCheck-default.svg"
                                    }
                                }
                            }
                        }
                    }

                    // 对话类型图标
                    Rectangle {
                        id: fileIconRectangle
                        width: 28
                        height: itemHeight
                        color: "transparent"
                        visible: true
                        Image{
                            id: fileIconImage
                            anchors.centerIn: parent
                            source: fileIcon
                            width: 16
                            height: 16
                        }
                    }

                    Label {
                        id: dialogueTitle
                        height: dialogueTitle.implicitHeight
                        width: parent.width - selectDialogueRectangle.width - fileIconRectangle.width -
                               (progressBar.visible ? progressBar.width : 0) -
                               (creatTimeLabel .visible ? creatTimeLabel.width : 0) -
                               (fileSizeLabel.visible ? fileSizeLabel.width : 0) -
                               (creatFailLabel.visible ? creatFailLabel.width : 0) -
                               //deleteDialogueImageRet.width -
                               (recorderRet.isHover ? deleteDialogueImageRet.width : 0) -
                               (deleteDialogueImageSpacingRet.visible ? deleteDialogueImageSpacingRet.width : 0)
                        text: fileName
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        // 1. 核心：判断是否显示省略号
                        property bool isElided: dialogueTitleMertics.width > dialogueTitle.width

                        // 2. 悬浮触发Tooltip的关键：鼠标是否悬停
                        property bool isHovered: false

                        // 悬浮提示框
                        ToolTip {
                            id: dialogueTitleTooltip
                            visible: dialogueTitle.isElided && dialogueTitle.isHovered && dialogueTitleMouseArea.containsMouse
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: fileName
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                        MouseArea {
                            id: dialogueTitleMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true // 允许事件冒泡给父容器
                            z: 10
                            onEntered: {
                                dialogueTitle.isHovered = true
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                    return
                                }else{
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                }
                                recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                                //deleteDialogueImageRet.visible = true
                            }
                            onExited: {
                                dialogueTitle.isHovered = false
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(!recorderMouseArea.containsMouse){
                                        recorderRet.isHover = false
                                    }
                                    return
                                }
                                recorderRet.isHover = false
                                if(knowledgeOperatePopup.visible){
                                    return
                                }
                                if(!dialogueTitleMouseArea.containsMouse){
                                    recorderRet.isHover = false
                                }

                                recorder.color = "transparent";
                                //deleteDialogueImageRet.visible = false
                            }
                            onPressed: {
                                dialogueTitle.isHovered = false
                            }
                            onReleased: {
                                dialogueTitle.isHovered = false
                            }
                            onClicked: {
                                doubleClickTimer.running = true
                                if(knowledgeListViewItem.doubleClickFlag){
                                    return
                                }
                                knowledgeListViewItem.doubleClickFlag = true
                                if(mouse.button === Qt.LeftButton){
                                    changeItemChecked()
                                }
                            }
                            onDoubleClicked: {
                                if(mouse.button === Qt.LeftButton){
                                    if(isDir){
                                        changeEditCount(0)
                                        changeEditFlag(false)
                                        requestToLoadDir()
                                    }
                                }
                            }
                        }
                    }

                    // 文字计算宽度区域，永远不展示
                    Label {
                        id: dialogueTitleMertics
                        height: dialogueTitle.implicitHeight
                        text: fileName
                        visible: false
                    }

                    // 进度条区域
                    ProgressBar{
                        id: progressBar
                        progressBarWidth: 72
                        progressBarHeight: 12
                        anchors.verticalCenter: parent.verticalCenter
                        progress: fileProgressNum
                        visible: (fileProgressNum === 100 ? false :　true) && (fileStatusStr === "uploading")
                    }

                    // 创建失败
                    Label{
                        id: creatFailLabel
                        height: dialogueTitle.implicitHeight
                        width: Math.min(creatFailLabel.implicitWidth, root.winMode === 2 ? (72 + 16) : (144 + 16))
                        text: fileStatusStr
                        color: fileProgressNum === 0 && fileStatus === 0 ? Qt.rgba(0/255, 0/255, 0/255, 0.35) : "red"
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        leftPadding: 16
                        visible: fileStatusStr !== "uploading"

                        // 悬浮提示框
                        ToolTip {
                            id: creatFailLabelTooltip
                            visible: creatFailLabelMouseArea.containsMouse &&
                                     creatFailLabelMertics.width > (root.winMode === 2 ? (72 + 16) : (144 + 16))
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: fileStatusStr
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }
                        MouseArea {
                            id: creatFailLabelMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true // 允许事件冒泡给父容器
                            z: 10
                            onEntered: {
                                dialogueTitle.isHovered = true
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                    return
                                }else{
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                }
                                recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onExited: {
                                dialogueTitle.isHovered = false
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(!recorderMouseArea.containsMouse){
                                        recorderRet.isHover = false
                                    }
                                    return
                                }
                                recorderRet.isHover = false
                                if(knowledgeOperatePopup.visible){
                                    return
                                }
                                if(!dialogueTitleMouseArea.containsMouse){
                                    recorderRet.isHover = false
                                }

                                recorder.color = "transparent";
                            }
                            onPressed: {
                                dialogueTitle.isHovered = false
                            }
                            onReleased: {
                                dialogueTitle.isHovered = false
                            }
                            onClicked: {
                                doubleClickTimer.running = true
                                if(knowledgeListViewItem.doubleClickFlag){
                                    return
                                }
                                knowledgeListViewItem.doubleClickFlag = true
                                if(mouse.button === Qt.LeftButton){
                                    changeItemChecked()
                                }
                            }
                            onDoubleClicked: {
                                if(mouse.button === Qt.LeftButton){
                                    if(isDir){
                                        changeEditCount(0)
                                        changeEditFlag(false)
                                        requestToLoadDir()
                                    }
                                }
                            }
                        }
                    }

                    // 报错信息计算文本宽度
                    Label{
                        id: creatFailLabelMertics
                        height: dialogueTitle.implicitHeight
                        width: creatFailLabelMertics.implicitWidth
                        text: fileStatusStr
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 16
                        visible: false
                    }

                    // 删除按钮
                    Rectangle {
                        id: deleteDialogueImageRet
                        width: 26
                        height: 26
                        radius: 4
                        color: "transparent"
                        anchors.verticalCenter: parent.verticalCenter
                        //visible: recorderRet.isHover
                        opacity: recorderRet.isHover
                        Image {
                            id: deleteDialogueImage
                            source: checked ? "qrc:/icon/icon/delete-click.svg" : "qrc:/icon/icon/delete.svg"
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
                            propagateComposedEvents: true // 允许事件冒泡给父容器
                            onEntered: {
                                if(totelCheckedCount > 1){
                                    recorderRet.isHover = false
                                    return
                                }
                                deleteDialogueImageTooltip.visible = true
                                deleteDialogueImageRet.color = "#E93C3C"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                    return
                                }else{
                                    if(totelCheckedCount > 1){
                                        recorderRet.isHover = false
                                    }else{
                                        recorderRet.isHover = true
                                    }
                                }
                                recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onExited: {
                                deleteDialogueImageTooltip.visible = false
                                deleteDialogueImageRet.color = "transparent"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                                if(knowledgeListModel.data(knowledgeListModel.index(index,0), 269) === true){
                                    if(!recorderMouseArea.containsMouse){
                                        recorderRet.isHover = false
                                    }
                                    return
                                }
                                recorderRet.isHover = false
                                if(!recorderMouseArea.containsMouse){
                                    recorderRet.isHover = false
                                }
                                recorder.color = "transparent";
                            }
                            onPressed: {
                                deleteDialogueImageTooltip.visible = false
                                deleteDialogueImageRet.color = "#C63432"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete-click.svg"
                            }
                            onReleased: {
                                deleteDialogueImageTooltip.visible = false
                                deleteDialogueImageRet.color = "transparent"
                                deleteDialogueImage.source = "qrc:/icon/icon/delete.svg"
                            }
                            onClicked: {
                                if(mouse.button === Qt.LeftButton){
                                    changeItemChecked()
                                    knowledgeListViewItem.openDeleteLayout(2)
                                }
                            }
                        }
                    }

                    // 用于侧边栏模式下右边距
                    Rectangle {
                        id: deleteDialogueImageSpacingRet
                        width: 6
                        height: parent.height
                        radius: 4
                        color: "transparent"
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.winMode === 2
                    }

                    // 创建日期
                    Label{
                        id: creatTimeLabel
                        height: dialogueTitle.implicitHeight
                        width: itemCreatTimeWidth
                        text: createTime
                        color: Qt.rgba(0, 0, 0, 0.35)
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: recorderRet.isHover ?　16 :  16 - deleteDialogueImageRet.width
                        visible: root.winMode === 3 ? true : false
                        onFontChanged: {
                            if(index === 0){
                                changeItemCreatTimeWidth(creatTimeLabel.implicitWidth + 16)
                            }

                        }
                        Component.onCompleted: {
                            if(index === 0){
                                changeItemCreatTimeWidth(creatTimeLabel.implicitWidth + 16)
                            }
                        }
                    }

                    // 文件大小
                    Label{
                        id: fileSizeLabel
                        height: dialogueTitle.implicitHeight
                        width: itemFileSizeWidth
                        text: isDir ? "" : fileSize
                        color: Qt.rgba(0, 0, 0, 0.35)
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: recorderRet.isHover ?　16 :  16 - deleteDialogueImageRet.width
                        visible: root.winMode === 3 ? true : false
                        onFontChanged: {
                            if(fileSizeLabel.implicitWidth + 16 > itemFileSizeWidth){
                                changeItemFileSizeWidth(96 + 16)
                            }
                        }
                        Component.onCompleted: {
                            if(fileSizeLabel.implicitWidth + 16 > itemFileSizeWidth){
                                changeItemFileSizeWidth(96 + 16)
                            }
                        }
                    }

                    Label {
                        id: knowledgeTitleMetrics
                        visible: false
                        text: "用于计算汉字高度"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
