import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."


Row{
    id: knowledgeOperateArea
    Layout.fillWidth: true
    height: 36

    property bool editFlag: false
    property var breadcrumbPaths: [""]
    property int fileTotelCounts: 0
    property bool isCanOperateFlag: true
    property int paddingSize: 24

    onFileTotelCountsChanged: {
        if(fileTotelCounts >= 100){
            newFileRet.newFileShowFlag = false
            newFileImage.source = "qrc:/icon/icon/add-file-click.svg"
            newFileImage.opacity = 0.45
            newFileLabel.color = Qt.rgba(55/255, 144/255, 250/255, 0.45)
        }else{
            newFileRet.newFileShowFlag = true
            newFileImage.source = "qrc:/icon/icon/add-file-black.svg"
            newFileImage.opacity = 1
            newFileLabel.color = "#000000"
        }
    }

    onBreadcrumbPathsChanged: {
        var filePath = knowledgeListModel.uploadRootPath
        for(var i = 0; i < breadcrumbPaths.length; i++){
            filePath = filePath + "/" + breadcrumbPaths[i]
        }
        changeCurrentFilePath(filePath)
    }

    function refreshBreadcrumb() {
       let jsonStr = knowledgeListModel.currentFolderPathJson;
       let newPaths = JSON.parse(jsonStr);
       
       // 比较新旧值，避免不必要的更新导致循环依赖
       var pathsChanged = false;
       if (breadcrumbPaths.length !== newPaths.length) {
           pathsChanged = true;
       } else {
           for (var j = 0; j < newPaths.length; j++) {
               if (breadcrumbPaths[j] !== newPaths[j]) {
                   pathsChanged = true;
                   break;
               }
           }
       }
       
       // 只有在值真正改变时才更新
       if (pathsChanged) {
           Qt.callLater(function() {
               knowledgeOperateArea.breadcrumbPaths = newPaths;
           });
       }
    }

    function refreshListmodel(){
        let rootPath = knowledgeListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        let targetFullPath = rootPath;
        for (let i = 0; i <= knowledgeOperateArea.breadcrumbPaths.length; i++) {
            if (knowledgeOperateArea.breadcrumbPaths[i]) {
                targetFullPath += "/" + knowledgeOperateArea.breadcrumbPaths[i];
            }
        }

        quitEdit()
        knowledgeListModel.loadDirectory(targetFullPath);
    }

    Component.onCompleted: {
        refreshBreadcrumb();
    }

    Connections {
        target: knowledgeListModel
        onFolderPathChanged: {
            Qt.callLater(refreshBreadcrumb);
        }
    }

    signal requestToUploadDocument()
    signal requestToCreateDir()
    signal deleteSuccess()
    signal moveFiles(var filePaths)
    signal copyFiles(var filePaths)
    signal quitEdit()
    signal changeCurrentFilePath(var currentFilePath)
    signal knowledgeOperateAreaTipShow(var iconPath, var message)

    // 窗口模式下知识库操作导航栏
    Row{
        id: knowledgeOperateAreaWin
        width: parent.width
        height: 36
        leftPadding: paddingSize
        rightPadding: paddingSize
        spacing: 12
        visible: root.winMode === 3

        Row{
            id: breadcrumbRow
            height: parent.height
            width: Math.max(0, knowledgeOperateAreaWin.width -
                               (newFileRet.visible ? newFileRet.width + 20 : 0) -
                               (addFolderRet.visible ? addFolderRet.width + 20 : 0) -
                               (moveRet.visible ? moveRet.width + 20 : 0) -
                               (copyRet.visible ? copyRet.width + 20 : 0) -
                               (deleteRet.visible ? deleteRet.width + 20 : 0) -
                               knowledgeOperateAreaWin.leftPadding - knowledgeOperateAreaWin.rightPadding)
            spacing: 0
            Rectangle{
                id: allFileLabelRet
                width: allFileLabel.implicitWidth > 194 ? 194 : allFileLabel.implicitWidth // 同步限制容器宽度
                height: parent.height
                color: "transparent"
                radius: 6
                Label {
                    id: allFileLabel
                    text: breadcrumbPaths.length > 0 ?  qsTr("All Files    >    ") :qsTr("All Files  ")
                    color: breadcrumbPaths.length === 0 ? Qt.rgba(0/255, 0/255, 0/255, 0.85) :Qt.rgba(0/255, 0/255, 0/255, 0.6)
                    anchors.verticalCenter: parent.verticalCenter
                    visible: true
                    leftPadding: 6
                    font.bold: breadcrumbPaths.length === 0 ? true : false
                    font.family: root.fontFamily
                    font.pointSize: globals.fontsize
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    width: Math.min(194, implicitWidth)
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // todo knowledge 0303
                    enabled: false

                    onEntered: {
                        allFileLabelRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                        allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                    }
                    onExited: {
                        if(breadcrumbPaths.length === 0){
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                        }else{
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                        }
                        allFileLabelRet.color = "transparent"
                    }
                    onPressed: {
                        allFileLabelRet.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                        allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                    }
                    onReleased: {
                        if(breadcrumbPaths.length === 0){
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                        }else{
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                        }
                        allFileLabelRet.color = "transparent"

                    }
                    onClicked: {
                        let rootPath = knowledgeListModel.uploadRootPath;
                        rootPath = rootPath.replace(/\/$/, "");
                        let targetFullPath = rootPath;
                        if(breadcrumbPaths.length === 0){
                            return
                        }
                        quitEdit()
                        knowledgeListModel.loadDirectory(targetFullPath);
                    }
                }
            }
            Row {
                id: breadcrumbContainer
                height: parent.height
                spacing: 0
                width: Math.max(0,breadcrumbRow.width - allFileLabelRet.width)
                Repeater {
                    id: breadcrumbContainerRepeater
                    model: knowledgeOperateArea.breadcrumbPaths
                    onModelChanged: {
                        console.log("Repeater Model最终数据：", model);
                        if(model.length === 0){
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                        }else{
                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                        }
                    }
                    width: parent.width

                    delegate: Row {
                        height: parent.height
                        spacing: 0

                        Rectangle {
                            id: breadcrumbBtn
                            height: parent.height
                            width: Math.min(194,breadcrumbBtnRow.width)
                            radius: 6
                            color: "transparent"
                            // 悬浮提示框
                            ToolTip {
                                id: firstBreadcrumbRetTooltip
                                visible: breadcrumbBtnMouseArea.containsMouse && btnLabelMertics.width > btnLabel.width
                                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                                text: btnLabel.text
                                property var maxWidth: 400
                                property var wrapMode: Text.WordWrap
                            }

                            Row{
                                id: breadcrumbBtnRow
                                width: Math.min(194,
                                            (btnLabelMertics.width + btnLabel.leftPadding + btnLabel.rightPadding + 12 + (arrowLabel.visible ?　arrowLabel.width : 0)))
                                height: parent.height
                                Label {
                                    id: btnLabel
                                    text: modelData
                                    anchors.verticalCenter: parent.verticalCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    color: index === (breadcrumbPaths.length - 1) ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    elide: Text.ElideRight
                                    leftPadding: {
                                        if(index < (breadcrumbPaths.count - 1)){
                                            return 6
                                        }else{
                                            return 6
                                        }
                                    }
                                    rightPadding: {
                                        if(index < (breadcrumbPaths.count - 1)){
                                            return 0
                                        }else{
                                            return 6
                                        }
                                    }
                                    font.bold: index === (breadcrumbPaths.length - 1) ? true : false
                                    font.family: root.fontFamily
                                    wrapMode: Text.NoWrap
                                    width: breadcrumbBtn.width - (arrowLabel.visible ? arrowLabel.width : 0)
                                }

                                Label {
                                    id: arrowLabel
                                    text: "  >    "
                                    color: index === (breadcrumbPaths.length - 1) ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: (index >= 0 && index < 2 && (index !== (breadcrumbPaths.length - 1)))
                                }
                            }

                            Label {
                                id: btnLabelMertics
                                text: modelData
                                leftPadding: 6
                                visible: false
                            }

                            MouseArea {
                                id: breadcrumbBtnMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onEntered: {
                                    breadcrumbBtn.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                                    btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                    arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                }
                                onExited: {
                                    breadcrumbBtn.color = "transparent"
                                    if(index === (breadcrumbPaths.length - 1)){
                                        btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                    }else{
                                        btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                        arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    }
                                }
                                onPressed: {
                                    breadcrumbBtn.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                                    btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                    arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                }
                                onReleased: {
                                    breadcrumbBtn.color = "transparent"
                                    if(index === (breadcrumbPaths.length - 1)){
                                        btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                    }else{
                                        btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                        arrowLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    }
                                }

                                onClicked: {
                                    let rootPath = knowledgeListModel.uploadRootPath;
                                    rootPath = rootPath.replace(/\/$/, "");
                                    let targetFullPath = rootPath;
                                    const currentIndex = index;
                                    if (currentIndex >= 0) {
                                        for (let i = 0; i <= currentIndex; i++) {
                                            if (knowledgeOperateArea.breadcrumbPaths[i]) {
                                                targetFullPath += "/" + knowledgeOperateArea.breadcrumbPaths[i];
                                            }
                                        }
                                    }

                                    if(index === (breadcrumbPaths.length - 1)){
                                        return
                                    }

                                    quitEdit()
                                    knowledgeListModel.loadDirectory(targetFullPath);
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle{
            id: addFolderRet
            height: parent.height
            width: 94
            // todo knowledge 0303
            visible: false//!editFlag && breadcrumbPaths.length <= 2
            Row{
                id: addFolderRow
                width: addFolderImage.width + addFolderLabel.implicitWidth + addFolderRow.spacing
                height: parent.height
                spacing: 6
                Image {
                    id: addFolderImage
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/new-folder-black.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: addFolderLabel
                    text: qsTr("New Folder")
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                    width: 70
                    color: "#000000"
                }
            }
            MouseArea{
                id: addFolderRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    addFolderImage.source = "qrc:/icon/icon/new-folder-hover.svg"
                    addFolderLabel.color = "#3489EE";
                }
                onExited: {
                    addFolderImage.source = "qrc:/icon/icon/new-folder-black.svg"
                    addFolderLabel.color = "#000000";
                }
                onPressed: {
                    addFolderImage.source = "qrc:/icon/icon/new-folder-click.svg"
                    addFolderLabel.color = "#2C73C8";
                }
                onReleased: {
                    addFolderImage.source = "qrc:/icon/icon/new-folder-black.svg"
                    addFolderLabel.color = "#000000";
                }
                onClicked: {
                    if(breadcrumbPaths.length <= 2){
                        knowledgeOperateArea.requestToCreateDir()
                    }
                }
            }
        }

        Rectangle{
            id: newFileRet
            height: parent.height
            width: newFileRow.width
            visible: !editFlag
            property bool newFileShowFlag: fileTotelCounts >= 100 ? false :　true
            ToolTip {
                id: newFileRetTooltip
                visible: newFileRetRetMouseArea.containsMouse && !newFileRet.newFileShowFlag
                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                text: qsTr("The number of files cannot exceed 100")
                property var maxWidth: 400
                property var wrapMode: Text.WrapAnywhere
            }
            Row{
                id: newFileRow
                width: newFileImage.width + newFileLabel.implicitWidth + newFileRow.spacing
                height: parent.height
                spacing: 6
                Image {
                    id: newFileImage
                    width: 16
                    height: 16
                    source: newFileRet.newFileShowFlag ? "qrc:/icon/icon/add-file-black.svg" : "qrc:/icon/icon/add-file-click.svg"
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: newFileRet.newFileShowFlag ? 1 : 0.45
                }
                Label{
                    id: newFileLabel
                    text: qsTr("Add File")
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                    width: newFileLabel.implicitWidth
                    color: newFileRet.newFileShowFlag ? "#000000" : Qt.rgba(55/255, 144/255, 250/255, 0.45)
                }
            }
            MouseArea{
                id: newFileRetRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    if(!newFileRet.newFileShowFlag){
                        return
                    }
                    newFileImage.source = "qrc:/icon/icon/add-file-hover.svg"
                    newFileLabel.color = "#3489EE";
                }
                onExited: {
                    if(!newFileRet.newFileShowFlag){
                        return
                    }
                    newFileImage.source = "qrc:/icon/icon/add-file-black.svg"
                    newFileLabel.color = "#000000";
                }
                onPressed: {
                    if(!newFileRet.newFileShowFlag){
                        return
                    }
                    newFileImage.source = "qrc:/icon/icon/add-file-click.svg"
                    newFileLabel.color = "#2C73C8";
                }
                onReleased: {
                    if(!newFileRet.newFileShowFlag){
                        return
                    }
                    newFileImage.source = "qrc:/icon/icon/add-file-black.svg"
                    newFileLabel.color = "#000000";
                }
                onClicked: {
                    if(!newFileRet.newFileShowFlag){
                        return
                    }
                    knowledgeOperateArea.requestToUploadDocument()
                }
            }
        }

        Rectangle{
            id: moveRet
            height: parent.height
            width: 52
            // todo knowledge 0303
            visible: false//editFlag
            opacity: isCanOperate ? 1 : 0.5
            Row{
                id: moveRow
                width: moveImage.width + moveLabel.implicitWidth + moveRow.spacing
                height: parent.height
                spacing: 6
                Image {
                    id: moveImage
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/move.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: moveLabel
                    text: qsTr("Move")
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    color: "#000000"
                }
            }
            MouseArea{
                id: moveRetRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: isCanOperate
                onEntered: {
                    moveImage.source = "qrc:/icon/icon/move-hover.svg"
                    moveLabel.color = "#3489EE";
                }
                onExited: {
                    moveImage.source = "qrc:/icon/icon/move.svg"
                    moveLabel.color = "#000000";
                }
                onPressed: {
                    moveImage.source = "qrc:/icon/icon/move-click.svg"
                    moveLabel.color = "#2C73C8";
                }
                onReleased: {
                    moveImage.source = "qrc:/icon/icon/move.svg"
                    moveLabel.color = "#000000";
                }
                onClicked: {
                    var filePaths = [];   // 文件路径数组
                    // 遍历 ListModel 所有项，提取 uuid 并插入数组
                    for (let i = 0; i < knowledgeListModel.count; i++) {
                        // 获取第 i 项的 uuid 字段
                        let filePath = knowledgeListModel.data(knowledgeListModel.index(i,0), 260)
                        // 过滤空值（可选，避免无效数据）
                        if (filePath && filePath !== "" && knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true) {
                            filePaths.push(filePath)
                        }
                    }
                    moveFiles(filePaths)
                }
            }
        }

        Rectangle{
            id: copyRet
            height: parent.height
            width: 52
            // todo knowledge 0303
            visible: false//editFlag
            opacity: isCanOperate ? 1 : 0.5
            Row{
                id: copyRow
                width: copyImage.width + copyLabel.implicitWidth + copyRow.spacing
                height: parent.height
                spacing: 6
                Image {
                    id: copyImage
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/copy.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: copyLabel
                    text: qsTr("Copy")
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    color: "#000000"
                }
            }
            MouseArea{
                id: copyRetRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: isCanOperate
                onEntered: {
                    copyImage.source = "qrc:/icon/icon/copy-hover.svg"
                    copyLabel.color = "#3489EE";
                }
                onExited: {
                    copyImage.source = "qrc:/icon/icon/copy.svg"
                    copyLabel.color = "#000000";
                }
                onPressed: {
                    copyImage.source = "qrc:/icon/icon/copy-click.svg"
                    copyLabel.color = "#2C73C8";
                }
                onReleased: {
                    copyImage.source = "qrc:/icon/icon/copy.svg"
                    copyLabel.color = "#000000";
                }
                onClicked: {
                    var filePaths = [];   // 文件路径数组
                    // 遍历 ListModel 所有项，提取 uuid 并插入数组
                    for (let i = 0; i < knowledgeListModel.count; i++) {
                        // 获取第 i 项的 uuid 字段
                        let filePath = knowledgeListModel.data(knowledgeListModel.index(i,0), 260)
                        // 过滤空值（可选，避免无效数据）
                        if (filePath && filePath !== "" && knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true) {
                            filePaths.push(filePath)
                        }
                    }
                    copyFiles(filePaths)
                }
            }
        }

        Rectangle{
            id: deleteRet
            height: parent.height
            width: deletRow.width
            visible: editFlag
            Row{
                id: deletRow
                width: deletImage.width + deletLabel.implicitWidth + deletRow.spacing
                height: parent.height
                spacing: 6
                Image {
                    id: deletImage
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/delete-blue.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: deletLabel
                    text: qsTr("Delete")
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                    width: deletLabel.implicitWidth
                    color: "#000000"
                }
            }
            MouseArea{
                id: deletRetRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    deletImage.source = "qrc:/icon/icon/delete-red-hover.svg"
                    deletLabel.color = "#E93C3C"
                }
                onExited: {
                    deletImage.source = "qrc:/icon/icon/delete-blue.svg"
                    deletLabel.color = "#000000"
                }
                onPressed: {
                    deletImage.source = "qrc:/icon/icon/delete-red-click.svg"
                    deletLabel.color = "#C63432"
                }
                onReleased: {
                    deletImage.source = "qrc:/icon/icon/delete-blue.svg"
                    deletLabel.color = "#000000"
                }
                onClicked: {
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
                                        "documentTargetPath": []
                                    }
                                }
                                // 遍历 ListModel 所有项，提取 uuid 并插入数组
                                for (let i = 0; i < knowledgeListModel.count; i++) {
                                    // 获取第 i 项的 uuid 字段
                                    let path = knowledgeListModel.data(knowledgeListModel.index(i,0), 260)
                                    // 过滤空值（可选，避免无效数据）
                                    if (path && path !== "" && knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true) {
                                        deleteMsg.msgBody.documentTargetPath.push(path)
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
                                    knowledgeOperateAreaTipShow("qrc:/icon/icon/dialog-error.svg", qsTr("Delete failed. The knowledge base service is unavailable, please try again later"))
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
            }
        }
    }
}
