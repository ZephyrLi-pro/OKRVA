import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3
import TextEditMenu 1.0
import FileListModelProvider 1.0
import "."

Rectangle {
    id: knowledgeShowPageRet
    visible: true
    color: "#FFFFFF"

    property bool isEdit: false
    property bool showDragShadowFlag: false
    property var uploadFileUrl: []
    property var replaceFilePaths: []
    // 标记：是否触发“全部应用”（替换/取消都可能触发）
    property bool isAllUsedTriggered: false
    // 标记：全部应用的类型（replace=替换全部，cancel=取消全部）
    property string allUsedType: "" // ""=未触发，"replace"=替换全部，"cancel"=取消全部
    // 记录“全部应用”时的索引
    property int allUsedIndex: 0
    property bool isCanOperate: true
    property string currentFilePath: knowledgeListModel.uploadRootPath
    property int commonPadding: root.winMode === 2 ? 16 : 24

    signal showKnowledgeTip(string tipImageSource, string tipText)
    signal changeTransparentKnowledgeShow(var flag)

    KnowledgeUploadPage{
        id: knowledgeUploadPage
        anchors.fill: parent
        visible: showDragShadowFlag
        bgColor: "#FFFFFF"
    }

    onUploadFileUrlChanged: {
        console.log("uploadFileUrl===",uploadFileUrl)
        if(uploadFileUrl.length === 0){
            return
        }
        onUploadDocument(uploadFileUrl)
    }

    FileListModel {
        id: knowledgeListModel
        property var currentFolderPath: []

        // 移除直接绑定，让各个组件自己通过 Connections 监听更新
        // 避免多个组件同时更新导致冲突
        // onFolderPathChanged: {
        //     knowledgeOperateArea.breadcrumbPaths = knowledgeListModel.currentFolderPath;
        // }

        Component.onCompleted: {
            let initMsg = {
                msgType: "sendToInitUploadThread",
                msgBody: {}
            }
            knowledgeListModel.sendMessage(JSON.stringify(initMsg))
        }
    }

    // 知识库上传文件选择对话框
    FileDialog {
        id: uploadFileDialog
        selectMultiple: true
        selectExisting: true
        folder: shortcuts.desktop
        onAccepted: {
            var selectedFiles = []
            for (var i = 0; i < fileUrls.length; i++) {
                // 将 QUrl 转换为本地文件路径
                var url = fileUrls[i]
                var filePath = url.toString()

                // 移除 file:// 或 file:/// 前缀
                if (filePath.startsWith("file://")) {
                    filePath = filePath.substring(7)
                }

                // 解码URL编码的字符（如 %26 -> &）
                filePath = decodeURIComponent(filePath)

                selectedFiles.push(filePath)
            }
            console.log("FileDialog selected files:", selectedFiles)
            // 调用上传文档函数，传入选中的文件路径
            onUploadDocument(selectedFiles)
            setFolder(shortcuts.desktop)
        }
        onRejected: {
            console.log("FileDialog cancelled")
            setFolder(shortcuts.desktop)
        }
    }

    function parentWindowFocusChanged(flag){
        if(!flag){
            knowledgeSideOperateArea.closePopup()
            knowledgeItemArea.itemClosePopup()
        }
    }

    // 知识库上传文件点击/拖入   uploadFileUrl传空则走弹窗上传，传数据就走拖入文件
    function onUploadDocument(uploadFileUrl){
        console.log("uploadFileUrl==", uploadFileUrl)
        console.log("onUploadDocument== " + knowledgeListModel.isKbEmpty)
        let filePath = []
        if(uploadFileUrl.length >　0){
            filePath = uploadFileUrl
        }else{
            // 使用 Qt Quick Dialogs 的 FileDialog 组件打开文件选择对话框
            uploadFileDialog.open()
            return
        }

        console.log("uploadFilePath== " + filePath)
        // 上传文件校验文件数量
        if(filePath.length > (100 - knowledgeListModel.fileTotelCount)){
            showKnowledgeTips("qrc:/icon/icon/hint.svg", qsTr("File count cannot exceed 100"))
            return
        }

        // 上传文件容量校验 start
        let spaceCapacityMsg = {
            msgType: "sendToCheckSpaceCapacity",
            msgBody: {
                "sourcePath": filePath
            }
        }
        const spaceCapacityMsgRet = knowledgeListModel.sendMessage(JSON.stringify(spaceCapacityMsg))

        console.log("spaceCapacityMsgRet=",spaceCapacityMsgRet)

        let spaceCapacityMsgRetJsonObj = JSON.parse(spaceCapacityMsgRet)
        if (!spaceCapacityMsgRetJsonObj.hasOwnProperty("msgType") || (typeof spaceCapacityMsgRetJsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let spaceCapacityMsgType = spaceCapacityMsgRetJsonObj["msgType"]

        if(spaceCapacityMsgType === "sendToCheckSpaceCapacity"){
            let spaceCapacityMsgBody = spaceCapacityMsgRetJsonObj["msgBody"]

            if (!spaceCapacityMsgBody.hasOwnProperty("spaceCapacityAccess")) {
                console.log("Not spaceCapacityAccess!")
                return undefined
            }
            let spaceCapacityFlag =  spaceCapacityMsgBody["spaceCapacityAccess"]
            if(!spaceCapacityFlag){
                showKnowledgeTips("qrc:/icon/icon/hint.svg", qsTr("Insufficient knowledge base space, please delete some files and try again"))
                return
            }

        } else {
            return undefined
        }
        // 上传文件容量校验 end

        // 查询需要替换的文件start
        let getAllFileNamesMsg = {
            msgType: "sendToGetAllFileNames",
            msgBody: {
                "sourcePath": filePath
            }
        }
        let getAllFileNamesMsgMsgRet = knowledgeListModel.sendMessage(JSON.stringify(getAllFileNamesMsg));

        console.log("getAllFileNamesMsgMsgRet===", getAllFileNamesMsgMsgRet)

        let getAllFileNamesJsonObj = JSON.parse(getAllFileNamesMsgMsgRet)
        if (!getAllFileNamesJsonObj.hasOwnProperty("msgType") || (typeof getAllFileNamesJsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let getAllFileNamesMsgType = getAllFileNamesJsonObj["msgType"]
        // 返回成功，没有重名文件
        if(getAllFileNamesMsgType === "success"){

            let uploadMsg = {
                msgType: "sendToAddDocument",
                msgBody: {
                    "uploadFilePath": filePath
                }
            }
            knowledgeListModel.sendMessage(JSON.stringify(uploadMsg))

            console.log("upload - all - success" , filePath)

            // 操作后重新刷新model
            loadDirectoryCommon()
        }else if(getAllFileNamesMsgType === "sendToGetAllFileNames"){
            // 有重名文件，移动不重复的文件和用户选择的操作文件
            let getAllFileNamesMsgBody = getAllFileNamesJsonObj["msgBody"]

            if (!getAllFileNamesMsgBody.hasOwnProperty("allFileNames")) {
                console.log("Not repeatFileNames!")
                return undefined
            }
            let allFileNames =  getAllFileNamesMsgBody["allFileNames"]

            let repeatFileNames = findCommonStrings(filePath, allFileNames)

            // 无重名文件直接调用上传接口上传
            if(repeatFileNames.length === 0){
                let uploadMsg = {
                    msgType: "sendToAddDocument",
                    msgBody: {
                        "uploadFilePath": filePath
                    }
                }
                knowledgeListModel.sendMessage(JSON.stringify(uploadMsg))

                console.log("upload - all - success" , filePath)

                // 操作后重新刷新model
                loadDirectoryCommon()
                return
            }
            // 文件替换、取消确认弹窗的操作
            handleRepeatFiles(repeatFileNames, function(finalReplacePaths) {
                // 这里是所有弹窗操作完成后的回调逻辑
                console.log("需要替换的文件列表：", finalReplacePaths)

                let noOperateFiles = getNoReplacePaths(repeatFileNames, finalReplacePaths);

                console.log("不需要需要替换的文件列表：", noOperateFiles)
                // 后续的业务逻辑（比如执行文件替换）都写在这里

                // 调用移动接口，先校验，在移动 start
                // 调用移动接口，传入要移动的源文件路径数组
                // 使用对话框内部的 knowledgeListModel 实例，因为目标文件夹是对话框当前显示的文件夹

                let finalOperateFiles = getFinalOperateFiles(filePath, noOperateFiles)
                console.log("最终需要上传的文件列表：", finalOperateFiles)

                let deletePaths = findDeletePath(allFileNames, finalReplacePaths)
                console.log("最终需要删除的文件列表：", deletePaths)

                // 删除重复的需要替换的数据 start
                let deleteMsg = {
                    msgType: "sendToDeleteDocument",
                    msgBody: {
                        "documentTargetPath": deletePaths
                    }
                }
                knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
                // 删除重复的需要替换的数据 end

                // 调用上传文件接口 start
                let uploadMsg = {
                    msgType: "sendToAddDocument",
                    msgBody: {
                        "uploadFilePath": finalOperateFiles
                    }
                }
                knowledgeListModel.sendMessage(JSON.stringify(uploadMsg))
                // 调用上传文件接口 end

                console.log("upload - finalOperateFiles - success", finalOperateFiles)

                // 操作后重新刷新model
                loadDirectoryCommon()
            })
        }else{
            return undefined
        }
        // 查询需要替换的文件end
    }

    function operateMoveFiles(filePaths){
        console.log("moveFiles=filePaths=", filePaths.length)
        let getDirHierarchy= {
            msgType: "sendToGetDirHierarchy",
            msgBody: {
                "sourcePath": filePaths.filter(path => path.trim() !== "")
            }
        }
        const ret = knowledgeListModel.sendMessage(JSON.stringify(getDirHierarchy));
        let jsonObj = JSON.parse(ret)
        if (!jsonObj.hasOwnProperty("msgType") || (typeof jsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let msgType = jsonObj["msgType"]
        if (msgType !== "sendToGetDirHierarchy") {
            globals.sendLog("msgType is not equal sendToGetDirHierarchy!")
            return undefined
        }

        let msgBody = jsonObj["msgBody"]

        if (!msgBody.hasOwnProperty("maxDirHierarchy")) {
            console.log("Not carried maxDirHierarchy!")
            return undefined
        }

        let maxDirHierarchy =  msgBody["maxDirHierarchy"]
        console.log("moveFiles=sendToGetDirHierarchy=maxDirHierarchy=", maxDirHierarchy)

        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeOperateFileWindow.qml");
        if (dialogComponent.status === Component.Ready) {
            let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
            // 先处理参数（示例：过滤空路径）
            let validFilePaths = filePaths.filter(path => path.trim() !== "")
            // 给弹窗属性赋值
            dialog.filePaths = validFilePaths
            dialog.maxDirHierarchy = maxDirHierarchy
            dialog.windowType = 1 // 窗口移动
            dialog.sideCurrentPath = knowledgeShowPageRet.currentFilePath
            dialog.moveFileWindowWidth = root.winMode === 3 ? 452 : 376
            dialog.moveFileWindowHeight = root.winMode === 3 ? 568 : 492
            dialog.winMode = root.winMode === 3 ? 3 : 2
            if (dialog) {
                dialog.moveTo.connect(function(targetFilePath) {
                    console.log("moveTo===", targetFilePath)
                    dialog.destroy();

                    // 判断移动的文件夹是否在其自身存在目录或者子目录下 start
                    let sendToIsCanMoveMsg = {
                        msgType: "sendToIsCanMove",
                        msgBody: {
                            "sourcePath": validFilePaths
                        }
                    }
                    let sendToIsCanMoveMsgRet = dialog.knowledgeListModel.sendMessage(JSON.stringify(sendToIsCanMoveMsg));

                    let jsonObj = JSON.parse(sendToIsCanMoveMsgRet)
                    if (!jsonObj.hasOwnProperty("msgType") || (typeof jsonObj["msgType"] != "string")) {
                        globals.sendLog("Not carried msgType!")
                        return undefined
                    }

                    let msgType = jsonObj["msgType"]
                    if(msgType === "success"){

                    }else if(msgType === "getIsCanMoveFlag"){
                        let msgBody = jsonObj["msgBody"]

                        if (!msgBody.hasOwnProperty("isCanMoveFlag")) {
                            console.log("Not carried maxDirHierarchy!")
                            return undefined
                        }
                        let isCanMoveFlag =  msgBody["isCanMoveFlag"]
                        if(!isCanMoveFlag){
                            showKnowledgeTips("qrc:/icon/icon/dialog-error.svg", qsTr("Folder cannot be moved to itself or its subdirectories"))
                            return
                        }
                    }
                    // 判断移动的文件夹是否在其自身存在目录或者子目录下 end

                    // 查询需要替换的文件和文件夹start
                    let getRepeatFileNamesMsg = {
                        msgType: "sendToGetRepeatFileNames",
                        msgBody: {
                            "sourcePath": validFilePaths
                        }
                    }
                    let getRepeatFileNamesMsgRet = dialog.knowledgeListModel.sendMessage(JSON.stringify(getRepeatFileNamesMsg));

                    console.log("getRepeatFileNamesMsgRet===", getRepeatFileNamesMsgRet)

                    let getRepeatFileNamesJsonObj = JSON.parse(getRepeatFileNamesMsgRet)
                    if (!getRepeatFileNamesJsonObj.hasOwnProperty("msgType") || (typeof getRepeatFileNamesJsonObj["msgType"] != "string")) {
                        globals.sendLog("Not carried msgType!")
                        return undefined
                    }

                    let getRepeatFileNamesMsgType = getRepeatFileNamesJsonObj["msgType"]
                    if(getRepeatFileNamesMsgType === "success"){
                        // 没有重名文件/文件夹，全部文件都移动
                        let moveMsg = {
                            msgType: "sendToMove",
                            msgBody: {
                                "sourcePath": validFilePaths,
                                "targetFilePath": targetFilePath
                            }
                        }
                        let sendToMoveRet = knowledgeListModel.sendMessage(JSON.stringify(moveMsg));
                        // 调用移动接口，先校验，在移动 end

                        console.log("move - all - success" , validFilePaths)

                        // 操作后重新刷新model
                        loadDirectoryCommon()
                    }else if(getRepeatFileNamesMsgType === "sendToGetRepeatFileNames"){
                        // 有重名文件/文件夹，移动不重复的文件和用户选择的操作文件
                        let getRepeatFileNamesMsgBody = getRepeatFileNamesJsonObj["msgBody"]

                        if (!getRepeatFileNamesMsgBody.hasOwnProperty("repeatFileNames")) {
                            console.log("Not repeatFileNames!")
                            return undefined
                        }
                        let repeatFileNames = getRepeatFileNamesMsgBody["repeatFileNames"]

                        console.log("移动所有的重名文件列表：", repeatFileNames)

                        // 获取目标路径下的全部文件名 start
                        let allTargetFileNames = []
                        let getAllFileNamesMsg = {
                            msgType: "sendToGetAllFileNames",
                            msgBody: {
                                "sourcePath": targetFilePath
                            }
                        }
                        let getAllFileNamesMsgMsgRet = dialog.knowledgeListModel.sendMessage(JSON.stringify(getAllFileNamesMsg));

                        console.log("getAllFileNamesMsgMsgRet===", getAllFileNamesMsgMsgRet)

                        let getAllFileNamesJsonObj = JSON.parse(getAllFileNamesMsgMsgRet)
                        if (!getAllFileNamesJsonObj.hasOwnProperty("msgType") || (typeof getAllFileNamesJsonObj["msgType"] != "string")) {
                            globals.sendLog("Not carried msgType!")
                            return undefined
                        }

                        let getAllFileNamesMsgType = getAllFileNamesJsonObj["msgType"]
                        // 返回成功，没有重名文件
                        if(getAllFileNamesMsgType === "success"){
                        }else if(getAllFileNamesMsgType === "sendToGetAllFileNames"){
                            // 有重名文件，移动不重复的文件和用户选择的操作文件
                            let getAllFileNamesMsgBody = getAllFileNamesJsonObj["msgBody"]

                            if (!getAllFileNamesMsgBody.hasOwnProperty("allFileNames")) {
                                console.log("Not repeatFileNames!")
                                return undefined
                            }
                            allTargetFileNames =  getAllFileNamesMsgBody["allFileNames"]
                        }else{
                        }
                        console.log("移动获取目标路径下全部的文件路径名：", allTargetFileNames)
                        // 获取目标路径下的全部文件名 end

                        handleRepeatFiles(repeatFileNames, function(finalReplacePaths) {
                            // 如果移动的路径是自己本身，只出替换弹窗，不进行任何操作
                            if(targetFilePath === knowledgeShowPageRet.currentFilePath){

                            }else{
                                // 这里是所有弹窗操作完成后的回调逻辑
                                console.log("移动需要替换的文件列表：", finalReplacePaths)

                                let noOperateFiles = getNoReplacePaths(repeatFileNames, finalReplacePaths);

                                console.log("移动不需要需要替换的文件列表：", noOperateFiles)
                                // 后续的业务逻辑（比如执行文件替换）都写在这里

                                let finalOperateFiles = getFinalOperateFiles(filePaths, noOperateFiles)
                                console.log("移动最终需要替换的文件列表：", finalOperateFiles)

                            let deleteFiles = findDeletePath(allTargetFileNames, finalReplacePaths)
                            console.log("移动最终需要删除的文件列表：", deleteFiles)

                                // 删除重复的需要替换的数据 start
                                let deleteMsg = {
                                    msgType: "sendToDeleteDocument",
                                    msgBody: {
                                        "documentTargetPath": deleteFiles
                                    }
                                }
                                knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
                                // 删除重复的需要替换的数据 end

                                // 调用移动接口，先校验，在移动 start
                                let moveMsg = {
                                    msgType: "sendToMove",
                                    msgBody: {
                                        "sourcePath": finalOperateFiles,
                                        "targetFilePath": targetFilePath
                                    }
                                }
                                let sendToMoveRet = knowledgeListModel.sendMessage(JSON.stringify(moveMsg));
                                // 调用移动接口，先校验，在移动 end

                                console.log("move - finalOperateFiles - success", finalOperateFiles)

                                // 如果最终没有要移动的列表，不刷新
                                if(finalOperateFiles.length === 0){
                                    return
                                }
                            }

                            // 操作后重新刷新model
                            loadDirectoryCommon()
                        })
                    }else{
                        return undefined
                    }
                    // 查询需要替换的文件和文件夹end
                });

                dialog.cancel.connect(function(isCreateCurrentFolder) {
                    // 如果创建了新的文件夹，刷新列表
                    if(isCreateCurrentFolder){
                        // 操作后重新刷新model
                        loadDirectoryCommon()
                    }
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

    function operateCopyFiles(filePaths){
        console.log("copyFiles=filePaths=", filePaths.length)

        // 上传文件数量校验 start
        let allFileCount = 0
        let allFileCountMsg = {
            msgType: "sendToGetAllFileCountByPath",
            msgBody: {
                "sourcePath": filePaths
            }
        }

        let allFileCountMsgRet = knowledgeListModel.sendMessage(JSON.stringify(allFileCountMsg))

        console.log("allFileCountMsgRet=",allFileCountMsgRet)

        let allFileCountMsgRetJsonObj = JSON.parse(allFileCountMsgRet)
        if (!allFileCountMsgRetJsonObj.hasOwnProperty("msgType") || (typeof allFileCountMsgRetJsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let allFileCountMsgType = allFileCountMsgRetJsonObj["msgType"]

        if(allFileCountMsgType === "sendToGetAllFileCountByPath"){
            let allFileCountMsgBody = allFileCountMsgRetJsonObj["msgBody"]

            if (!allFileCountMsgBody.hasOwnProperty("totelCount")) {
                console.log("Not spaceCapacityAccess!")
                return undefined
            }
            allFileCount =  allFileCountMsgBody["totelCount"]
        } else {
            return undefined
        }
        console.log("复制选中文件一共的文件数量allFileCount==", allFileCount)

        // 复制文件校验文件数量
        if(allFileCount > (100 - knowledgeListModel.fileTotelCount)){
            showKnowledgeTips("qrc:/icon/icon/hint.svg", qsTr("File count cannot exceed 100"))
            return
        }
        // 上传文件数量校验 end

        // 上传文件容量校验 start
        let spaceCapacityMsg = {
            msgType: "sendToCheckSpaceCapacity",
            msgBody: {
                "validFilePaths": filePaths
            }
        }
        const spaceCapacityMsgRet = knowledgeListModel.sendMessage(JSON.stringify(spaceCapacityMsg))

        console.log("spaceCapacityMsgRet=",spaceCapacityMsgRet)

        let spaceCapacityMsgRetJsonObj = JSON.parse(spaceCapacityMsgRet)
        if (!spaceCapacityMsgRetJsonObj.hasOwnProperty("msgType") || (typeof spaceCapacityMsgRetJsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let spaceCapacityMsgType = spaceCapacityMsgRetJsonObj["msgType"]

        if(spaceCapacityMsgType === "sendToCheckSpaceCapacity"){
            let spaceCapacityMsgBody = spaceCapacityMsgRetJsonObj["msgBody"]

            if (!spaceCapacityMsgBody.hasOwnProperty("spaceCapacityAccess")) {
                console.log("Not spaceCapacityAccess!")
                return undefined
            }
            let spaceCapacityFlag =  spaceCapacityMsgBody["spaceCapacityAccess"]
            if(!spaceCapacityFlag){
                showKnowledgeTips("qrc:/icon/icon/hint.svg", qsTr("Insufficient knowledge base space, please delete some files and try again"))
                return
            }

        } else {
            return undefined
        }
        // 上传文件容量校验 end

        let getDirHierarchy= {
            msgType: "sendToGetDirHierarchy",
            msgBody: {
                "sourcePath": filePaths.filter(path => path.trim() !== "")
            }
        }
        const ret = knowledgeListModel.sendMessage(JSON.stringify(getDirHierarchy));
        let jsonObj = JSON.parse(ret)
        if (!jsonObj.hasOwnProperty("msgType") || (typeof jsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let msgType = jsonObj["msgType"]
        if (msgType !== "sendToGetDirHierarchy") {
            globals.sendLog("msgType is not equal sendToGetDirHierarchy!")
            return undefined
        }

        let msgBody = jsonObj["msgBody"]

        if (!msgBody.hasOwnProperty("maxDirHierarchy")) {
            console.log("Not carried maxDirHierarchy!")
            return undefined
        }

        let maxDirHierarchy =  msgBody["maxDirHierarchy"]
        console.log("moveFiles=sendToGetDirHierarchy=maxDirHierarchy=", maxDirHierarchy)

        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeOperateFileWindow.qml");
        if (dialogComponent.status === Component.Ready) {
            let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
            // 先处理参数（示例：过滤空路径）
            let validFilePaths = filePaths.filter(path => path.trim() !== "")
            // 给弹窗属性赋值
            dialog.filePaths = validFilePaths
            dialog.maxDirHierarchy = maxDirHierarchy
            dialog.windowType = 2 // 窗口复制
            dialog.sideCurrentPath = knowledgeShowPageRet.currentFilePath
            dialog.moveFileWindowWidth = root.winMode === 3 ? 452 : 376
            dialog.moveFileWindowHeight = root.winMode === 3 ? 568 : 492
            dialog.winMode = root.winMode === 3 ? 3 : 2
            if (dialog) {
                dialog.moveTo.connect(function(targetFilePath) {
                    console.log("copyTo===", targetFilePath)

                    dialog.destroy();

                    // 查询需要替换的文件和文件夹start
                    let getRepeatFileNamesMsg = {
                        msgType: "sendToGetRepeatFileNames",
                        msgBody: {
                            "sourcePath": validFilePaths
                        }
                    }
                    let getRepeatFileNamesMsgRet = dialog.knowledgeListModel.sendMessage(JSON.stringify(getRepeatFileNamesMsg));

                    console.log("getRepeatFileNamesMsgRet===", getRepeatFileNamesMsgRet)

                    let getRepeatFileNamesJsonObj = JSON.parse(getRepeatFileNamesMsgRet)
                    if (!getRepeatFileNamesJsonObj.hasOwnProperty("msgType") || (typeof getRepeatFileNamesJsonObj["msgType"] != "string")) {
                        globals.sendLog("Not carried msgType!")
                        return undefined
                    }

                    let getRepeatFileNamesMsgType = getRepeatFileNamesJsonObj["msgType"]
                    if(getRepeatFileNamesMsgType === "success"){
                        // 没有重名文件/文件夹，全部文件都复制
                        // 调用复制接口，传入源文件路径数组
                        let copyMsg = {
                            msgType: "sendToCopy",
                            msgBody: {
                                "sourcePath": validFilePaths,
                                "targetFilePath": targetFilePath

                            }
                        }
                        dialog.knowledgeListModel.sendMessage(JSON.stringify(copyMsg));

                        console.log("copy - all - success" , validFilePaths)

                        // 操作后重新刷新model
                        loadDirectoryCommon()
                    }else if(getRepeatFileNamesMsgType === "sendToGetRepeatFileNames"){
                        // 有重名文件/文件夹，移动不重复的文件和用户选择的操作文件
                        let getRepeatFileNamesMsgBody = getRepeatFileNamesJsonObj["msgBody"]

                        if (!getRepeatFileNamesMsgBody.hasOwnProperty("repeatFileNames")) {
                            console.log("Not repeatFileNames!")
                            return undefined
                        }
                        let repeatFileNames =  getRepeatFileNamesMsgBody["repeatFileNames"]

                        // 获取目标路径下的全部文件名 start
                        let allTargetFileNames = []
                        let getAllFileNamesMsg = {
                            msgType: "sendToGetAllFileNames",
                            msgBody: {
                                "sourcePath": targetFilePath
                            }
                        }
                        let getAllFileNamesMsgMsgRet = dialog.knowledgeListModel.sendMessage(JSON.stringify(getAllFileNamesMsg));

                        console.log("getAllFileNamesMsgMsgRet===", getAllFileNamesMsgMsgRet)

                        let getAllFileNamesJsonObj = JSON.parse(getAllFileNamesMsgMsgRet)
                        if (!getAllFileNamesJsonObj.hasOwnProperty("msgType") || (typeof getAllFileNamesJsonObj["msgType"] != "string")) {
                            globals.sendLog("Not carried msgType!")
                            return undefined
                        }

                        let getAllFileNamesMsgType = getAllFileNamesJsonObj["msgType"]
                        // 返回成功，没有重名文件
                        if(getAllFileNamesMsgType === "success"){
                        }else if(getAllFileNamesMsgType === "sendToGetAllFileNames"){
                            // 有重名文件，移动不重复的文件和用户选择的操作文件
                            let getAllFileNamesMsgBody = getAllFileNamesJsonObj["msgBody"]

                            if (!getAllFileNamesMsgBody.hasOwnProperty("allFileNames")) {
                                console.log("Not repeatFileNames!")
                                return undefined
                            }
                            allTargetFileNames =  getAllFileNamesMsgBody["allFileNames"]
                        }else{
                        }
                        console.log("移动获取目标路径下全部的文件路径名：", allTargetFileNames)
                        // 获取目标路径下的全部文件名 end

                        handleRepeatFiles(repeatFileNames, function(finalReplacePaths) {
                            // 这里是所有弹窗操作完成后的回调逻辑
                            console.log("复制需要替换的文件列表：", finalReplacePaths)

                            // 如果复制的路径是自己本身，只出替换弹窗，不进行任何操作
                            if(targetFilePath === knowledgeShowPageRet.currentFilePath){

                            }else{
                                let noOperateFiles = getNoReplacePaths(repeatFileNames, finalReplacePaths);

                                console.log("复制不需要需要替换的文件列表：", noOperateFiles)
                                // 后续的业务逻辑（比如执行文件替换）都写在这里

                                let operateFiles = getFinalOperateFiles(filePaths, noOperateFiles)
                                console.log("复制需要替换的文件列表：", operateFiles)

                                let deleteFiles = findDeletePath(allTargetFileNames, finalReplacePaths)
                                console.log("复制需要删除的文件列表：", deleteFiles)

                                let noDeleteFiles = []
                                var result = findNoDeletePath(operateFiles, deleteFiles)
                                // 子路径覆盖父路径额外处理列表,不删除，不替换
                                let resultDelete = result.resultDelete
                                let resultReplace = result.resultReplace
                                console.log("不删除，但需要转换层级的文件列表:", resultDelete)
                                console.log("不替换，但需要转换层级的文件列表:", resultReplace)

                                let finalDeleteFiles = removeContainedPaths(deleteFiles, resultDelete)
                                console.log("复制最终需要删除的文件列表：", finalDeleteFiles)

                                let finalOperateFiles = removeContainedPaths(operateFiles, resultReplace)
                                console.log("复制最终需要替换的文件列表：", finalOperateFiles)

                                // 删除重复的需要替换的数据 start
                                let deleteMsg = {
                                    msgType: "sendToDeleteDocument",
                                    msgBody: {
                                        "documentTargetPath": deleteFiles
                                    }
                                }
                                knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
                                // 删除重复的需要替换的数据 end

                                let copyMsg = {
                                    msgType: "sendToCopy",
                                    msgBody: {
                                        "sourcePath": operateFiles,
                                        "targetFilePath": targetFilePath

                                    }
                                }
                                let sendToCopyRet = knowledgeListModel.sendMessage(JSON.stringify(copyMsg));
                                // 调用移动接口，先校验，在移动 end

                                console.log("copy - finalOperateFiles - success", finalOperateFiles)

                                // 子路径覆盖父路径额外处理列表,不删除，不替换，调用后端额外处理
//                                if(resultDelete.length > 0){
//                                    let deleteMsg = {
//                                        msgType: "sendToChildrenCoverParent",
//                                        msgBody: {
//                                            "resultDeletePaths": resultDelete,
//                                            "resultReplacePaths": resultReplace
//                                        }
//                                    }
//                                    knowledgeListModel.sendMessage(JSON.stringify(deleteMsg))
//                                }

                                // 如果最终没有要移动的列表，不刷新
                                //if(finalOperateFiles.length === 0 && resultDelete.length === 0){
                                if(operateFiles.length === 0){
                                    return
                                }
                            }

                            // 操作后重新刷新model
                            loadDirectoryCommon()
                        })
                    }else{
                        return undefined
                    }
                    // 查询需要替换的文件和文件夹end
                });

                dialog.cancel.connect(function(isCreateCurrentFolder) {
                    // 如果创建了新的文件夹，刷新列表
                    if(isCreateCurrentFolder){
                        // 操作后重新刷新model
                        loadDirectoryCommon()
                    }
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

    function operateRenameFiles(filePaths){
        console.log("operateRenameFiles=filePaths=", filePaths.length,"---",filePaths[0])
        let alreadyCreateFolderName = []
        let alreadyCreateFileName = []
        let operatorFlag = 2 // 1：文件夹 2：文件
        let currentFileName = ""
        for (var i = 0; i < knowledgeListModel.count; ++i) {
            if(filePaths.length > 0){
                if(knowledgeListModel.data(knowledgeListModel.index(i,0), 260) === filePaths[0]){
                    currentFileName = knowledgeListModel.data(knowledgeListModel.index(i,0), 258)
                    if(knowledgeListModel.data(knowledgeListModel.index(i,0), 268) === true){
                        operatorFlag = 1
                    }
                }
            }

            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 268) === true){
                alreadyCreateFolderName.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 258))
            }else{
                alreadyCreateFileName.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 258))
            }
        }

        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeNewFileFolderWindow.qml");
        if (dialogComponent.status === Component.Ready) {
          let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
          if(operatorFlag === 1){
              dialog.alreadyCreateFolderName = alreadyCreateFolderName
          }else{
              dialog.alreadyCreateFileName = alreadyCreateFileName
          }
          dialog.renameFileName = currentFileName
          dialog.windowType = 2
          if (dialog) {
              dialog.confirmed.connect(function(fileName) {
                  let renameDirMsg = {
                      msgType: "sendToRename",
                      msgBody: {
                          "src": filePaths[0],
                          "dest": fileName.trim()
                      }
                  }
                  knowledgeListModel.sendMessage(JSON.stringify(renameDirMsg));
                  dialog.destroy();
                  // 操作后重新刷新model
                  //loadDirectoryCommon()
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

    function createDir(){
        let alreadyCreateFolderName = []
        for (var i = 0; i < knowledgeListModel.count; ++i) {
            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 268) === true){
                alreadyCreateFolderName.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 258))
            }
        }
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeNewFileFolderWindow.qml");
        if (dialogComponent.status === Component.Ready) {
          let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
          dialog.alreadyCreateFolderName = alreadyCreateFolderName
          dialog.windowType = 1
          if (dialog) {
              dialog.confirmed.connect(function(fileName) {
                  let createDirMsg = {
                      msgType: "sendToCreateDir",
                      msgBody: {
                      "directoryPath": fileName.trim()
                      }
                  }
                  knowledgeListModel.sendMessage(JSON.stringify(createDirMsg));
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

    // 定义获取最后文件名/名称的方法
    function getFileNameFromPath(fullPath) {
        // 空路径直接返回空
        if (!fullPath || fullPath.trim() === "") {
            return "";
        }

        // 统一处理路径分隔符（兼容Windows和Linux）
        let normalizedPath = fullPath.replace(/\\/g, "/");
        // 移除末尾的所有斜杠（避免分割后出现空字符串）
        normalizedPath = normalizedPath.replace(/\/+$/, "");
        // 按斜杠分割路径
        const pathParts = normalizedPath.split("/");
        // 返回最后一个部分（即文件名/文件夹名）
        return pathParts[pathParts.length - 1] || "";
    }

    function getAllFileNameByFilePaths(filePaths){
        let allFileName = []
        for(let i = 0; i < filePaths.length; i++){
            allFileName.push(getFileNameFromPath(filePaths[i]))
        }
        return allFileName
    }

    // 2. 基础版路径类型判断（无Qt.labs.platform依赖）
    function getPathType(fullPath) {
        if (!fullPath || fullPath.trim() === "") return "";

        // 统一路径分隔符，移除末尾斜杠
        let normalizedPath = fullPath.replace(/\\/g, "/").replace(/\/+$/, "");

        // 规则1：路径末尾无后缀（无.）且不含扩展名 → 判定为文件夹
        // 规则2：路径包含有效文件后缀（如.txt/.pdf等）→ 判定为文件
        // （注：该规则不绝对，比如文件夹名可能带.，如 "test.123"）
        const fileName = getFileNameFromPath(normalizedPath);
        const hasExtension = fileName.includes(".") && fileName.lastIndexOf(".") > 0;

        if (hasExtension) {
            return "file";
        } else {
            return "folder";
        }
    }

    // 核心方法：逐个显示重复文件弹窗，操作完成后通过回调返回结果
    function handleRepeatFiles(repeatFileNames, callback) {
    // 重置状态
        replaceFilePaths = []
        isAllUsedTriggered = false
        allUsedType = ""
        allUsedIndex = 0

        // 递归核心函数（改为异步安全版）
        function showDialog(index) {
            // 终止条件：索引越界 或 触发全部应用
            if (index >= repeatFileNames.length || isAllUsedTriggered) {
                // 处理替换+全部应用的剩余文件
                if (isAllUsedTriggered && allUsedType === "replace") {
                    for (let j = allUsedIndex; j < repeatFileNames.length; j++) {
                        replaceFilePaths.push(repeatFileNames[j])
                    }
                }
                // 返回最终结果
                callback(replaceFilePaths)
                return
            }

            const currentPath = repeatFileNames[index]
            console.log(`处理第${index+1}个文件：`, currentPath)

            // ========== 关键修复1：处理Component异步加载 ==========
            let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgePromptWindow.qml")
            // 监听组件加载完成（兼容异步加载）
            function onComponentReady() {
                if (dialogComponent.status === Component.Error) {
                    console.error("弹窗组件加载失败：", dialogComponent.errorString())
                    // 加载失败，直接递归下一个（核心：确保递归执行）
                    showDialog(index + 1)
                    return
                }
                if (dialogComponent.status === Component.Ready) {
                    // 创建弹窗实例
                    let dialogPrompt = dialogComponent.createObject(root)
                    if (!dialogPrompt) {
                        console.error("弹窗创建失败：", dialogComponent.errorString())
                        showDialog(index + 1)
                        return
                    }

                    // 弹窗属性赋值
                    dialogPrompt.fileName = getFileNameFromPath(currentPath)
                    dialogPrompt.typeParam = getPathType(currentPath)
                    dialogPrompt.fileCounts = repeatFileNames.length

                    // ========== 确认按钮逻辑（不变） ==========
                    dialogPrompt.confirmed.connect(function(isAllUsed) {
                        console.log("确认-是否全部应用：", isAllUsed)
                        replaceFilePaths.push(currentPath)

                        if (isAllUsed) {
                            isAllUsedTriggered = true
                            allUsedType = "replace"
                            allUsedIndex = index + 1
                        }

                        // 先销毁弹窗，再递归（避免弹窗叠加）
                        dialogPrompt.destroy()
                        // 核心：无论是否全部应用，都调用showDialog（终止条件会判断标记）
                        showDialog(index + 1)
                    })

                    // ========== 取消按钮逻辑（修复核心） ==========
                    dialogPrompt.cancel.connect(function(isAllUsed) {
                        console.log("取消-是否全部应用：", isAllUsed)
                        // 取消当前：不加入替换列表

                        if (isAllUsed) {
                            isAllUsedTriggered = true
                            allUsedType = "cancel"
                            allUsedIndex = index + 1
                        }

                        // ========== 关键修复2：先销毁弹窗，再递归下一个 ==========
                        dialogPrompt.destroy()
                        // 强制调用下一个，即使取消也会执行（终止条件会判断标记）
                        showDialog(index + 1)
                    })

                    // 显示弹窗
                    dialogPrompt.showDialog()
                }
            }

            // 立即检查组件状态（同步加载）
            onComponentReady()
            // 监听组件加载状态变化（异步加载兼容）
            dialogComponent.statusChanged.connect(onComponentReady)
        }

        // 启动递归（从第0个开始）
        showDialog(0)
    }

    /**
     * 新增核心方法：筛选出“不替换”的文件列表
     * @param originalList 原始重复文件列表（repeatFileNames）
     * @param replaceList 最终替换列表（finalReplacePaths）
     * @returns 不替换的文件列表
     */
    function getNoReplacePaths(originalList, replaceList) {
        // 边界处理：原始列表为空，返回空
        if (!originalList || originalList.length === 0) {
            return []
        }
        // 替换列表为空，返回全部原始列表（全部不替换）
        if (!replaceList || replaceList.length === 0) {
            return [...originalList] // 浅拷贝，避免修改原数组
        }

        // 核心逻辑：过滤出原始列表中不在替换列表里的项
        const noReplaceList = originalList.filter(function(path) {
            // 严格匹配路径（区分大小写，根据系统调整）
            return !replaceList.includes(path)
        })

        return noReplaceList
    }

    /**
     * 从原始文件列表中移除不替换的文件，得到最终需要处理的文件列表
     * @param {Array} filePaths - 原始完整的文件路径列表（如repeatFileNames）
     * @param {Array} noOperateFiles - 不替换/不操作的文件路径列表
     * @returns {Array} 最终需要处理（替换）的文件列表（浅拷贝，不修改原数组）
     */
    function getFinalOperateFiles(filePaths, noOperateFiles) {
        // 边界处理1：原始列表为空，直接返回空数组
        if (!filePaths || !Array.isArray(filePaths) || filePaths.length === 0) {
            return [];
        }
        // 边界处理2：不操作列表为空，返回原始列表的浅拷贝（避免修改原数组）
        if (!noOperateFiles || !Array.isArray(noOperateFiles) || noOperateFiles.length === 0) {
            return [...filePaths];
        }

        // 核心逻辑：过滤出原始列表中不在不操作列表里的项
        // 使用filter + includes实现精准匹配，区分路径大小写（符合文件系统规则）
        const finalOperateList = filePaths.filter(function(path) {
            // 排除空路径（避免无效数据）
            if (!path || path.trim() === "") return false;
            // 返回“不在不操作列表中”的项
            return !noOperateFiles.includes(path);
        });

        return finalOperateList;
    }

    function findCommonStrings(arr1, arr2) {
        if (!arr1 || !arr2 || arr1.length === 0 || arr2.length === 0) {
            return [];
        }
        let newArr2 = []
        for(let i = 0; i < arr2.length; i++){
            newArr2.push(getFileNameFromPath(arr2[i]))
        }

        // 将第二个数组转为Set，提升查找效率
        const set2 = new Set(newArr2);
        const result = new Set(); // 用Set自动去重

        for (const str of arr1) {
            if (typeof str === "string" && set2.has(getFileNameFromPath(str))) {
                result.add(str);
            }
        }
        // 转回数组返回
        return Array.from(result);
    }

    // 获取需要删除的知识库内已有的文件路径
    function findDeletePath(arr1, arr2) {
        if (!arr1 || !arr2 || arr1.length === 0 || arr2.length === 0) {
            return [];
        }
        let result = []
        for(let i = 0; i < arr2.length; i++){
            for(let j = 0; j　< arr1.length; j++){
                if(getFileNameFromPath(arr2[i]) === getFileNameFromPath(arr1[j])){
                    result.push(arr1[j])
                }
            }

        }
        return result;
    }

    // 查询替换删除的文件是否替换其父文件夹，是的话查询出来 arr1 替换的文件列表   arr2删除的文件列表
    function findNoDeletePath(arr1, arr2) {
        if (!arr1 || !arr2 || arr1.length === 0 || arr2.length === 0) {
            return [];
        }
        var resultReplace = []
        var resultDelete = []

        for (var i = 0; i < arr1.length; i++) {
            var childPath = arr1[i]
            var childName = childPath.split("/").pop()

            for (var j = 0; j < arr2.length; j++) {
                var parentPath = arr2[j]
                var parentName = parentPath.split("/").pop()

                // 1️⃣ 文件夹名相同
                if (childName !== parentName)
                    continue

                // 2️⃣ 子文件夹关系
                if (childPath.indexOf(parentPath + "/") === 0) {
                    resultReplace.push(childPath)
                    resultDelete.push(parentPath)
                }
            }
        }

        return {
            resultReplace: resultReplace,
            resultDelete: resultDelete
        }
    }

    // 去除包含的路径
    function removeContainedPaths(arr1, arr2) {
        var result = []

        for (var i = 0; i < arr1.length; i++) {
            var path1 = arr1[i]
            var contained = false

            // 遍历 arr2，检查 arr1 中的路径是否完全在 arr2 中
            for (var j = 0; j < arr2.length; j++) {
                var path2 = arr2[j]

                // 完全匹配
                if (path1 === path2) {
                    contained = true
                    break
                }
            }

            // 如果没有匹配的，保留该路径
            if (!contained) {
                result.push(path1)
            }
        }

        return result
    }

    function loadDirectoryCommon(){
        knowledgeOperateArea.refreshListmodel()
        knowledgeSideOperateArea.refreshBreadcrumb()
    }

    function changeIsCanOperate(){
        let filePaths = []
        for (var i = 0; i < knowledgeListModel.count; ++i) {
            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 269) === true){
                filePaths.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 260))
            }
        }

        let isAllComplete = true
        let isAllCompleteMsg = {
            msgType: "sendToGetIsAllComplete",
            msgBody: {
                "sourcePath": filePaths,
            }
        }
        let isAllCompleteMsgRet = knowledgeListModel.sendMessage(JSON.stringify(isAllCompleteMsg));

        let isAllCompleteJsonObj = JSON.parse(isAllCompleteMsgRet)
        if (!isAllCompleteJsonObj.hasOwnProperty("msgType") || (typeof isAllCompleteJsonObj["msgType"] != "string")) {
            globals.sendLog("Not carried msgType!")
            return undefined
        }

        let isAllCompleteMsgType = isAllCompleteJsonObj["msgType"]
        if(isAllCompleteMsgType === "sendToGetIsAllComplete"){
            // 有重名文件，移动不重复的文件和用户选择的操作文件
            let isAllCompleteMsgBody = isAllCompleteJsonObj["msgBody"]

            if (!isAllCompleteMsgBody.hasOwnProperty("isAllComplete")) {
                isAllComplete = true
            }else{
                isAllComplete = isAllCompleteMsgBody["isAllComplete"]
            }
        }else{
            isAllComplete = true
        }
        isCanOperate = isAllComplete
    }

    function showKnowledgeTips(tipImageSource, tipText){
        root.commonTipsText = tipText
        root.commonTipsSources = tipImageSource
    }

    ColumnLayout{
        id: knowledgeShowPageColumnLayout
        //anchors.fill: parent
        width: Math.min(knowledgeShowPageRet.width, root.maxWidth)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter

        spacing: 0

        // 知识库首页展示区域
        KnowledgeTitleArea{
            id: knowledgeTitleArea
            Layout.fillWidth: true
            visible: (knowledgeListModel.count === 0 && knowledgeListModel.isKbEmpty) || root.winMode === 2
            paddingSize: commonPadding
        }

        // 知识库首页窗口模式知识库不为空展示区域
        KnowledgeWindowTitleArea{
            id: knowledgeWindowTitleArea
            Layout.fillWidth: true
            visible: (knowledgeListModel.count > 0 || !knowledgeListModel.isKbEmpty) && root.winMode === 3
            paddingSize: commonPadding
        }

        // 窗口模式下知识库操作区域
        KnowledgeOperateArea{
            id: knowledgeOperateArea
            editFlag: isEdit
            visible: root.winMode === 3
            breadcrumbPaths: knowledgeListModel.currentFolderPath
            fileTotelCounts: knowledgeListModel.fileTotelCount
            isCanOperateFlag: isCanOperate
            paddingSize: commonPadding
            onRequestToUploadDocument: () => {
                let filePath = []
                onUploadDocument(filePath)
            }

            onRequestToCreateDir: () => {
                createDir()
            }

            onDeleteSuccess: function(){
                isEdit = false
                knowledgeItemArea.isAllChecked = false
                knowledgeItemArea.isEditFlag = false
                knowledgeItemArea.editCount = 0
            }

            onMoveFiles: function(filePaths){
                operateMoveFiles(filePaths)
            }

            onCopyFiles: function(filePaths){
                operateCopyFiles(filePaths)
            }

            onQuitEdit: function(){
                isEdit = false
                knowledgeItemArea.isAllChecked = false
                knowledgeItemArea.editCount = 0
                knowledgeItemArea.isEditFlag = false
            }
            onChangeCurrentFilePath: function(filePath){
                knowledgeShowPageRet.currentFilePath = filePath
            }
            onKnowledgeOperateAreaTipShow: function(iconPath, message){
                showKnowledgeTips(iconPath, message)
            }
        }


        // 侧边模式下知识库操作区域
        KnowledgeSideOperateArea{
            id: knowledgeSideOperateArea
            visible: root.winMode === 2
            sideEditFlag: isEdit
            // sideBreadcrumbPaths 通过 refreshBreadcrumb() 函数更新，避免绑定冲突
            // sideBreadcrumbPaths: knowledgeListModel.currentFolderPath
            fileTotelCounts: knowledgeListModel.fileTotelCount
            isCanOperateFlag: isCanOperate
            paddingSize: commonPadding
            onRequestToUploadDocument: () => {
                let filePath = []
                onUploadDocument(filePath)
            }

            onRequestToCreateDir: () => {
                createDir()
            }

            onDeleteSuccess: function(){
                isEdit = false
                knowledgeItemArea.isAllChecked = false
                knowledgeItemArea.isEditFlag = false
                knowledgeItemArea.editCount = 0
            }

            onMoveFiles: function(filePaths){
                operateMoveFiles(filePaths)
            }

            onCopyFiles: function(filePaths){
                operateCopyFiles(filePaths)
            }

            onQuitEdit: function(){
                isEdit = false
                knowledgeItemArea.isAllChecked = false
                knowledgeItemArea.editCount = 0
                knowledgeItemArea.isEditFlag = false
            }
            onChangeSideCurrentFilePath: function(filePath){
                knowledgeShowPageRet.currentFilePath = filePath
            }
            onKnowledgeSideOperateAreaTipShow: function(iconPath, message){
                showKnowledgeTips(iconPath, message)
            }
        }

        // 知识库列表区域
        KnowledgeItemArea{
            id: knowledgeItemArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: knowledgeListModel.count > 0
            isAllChecked: false
            isEditFlag: false
            editCount: 0
            isCanOperateFlag: isCanOperate
            paddingSize: commonPadding

            onChangeIsEditFlag: function(flag){
                isEdit = flag
                changeIsCanOperate()
            }

            onDeleteSuccess: function(){
                isEdit = false
                knowledgeItemArea.isAllChecked = false
                knowledgeItemArea.isEditFlag = false
                knowledgeItemArea.editCount = 0
            }

            onMoveFiles: function(filePaths){
                operateMoveFiles(filePaths)
            }

            onCopyFiles: function(filePaths){
                operateCopyFiles(filePaths)
            }

            onRenameFiles: function(filePaths){
                operateRenameFiles(filePaths)
            }
            onLoadDir: function(path){
                knowledgeListModel.loadDirectory(path)
            }
            onChangeTransparentKnowledgeItemArea: function(flag){
                changeTransparentKnowledgeShow(flag)
            }
            onKnowledgeItemAreaTipShow: function(iconPath, message){
                showKnowledgeTips(iconPath, message)
            }
        }

        // 知识库上传文件区域
        KnowledgeFileUploadArea{
            id: knowledgeFileUploadArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: knowledgeListModel.count === 0 || knowledgeListModel.isKbEmpty
            listmodeCount: knowledgeListModel.count
            isKbEmpty: knowledgeListModel.isKbEmpty
            paddingSize: commonPadding
            onRequestToUploadDocument: () => {
                let filePath = []
                onUploadDocument(filePath)
            }
        }

        // 知识库底部容量区域
        KnowledgeBottomArea {
            id: knowledgeBottomArea
            spaceStatus: knowledgeBaseSpaceManager.spaceStatus
            spaceUsedPercent: knowledgeBaseSpaceManager.spaceUsedPercent
            paddingSize: commonPadding
//            MouseArea {
//                anchors.fill: parent
//                onClicked: () => console.log(knowledgeBaseSpaceManager.spaceStatus)
//            }
        }
    }
}
