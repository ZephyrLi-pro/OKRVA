import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Row{
    id: knowledgeSideOperateArea
    Layout.fillWidth: true
    height: 36

    property bool sideEditFlag: false
    property var sideBreadcrumbPaths: [""]
    property int fileTotelCounts: 0
    property real maxBreadcrumbWidth: 0
    property bool isCanOperateFlag: true
    property bool isInitialized: false
    property int paddingSize: 24
    property int maxBreadcrumbPathWidth: root.isSidebarClose ? 116 : 144

    // 文本宽度计算缓存，避免频繁创建QML对象
    property var textWidthCache: ({})

    onChangeSideCurrentFilePath: {
        var filePath = knowledgeListModel.uploadRootPath
        for(var i = 0; i < sideBreadcrumbPaths.length; i++){
            filePath = filePath + "/" + sideBreadcrumbPaths[i]
        }
        changeSideCurrentFilePath(filePath)
    }

    signal changeSideCurrentFilePath(var currentFilePath)
    signal knowledgeSideOperateAreaTipShow(var iconPath, var message)

    function closePopup(){
        knowledgeBreadcrumbPopup.visible = false
        knowledgeBreadcrumbOperatePopup.visible = false
    }

    function getTextWidth(text, fontSize) {
        if (!text || text === "") {
            return 0;
        }

        var cacheKey = text + "_" + fontSize;

        if (textWidthCache.hasOwnProperty(cacheKey)) {
            return textWidthCache[cacheKey];
        }

        var maxTextLength = 200;
        var textToMeasure = text.length > maxTextLength ? text.substring(0, maxTextLength) : text;

        // 转义双引号避免创建对象时出错
        var safeText = textToMeasure.replace(/"/g, '\\"').replace(/'/g, "\\'");

        // 使用 TextMetrics 而不是动态创建对象（如果可用）
        // 否则使用临时 Text 对象
        var textObj = Qt.createQmlObject('import QtQuick 2.12; Text { font.pointSize: ' + fontSize + '; text: "' + safeText + '"; }', knowledgeSideOperateArea);
        var width = textObj.implicitWidth;
        textObj.destroy();

        // 如果文本被截断，按比例估算完整宽度
        if (text.length > maxTextLength) {
            width = width * (text.length / maxTextLength);
        }

        // 缓存结果（限制缓存大小，避免内存泄漏）
        var cacheSize = Object.keys(textWidthCache).length;
        if (cacheSize < 100) { // 限制缓存大小
            textWidthCache[cacheKey] = width;
        }

        return width;
    }

    // 处理单个层级的显示
    function processBreadcrumbLevel(index, currentPaths, availableWidth) {
        var fontSize = globals.fontsize
        var separator = " > ";
        var separatorWidth = getTextWidth(separator, fontSize);
        var ellipsisWidth = getTextWidth("...", fontSize);
        var allFileText = qsTr("All Files");

        // 如果只有1层，显示"全部文件 > 层级名"
        if (currentPaths.length === 1) {
            var fullText = allFileText + separator + currentPaths[0];
            var fullWidth = getTextWidth(fullText, fontSize);

            if (fullWidth <= availableWidth) {
                return {
                    displayText: fullText,
                    showAllFile: true
                };
            } else {
                // 需要省略，优先省略层级名
                var allFileWidth = getTextWidth(allFileText + separator, fontSize);
                var levelAvailableWidth = availableWidth - allFileWidth;
                var levelText = currentPaths[0];
                var levelWidth = getTextWidth(levelText, fontSize);

                if (levelWidth > levelAvailableWidth) {
                    var truncatedLevel = truncateText(levelText, levelAvailableWidth, fontSize);
                    return {
                        displayText: allFileText + separator + truncatedLevel,
                        showAllFile: true
                    };
                } else {
                    return {
                        displayText: fullText,
                        showAllFile: true
                    };
                }
            }
        }

        // 如果有2层及以上，只显示最后2层
        if (currentPaths.length >= 2) {
            var lastTwoPaths = currentPaths.slice(currentPaths.length - 2);
            var totalSeparatorWidth = separatorWidth; // 只有一个分隔符
            var textAvailableWidth = availableWidth - totalSeparatorWidth;

            if (textAvailableWidth <= 0) {
                return {
                    displayText: "...",
                    showAllFile: false
                };
            }

            // 计算每层的宽度
            var level1Width = getTextWidth(lastTwoPaths[0], fontSize);
            var level2Width = getTextWidth(lastTwoPaths[1], fontSize);
            var totalWidth = level1Width + level2Width;

            if (totalWidth <= textAvailableWidth) {
                // 完全显示
                return {
                    displayText: lastTwoPaths[0] + separator + lastTwoPaths[1],
                    showAllFile: false
                };
            } else {
                // 需要省略，按比例分配宽度
                var ratio1 = level1Width / totalWidth;
                var ratio2 = level2Width / totalWidth;
                var width1 = Math.floor(textAvailableWidth * ratio1);
                var width2 = Math.floor(textAvailableWidth * ratio2);

                // 确保每层至少能显示省略号
                var minWidth = ellipsisWidth + 10;
                width1 = Math.max(minWidth, width1);
                width2 = Math.max(minWidth, width2);

                // 如果分配后超过可用宽度，按比例缩减
                if (width1 + width2 > textAvailableWidth) {
                    var scale = textAvailableWidth / (width1 + width2);
                    width1 = Math.floor(width1 * scale);
                    width2 = Math.floor(width2 * scale);
                    width1 = Math.max(minWidth, width1);
                    width2 = Math.max(minWidth, width2);
                }

                var truncated1 = level1Width > width1 ? truncateText(lastTwoPaths[0], width1, fontSize) : lastTwoPaths[0];
                var truncated2 = level2Width > width2 ? truncateText(lastTwoPaths[1], width2, fontSize) : lastTwoPaths[1];

                return {
                    displayText: truncated1 + separator + truncated2,
                    showAllFile: false
                };
            }
        }

        return {
            displayText: "",
            showAllFile: false
        };
    }

    // 核心函数：处理面包屑文本和层级显示逻辑
    function processBreadcrumbItem(index) {
        var fontSize = globals.fontsize;
        var separator = " > ";
        var separatorWidth = getTextWidth(separator, fontSize);
        var minDisplayLevels = 2; // 最少显示的层级数（不包括"全部文件"）
        var availableWidth = Math.max(0, maxBreadcrumbWidth); // 总可用宽度，确保不为负数

        // 获取当前层级及之前的所有路径，过滤掉空字符串
        var rawPaths = sideBreadcrumbPaths.slice(0, index + 1);
        var allPaths = [];
        for (var i = 0; i < rawPaths.length; i++) {
            if (rawPaths[i] && rawPaths[i] !== "") {
                allPaths.push(rawPaths[i]);
            }
        }

        var allFileText = qsTr("All Files");
        var ellipsisWidth = getTextWidth("...", fontSize);

        // 如果没有任何路径，只显示"全部文件"
        if (allPaths.length === 0) {
            return {
                showAllFile: true,
                displayText: allFileText,
                fullPaths: [],
                processedPaths: [allFileText],
                totalDisplayWidth: getTextWidth(allFileText, fontSize)
            };
        }

        // 如果可用宽度太小，至少显示"全部文件"
        if (availableWidth < 50) {
            return {
                showAllFile: true,
                displayText: allFileText,
                fullPaths: allPaths,
                processedPaths: [allFileText],
                totalDisplayWidth: getTextWidth(allFileText, fontSize)
            };
        }

        // 构建包含"全部文件"的完整路径数组
        var fullPathsWithAllFile = [allFileText].concat(allPaths);

        // 计算完整路径的宽度（不省略）
        var fullTextWithAllFile = fullPathsWithAllFile.join(separator);
        var fullWidthWithAllFile = getTextWidth(fullTextWithAllFile, fontSize);

        // 情况1：如果包含"全部文件"的完整路径不超过可用宽度，完全显示
        if (fullWidthWithAllFile <= availableWidth) {
            return {
                showAllFile: true,
                displayText: fullTextWithAllFile,
                fullPaths: allPaths,
                processedPaths: fullPathsWithAllFile,
                totalDisplayWidth: fullWidthWithAllFile
            };
        }

        // 情况2：超过可用宽度，需要折叠处理
        var pathLevels = allPaths.length;
        var bestDisplayPaths = null;
        var bestShowAllFile = false;

        // 策略1：尝试显示"全部文件" + 所有后续层级（带省略）
        var testPathsWithAllFile = [allFileText].concat(allPaths);
        var resultWithAllFile = optimizePathDisplay(testPathsWithAllFile, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);

        if (resultWithAllFile && resultWithAllFile.totalWidth <= availableWidth) {
            bestDisplayPaths = resultWithAllFile.processedPaths;
            bestShowAllFile = true;
        }

        // 策略2：如果策略1不行，收起"全部文件"，尝试显示所有层级（不包括"全部文件"）
        if (!bestDisplayPaths && pathLevels > 0) {
            var resultAllPaths = optimizePathDisplay(allPaths, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);
            if (resultAllPaths && resultAllPaths.totalWidth <= availableWidth) {
                bestDisplayPaths = resultAllPaths.processedPaths;
                bestShowAllFile = false;
            }
        }

        // 策略3：如果策略2不行，收起前面的层级，只显示最后2层（如果层级数>=2）
        if (!bestDisplayPaths && pathLevels >= minDisplayLevels) {
            var lastTwoPaths = allPaths.slice(allPaths.length - minDisplayLevels);
            var resultLastTwo = optimizePathDisplay(lastTwoPaths, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);

            if (resultLastTwo && resultLastTwo.totalWidth <= availableWidth) {
                bestDisplayPaths = resultLastTwo.processedPaths;
                bestShowAllFile = false;
            }
        }

        // 策略4：如果层级数少于2，显示所有层级（不包括"全部文件"），即使可能稍微超出
        if (!bestDisplayPaths && pathLevels > 0 && pathLevels < minDisplayLevels) {
            var resultAllPaths2 = optimizePathDisplay(allPaths, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);
            bestDisplayPaths = resultAllPaths2 ? resultAllPaths2.processedPaths : allPaths;
            bestShowAllFile = false;
        }

        // 兜底：如果还是找不到合适的方案，强制使用最后2层或所有层级
        if (!bestDisplayPaths) {
            if (pathLevels >= minDisplayLevels) {
                var fallbackPaths = allPaths.slice(allPaths.length - minDisplayLevels);
                var fallbackResult = optimizePathDisplay(fallbackPaths, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);
                bestDisplayPaths = fallbackResult ? fallbackResult.processedPaths : fallbackPaths;
            } else if (pathLevels > 0) {
                var fallbackResult2 = optimizePathDisplay(allPaths, availableWidth, separator, separatorWidth, fontSize, ellipsisWidth);
                bestDisplayPaths = fallbackResult2 ? fallbackResult2.processedPaths : allPaths;
            }
            // 如果还是没有结果，至少显示最后一项
            if (!bestDisplayPaths && pathLevels > 0) {
                bestDisplayPaths = [allPaths[allPaths.length - 1]];
            }
            // 如果还是没有结果，显示"全部文件"
            if (!bestDisplayPaths) {
                bestDisplayPaths = [allFileText];
                bestShowAllFile = true;
            }
            bestShowAllFile = false;
        }

        // 构建最终显示文本
        var finalDisplayText = bestDisplayPaths ? bestDisplayPaths.join(separator) : allFileText;

        // 确保最终显示文本不为空
        if (!finalDisplayText || finalDisplayText === "") {
            finalDisplayText = allFileText;
            bestShowAllFile = true;
            bestDisplayPaths = [allFileText];
        }

        return {
            showAllFile: bestShowAllFile,
            displayText: finalDisplayText,
            fullPaths: allPaths,
            processedPaths: bestDisplayPaths || [allFileText],
            totalDisplayWidth: getTextWidth(finalDisplayText, fontSize)
        };
    }

    // 辅助函数：优化路径显示，确保不超过宽度且尽量占满宽度
    function optimizePathDisplay(paths, maxWidth, separator, separatorWidth, fontSize, ellipsisWidth) {
        if (paths.length === 0) {
            return null;
        }

        var totalSeparatorWidth = (paths.length - 1) * separatorWidth;
        var availableTextWidth = maxWidth - totalSeparatorWidth;

        if (availableTextWidth <= 0) {
            return null;
        }

        var processedPaths = [];

        if (paths.length === 1) {
            // 单层显示：占满宽度并省略
            var singleText = paths[0];
            var singleTextWidth = getTextWidth(singleText, fontSize);

            if (singleTextWidth > availableTextWidth) {
                // 需要省略，使用全部可用宽度
                processedPaths.push(truncateText(singleText, availableTextWidth, fontSize));
            } else {
                processedPaths.push(singleText);
            }
        } else {
            // 多层显示：按比例分配宽度，每层按比例省略
            // 计算每层的原始宽度
            var originalWidths = [];
            var totalOriginalWidth = 0;
            for (var i = 0; i < paths.length; i++) {
                var w = getTextWidth(paths[i], fontSize);
                originalWidths.push(w);
                totalOriginalWidth += w;
            }

            // 如果总宽度不超过可用宽度，完全显示
            if (totalOriginalWidth <= availableTextWidth) {
                processedPaths = paths.slice();
            } else {
                // 需要省略，按比例分配宽度
                // 确保每层至少能显示省略号和一些字符
                var minLevelWidth = ellipsisWidth + 20; // 至少能显示一些字符和省略号
                var totalMinWidth = paths.length * minLevelWidth;

                if (totalMinWidth > availableTextWidth) {
                    // 如果连最小宽度都不够，强制平均分配
                    var perLevelWidth = Math.floor(availableTextWidth / paths.length);
                    var perLevelTextWidth = Math.max(ellipsisWidth, perLevelWidth - ellipsisWidth);
                    for (var k = 0; k < paths.length; k++) {
                        processedPaths.push(truncateText(paths[k], perLevelTextWidth, fontSize));
                    }
                } else {
                    // 按原始宽度比例分配可用宽度
                    var allocatedWidths = [];

                    // 按原始宽度比例分配可用宽度
                    for (var n = 0; n < paths.length; n++) {
                        var ratio = originalWidths[n] / totalOriginalWidth;
                        var allocatedWidth = Math.floor(availableTextWidth * ratio);
                        // 确保至少能显示最小宽度
                        allocatedWidth = Math.max(minLevelWidth, allocatedWidth);
                        allocatedWidths.push(allocatedWidth);
                    }

                    // 检查总分配宽度是否超过可用宽度，如果超过则按比例缩减
                    var totalAllocated = 0;
                    for (var p = 0; p < allocatedWidths.length; p++) {
                        totalAllocated += allocatedWidths[p];
                    }

                    if (totalAllocated > availableTextWidth) {
                        var scale = availableTextWidth / totalAllocated;
                        for (var q = 0; q < allocatedWidths.length; q++) {
                            allocatedWidths[q] = Math.floor(allocatedWidths[q] * scale);
                            // 确保至少能显示最小宽度
                            allocatedWidths[q] = Math.max(minLevelWidth, allocatedWidths[q]);
                        }
                    }

                    // 处理每层文本
                    for (var j = 0; j < paths.length; j++) {
                        var levelText = paths[j];
                        var levelTextWidth = originalWidths[j];
                        var allocatedWidth = allocatedWidths[j];
                        var maxLevelWidth = allocatedWidth - ellipsisWidth;

                        // 确保至少能显示省略号
                        maxLevelWidth = Math.max(ellipsisWidth, maxLevelWidth);

                        if (levelTextWidth > maxLevelWidth) {
                            // 需要省略
                            processedPaths.push(truncateText(levelText, maxLevelWidth, fontSize));
                        } else {
                            processedPaths.push(levelText);
                        }
                    }
                }
            }
        }

        // 计算最终宽度
        var finalText = processedPaths.join(separator);
        var totalWidth = getTextWidth(finalText, fontSize);

        return {
            processedPaths: processedPaths,
            totalWidth: totalWidth
        };
    }

    // 辅助函数：截断文本并添加省略号（优化版本，减少 getTextWidth 调用）
    function truncateText(text, maxWidth, fontSize) {
        if (maxWidth <= 0 || !text || text === "") {
            return "...";
        }

        // 缓存省略号宽度
        var ellipsisWidth = getTextWidth("...", fontSize);
        var maxTextWidth = maxWidth - ellipsisWidth;

        if (maxTextWidth <= 0) {
            return "...";
        }

        // 快速路径：如果文本很短，直接返回
        if (text.length <= 5) {
            var shortWidth = getTextWidth(text, fontSize);
            if (shortWidth <= maxWidth) {
                return text;
            }
            return "...";
        }

        // 使用二分查找优化，减少 getTextWidth 调用次数
        var left = 0;
        var right = text.length;
        var bestIndex = 0;
        var iterations = 0;
        var maxIterations = 20; // 限制迭代次数，避免无限循环

        while (left < right && iterations < maxIterations) {
            iterations++;
            var mid = Math.floor((left + right) / 2);
            var testText = text.substring(0, mid);
            var testWidth = getTextWidth(testText, fontSize);

            if (testWidth <= maxTextWidth) {
                bestIndex = mid;
                left = mid + 1;
            } else {
                right = mid;
            }
        }

        // 如果找到了合适的索引
        if (bestIndex > 0 && bestIndex < text.length) {
            return text.substring(0, bestIndex) + "...";
        }

        // 如果文本本身就不超过宽度，直接返回
        var fullWidth = getTextWidth(text, fontSize);
        if (fullWidth <= maxWidth) {
            return text;
        }

        // 兜底：返回省略号
        return "...";
    }


    function refreshBreadcrumb() {
       // 如果组件未完全初始化，延迟执行
       if (!isInitialized) {
           Qt.callLater(refreshBreadcrumb);
           return;
       }

       let jsonStr = knowledgeListModel.currentFolderPathJson;
       let newPaths = JSON.parse(jsonStr);
       // 过滤掉空字符串，确保数组只包含有效的路径
       var validPaths = [];
       if (Array.isArray(newPaths)) {
           for (var i = 0; i < newPaths.length; i++) {
               if (newPaths[i] && newPaths[i] !== "") {
                   validPaths.push(newPaths[i]);
               }
           }
       }

       // 比较新旧值，避免不必要的更新导致循环依赖
       var pathsChanged = false;
       if (sideBreadcrumbPaths.length !== validPaths.length) {
           pathsChanged = true;
       } else {
           for (var j = 0; j < validPaths.length; j++) {
               if (sideBreadcrumbPaths[j] !== validPaths[j]) {
                   pathsChanged = true;
                   break;
               }
           }
       }

       // 只有在值真正改变时才更新，避免触发不必要的绑定重新计算
       if (pathsChanged) {
           // 使用 Qt.callLater 延迟更新，避免在布局计算过程中更新导致循环
           Qt.callLater(function() {
               knowledgeSideOperateArea.sideBreadcrumbPaths = validPaths;
               // 更新 maxBreadcrumbWidth
               updateMaxBreadcrumbWidth();
           });
       }
    }

    // 更新 maxBreadcrumbWidth 的辅助函数
    function updateMaxBreadcrumbWidth() {
        if (!isInitialized || breadcrumbRow.width <= 0) {
            return;
        }
        var allFileWidth = allFileLabelRet.visible ? allFileLabelRet.width : 0;
        var newMaxWidth = Math.max(0, breadcrumbRow.width - allFileWidth - (sideBreadcrumbPaths.length > 1 ? 36 : 0) - 10);
        // 只有当值真正改变时才更新，避免循环
        if (Math.abs(maxBreadcrumbWidth - newMaxWidth) > 0.1) {
            maxBreadcrumbWidth = newMaxWidth;
        }
    }

    function refreshListmodel(){
        let rootPath = knowledgeListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        let targetFullPath = rootPath;
        for (let i = 0; i <= knowledgeSideOperateArea.sideBreadcrumbPaths.length; i++) {
            if (knowledgeSideOperateArea.sideBreadcrumbPaths[i]) {
                targetFullPath += "/" + knowledgeSideOperateArea.sideBreadcrumbPaths[i];
            }
        }

        quitEdit()
        knowledgeListModel.loadDirectory(targetFullPath);
    }

    Component.onCompleted: {
        // 等待组件完全初始化后再执行
        Qt.callLater(function() {
            // 确保 breadcrumbRow 已经初始化且有有效宽度
            if (breadcrumbRow.width > 0) {
                isInitialized = true;
                refreshBreadcrumb();
                updateMaxBreadcrumbWidth();
            } else {
                // 如果宽度还未确定，再延迟一次
                Qt.callLater(function() {
                    isInitialized = true;
                    refreshBreadcrumb();
                    updateMaxBreadcrumbWidth();
                });
            }
        });
    }

    Connections {
        target: knowledgeListModel
        onFolderPathChanged: {
            Qt.callLater(refreshBreadcrumb);
        }
    }

    // 监听 sideBreadcrumbPaths 变化，更新 maxBreadcrumbWidth
    onSideBreadcrumbPathsChanged: {
        if (isInitialized) {
            Qt.callLater(function() {
                updateMaxBreadcrumbWidth();
            });
        }
    }

    function openAllFileCommon(){
        let rootPath = knowledgeListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        let targetFullPath = rootPath;
        if(sideBreadcrumbPaths.length === 0){
            return
        }
        quitEdit()
        knowledgeListModel.loadDirectory(targetFullPath);
    }

    function openBreadcrumbPathCommon(){
        let rootPath = knowledgeListModel.uploadRootPath;
        rootPath = rootPath.replace(/\/$/, "");
        let targetFullPath = rootPath;
        const currentIndex = 0;
        if (currentIndex >= 0) {
            for (let i = 0; i <= currentIndex; i++) {
                if (knowledgeSideOperateArea.sideBreadcrumbPaths[i]) {
                    targetFullPath += "/" + knowledgeSideOperateArea.sideBreadcrumbPaths[i];
                }
            }
        }

        if(0 === (sideBreadcrumbPaths.length - 1)){
            return
        }

        quitEdit()
        knowledgeListModel.loadDirectory(targetFullPath);
    }

    function moveFilesCommon(){
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

    function copyFilesCommon(){
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

    function deleteFilesCommon(){
        var options = []
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
        if (dialogComponent.status === Component.Ready) {
            let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
            dialog.selectedOptions = options
            dialog.questionText = qsTr("Are you sure you want to delete the file?")
            dialog.infoText = qsTr("After deletion, this operation cannot be undone and you will not be able to perform knowledge Q&A on the file content.")
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
                        knowledgeSideOperateAreaTipShow("qrc:/icon/icon/dialog-error.svg", qsTr("Delete failed. The knowledge base service is unavailable, please try again later"))
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

    KnowledgeBreadcrumbPopup{
        id: knowledgeBreadcrumbPopup
        firstBreadcrumb: sideBreadcrumbPaths.length > 2 ? sideBreadcrumbPaths[0] : ""
        onOpenAllFile: function(){
            openAllFileCommon()
        }
        onOpenBreadcrumbPath: function(){
            openBreadcrumbPathCommon()
        }
        onVisibleChanged: {
            if(!visible){
                breadcrumbLeftImage.source = "qrc:/icon/icon/knowledge-bread-right.svg"
            }
        }
    }

    KnowledgeBreadcrumbOperatePopup{
        id: knowledgeBreadcrumbOperatePopup
        popupType: sideEditFlag ? 2 : 1
        showNewFolderFlag: sideBreadcrumbPaths.length > 2 ? false : true
        newFileShowFlag: fileTotelCounts >= 100 ? false :　true
        isCanOperateFlag: isCanOperate
        onMoveFile: function(){
            moveFilesCommon()
        }
        onCopyFile: function(){
            copyFilesCommon()
        }
        onDeleteFile: function(){
            deleteFilesCommon()
        }
        onNewFloder: function(){
            requestToCreateDir()
        }
        onAddFile: function(){
            requestToUploadDocument()
        }
        onVisibleChanged: {
            if(!visible){
                breadcrumbOperateRightRet.color = "transparent"
            }else{
                breadcrumbOperateRightRet.color = Qt.rgba(200/255, 200/255, 200/255, 1)
            }
        }
    }

    signal requestToUploadDocument()
    signal requestToCreateDir()
    signal deleteSuccess()
    signal moveFiles(var filePaths)
    signal copyFiles(var filePaths)
    signal quitEdit()

    Row{
        id: knowledgeOperateAreaSide
        width: parent.width
        height: 36
        leftPadding: paddingSize
        rightPadding: paddingSize
        spacing: 12
        visible: root.winMode === 2
        Row{
            id: breadcrumbRow
            height: parent.height
            width: Math.max(0, knowledgeOperateAreaSide.width -
                               (breadcrumbOperateRightRet.visible ? breadcrumbOperateRightRet.width + knowledgeOperateAreaSide.spacing : 0) -
                               knowledgeOperateAreaSide.leftPadding - knowledgeOperateAreaSide.rightPadding)
            spacing: 0

            // 监听宽度变化，重新计算面包屑显示
            // 只有在组件初始化完成后才更新，避免初始化时的循环依赖
            onWidthChanged: {
                if (knowledgeSideOperateArea.isInitialized && width > 0) {
                    Qt.callLater(function() {
                        knowledgeSideOperateArea.updateMaxBreadcrumbWidth();
                    });
                }
            }

            // 左侧折叠按钮：当有2层及以上时显示，点击可以展开显示"全部文件"
            Rectangle{
                id: breadcrumbLeftRet
                height: 36
                width: 36
                color: Qt.rgba(220/255, 220/255, 220/255, 1)
                radius: 6
                visible: sideBreadcrumbPaths.length > 1
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
                        knowledgeBreadcrumbPopup.x = commonPadding      //显示在按钮最左侧
                        knowledgeBreadcrumbPopup.y = breadcrumbLeftRet.height + 6 //显示在按钮下方，留6的缝隙
                        knowledgeBreadcrumbPopup.open()
                    }
                }
            }

            // "全部文件"标签：只在有1层时显示
            Rectangle{
                id: allFileLabelRet
                width: allFileLabelRow.width > 194 ? 194 : allFileLabelRow.implicitWidth
                height: parent.height
                color: "transparent"
                radius: 6
                visible: sideBreadcrumbPaths.length <= 1
                Row{
                    id: allFileLabelRow
                    anchors.verticalCenter: parent.verticalCenter
                    Label {
                        id: allFileLabel
                        text: qsTr("All Files  ")
                        color: sideBreadcrumbPaths.length === 0 ? Qt.rgba(0/255, 0/255, 0/255, 0.85) :Qt.rgba(0/255, 0/255, 0/255, 0.6)
                        anchors.verticalCenter: parent.verticalCenter
                        leftPadding: 6
                        elide: Text.ElideRight
                        font.bold: sideBreadcrumbPaths.length === 0 ? true : false
                        font.family: root.fontFamily
                        font.pointSize: globals.fontsize
                        maximumLineCount: 1
                        wrapMode: Text.NoWrap
                        width: Math.min(194 - separatorLabel.width, implicitWidth)
                    }
                    // 分隔符：只在有1层时显示
                    Label {
                        id: separatorLabel
                        text: "  >    "
                        color: Qt.rgba(0/255, 0/255, 0/255, 0.6)
                        anchors.verticalCenter: parent.verticalCenter
                        visible: sideBreadcrumbPaths.length >= 1
                    }
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
                        if(sideBreadcrumbPaths.length === 0){
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
                        allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
//                        if(sideBreadcrumbPaths.length === 0){
//                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
//                        }else{
//                            allFileLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.6)
//                        }
                        allFileLabelRet.color = "transparent"
                    }
                    onClicked: {
                        openAllFileCommon()
                    }
                }
            }

            // 面包屑容器：显示层级，每个层级独立可点击
            Row {
                id: breadcrumbContainer
                height: parent.height
                spacing: 0
                width: {
                    var totalWidth = breadcrumbRow.width;
                    var leftBtnWidth = breadcrumbLeftRet.visible ? 36 : 0;
                    var allFileWidth = allFileLabelRet.visible ? allFileLabelRet.width : 0;
                    var separatorWidth = separatorLabel.visible ? separatorLabel.implicitWidth : 0;
                    var rightBtnWidth = breadcrumbOperateRightRet.visible ? 36 + 12 : 0;
                    return Math.max(0, totalWidth - leftBtnWidth - allFileWidth - separatorWidth - rightBtnWidth);
                }

                // 当只有1层时，显示该层级；当有2层及以上时，只显示最后2层
                Repeater {
                    id: breadcrumbRepeater
                    model: {
                        if (sideBreadcrumbPaths.length === 1) {
                            // 只有1层，显示该层级
                            return [0];
                        } else if (sideBreadcrumbPaths.length >= 2) {
                            // 有2层及以上，只显示最后2层
                            return [sideBreadcrumbPaths.length - 2, sideBreadcrumbPaths.length - 1];
                        } else {
                            return [];
                        }
                    }

                    delegate: Row {
                        height: parent.height
                        spacing: 0

                        // 层级按钮（包含文字和分隔符，作为一个整体三态控件）
                        Rectangle {
                            id: levelBtn
                            height: parent.height
                            width: Math.min(maxBreadcrumbPathWidth, levelBtnRow.width)
                            radius: 6
                            color: "transparent"
                            // 悬浮提示框
                            ToolTip {
                                id: firstBreadcrumbRetTooltip
                                visible: levelBtnMouseArea.containsMouse && levelLabelMertics.width > levelLabel.width
                                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                                text: levelLabel.text
                                property var maxWidth: 400
                                property var wrapMode: Text.WordWrap
                            }
                            Row {
                                id: levelBtnRow
                                anchors.centerIn: parent
                                spacing: 0
                                width: Math.min(maxBreadcrumbPathWidth,
                                                (levelLabelMertics.width + levelLabel.leftPadding + levelLabel.rightPadding + 12 + (itemSeparatorLabel.visible ?　itemSeparatorLabel.width : 0)))
                                Label {
                                    id: levelLabel
                                    text: {
                                        var levelIndex = modelData;
                                        if (levelIndex >= 0 && levelIndex < sideBreadcrumbPaths.length) {
                                            return sideBreadcrumbPaths[levelIndex] || "";
                                        }
                                        return "";
                                    }
                                    color: modelData === (sideBreadcrumbPaths.length - 1)
                                           ? Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                           : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    font.bold: modelData === (sideBreadcrumbPaths.length - 1)
                                    font.family: root.fontFamily
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    leftPadding: {
                                        if(index < (breadcrumbRepeater.count - 1)){
                                            return 6
                                        }else{
                                            return 6
                                        }
                                    }
                                    rightPadding: {
                                        if(index < (breadcrumbRepeater.count - 1)){
                                            return 0
                                        }else{
                                            return 6
                                        }
                                    }

                                    width: levelBtn.width - (itemSeparatorLabel.visible ? itemSeparatorLabel.width : 0)
                                }

                                // 分隔符放在按钮内部，最后一个不显示
                                Label {
                                    id: itemSeparatorLabel
                                    text: "  >    "
                                    color: Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                    visible: index < (breadcrumbRepeater.count - 1)
                                }
                            }

                            Label {
                                id: levelLabelMertics
                                text: levelLabel.text
                                visible: false
                            }

                            MouseArea {
                                id: levelBtnMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onEntered: {
                                    levelBtn.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                                    levelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                }
                                onExited: {
                                    levelBtn.color = "transparent"
                                    levelLabel.color = modelData === (sideBreadcrumbPaths.length - 1)
                                            ? Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                }
                                onPressed: {
                                    levelBtn.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                                    levelLabel.color = Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                }
                                onReleased: {
                                    levelBtn.color = "transparent"
                                    levelLabel.color = modelData === (sideBreadcrumbPaths.length - 1)
                                            ? Qt.rgba(0/255, 0/255, 0/255, 0.85)
                                            : Qt.rgba(0/255, 0/255, 0/255, 0.6)
                                }

                                onClicked: {
                                    let rootPath = knowledgeListModel.uploadRootPath;
                                    rootPath = rootPath.replace(/\/$/, "");
                                    let targetFullPath = rootPath;
                                    var levelIndex = modelData;
                                    for (let i = 0; i <= levelIndex; i++) {
                                        if (knowledgeSideOperateArea.sideBreadcrumbPaths[i]) {
                                            targetFullPath += "/" + knowledgeSideOperateArea.sideBreadcrumbPaths[i];
                                        }
                                    }

                                    if (levelIndex === (sideBreadcrumbPaths.length - 1)) {
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
            id: breadcrumbOperateRightRet
            height: 36
            width: 36
            color: "transparent"//Qt.rgba(220/255, 220/255, 220/255, 1)
            radius: 6
            visible: true
            antialiasing: true
            
            ToolTip {
                id: breadcrumbOperateRightRetTooltip
                visible: breadcrumbOperateRightRetMouseArea.containsMouse && knowledgeBreadcrumbOperatePopup.visible === false
                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                text: qsTr("More")
                property var maxWidth: 400
                property var wrapMode: Text.WrapAnywhere
            }
            Image {
                id: breadcrumbOperateRightImage
                source: "qrc:/icon/icon/operate-omit.svg"
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                width: 16
                height: 16
            }
            MouseArea{
                id: breadcrumbOperateRightRetMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onEntered: {
                    breadcrumbOperateRightRet.color = Qt.rgba(200/255, 200/255, 200/255, 1)
                }
                onExited: {
                    //breadcrumbOperateRightRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                    if(knowledgeBreadcrumbOperatePopup.visible){
                        breadcrumbOperateRightRet.color = Qt.rgba(200/255, 200/255, 200/255, 1)
                        return
                    }
                    breadcrumbOperateRightRet.color = "transparent"
                }
                onPressed: {
                    breadcrumbOperateRightRet.color = Qt.rgba(185/255, 185/255, 185/255, 1)
                }
                onReleased: {
                    //breadcrumbOperateRightRet.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                    if(knowledgeBreadcrumbOperatePopup.visible){
                        breadcrumbOperateRightRet.color = Qt.rgba(200/255, 200/255, 200/255, 1)
                        return
                    }

                    breadcrumbOperateRightRet.color = "transparent"
                }
                onClicked: {
                    // 将按钮坐标转换为相对于 knowledgeSideOperateArea 的坐标
                    var buttonPos = breadcrumbOperateRightRet.mapToItem(knowledgeSideOperateArea, 0, 0)
                    // 计算菜单位置：按钮下方6px，右对齐
                    var popX = buttonPos.x + breadcrumbOperateRightRet.width - knowledgeBreadcrumbOperatePopup.width
                    var popY = buttonPos.y + breadcrumbOperateRightRet.height + 6
                    knowledgeBreadcrumbOperatePopup.x = popX
                    knowledgeBreadcrumbOperatePopup.y = popY
                    knowledgeBreadcrumbOperatePopup.open()
                }
            }
        }
    }
}

