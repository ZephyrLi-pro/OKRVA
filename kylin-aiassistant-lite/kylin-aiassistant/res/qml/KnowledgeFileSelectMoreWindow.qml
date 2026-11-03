import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import FileListModelProvider 1.0
import KnowledgeBaseFilterProxyModelProvider 1.0
import "."

Window {
    id: mainWindow
    width: root.winMode === 2 ? 376 : 452
    height: root.winMode === 2 ? 492 : 568
    title: " "
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    visible: false
    color: "transparent"
    // 外部使用属性
    property bool isSupportAllFile: false // 是否支持选择全部文件
    property int supportSelectNum: 0 // 0:默认支持所有文件
    property var currentFilesPath: [] // 默认选中文件
    property bool isSelectedRootPath: false // 是否全选个人知识库

    property int showCount: 0
    property bool isEditFlag: false
    property int editCount: 0 // 总文件数（文件+文件夹所有的文件数）

    signal confirmed(string fileInfo)
    signal canceled()
    signal dialogClosed()

    onVisibleChanged: {
        if (visible) {
            let targetFullPath = getCurrentFolderPath();
            // 加载目录
            kbMoreFileListModel.loadDirectory(targetFullPath)

            // 如果 currentFilesPath 不为空，则解析路径并加载对应的目录
            if (currentFilesPath && currentFilesPath.length > 0) {
                // 添加选中文件的逻辑
                for (var i = 0; i < filteredModel.rowCount(); i++) {
                    var latestItem = filteredModel.getPropertyByIndex(i)
                    if (latestItem) {
                        var itemPath = latestItem.documentTargetPath
                        var isSelected = false
                        // 检查该文件的路径是否在 currentFilesPath 中
                        for (var j = 0; j < currentFilesPath.length; j++) {
                            var targetPath = currentFilesPath[j];
                            // 路径比较（考虑可能的格式差异）
                            if (itemPath && targetPath && itemPath === targetPath) {
                                isSelected = true;
                                break;
                            }
                        }
                        // 根据是否在选中列表中设置 checked 状态
                        filteredModel.setProperty(i, "checked", isSelected);
                    }
                }
                // 更新编辑状态
                editCount = getSelectedCount();
                isEditFlag = true;
                selectAllCheckBox.allChecked = updateSelectAllState()
            } else {
                console.log("从知识库选择首次加载空目录，是否支持全部文件：",isSelectedRootPath);
                if (isSelectedRootPath) {
                    // 知识库问答全选，需要去除空文件夹的勾选
                    checkedAllSelection()
                    // 恢复界面状态
                    editCount = getCurrentSelectedFilesNum()
                    isEditFlag = true
                    selectAllCheckBox.allChecked = Qt.Checked
                    console.log("从知识库选择首次加载空目录，知识库问答全选：",getSelectedCount());
                } else {
                    // 写作模板加载默认目录
                    // 恢复界面状态
                    editCount = 0
                    isEditFlag = false
                    selectAllCheckBox.allChecked = Qt.Unchecked
                }
            }
        }
    }

    FileListModel {
        id: kbMoreFileListModel
        onFolderPathChanged: {
            // 进入新目录，需要清除所有原操作记录
            clearAllSelection()
            selectAllCheckBox.allChecked = Qt.Unchecked
            editCount = 0
            isEditFlag = false
            triggerGrayOutUpdate()
        }
    }
    // 代理模型（始终按创建时间排序）
    KnowledgeBaseFilterProxyModel {
        id: filteredModel
        sourceModel: kbMoreFileListModel

        // 过滤条件
        searchText: searchBoxComponent.textInputText
        fileTypeFilter: isSupportAllFile ? KnowledgeBaseFilterProxyModel.AllFiles : KnowledgeBaseFilterProxyModel.Documents
        statusFilter: KnowledgeBaseFilterProxyModel.Uploaded

        // 默认显示所有文件和目录
        showDirectories: true
        showFiles: true
        foldersFirst: true  // 文件夹优先

        onFilterCountChanged: {
            console.log("显示文件数:", filteredCount, "总文件数:", totalCount);
            showCount = filteredCount
        }
        onDataChanged: {
            selectAllCheckBox.allChecked = updateSelectAllState()
            if (getSelectedCount() === 0) {
                editCount = 0
                isEditFlag = false
            } else {
                editCount = getCurrentSelectedFilesNum()
                isEditFlag = true
            }
            triggerGrayOutUpdate()
        }
    }

    function showDialog() {
        visible = true;
    }

    function hideDialog() {
        visible = false;
        dialogClosed();
        close()
    }

    function escapeHtml(s) {
        if (s === undefined || s === null) return ""
        return String(s)
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#39;")
    }

    // 仅将匹配到的搜索子串加粗，其余文本保持正常样式
    function boldMatchedText(sourceText, queryText) {
        var src = sourceText === undefined || sourceText === null ? "" : String(sourceText)
        var q = queryText === undefined || queryText === null ? "" : String(queryText)
        if (q.length === 0) return escapeHtml(src)

        var lowerSrc = src.toLowerCase()
        var lowerQ = q.toLowerCase()

        var out = ""
        var i = 0
        while (true) {
            var idx = lowerSrc.indexOf(lowerQ, i)
            if (idx === -1) break
            out += escapeHtml(src.substring(i, idx))
            out += "<b>" + escapeHtml(src.substring(idx, idx + q.length)) + "</b>"
            i = idx + q.length
        }
        out += escapeHtml(src.substring(i))
        return out
    }

    // 清空所有选择
    function clearAllSelection() {
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            filteredModel.setProperty(i, "checked", false)
        }
    }
    // 全选，去除空文件夹的选择
    function checkedAllSelection() {
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            var item = filteredModel.getPropertyByIndex(i)
            let childCount = 0;
            if (item && item.isDir) {
                // 根据目录获取子文件数量
                childCount = msgPane.getKnowledgeFileCountByFilepath(item.documentTargetPath)
                if (childCount > 0) {
                    filteredModel.setProperty(i, "checked", true)
                } else {
                    filteredModel.setProperty(i, "checked", false)
                }
            } else {
                filteredModel.setProperty(i, "checked", true)
            }
        }
    }

    Rectangle {
        id: windowRectangle
        anchors.fill: parent
        radius: 12
        focus: true
        // 将键盘事件放在这里
        Keys.onEscapePressed: {
            console.log("ESC pressed")
            hideDialog()
            canceled()
            event.accepted = true
        }

        Keys.onShortcutOverride: {
            if (event.key === Qt.Key_Escape) {
                hideDialog()
                canceled()
                event.accepted = true
            }
        }

        Keys.onReturnPressed: {
            console.log("Enter pressed1, isEditFlag:", isEditFlag)
            if (isEditFlag) {
                confirmBtn.clicked()
                event.accepted = true
            }
        }
        Keys.onEnterPressed: {
            console.log("Enter pressed2, isEditFlag:", isEditFlag)
            if (isEditFlag) {
                confirmBtn.clicked()
                event.accepted = true
            }
        }

        // 窗口阴影效果
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            anchors.bottomMargin: 24

            // 自定义标题栏
            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                height: 40
                Layout.leftMargin: 8
                Layout.rightMargin: 4
                radius: 12

                // 鼠标拖动区域
                MouseArea {
                    anchors.fill: parent
                    property point startDragPos: Qt.point(0, 0)

                    onPressed: {
                        startDragPos = Qt.point(mouse.x, mouse.y)
                    }

                    onPositionChanged: {
                        var delta = Qt.point(mouse.x - startDragPos.x, mouse.y - startDragPos.y)
                        mainWindow.x += delta.x
                        mainWindow.y += delta.y
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    Image {
                        source: "qrc:/icon/icon/knowledge.svg"
                        width: 24
                        height: 24
                    }
                    Label {
                        text: qsTr("Select Knowledge Base file")
                        Layout.alignment: Qt.AlignLeft
                    }

                    Item { Layout.fillWidth: true }

                    // 自定义关闭按钮
                    Button {
                        id: closeButton
                        Layout.preferredHeight:32
                        Layout.preferredWidth:32
                        Layout.minimumHeight:32
                        Layout.minimumWidth:32
                        Layout.maximumHeight:32
                        Layout.maximumWidth:32
                        property var buttonType: "windowTitleBarCloseButton"
                        hoverEnabled: true
                        icon.name: "window-close-symbolic"
                        focusPolicy: Qt.NoFocus
                        flat: true

                        ToolTip {
                            visible: closeButton.hovered
                            text: qsTr("Close")
                            delay: Qt.styleHints.mousePressAndHoldInterval
                        }
                        onClicked: {
                            mainWindow.close()
                        }
                    }
                }
            }

            // 搜索栏
            SearchBox {
                id: searchBoxComponent
                Layout.fillWidth: true
                Layout.rightMargin: 24
                Layout.leftMargin: 24
                Layout.topMargin: 1
                onTextInputTextChanged: {
                    // 清空所有选择
                    clearAllSelection()
                }
            }

            // 主内容区域
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    // 面包屑导航
                    BreadCrumbNavigation {
                        id: kbBreadCrumbNavigation
                        height: 36
                        rootPath: getRootPath()
                        currentPath: getCurrentFolderPath()
                        visible: searchBoxComponent.textInputText === ""
                        onCrumbClicked: {
                            currentPath = fullPath
                            kbMoreFileListModel.loadDirectory(fullPath)
                        }
                    }

                    // 空状态占位图
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: showCount === 0
                        Column{
                            width: parent.width
                            anchors.centerIn: parent
                            Image{
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: searchBoxComponent.textInputText === "" ?
                                            "qrc:/icon/icon/folder-grey.svg" :
                                            "qrc:/icon/icon/no-search-result.png"
                            }
                            Label{
                                text: searchBoxComponent.textInputText === "" ?
                                          qsTr("No content in current directory") :
                                          qsTr("No search results")
                                color: Qt.rgba(0, 0, 0, 0.35)
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                wrapMode: Text.WrapAnywhere
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    // 文件列表区域
                    Rectangle {
                        id: fileListContainer
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        visible: showCount > 0

                        ColumnLayout {
                            anchors.fill: parent

                            // 表头
                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                color: "transparent"

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    spacing: 0

                                    // 自定义全选复选框（不使用CheckBox组件）
                                    Rectangle {
                                        id: selectAllCheckBoxRect
                                        width: 24 + selectAllCheckBoxLabel.implicitWidth
                                        height: 36
                                        color: "transparent"
                                        property bool isDisabled: supportSelectNum === 0
                                        CheckBox {
                                            id: selectAllCheckBox
                                            height: 36
                                            checked: false
                                            Label {
                                                id:selectAllCheckBoxLabel
                                                anchors.left: supportSelectNum === 1 ? parent.left : selectAllCheckBox.indicator.right
                                                anchors.leftMargin: 8
                                                anchors.verticalCenter: selectAllCheckBox.verticalCenter
                                                width: selectAllCheckBoxLabel.implicitWidth
                                                text: qsTr("File Name")
                                            }
                                            indicator: Item {
                                                id: selectAllCheckBoxItem
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 16
                                                height: 16
                                                visible: supportSelectNum !== 1
                                                Image {
                                                    id: selectAllCheckBoxImage
                                                    source: updateIndicator(selectAllCheckBox.allChecked, selectAllCheckBox.isHovered, selectAllCheckBox.isPressed, !selectAllCheckBoxRect.isDisabled)
                                                }
                                            }
                                            // 鼠标悬停和按下状态
                                            property bool isHovered: false
                                            property bool isPressed: false

                                            // 计算选中状态
                                            property int allChecked: updateSelectAllState()

                                            // 监听所有相关属性的变化
                                            onAllCheckedChanged: updateIndicator(allChecked, isHovered, isPressed, !selectAllCheckBoxRect.isDisabled)

                                            // 初始化颜色
                                            Component.onCompleted: updateIndicator(allChecked, isHovered, isPressed, !selectAllCheckBoxRect.isDisabled)
                                        }

                                        // 点击区域
                                        MouseArea {
                                            id: checkboxMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: selectAllCheckBoxRect.isDisabled

                                            onEntered: {
                                                selectAllCheckBox.isHovered = true
                                            }

                                            onExited: {
                                                selectAllCheckBox.isHovered = false
                                            }

                                            onPressed: {
                                                selectAllCheckBox.isPressed = true
                                            }

                                            onReleased: {
                                                selectAllCheckBox.isPressed = false
                                            }

                                            onClicked: {
                                                var shouldSelectAll = (selectAllCheckBox.allChecked !== Qt.Checked)

                                                // 更新所有文件的选择状态，文件夹内容为空就不能设置选中，但是
                                                for (var i = 0; i < filteredModel.rowCount(); i++) {
                                                    var item = filteredModel.getPropertyByIndex(i)
                                                    let childCount = 0;
                                                    if (item && item.isDir) {
                                                        // 根据目录获取子文件数量
                                                        childCount = msgPane.getKnowledgeFileCountByFilepath(item.documentTargetPath)
                                                        if (childCount > 0) {
                                                            filteredModel.setProperty(i, "checked", shouldSelectAll)
                                                        } else {
                                                            filteredModel.setProperty(i, "checked", false)
                                                        }
                                                    } else {
                                                        filteredModel.setProperty(i, "checked", shouldSelectAll)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // 已选择N项
                                    Rectangle {
                                        id: sumLabelRect
                                        height: 36
                                        width: parent.width - selectAllCheckBoxRect.width - createTimeRect.width
                                        color: "transparent"

                                        Label{
                                            id: sumLabel
                                            text: {
                                                if (supportSelectNum === 1) {
                                                    return ""
                                                } else {
                                                    return isEditFlag ? qsTr("(Selected ") + editCount + qsTr(" items)") : ""
                                                }
                                            }
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width
                                            elide: Qt.ElideRight
                                            color: Qt.rgba(0, 0, 0, 0.85)

                                            ToolTip {
                                                visible: sumLabel.truncated && sumLabelRectMouseArea.containsMouse
                                                text: sumLabel.text
                                                delay: Qt.styleHints.mousePressAndHoldInterval
                                                property var maxWidth: 400
                                                property var wrapMode: Text.WrapAnywhere
                                            }

                                        }

                                        MouseArea {
                                            id: sumLabelRectMouseArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                        }
                                    }

                                    // 添加时间
                                    Rectangle {
                                        id: createTimeRect
                                        width: 112 + 16
                                        height: 36
                                        color: "transparent"

                                        Label {
                                            id: createTimeLabel
                                            text: qsTr("Added Time")
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: Qt.rgba(0, 0, 0, 0.85)
                                        }
                                    }
                                }
                            }

                            // 分隔线
                            Rectangle {
                                Layout.fillWidth: true
                                anchors.leftMargin: 8
                                height: 1
                                color: "#e0e0e0"
                            }

                            // 文件列表
                            ScrollView {
                                id: kbFileSelectScrollView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ScrollBar.horizontal.visible : false
                                ScrollBar.vertical.visible : false
                                rightPadding: fileListView.isContentOverflowing ? 16 : 0
                                ListView {
                                    id: fileListView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    spacing: 0
                                    property bool isContentOverflowing: false // 内容是否超出容器高度
                                    boundsMovement: Flickable.StopAtBounds     // 防止过度滚动
                                    model: filteredModel
                                    onContentHeightChanged: {
                                        // 判断内容是否超出容器高度，超出显示滚动条
                                        isContentOverflowing = contentHeight > height
                                        if(isContentOverflowing){
                                            kbFileSelectScrollView.ScrollBar.vertical.visible = true
                                        }else{
                                            kbFileSelectScrollView.ScrollBar.vertical.visible = false
                                        }
                                    }
                                    onHeightChanged: {
                                        // 判断内容是否超出容器高度，超出显示滚动条
                                        isContentOverflowing = contentHeight > height
                                        if(isContentOverflowing){
                                            kbFileSelectScrollView.ScrollBar.vertical.visible = true
                                        }else{
                                            kbFileSelectScrollView.ScrollBar.vertical.visible = false
                                        }
                                    }

                                    // 覆盖整个 ListView 的背景 MouseArea
                                    MouseArea {
                                        anchors.fill: parent
                                        z: -1              // 放在内容下面，避免挡住 delegate
                                        hoverEnabled: true
                                        onClicked: {
                                            clearAllSelection()
                                        }
                                    }

                                    delegate: Rectangle {
                                        id: fileItem
                                        width: fileListView.width
                                        height: 36
                                        radius: 6
                                        color: {
                                            if (checked) {
                                                // 选中：蓝色
                                                return "#3790FA";
                                            } else {
                                                // 非全部可选或者单选的，达到最大可选限制数量文件时，其他置灰
                                                if (shouldGrayOut) {
                                                    // 不支持：浅灰色背景
                                                    return "transparent"
                                                } else {
                                                    // 正常：透明或悬停色
                                                    return isHovered ? "#F6F6F6" : "transparent";
                                                }
                                            }
                                        }
                                        property bool isHovered: false
                                        property bool isPressed: false
                                        // 存储是否需要置灰
                                        property bool shouldGrayOut: false
                                        property bool isEnabled: {
                                            let childCount = 0;
                                            if (model.isDir){
                                                // 根据目录获取子文件数量
                                                childCount = msgPane.getKnowledgeFileCountByFilepath(documentTargetPath)
                                            }
                                            return childCount > 0 || !model.isDir ? true : false
                                        }

                                        // 更新置灰状态的函数
                                        function updateGrayOutState() {
                                            if (supportSelectNum <= 1) {
                                                shouldGrayOut = false
                                            } else if (checked) {
                                                shouldGrayOut = false
                                            } else {
                                                shouldGrayOut = getSelectedCount() >= supportSelectNum
                                            }
                                        }

                                        Row {
                                            id: fileItemRow
                                            anchors.fill: parent
                                            spacing: 8
                                            leftPadding: 8

                                            // 单个文件复选框
                                            Rectangle {
                                                id: singleCheckBoxRect
                                                width: 16
                                                height: parent.height
                                                visible: supportSelectNum !== 1
                                                color: "transparent"
                                                property bool isHovered: false
                                                property bool isPressed: false
                                                property bool isDisabled: (shouldGrayOut || (supportSelectNum > 1 && model.isDir)) || !fileItem.isEnabled

                                                CheckBox {
                                                    id: fileCheckBox
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    checked: model.checked
                                                    enabled: !singleCheckBoxRect.isDisabled

                                                    indicator: Item {
                                                        width: 16
                                                        height: 16
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        Image {
                                                            source: updateIndicator(fileCheckBox.checkState, singleCheckBoxRect.isHovered, singleCheckBoxRect.isPressed, singleCheckBoxRect.isDisabled)
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    enabled: !singleCheckBoxRect.isDisabled

                                                    onEntered: {
                                                        singleCheckBoxRect.isHovered = true
                                                    }

                                                    onExited: {
                                                        singleCheckBoxRect.isHovered = false
                                                    }

                                                    onPressed: {
                                                        singleCheckBoxRect.isPressed = true
                                                    }

                                                    onReleased: {
                                                        singleCheckBoxRect.isPressed = false
                                                    }

                                                    onClicked: {
                                                        var newState = !checked;
                                                        if (supportSelectNum === 0 || !newState){
                                                            filteredModel.setProperty(index, "checked", newState);
                                                        } else {
                                                            if (supportSelectNum > getSelectedCount() && !model.isDir) {
                                                                filteredModel.setProperty(index, "checked", newState);
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // 复选框以外区域 - 使用 Row 布局
                                            Row {
                                                id: contentRow
                                                width: parent.width - (supportSelectNum !== 1 ? 24 : 8) - 8
                                                height: parent.height
                                                spacing: 8

                                                // 文件图标
                                                Item {
                                                    width: 16
                                                    height: parent.height

                                                    Image {
                                                        id: fileIcon
                                                        source: model.fileIcon
                                                        anchors.centerIn: parent
                                                        width: 16
                                                        height: 16
                                                        fillMode: Image.PreserveAspectFit
                                                        opacity: shouldGrayOut || !isEnabled ? 0.35 : 1.0
                                                    }
                                                }

                                                // 文件名称 - 弹性宽度
                                                Item {
                                                    width: parent.width - 20 - (createTime ? 128 : 24) - 16
                                                    height: parent.height

                                                    Label {
                                                        id: fileNameText
                                                        text: boldMatchedText(documentName, searchBoxComponent.textInputText)
                                                        textFormat: Text.StyledText
                                                        color: {
                                                            if (checked) {
                                                                return "#FFFFFF"; // 选中：白色
                                                            } else {
                                                                if (shouldGrayOut || !isEnabled) {
                                                                    return Qt.rgba(0, 0, 0, 0.35); // 不支持：灰色
                                                                } else {
                                                                    return Qt.rgba(0, 0, 0, 0.85); // 正常：黑色
                                                                }
                                                            }
                                                        }
                                                        elide: Text.ElideRight
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        width: parent.width
                                                        // 搜索文本加粗：仅加粗命中的子串（通过 StyledText 内的 <b> 实现）
                                                        ToolTip {
                                                            visible: fileNameText.truncated && contentMouseArea.containsMouse
                                                            text: documentName
                                                            delay: Qt.styleHints.mousePressAndHoldInterval
                                                            property var maxWidth: 400
                                                            property var wrapMode: Text.WrapAnywhere
                                                        }
                                                    }
                                                }

                                                // 添加时间
                                                Item {
                                                    width: 112
                                                    height: parent.height
                                                    visible: createTime

                                                    Label {
                                                        id: fileTimeLabel
                                                        text: createTime ? createTime.substring(0, 16) : ""
                                                        color: checked ? Qt.rgba(255, 255, 255, 0.75) : Qt.rgba(0, 0, 0, 0.35)
                                                        width: parent.width
                                                        font.pointSize: globals.fontsize - 2
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        horizontalAlignment: Text.AlignRight
                                                        elide: Text.ElideRight
                                                    }
                                                }

                                                // 无时间时的占位符
                                                Item {
                                                    width: 24
                                                    height: parent.height
                                                    visible: !createTime
                                                }
                                            }
                                        }

                                        // 覆盖整个内容区域的 MouseArea
                                        MouseArea {
                                            id: contentMouseArea
                                            anchors.left: fileItemRow.left
                                            anchors.leftMargin: supportSelectNum !== 1 ? 24 : 8
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            height: parent.height
                                            hoverEnabled: true
                                            enabled: !shouldGrayOut && fileItem.isEnabled

                                            onEntered: {
                                                fileItem.isHovered = true
                                            }

                                            onExited: {
                                                fileItem.isHovered = false
                                            }

                                            onPressed: {
                                                fileItem.isPressed = true
                                            }

                                            onReleased: {
                                                fileItem.isPressed = false
                                            }

                                            onClicked: {
                                                // 延迟执行单击，等待可能的双击
                                                // 复选框以外区域点击效果等于单选
                                                singleClickTimer.start();
                                            }

                                            onDoubleClicked: {
                                                // 停止单击定时器
                                                singleClickTimer.stop();

                                                // 清除输入框
                                                searchBoxComponent.clearTextInput()

                                                // 双击操作：如果是文件夹则进入，如果是文件就选中
                                                if (model.isDir) {
                                                    kbBreadCrumbNavigation.currentPath = documentTargetPath
                                                    kbMoreFileListModel.loadDirectory(documentTargetPath)
                                                } else {
                                                    for (var i = 0; i < filteredModel.rowCount(); i++) {
                                                        if (i !== index) {
                                                            filteredModel.setProperty(i, "checked", false)
                                                        } else {
                                                            if (supportSelectNum === 0 || !model.isDir) {
                                                                filteredModel.setProperty(index, "checked", true);
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            Timer {
                                                id: singleClickTimer
                                                interval: 200 // 等待双击的时间

                                                onTriggered: {
                                                    for (var i = 0; i < filteredModel.rowCount(); i++) {
                                                        if (i !== index) {
                                                            filteredModel.setProperty(i, "checked", false)
                                                        } else {
                                                            if (supportSelectNum === 0 || !model.isDir) {
                                                                filteredModel.setProperty(index, "checked", true);
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
                    }

                    // 底部按钮区域
                    Item {
                        Layout.fillWidth: true
                        height: 44

                        RowLayout {
                            anchors.fill: parent
                            anchors.topMargin: 8
                            // 知识库管理
                            Label {
                                id: kbFileManagementText
                                color: "#3790FA"
                                text: qsTr("Knowledge Base Management")
                                MouseArea{
                                    id: kbFileManagementTextMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: {
                                        kbFileManagementText.color= "#3489EE"
                                    }
                                    onExited: {
                                        kbFileManagementText.color= "#3790FA"
                                    }
                                    onPressed: {
                                        kbFileManagementText.color= "#2C73C8"
                                    }
                                    onReleased: {
                                        kbFileManagementText.color= "#3790FA"
                                    }
                                    onClicked: {
                                        // 点击跳转到知识库管理模块
                                        hideDialog()
                                        root.onChangeWindowPage(5)
                                    }
                                }
                            }


                            Item { Layout.fillWidth: true }

                            Button {
                                id: cancelBtn
                                text: qsTr("Cancel")
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 36
                                focusPolicy: Qt.NoFocus
                                onClicked: {
                                    canceled();
                                    hideDialog();
                                }
                            }

                            Button {
                                id: confirmBtn
                                text: qsTr("Confirm")
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 36
                                highlighted: true
                                enabled: isEditFlag
                                focusPolicy: Qt.NoFocus
                                onClicked: {
                                    processFileSelection()
                                    hideDialog()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 获取已选择文件数量
    function getSelectedCount() {
        var count = 0
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            // 获取模型索引
            var isChecked = false
            var latestItem = filteredModel.getPropertyByIndex(i)
            if (latestItem) {
                isChecked = latestItem.checked
            }
            if (isChecked) {
                count++
            }
        }
        return count
    }

    // 统一处理文件选择
    function processFileSelection() {
        console.log("开始处理文件选择...");

        // 获取选中数量
        var selectedCount = getSelectedCount();
        var totalCount = filteredModel.rowCount() - getEmptyFolderNum()
        if (selectedCount === 0) {
            console.log("没有选中任何文件");
            return;
        }

        console.log("已选中", selectedCount, "个文件");

        // 获取详细文件信息
        var selectedFiles = [];

        // 知识库问答-当前路径为根目录且非搜索全选，识别为个人知识库
        // To-DO: 面包屑路径需要更新
        if (kbBreadCrumbNavigation.currentPath === getRootPath() &&
                selectedCount === totalCount &&
                isSupportAllFile && searchBoxComponent.textInputText === "") {
            var fileData = {};

            // 确保所有字段都有值
            fileData.documentName = "";
            fileData.documentTargetPath = "";
            fileData.documentSize = "";
            fileData.fileIcon = "";
            fileData.uuid = "";
            fileData.isDir = false;

            selectedFiles.push(fileData);
        } else {
            for (var i = 0; i < filteredModel.rowCount(); i++) {
                // 检查选中状态
                var isChecked = false;
                var latestItem = filteredModel.getPropertyByIndex(i)
                if (latestItem) {
                    isChecked = latestItem.checked
                }

                if (isChecked) {
                    // 收集文件信息
                    var fileInfo = {};

                    // 确保所有字段都有值
                    fileInfo.documentName = latestItem.documentName || "";
                    fileInfo.documentTargetPath = latestItem.documentTargetPath || "";
                    fileInfo.documentSize = latestItem.documentSize || "";
                    fileInfo.fileIcon = latestItem.fileIcon || "";
                    fileInfo.uuid = latestItem.uuid || "";
                    fileInfo.isDir = latestItem.isDir || false

                    selectedFiles.push(fileInfo);
                    console.log("收集文件:", fileInfo.documentName);
                }
            }
        }

        // 构建 JSON
        var jsonData = {
            msgType: "knowledgeFilesSelected",
            msgBody: selectedFiles
        };

        var jsonString = JSON.stringify(jsonData);

        // 发送信号
        confirmed(jsonString);

        // 可选：显示成功消息
        console.log("成功发送", selectedFiles.length, "个文件的选择信息",jsonString);
    }

    function updateSelectAllState() {
        var selectedCount = getSelectedCount()
        var totalCount = filteredModel.rowCount() - getEmptyFolderNum()
        if (selectedCount === 0) {
            return Qt.Unchecked
        } else if (selectedCount === totalCount) {
            return Qt.Checked
        } else {
            return Qt.PartiallyChecked
        }
    }

    // 更新边框和背景颜色 - 考虑所有状态
    function updateIndicator(status, hoverStatus, pressStatus, disableStatus) {
        if (status === Qt.Unchecked) {
            if (disableStatus) {
                return "qrc:/icon/icon/sidebar/noCheck-disable.svg"
            } else {
                if (pressStatus) {
                   // 按下状态
                   return "qrc:/icon/icon/sidebar/noCheck-click.svg"
               } else if (hoverStatus) {
                   // 悬停状态
                   return "qrc:/icon/icon/sidebar/noCheck-hover.svg"
               } else {
                   // 正常状态
                   return "qrc:/icon/icon/sidebar/noCheck-default.svg"
               }
            }
        } else if (status === Qt.PartiallyChecked) {
            if (disableStatus) {
                return "qrc:/icon/icon/part-check-disable.svg"
            } else {
                // 选中或部分选中状态
                if (pressStatus) {
                    return "qrc:/icon/icon/part-check-click.svg"
                } else if (hoverStatus) {
                    return "qrc:/icon/icon/part-check-hover.svg"
                } else {
                    return "qrc:/icon/icon/part-check.svg"
                }
            }
        } else {
            if (disableStatus) {
                return "qrc:/icon/icon/sidebar/CheckBox-disable.svg"
            } else {
                if (pressStatus) {
                    // 按下状态
                    return "qrc:/icon/icon/sidebar/CheckBox-click.svg"
                } else if (hoverStatus) {
                    // 悬停状态
                    return "qrc:/icon/icon/sidebar/CheckBox-hover.svg"
                } else {
                    // 正常状态
                    return "qrc:/icon/icon/sidebar/CheckBox-default.svg"
                }
            }
        }
    }

    // 在每次修改 checked 属性后，调用一个更新函数
    function triggerGrayOutUpdate() {
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            var item = fileListView.itemAtIndex(i)
            if (item && item.updateGrayOutState) {
                item.updateGrayOutState()
            }
        }
    }

    // 获取根目录
    function getRootPath() {
        let rootPath = kbMoreFileListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        return rootPath
    }

    // 获取空文件夹数量
    function getEmptyFolderNum() {
        let count = 0
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            var item = filteredModel.getPropertyByIndex(i)
            if (item && item.isDir) {
                // 根据目录获取子文件数量
                let childCount = 0;
                childCount = msgPane.getKnowledgeFileCountByFilepath(item.documentTargetPath)
                if (childCount === 0) {
                    count++
                }
            }
        }
        return count
    }

    // 获取选中项的所有文件数
    function getCurrentSelectedFilesNum() {
        let currentPageSelectNum = getSelectedCount()
        let folderCount = 0
        let fileCount = 0
        // 减去选中的文件夹数目，加上文件夹内文件数量
        for (var i = 0; i < filteredModel.rowCount(); i++) {
            var item = filteredModel.getPropertyByIndex(i)
            let childCount = 0;
            if (item && item.isDir && item.checked) {
                folderCount++
                // 根据目录获取子文件数量
                childCount = msgPane.getKnowledgeFileCountByFilepath(item.documentTargetPath)
                fileCount = fileCount + childCount
            }
        }

        return currentPageSelectNum + fileCount - folderCount
    }

    function getCurrentFolderPath() {
        let targetFullPath = getRootPath();
        if (currentFilesPath && currentFilesPath.length > 0) {
            var firstFilePath = currentFilesPath[0];
            // 解析文件所在的目录路径
            var filePathStr = firstFilePath.toString();

            // 获取文件所在目录
            var lastSlashIndex = filePathStr.lastIndexOf('/');
            if (lastSlashIndex !== -1) {
                var directoryPath = filePathStr.substring(0, lastSlashIndex);
                targetFullPath = directoryPath;
                console.log("Loading directory from currentFilesPath:", targetFullPath);
            }
        }

        return targetFullPath
    }
}
