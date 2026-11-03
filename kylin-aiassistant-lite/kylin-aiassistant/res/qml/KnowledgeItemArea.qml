import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import FileListModelProvider 1.0
import "."

Row{
    id: knowledgeItemAreaRow
    property int paddingSize: 24
    leftPadding: paddingSize
    rightPadding: paddingSize

    property bool isEditFlag: false
    property int editCount: 0
    property bool isAllChecked: false
    property bool isCanOperateFlag: true
    property int creatTimeWidth: 176
    property int fileSizeWidth: 96
    property bool closePopupFlag: false

    signal changeIsEditFlag(bool flag)

    onIsEditFlagChanged: {
        //changeIsEditFlag(isEditFlag)
        changeAllCheckboxStatus()
    }

    signal deleteSuccess()
    signal copyFiles(var filepaths)
    signal moveFiles(var filepaths)
    signal renameFiles(var filepaths)
    signal loadDir(var path)
    signal changeTransparentKnowledgeItemArea(var flag)
    signal knowledgeItemAreaTipShow(var iconPath, var message)

    function itemClosePopup(){
        closePopupFlag = !closePopupFlag
    }

    // 根据选中的checkbox更新全选按钮的状态
    function changeAllCheckboxStatus(){
        if(isEditFlag){
            if(isAllChecked){
                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg"
            }else{
                selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check.svg"
            }
        }else{
            selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg"
            selectAllCheckBox.checked = false
        }
    }

    Rectangle{
        id: knowledgeItemAreaRet
        width: knowledgeItemAreaRow.width - knowledgeItemAreaRow.leftPadding - knowledgeItemAreaRow.rightPadding
        height:  knowledgeItemAreaRow.height
        color: "#F6F6F6"
        //border.width: 1
        //border.color: Qt.rgba(0, 0, 0, 0.1)
        radius: 6
        Column{
            id: knowledgeItemAreaColumn
            width: parent.width
            height:  parent.height
            Row{
                id: titleItemDelegateTitleRow
                width: parent.width
                height: 44
                topPadding: 8
                leftPadding: 16
                rightPadding: 8
                //bottomPadding: 10
                spacing: 0
                Rectangle {
                    id: selectAllCheckBoxRet
                    color: "transparent"
                    width: 16 + selectAllCheckBoxLabel.width
                    height: 36
                    CheckBox {
                        id: selectAllCheckBox
                        checked: false
                        height: 36
                        Label {
                            id:selectAllCheckBoxLabel
                            anchors.left: selectAllCheckBox.indicator.right
                            anchors.leftMargin: 8
                            anchors.verticalCenter: selectAllCheckBox.verticalCenter
                            width: selectAllCheckBoxLabel.implicitWidth
                            text: qsTr("File Name")
                            color: "black"// 设置文本颜色
                        }
                        onCheckedChanged: {isAllChecked = checked}
                        indicator: Item {
                            id: selectAllCheckBoxItem
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            Image {
                                id: selectAllCheckBoxImage
                                source: isAllChecked ?
                                            "qrc:/icon/icon/sidebar/CheckBox-default.svg" :
                                            "qrc:/icon/icon/sidebar/noCheck-default.svg"
                            }
                        }

                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        // 按钮三态
                        onEntered: {
                            if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check.svg")){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check-hover.svg"
                                return
                            }
                            if(selectAllCheckBox.checked){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-hover.svg";
                            }else{
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-hover.svg";
                            }

                        }
                        onExited: {
                            if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-hover.svg")){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check.svg"
                                return
                            }
                            if(selectAllCheckBox.checked){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                            }else{
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                            }
                        }
                        onPressed: {
                            if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-hover.svg")){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check-click.svg"
                                return
                            }
                            if(selectAllCheckBox.checked){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg";
                            }else{
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-click.svg";
                            }
                        }
                        onReleased: {
                            if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-click.svg")){
                                selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg"
                                return
                            }
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
                            for (var i = 0; i < knowledgeListModel.count; ++i) {
                                //var itemModel = knowledgeListModel.get(i);
                                if(!knowledgeListModel.data(knowledgeListModel.index(i,0), 269)){
                                    checkedFlag = true
                                    break
                                }
                            }
                            // 若没有全部选中则全选;若已经全选,点击按钮则全不选
                            for (var j = 0; j < knowledgeListModel.count; ++j) {
                                //var item = knowledgeListModel.data(j);
                                if(checkedFlag){
                                    knowledgeListModel.setData(knowledgeListModel.index(j,0), true, 269)
                                }else{
                                    knowledgeListModel.setData(knowledgeListModel.index(j,0), false ,269)
                                }
                            }
                            // 修改全选按钮勾选状态
                            if(checkedFlag){
                                isAllChecked = true
                                selectAllCheckBox.checked = true
                            }else{
                                isAllChecked = false
                                selectAllCheckBox.checked = false
                            }
                        }
                    }
                }

                Rectangle{
                    id: sumLabelRet
                    height: 36
                    width: sumLabel.implicitWidth + 16
                    visible: isEditFlag
                    color: "transparent"
                    Label{
                        id: sumLabel
                        text: qsTr("(Selected ") + editCount + qsTr(" items)")
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        width: sumLabel.implicitWidth
                        color: "#000000"
                    }
                }

                // 弹簧
                Rectangle{
                    id: elasticityTitleRet
                    width: titleItemDelegateTitleRow.width - (createTimeRet.visible ? createTimeRet.width : 0) -
                           (fileSizeRet.visible ? fileSizeRet.width : 0) -
                           selectAllCheckBoxRet.width - (sumLabelRet.visible ? sumLabelRet.width : 0) -
                           titleItemDelegateTitleRow.leftPadding - titleItemDelegateTitleRow.rightPadding
                    height: 36
                    color: "transparent"
                }

                Rectangle{
                    id: createTimeRet
                    height: 36
                    width: creatTimeWidth
                    color: "transparent"
                    visible: root.winMode === 3 ? true : false
                    // 竖线
                    Rectangle{
                        id: createTimeLineRet
                        height: 14
                        width: 1
                        color: Qt.rgba(0, 0, 0, 0.1)
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Row{
                        id: createTimeRow
                        width: createTimeLabel.implicitWidth
                        height: parent.height
                        Label{
                            id: createTimeLabel
                            text: qsTr("Add Time")
                            anchors.verticalCenter: parent.verticalCenter
                            width: createTimeLabel.implicitWidth
                            color: "#000000"
                            leftPadding: 16
                        }
                    }
                }

                Rectangle{
                    id: fileSizeRet
                    height: 36
                    width: fileSizeWidth
                    color: "transparent"
                    visible: root.winMode === 3 ? true : false
                    // 竖线
                    Rectangle{
                        id: fileSizeLineRet
                        height: 14
                        width: 1
                        color: Qt.rgba(0, 0, 0, 0.1)
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Row{
                        id: fileSizeRow
                        width: fileSizeLabel.implicitWidth
                        height: parent.height
                        Label{
                            id: fileSizeLabel
                            text: qsTr("File Size")
                            anchors.verticalCenter: parent.verticalCenter
                            width: fileSizeLabel.implicitWidth
                            color: "#000000"
                            leftPadding: 16
                        }
                    }
                }
            }

            ScrollView {
                id: knowledgeScrollView
                //Layout.fillWidth: true
                //Layout.fillHeight: true
                width: parent.width
                height: knowledgeItemAreaColumn.height - titleItemDelegateTitleRow.height
                ScrollBar.horizontal.visible : false
                ScrollBar.vertical.visible : false
                Layout.bottomMargin: 8  // 下边距为8px

                ListView {
                    id: knowledgeListView
//                    Layout.fillHeight: true
//                    Layout.fillWidth: true
                    width: parent.width
                    height: knowledgeItemAreaColumn.height - titleItemDelegateTitleRow.height
                    clip: true
                    boundsMovement: Flickable.StopAtBounds // 防止过度滚动
                    cacheBuffer: 5000  // 设置缓存的控件数量，避免滚动时刷新
                    model: knowledgeListModel
                    spacing: 0

                    property bool isContentOverflowing: false // 内容是否超出容器高度

                    Connections {
                        target: knowledgeListModel
                        onRowsInserted: {
                            // 当有新项目插入时，可以在这里处理
                        }
                    }
                    // 监听model的rowsRemoved信号
                    Connections {
                        target: knowledgeListModel
                        onRowsRemoved: {
                            // 当有项目被移除时，可以在这里处理
                        }
                    }
                    onContentHeightChanged: {
                        // 判断内容是否超出容器高度，超出显示滚动条
                        isContentOverflowing = contentHeight > height
                        if(isContentOverflowing){
                            knowledgeScrollView.ScrollBar.vertical.visible = true
                        }else{
                            knowledgeScrollView.ScrollBar.vertical.visible = false
                        }
                    }
                    onHeightChanged: {
                        // 判断内容是否超出容器高度，超出显示滚动条
                        isContentOverflowing = contentHeight > height
                        if(isContentOverflowing){
                            knowledgeScrollView.ScrollBar.vertical.visible = true
                        }else{
                            knowledgeScrollView.ScrollBar.vertical.visible = false
                        }
                    }

                    // 覆盖整个 ListView 的背景 MouseArea
                    MouseArea {
                        anchors.fill: parent
                        z: -1              // 放在内容下面，避免挡住 delegate
                        hoverEnabled: true
                        acceptedButtons: Qt.RightButton | Qt.LeftButton
                        onClicked: {
                            if(mouse.button === Qt.LeftButton){
                                for (var i = 0; i < knowledgeListModel.count; ++i) {
                                    knowledgeListModel.setData(knowledgeListModel.index(i,0), false, 269)
                                }
                            }
                        }
                        onDoubleClicked: {
                            if(mouse.button === Qt.RightButton){
                                for (var i = 0; i < knowledgeListModel.count; ++i) {
                                    knowledgeListModel.setData(knowledgeListModel.index(i,0), false, 269)
                                }
                            }
                        }
                    }

                    delegate: KnowledgeItemDelegate {
                        id: historyItemDelegate
                        width: knowledgeListView.width
                        uuid: model.uuid
                        fileName: model.documentName || ""
                        fileIcon: model.fileIcon || ""
                        filePath: model.documentTargetPath || ""
                        fileProgressNum: model.fileProgressNum || 0
                        createTime: model.createTime || ""
                        fileSize: model.fileSize || ""
                        fileStatus: model.status
                        fileStatusStr: model.statusStr
                        checked: model.checked
                        isEdit: isEditFlag
                        isDir: model.isDir
                        currentIndex: model.index
                        totelCheckedCount: editCount
                        canOperateFlag: isCanOperateFlag
                        itemCreatTimeWidth: creatTimeWidth
                        itemFileSizeWidth: fileSizeWidth
                        itemClosePopupFlag: closePopupFlag
                        onChangeEditFlag: function(flag){
                            isEditFlag = flag
                            changeIsEditFlag(flag)
                            changeAllCheckboxStatus()
                        }
                        onChangeEditCount: function(count){
                            editCount = count
                        }
                        onChangeSelectAllCheckBoxChecked: function(flag){
                            isAllChecked = flag
                            selectAllCheckBox.checked = flag
                            changeAllCheckboxStatus()
                        }
                        onOpenDeleteLayout: function(type){
                            var options = []
                            let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
                            if (dialogComponent.status === Component.Ready) {
                                let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
                                dialog.selectedOptions = options
                                dialog.questionText = qsTr("Are you sure you want to delete the file?")
                                dialog.infoText = qsTr("After deletion, this operation cannot be undone, and you will not be able to perform knowledge Q&A on the file content.")
                                dialog.confirmButtonText = qsTr("Delete")
                                dialog.cancelButtonText = qsTr("Cancel")
                                dialog.highlightConfirmButton = true
                                dialog.highlightCancelButton = false
                                dialog.showNoRemindCheckBox = false
                                if (dialog) {
                                    dialog.confirmed.connect(function(options, noRemindChecked) {
                                        let deleteMsg = {
                                            msgType: "sendToDeleteDocument",
                                            msgBody: {
                                                "documentTargetPath": [model.documentTargetPath]
                                            }
                                        }
                                        let deleteMsgRet = knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
                                        let deleteMsgRetJsonObj = JSON.parse(deleteMsgRet)
                                        if (!deleteMsgRetJsonObj.hasOwnProperty("msgType") || (typeof deleteMsgRetJsonObj["msgType"] != "string")) {
                                            globals.sendLog("Not carried msgType!")
                                            return undefined
                                        }

                                        let deleteMsgRetMsgType = deleteMsgRetJsonObj["msgType"]
                                        if(deleteMsgRetMsgType !== "success"){
                                            knowledgeItemAreaTipShow("qrc:/icon/icon/dialog-error.svg", qsTr("Delete failed. The knowledge base service is unavailable, please try again later"))
                                            return
                                        }
                                        deleteSuccess()
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

                        onRequestToLoadDir: function(){
                            loadDir(model.documentTargetPath)
                            //knowledgeListModel.loadDirectory(model.documentTargetPath)
                        }

                        onMoveFileItem: function(filePaths){
                            moveFiles(filePaths)
                        }
                        onCopyFileItem: function(filePaths){
                            copyFiles(filePaths)
                        }
                        onRenameFileItem:  function(filePaths){
                            renameFiles(filePaths)
                        }
                        onDeleteFileItem: function(filePaths){
                            var options = []
                            let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
                            if (dialogComponent.status === Component.Ready) {
                                let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
                                dialog.selectedOptions = options
                                dialog.questionText = qsTr("Are you sure you want to delete the file?")
                                dialog.infoText = qsTr("After deletion, this operation cannot be undone, and you will not be able to perform knowledge Q&A on the file content.")
                                dialog.confirmButtonText = qsTr("Delete")
                                dialog.cancelButtonText = qsTr("Cancel")
                                dialog.highlightConfirmButton = true
                                dialog.highlightCancelButton = false
                                dialog.showNoRemindCheckBox = false
                                if (dialog) {
                                    dialog.confirmed.connect(function(options, noRemindChecked) {
                                        let deleteMsg = {
                                            msgType: "sendToDeleteDocument",
                                            msgBody: {
                                                "documentTargetPath": filePaths
                                            }
                                        }
                                        let deleteMsgRet = knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
                                        let deleteMsgRetJsonObj = JSON.parse(deleteMsgRet)
                                        if (!deleteMsgRetJsonObj.hasOwnProperty("msgType") || (typeof deleteMsgRetJsonObj["msgType"] != "string")) {
                                            globals.sendLog("Not carried msgType!")
                                            return undefined
                                        }

                                        let deleteMsgRetMsgType = deleteMsgRetJsonObj["msgType"]
                                        if(deleteMsgRetMsgType !== "success"){
                                            knowledgeItemAreaTipShow("qrc:/icon/icon/dialog-error.svg", qsTr("Delete failed. The knowledge base service is unavailable, please try again later"))
                                            return
                                        }
                                        deleteSuccess()
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

                        onChangeTransparentKnowledgeItemDelegate:function(flag){
                            changeTransparentKnowledgeItemArea(flag)
                        }

                        onChangeItemCreatTimeWidth: function(timeWidth){
                            if(timeWidth < 176){
                                creatTimeWidth = 176
                            }else{
                                creatTimeWidth = timeWidth
                            }
                        }

                        onChangeItemFileSizeWidth: function(sizeWidth){
                            fileSizeWidth = sizeWidth
                        }
                    }
                }
            }
        }
    }
}
