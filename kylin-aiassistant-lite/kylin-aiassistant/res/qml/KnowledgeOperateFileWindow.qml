import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."
import FileListModelProvider 1.0
import KnowledgeBaseFilterProxyModelProvider 1.0

Window {
    id: moveFileWindow
    width: moveFileWindowWidth
    height: moveFileWindowHeight
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    visible: false
    color: "transparent"

    property var breadcrumbPaths: [""]
    property var filePaths: []  // 接收文件路径数组（默认空数组）
    property int windowType: 1  // 1:移动 2:复制
    property int showCount: 0
    property int maxDirHierarchy: 0
    property alias knowledgeListModel: knowledgeListModel  // 暴露模型实例供外部访问
    property bool isCreateCurrentFolder: false // 打开窗口后，是否在外部的当前路径下创建文件
    property string sideCurrentPath: "" // 打开窗口时外部的当前路径
    property int moveFileWindowWidth: 376 // 452
    property int moveFileWindowHeight: 492 // 568
    property int winMode: 2
    
    // 文本宽度计算缓存，避免频繁创建QML对象
    property var textWidthCache: ({})
    
    // 工具函数：计算文本的像素宽度（带缓存优化）
    function getTextWidth(text, fontSize) {
        if (!text || text === "") {
            return 0;
        }
        
        var cacheKey = text + "_" + fontSize;
        
        // 检查缓存
        if (textWidthCache.hasOwnProperty(cacheKey)) {
            return textWidthCache[cacheKey];
        }
        
        // 限制文本长度，避免超长文本导致性能问题
        var maxTextLength = 200;
        var textToMeasure = text.length > maxTextLength ? text.substring(0, maxTextLength) : text;
        
        // 转义双引号避免创建对象时出错
        var safeText = textToMeasure.replace(/"/g, '\\"').replace(/'/g, "\\'");
        
        var textObj = Qt.createQmlObject('import QtQuick 2.12; Text { font.pointSize: ' + fontSize + '; text: "' + safeText + '"; }', moveFileWindow);
        var width = textObj.implicitWidth;
        textObj.destroy();
        
        // 如果文本被截断，按比例估算完整宽度
        if (text.length > maxTextLength) {
            width = width * (text.length / maxTextLength);
        }
        
        // 缓存结果（限制缓存大小，避免内存泄漏）
        var cacheSize = Object.keys(textWidthCache).length;
        if (cacheSize < 100) {
            textWidthCache[cacheKey] = width;
        }
        
        return width;
    }
    
    // 判断是否需要显示折叠按钮：只有当层级数 > 2 且宽度超过可用宽度时才显示
    function shouldShowCollapseButton() {
        // 只有当层级数 > 2 时才可能显示折叠按钮
        if (breadcrumbPaths.length <= 2) {
            return false;
        }
        
        var fontSize = 14;
        var separator = "    >    ";
        var separatorWidth = getTextWidth(separator, fontSize);
        var allFileText = qsTr("All Files  ");
        var allFileWidth = getTextWidth(allFileText, fontSize);
        
        // 计算所有层级的宽度
        var totalPathsWidth = 0;
        for (var i = 0; i < breadcrumbPaths.length; i++) {
            if (breadcrumbPaths[i]) {
                totalPathsWidth += getTextWidth(breadcrumbPaths[i], fontSize);
            }
        }
        
        var totalSeparatorWidth = separatorWidth * breadcrumbPaths.length;

        var totalPaddingWidth = 12 + (12 * breadcrumbPaths.length);
        
        var totalWidth = allFileWidth + totalPathsWidth + totalSeparatorWidth + totalPaddingWidth;
        
        var availableWidth = 380;
        
        return totalWidth > availableWidth;
    }

    onWindowTypeChanged: {
        console.log("windowType===",windowType)
    }

    FileListModel {
        id: knowledgeListModel
        property var currentFolderPath: []

        onFolderPathChanged: {
            Qt.callLater(refreshBreadcrumb);
        }
    }

    // 代理模型（始终按创建时间排序）
    KnowledgeBaseFilterProxyModel {
        id: filteredModel
        sourceModel: knowledgeListModel

        // 过滤条件
        searchText: ""
        fileTypeFilter: KnowledgeBaseFilterProxyModel.AllFiles
        statusFilter: KnowledgeBaseFilterProxyModel.AllStatus

        // 默认显示所有文件和目录
        showDirectories: true
        showFiles: false
        foldersFirst: true  // 文件夹优先

        onFilterCountChanged: {
            console.log("显示文件数:", filteredCount, "总文件数:", totalCount, "====", filteredModel.rowCount());
            showCount = filteredCount
        }
    }

    function refreshBreadcrumb() {
        let jsonStr = knowledgeListModel.currentFolderPathJson
        let newPaths = JSON.parse(jsonStr);
        
        var validPaths = [];
        if (Array.isArray(newPaths)) {
            for (var i = 0; i < newPaths.length; i++) {
                if (newPaths[i] && newPaths[i] !== "") {
                    validPaths.push(newPaths[i]);
                }
            }
        }
        
        var pathsChanged = false;
        if (breadcrumbPaths.length !== validPaths.length) {
            pathsChanged = true;
        } else {
            for (var j = 0; j < validPaths.length; j++) {
                if (breadcrumbPaths[j] !== validPaths[j]) {
                    pathsChanged = true;
                    break;
                }
            }
        }
        
        if (pathsChanged) {
            Qt.callLater(function() {
                moveFileWindow.breadcrumbPaths = validPaths;
            });
        }
     }

    Component.onCompleted: {
        refreshBreadcrumb();
    }

    signal newFolder()
    signal cancel(var isCreateFolderFlag)
    signal moveTo(var filePath)
    
    // 打开全部文件的函数
    function openAllFileCommon(){
        let rootPath = knowledgeListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        knowledgeListModel.loadDirectory(rootPath);
    }
    

    function showDialog() {
        visible = true;
    }

    function hideDialog() {
        visible = false;
    }
    
    // 面包屑弹窗组件：只显示"All Files"，不显示第一个路径
    KnowledgeBreadcrumbPopup{
        id: moveFileBreadcrumbPopup
        firstBreadcrumb: ""  // 设置为空字符串，不显示第一个路径
        onOpenAllFile: function(){
            openAllFileCommon()
        }
        onOpenBreadcrumbPath: function(){
            // 不再需要此功能
        }
        onVisibleChanged: {
            if(!visible){
                breadcrumbLeftImage.source = "qrc:/icon/icon/knowledge-bread-right.svg"
            }
        }
    }

    Rectangle {
        id: contentContainer
        anchors.fill: parent
        color: "white"
        radius: 12
        focus: true

        Keys.onEscapePressed: {
            cancel(isCreateCurrentFolder)
            hideDialog()
        }

        Keys.onEnterPressed: {
            movetoBtn.clicked()
        }

        Keys.onReturnPressed: {
            // 默认按回车键响应确认按钮
            console.log("*****Return*******")
            if (movetoBtn.visible) {
                movetoBtn.clicked()
            }
        }

        Rectangle {
            id: titleBar
            Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_titleBar"
            height: 40
            color: "white"
            radius: 12
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onPressed: {
                    moveFileWindow.startSystemMove()
                }
            }

            Image {
                id: titleIcon
                Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_titleIcon"
                anchors {
                    left: parent.left
                    leftMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                width: 24
                height: 24
                source: "qrc:/icon/icon/knowledge.svg"
                fillMode: Image.PreserveAspectFit
            }

            Label {
                id: titleText
                Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_titleText"
                text: windowType === 1 ? qsTr("Move") : qsTr("Copy")
                anchors {
                    left: titleIcon.right
                    leftMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                color: Qt.rgba(0, 0, 0, 0.85)
            }

            // 关闭按钮
            Button {
                id: closeButton
                width: 32
                height: 32
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                icon.name: "window-close-symbolic"
                focusPolicy: Qt.NoFocus

                flat: true
                property string buttonType: "windowTitleBarCloseButton"

                ToolTip {
                    id: closeButtonTooltip
                    text: qsTr("Close")
                    delay: Qt.styleHints.mousePressAndHoldInterval
                }

                onHoveredChanged: {
                    if(hovered)
                        closeButtonTooltip.visible = true
                    else
                        closeButtonTooltip.visible = false
                }

                onClicked: {
                    cancel(isCreateCurrentFolder)
                    hideDialog()
                }

                Keys.onReturnPressed: {
                    if(closeButton.focus === true){
                        closeButton.clicked()
                    }
                }
            }
        }

        ColumnLayout {
            anchors {
                top: titleBar.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.bottomMargin: 24
            anchors.topMargin: 0
            spacing: 0

            Column{
                id: knowledgeItemAreaColumn
                width: contentContainer.width - 48
                height: contentContainer.height - 40
                Row{
                    id: breadcrumbRow
                    height: 36
                    width: parent.width
                    spacing: 0
                    
                    // 左侧折叠按钮：当内容宽度超过可用宽度时显示，点击可以展开显示"All Files"
                    Rectangle{
                        id: breadcrumbLeftRet
                        height: 36
                        width: 36
                        color: Qt.rgba(220/255, 220/255, 220/255, 1)
                        radius: 6
                        visible: shouldShowCollapseButton()
                        Image {
                            id: breadcrumbLeftImage
                            source: "qrc:/icon/icon/knowledge-bread-right.svg"
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        MouseArea{
                            id: breadcrumbLeftRetRetMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                breadcrumbLeftRet.color = Qt.rgba(200/255, 200/255, 200/255, 1)
                            }
                            onExited: {
                                breadcrumbLeftRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onPressed: {
                                breadcrumbLeftRet.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                            }
                            onReleased: {
                                breadcrumbLeftRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                            }
                            onClicked: {
                                breadcrumbLeftImage.source = "qrc:/icon/icon/knowledge-bread-down.svg"
                                // 获取按钮的全局坐标
                                var globalPos = breadcrumbLeftRet.mapToItem(null, 0, 0);
                                moveFileBreadcrumbPopup.x = globalPos.x;
                                moveFileBreadcrumbPopup.y = globalPos.y + breadcrumbLeftRet.height;
                                moveFileBreadcrumbPopup.open()
                            }
                        }
                    }
                    
                    // "All Files" 标签：当不需要折叠按钮时显示
                    Rectangle{
                        id: allFileLabelRet
                        width: allFileLabelRow.width
                        height: parent.height
                        color: "transparent"
                        radius: 6
                        visible: !shouldShowCollapseButton()
                        Row{
                            id: allFileLabelRow
                            anchors.verticalCenter: parent.verticalCenter
                            Label {
                                id: allFileLabel
                                text: qsTr("All Files  ")
                                color: breadcrumbPaths.length === 0 ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 6
                                font.bold: breadcrumbPaths.length === 0 ? true : false
                                font.family: root.fontFamily
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                            // 分隔符：当有 "All Files" 且层级数 > 0 时显示
                            Label {
                                id: separatorLabel
                                text: "  >    "
                                color: Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                anchors.verticalCenter: parent.verticalCenter
                                visible: allFileLabelRet.visible && breadcrumbPaths.length > 0
                            }
                        }
                        MouseArea{
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

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
                                knowledgeListModel.loadDirectory(rootPath);
                            }
                        }
                    }

                    // 面包屑容器：显示最多3级
                    Row {
                        id: breadcrumbContainer
                        height: parent.height
                        spacing: 0
                        width: {
                            var totalWidth = breadcrumbRow.width;
                            var leftBtnWidth = breadcrumbLeftRet.visible ? breadcrumbLeftRet.width : 0;
                            var allFileWidth = allFileLabelRet.visible ? allFileLabelRet.width : 0;
                            var separatorWidth = separatorLabel.visible ? separatorLabel.implicitWidth : 0;
                            return Math.max(0, totalWidth - leftBtnWidth - allFileWidth - separatorWidth);
                        }
                        
                        Repeater {
                            id: breadcrumbContainerRepeater
                            // 根据是否需要折叠按钮决定显示哪些层级
                            model: {
                                if (shouldShowCollapseButton()) {
                                    // 需要折叠按钮时，只显示最后3级
                                    if (breadcrumbPaths.length >= 3) {
                                        var startIndex = breadcrumbPaths.length - 3;
                                        var result = [];
                                        for (var i = startIndex; i < breadcrumbPaths.length; i++) {
                                            result.push(i);
                                        }
                                        return result;
                                    } else {
                                        // 如果层级数少于3，显示所有层级
                                        var result = [];
                                        for (var j = 0; j < breadcrumbPaths.length; j++) {
                                            result.push(j);
                                        }
                                        return result;
                                    }
                                } else {
                                    // 不需要折叠按钮时，显示所有层级
                                    var result = [];
                                    for (var k = 0; k < breadcrumbPaths.length; k++) {
                                        result.push(k);
                                    }
                                    return result;
                                }
                            }
                            
                            delegate: Row {
                                height: parent.height
                                spacing: 0

                                Rectangle {
                                    id: breadcrumbBtn
                                    height: parent.height
                                    // 简化宽度计算：平均分配，分隔符也在内部
                                    width: Math.min(Math.floor(breadcrumbContainer.width / breadcrumbContainerRepeater.count), breadcrumbBtnRow.width)
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

                                    Row {
                                        id: breadcrumbBtnRow
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 0
                                        width: Math.min(Math.floor(breadcrumbContainer.width / breadcrumbContainerRepeater.count),
                                                        (btnLabelMertics.width + btnLabel.leftPadding + btnLabel.rightPadding + 12 + (itemSeparatorLabel.visible ?　itemSeparatorLabel.width : 0)))

                                        Label {
                                            id: btnLabel
                                            // 使用 modelData 作为索引获取实际路径名
                                            text: {
                                                var pathIndex = modelData;
                                                if (pathIndex >= 0 && pathIndex < breadcrumbPaths.length) {
                                                    return breadcrumbPaths[pathIndex] || "";
                                                }
                                                return "";
                                            }
                                            color: {
                                                var pathIndex = modelData;
                                                var isLast = pathIndex === (breadcrumbPaths.length - 1);
                                                return isLast ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6);
                                            }
                                            elide: Text.ElideRight
                                            font.family: root.fontFamily
                                            font.bold: {
                                                var pathIndex = modelData;
                                                return pathIndex === (breadcrumbPaths.length - 1);
                                            }
                                            anchors.verticalCenter: parent.verticalCenter
                                            horizontalAlignment: Text.AlignHCenter
                                            leftPadding: {
                                                if(index < (breadcrumbContainerRepeater.count - 1)){
                                                    return 6
                                                }else{
                                                    return 6
                                                }
                                            }
                                            rightPadding: {
                                                if(index < (breadcrumbContainerRepeater.count - 1)){
                                                    return 0
                                                }else{
                                                    return 6
                                                }
                                            }
                                            // 让文本占用大部分宽度，预留分隔符空间
                                            width: breadcrumbBtn.width - (itemSeparatorLabel.visible ? itemSeparatorLabel.implicitWidth : 0)
                                        }

                                        // 分隔符放在同一个 Rectangle 里，方便统一三态效果
                                        Label {
                                            id: itemSeparatorLabel
                                            text: "  >    "
                                            color: Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                            visible: index < (breadcrumbContainerRepeater.count - 1)
                                        }
                                    }

                                    Label {
                                        id: btnLabelMertics
                                        text: btnLabel.text
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
                                        }
                                        onExited: {
                                            breadcrumbBtn.color = "transparent"
                                            var pathIndex = modelData;
                                            var isLast = pathIndex === (breadcrumbPaths.length - 1);
                                            btnLabel.color = isLast ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6);
                                        }
                                        onPressed: {
                                            breadcrumbBtn.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                                            btnLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                        }
                                        onReleased: {
                                            breadcrumbBtn.color = "transparent"
                                            var pathIndex = modelData;
                                            var isLast = pathIndex === (breadcrumbPaths.length - 1);
                                            btnLabel.color = isLast ? Qt.rgba(0/255, 0/255, 0/255, 0.85) : Qt.rgba(0/255, 0/255, 0/255, 0.6);
                                        }

                                        onClicked: {
                                            var pathIndex = modelData;
                                            let rootPath = knowledgeListModel.uploadRootPath;
                                            rootPath = rootPath.replace(/\/$/, "");
                                            let targetFullPath = rootPath;

                                            if (pathIndex >= 0) {
                                                for (let i = 0; i <= pathIndex; i++) {
                                                    if (breadcrumbPaths[i]) {
                                                        targetFullPath += "/" + breadcrumbPaths[i];
                                                    }
                                                }
                                            }

                                            knowledgeListModel.loadDirectory(targetFullPath);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle{
                    width: parent.width
                    height: knowledgeItemAreaColumn.height - breadcrumbRow.height
                    visible: showCount === 0
                    Column{
                        id: knowledgeNoFolderColumn
                        width: parent.width
                        anchors.centerIn: parent
                        Image{
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: "qrc:/icon/icon/insync-folder.png"
                        }
                        Label{
                            text: qsTr("No other folders in current directory")
                            color: Qt.rgba(0, 0, 0, 0.35)
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            wrapMode: Text.WrapAnywhere
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                ScrollView {
                    id: knowledgeScrollView
                    width: parent.width
                    height: winMode === 3 ? 392 : 320
                    ScrollBar.horizontal.visible : false
                    ScrollBar.vertical.visible : false
                    visible: showCount > 0

                    ListView {
                        id: knowledgeListView
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        boundsMovement: Flickable.StopAtBounds // 防止过度滚动
                        cacheBuffer: 5000  // 设置缓存的控件数量，避免滚动时刷新
                        model: filteredModel
                        spacing: 4

                        property bool isContentOverflowing: false // 内容是否超出容器高度

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

                        delegate: KnowledgeFolderItemDelegate {
                            id: historyItemDelegate
                            width: knowledgeListView.width - 8
                            fileName: model.documentName || ""
                            fileIcon: model.fileIcon || ""
                            filePath: ""
                            isEdit: false
                            padingSize: 0
                            onRequestToLoadDir: () => knowledgeListModel.loadDirectory(model.documentTargetPath)
                        }
                    }
                }
            }

            Row {
                id: bottomButtonRow
                width: contentContainer.width - 48
                height: 68
                spacing: 0
                topPadding: 32

                Button {
                    id: newfileBtn
                    Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_newfileBtn"
                    text: qsTr("New Folder")
                    Layout.preferredWidth: 102
                    Layout.preferredHeight: 36
                    visible: breadcrumbPaths.length < 3
                    focusPolicy: Qt.NoFocus

                    Keys.onReturnPressed: {
                        if(newfileBtn.focus === true){
                            newfileBtn.clicked()
                        }
                    }

                    onClicked: {
                        let alreadyCreateFolderName = []
                        let operatorFlag = 1 // 1：文件夹 2：文件
                        let currentFileName = ""
                        for (var i = 0; i < knowledgeListModel.count; ++i) {
                            if(knowledgeListModel.data(knowledgeListModel.index(i,0), 268) === true){
                                alreadyCreateFolderName.push(knowledgeListModel.data(knowledgeListModel.index(i,0), 258))
                            }
                        }

                        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeNewFileFolderWindow.qml");
                        if (dialogComponent.status === Component.Ready) {
                          let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
                          if(operatorFlag === 1){
                              dialog.alreadyCreateFolderName = alreadyCreateFolderName
                          }
                          dialog.renameFileName = currentFileName
                          dialog.windowType = 1
                          if (dialog) {
                              dialog.confirmed.connect(function(fileName) {
                              if (fileName.trim() === "") {
                                  console.log("文件夹名称不能为空！");
                                  return;
                              }
                              let createDirMsg = {
                                  msgType: "sendToCreateDir",
                                  msgBody: {
                                  "directoryPath": fileName.trim()
                                  }
                              }
                              let createDirMsgRet = knowledgeListModel.sendMessage(JSON.stringify(createDirMsg));

                              let createDirMsgRetJsonObj = JSON.parse(createDirMsgRet)
                              if (!createDirMsgRetJsonObj.hasOwnProperty("msgType") || (typeof createDirMsgRetJsonObj["msgType"] != "string")) {
                                  globals.sendLog("Not carried msgType!")
                                  return undefined
                              }

                              let createDirMsgType = createDirMsgRetJsonObj["msgType"]
                              if(createDirMsgType === "success"){
                                  let targetPath = ""
                                  let rootPath = knowledgeListModel.uploadRootPath;
                                  targetPath = rootPath
                                  for(let i = 0; i < breadcrumbPaths.length; i++){
                                      targetPath = targetPath + "/" + breadcrumbPaths[i]
                                  }
                                  console.log("sideCurrentPath",sideCurrentPath,"targetPath",targetPath)

                                  if(sideCurrentPath === targetPath){
                                      isCreateCurrentFolder = true
                                      console.log("isCreateCurrentFolder====",isCreateCurrentFolder)
                                  }

                                  targetPath = targetPath + "/" + fileName.trim()

                                  knowledgeListModel.loadDirectory(targetPath)
                              }

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

                Rectangle {
                    width: bottomButtonRow.width - (newfileBtn.visible ? newfileBtn.width : 0) - cancelBtn.width - movetoBtn.width -  movetoBtnItem.width
                    height: 1
                }

                Button {
                    id: cancelBtn
                    Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_cancelBtn"
                    text: qsTr("Cancel")
                    width: 64
                    Layout.preferredHeight: 36
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        cancel(isCreateCurrentFolder)
                        hideDialog()
                    }

                    Keys.onReturnPressed: {
                        if(cancelBtn.focus === true){
                            cancelBtn.clicked()
                        }
                    }
                }

                Rectangle {
                    id: movetoBtnItem
                    width: 8
                    height: 1
                }

                Button {
                    id: movetoBtn
                    Accessible.name: "kylin-aiassistant_Knowledge_moveFileWindow_movetoBtn"
                    text: windowType === 1 ? qsTr("Move Here") : qsTr("Copy Here")
                    Layout.preferredWidth: 102
                    Layout.preferredHeight: 36
                    highlighted: true
                    enabled: (breadcrumbPaths.length + maxDirHierarchy) > 3 ? false : true
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        let targetPath = ""
                        let rootPath = knowledgeListModel.uploadRootPath;
                        targetPath = rootPath
                        for(let i = 0; i < breadcrumbPaths.length; i++){
                            targetPath = targetPath + "/" + breadcrumbPaths[i]
                        }

                        moveTo(targetPath);
                    }

                    Keys.onReturnPressed: {
                        if(movetoBtn.focus === true){
                            movetoBtn.clicked()
                        }
                    }
                }
            }
        }
    }
}

