import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."

Window {
    id: newFileFolderWindow
    width: 376
    height: 216
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    visible: false
    color: "transparent"

    property var alreadyCreateFileName: [] // 已经存在的文件名集合
    property var alreadyCreateFolderName: [] // 已经存在的文件夹集合
    property int windowType: 1 // 1:新建文件夹弹窗  2：文件重命名弹窗
    property string renameFileName: "" // 重命名传进来文件名
    property string fileNamePrefix: ""
    property string newFolderName: ""

    onRenameFileNameChanged: {
        if(renameFileName !== ""){
            lineEdit.text = getFileNamePrefix(renameFileName)
            getSuffix(renameFileName)
        }
    }

    signal confirmed(string fileName)
    signal canceled()
    signal dialogClosed()

    function showDialog() {
        visible = true
        if(windowType === 1 && renameFileName === ""){
            newFolderName = generateNewFolderName()
            lineEdit.text = newFolderName
        }
        lineEdit.forceActiveFocus()
        lineEdit.selectAll()
    }

    function hideDialog() {
        visible = false
        dialogClosed()
    }

    // 获取文件后缀名
    function getSuffix(fileName) {
        if (!fileName || fileName.trim() === "") {
            return "";
        }
        // 2. 找到最后一个小数点的位置
        const lastDotIndex = fileName.lastIndexOf('.');
        // 3. 处理各种边界情况
        if (lastDotIndex === -1) {
            // 情况1：没有小数点（无扩展名），返回原文件名
            return ""
        } else if (lastDotIndex === 0) {
            // 情况2：小数点在开头（如 .gitignore），返回原文件名
            return ""
        } else if (lastDotIndex === fileName.length - 1) {
            // 情况3：小数点在结尾（如 key.），返回去掉结尾小数点的部分
            return ""
        } else {
            // 情况4：正常情况（如 key.txt、key.tar.gz），截取到最后一个小数点前
            fileNamePrefix = fileName.substring(lastDotIndex, fileName.length)
        }
    }

    // 获取文件名称，不包含文件后缀
    function getFileNamePrefix(fileName) {
        // 1. 空值校验
        if (!fileName || fileName.trim() === "") {
            return "";
        }

        // 2. 找到最后一个小数点的位置
        const lastDotIndex = fileName.lastIndexOf('.');

        // 3. 处理各种边界情况
        if (lastDotIndex === -1) {
            // 情况1：没有小数点（无扩展名），返回原文件名
            return fileName;
        } else if (lastDotIndex === 0) {
            // 情况2：小数点在开头（如 .gitignore），返回原文件名
            return fileName;
        } else if (lastDotIndex === fileName.length - 1) {
            // 情况3：小数点在结尾（如 key.），返回去掉结尾小数点的部分
            return fileName.substring(0, fileName.length - 1);
        } else {
            return fileName.substring(0, lastDotIndex);
        }
    }

    // 核心函数：生成下一个可用的文件夹名称
    function generateNewFolderName() {
        // 基础名称
        const baseName = "新建文件夹";
        const copySuffixPrefix = "-副本(";
        const copySuffixSuffix = ")";

        // 第一步：检查基础名称是否可用
        if (!alreadyCreateFolderName.includes(baseName)) {
            return baseName;
        }

        // 第二步：提取所有已存在的副本序号，并找出最大序号+填补空缺
        let existingNumbers = [];
        // 遍历已存在的文件名，匹配副本命名规则
        for (let fileName of alreadyCreateFolderName) {
            // 检查是否以"新建文件夹-副本("开头，以")"结尾
            if (fileName.startsWith(baseName + copySuffixPrefix) && fileName.endsWith(copySuffixSuffix)) {
                // 截取括号内的数字部分
                const numStr = fileName.substring(
                    baseName.length + copySuffixPrefix.length,
                    fileName.length - copySuffixSuffix.length
                );
                // 转换为数字并验证是否为有效整数
                const num = parseInt(numStr);
                if (!isNaN(num) && num > 0) {
                    existingNumbers.push(num);
                }
            }
        }

        // 第三步：查找最小的未使用正整数序号
        let newNumber = 1;
        while (existingNumbers.includes(newNumber)) {
            newNumber++;
        }

        // 第四步：拼接新的文件夹名称
        return `${baseName}${copySuffixPrefix}${newNumber}${copySuffixSuffix}`;
    }

    // 截取255字节内的文字
    function getSubStringText(text){
        for(let i = 0; i < text.length; i++){
            var subText = text.substring(0, text.length - i)
            let textBit = msgPane.getTextBit(subText)
            if(textBit <= 255){
                return subText
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "white"
        focus: true

        Keys.onEscapePressed: {
            canceled();
            hideDialog();
        }

        Keys.onEnterPressed: {
            confirmBtn.clicked()
        }

        Keys.onReturnPressed: {
            // 默认按回车键响应确认按钮
            console.log("*****Return*******")
            if (confirmBtn.visible) {
                confirmBtn.clicked()
            }
        }

        Rectangle {
            id: titleBar
            height: 40
            color: "white"
            radius: 12
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }

            // 鼠标拖动区域，需要放在前面，不然关闭按钮触发不了click
            MouseArea {
                anchors.fill: parent
                property point startDragPos: Qt.point(0, 0)

                onPressed: {
                    startDragPos = Qt.point(mouse.x, mouse.y)
                }

                onPositionChanged: {
                    var delta = Qt.point(mouse.x - startDragPos.x, mouse.y - startDragPos.y)
                    newFileFolderWindow.x += delta.x
                    newFileFolderWindow.y += delta.y
                }
            }

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
                property string buttonType: "closeButton"

                ToolTip {
                    id: closeButtonTooltip
                    text: qsTr("close")
                    delay: Qt.styleHints.mousePressAndHoldInterval
                }

                onHoveredChanged: {
                    if(hovered)
                        closeButtonTooltip.visible = true
                    else
                        closeButtonTooltip.visible = false
                }

                onClicked: {
                    canceled()
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
            anchors.fill: parent
            anchors.topMargin: titleBar.height
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.bottomMargin: 24
            spacing: 0

            Label {
                id: inputNametext
                Accessible.name: "kylin-aiassistant_Knowledge_newFileFolderWindow_inputNametext"
                Layout.preferredHeight: 20
                text: windowType === 1 ? qsTr("Enter folder name") : qsTr("Rename file")
                Layout.alignment: Qt.AlignLeft
            }

            Item { Layout.preferredHeight: 9 }

            TextField {
                id: lineEdit
                Accessible.name: "kylin-aiassistant_Knowledge_newFileFolderWindow_lineEdit"
                Layout.preferredWidth: 328
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignHCenter
                placeholderText: windowType === 1 ? "" : qsTr("Required")
                // 自定义样式
                background: Rectangle {
                    color: "#ffffff"
                    border.color: "#3790FA"
                    border.width: 2
                    radius: 8
                }

                // 合法文本缓存（避免重复过滤）
                property string lastLegalText: ""

                onFocusChanged: {
                    if(lineEdit.focus === true){
                        lineEdit.selectAll()
                    }
                }
                Keys.onPressed: {
                    let calText = ""
                    if(fileNamePrefix !== ""){
                        calText = text + fileNamePrefix
                    }else{
                        calText = text
                    }
                    let local8Bit = msgPane.getTextBit(calText)
                    const lineCount = local8Bit >= 255
                    const isIllegal = (() => {
                        // 1.基础非法字符（直接通过text判断）
                        const illegalText = ["\\", "/", ":", "*", "?", "\"", "<", ">", "|"];
                        if (illegalText.includes(event.text)) return true;

                        // 2. 小键盘特殊按键（无text，仅靠key判断）
                        const numpadIllegalKeys = [Qt.Key_Asterisk, Qt.Key_Division]; // 小键盘*、/
                        if (numpadIllegalKeys.includes(event.key)) return true;

                        // 3. Shift组合键对应的非法字符（需判断Shift+key）
                        const shiftIllegalMap = new Map([
                            [Qt.Key_8, "*"],        // Shift+8 → *
                            [Qt.Key_Slash, "?"],    // Shift+/ → ?
                            [Qt.Key_Semicolon, ":"],// Shift+; → :
                            [Qt.Key_Comma, "<"],    // Shift+, → <
                            [Qt.Key_Period, ">"],   // Shift+. → >
                            [Qt.Key_Backslash, "|"],// Shift+\ → |
                            [Qt.Key_Apostrophe, "\""]// Shift+' → "
                        ]);
                        // 4.判断是否是Shift+非法键组合
                        if (event.modifiers & Qt.ShiftModifier) {
                            if (shiftIllegalMap.has(event.key)) return true;
                        }

                        // 5. 兜底：直接匹配key（针对无text的场景）
                        const directIllegalKeys = [Qt.Key_Backslash, Qt.Key_Slash]; // \、/的key
                        if (directIllegalKeys.includes(event.key)) return true;

                        return false;
                    })();

                    // 1. 合法操作放行（Ctrl/Alt/Shift组合键、编辑键等）
                    const isLegalShortcut = (
                        event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier) ||
                        event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete ||
                        (event.key >= Qt.Key_Left && event.key <= Qt.Key_Down) || event.key === Qt.Key_Enter || event.key === Qt.Key_Return
                    );

                    // 2. 单独处理 Tab 键：手动传递焦点到下一个组件（取消按钮）
                    if (event.key === Qt.Key_Tab) {
                        event.accepted = true; // 拦截默认行为（避免失去焦点但不跳转）
                        cancelBtn.forceActiveFocus(); // 焦点直接跳转到取消按钮
                        return; // 终止后续逻辑
                    }

                    // 3. 拦截非法按键
                    if (isIllegal && !isLegalShortcut && !lineCount) {
                        event.accepted = true; // 阻止输入
                        lenthLimittext.text = qsTr("File name cannot contain the following characters: \\/:*?\"<>|")
                    }
                }

                onTextChanged: {
                    confirmBtn.isFileNameRepeat = false
                    if (text === lastLegalText) return;
                    // 先去除换行符，在进行非法字符校验
                    var legalTextReg = text.replace(/\n/g, "")  // 去除换行符
                    legalTextReg = legalTextReg.replace(/[\n]+/g, '')
                    legalTextReg = legalTextReg.replace(/[\t]+/g, '')
                    legalTextReg = legalTextReg.replace(/[\s]+/g, '')
                    legalTextReg = legalTextReg.replace(/[\n]/g, '')
                    legalTextReg = legalTextReg.replace(/[\t]/g, '')
                    // 过滤非法字符
                    const illegalReg = /[\\/:*?"<>|]/g;
                    var legalText = legalTextReg.replace(illegalReg, "");
                    const hasFiltered = legalText.length !== text.length || legalTextReg.length !== text.length;

                    if (hasFiltered) {
                        lineEdit.text = legalText;
                        lineEdit.cursorPosition = legalText.length;
                        lastLegalText = legalText; // 更新缓存                       
                    } else {
                        lastLegalText = text; // 无过滤，更新缓存
                    }

                    let calText = ""
                    if(fileNamePrefix !== ""){
                        calText = text + fileNamePrefix
                    }else{
                        calText = text
                    }
                    let local8Bit = msgPane.getTextBit(calText)

                    if(local8Bit > 255){
                        //lineEdit.text = lineEdit.text.substring(0, 100); // 截取前100字符
                        lineEdit.text = getSubStringText(lineEdit.text)
                        lineEdit.cursorPosition = lineEdit.text.length
                        lenthLimittext.text = qsTr("Length cannot exceed: 85 Chinese characters or 255 English characters")
                    }else{
                        if(hasFiltered){
                            lenthLimittext.text = qsTr("File name cannot contain the following characters: \\/:*?\"<>|")
                        }else{
                            lenthLimittext.text = ""
                        }
                    }
                    if(text.length === 0){
                        confirmBtn.isFileNameRepeat = true
                    }
                }
            }

            Item { Layout.preferredHeight: 1 }

            Label {
                id: lenthLimittext
                Layout.preferredWidth: 122
                Layout.preferredHeight: 17
                Layout.fillWidth: true
                text: ""
                color: "#F53F3F"
                font.pointSize: globals.fontsize - 2
                opacity: lenthLimittext.text.length > 0
            }

            Item { Layout.preferredHeight: 6 }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Button {
                    id: cancelBtn
                    Accessible.name: "kylin-aiassistant_Knowledge_newFileFolderWindow_cancelBtn"
                    text: qsTr("Cancel")
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        canceled();
                        hideDialog();
                    }

                    Keys.onReturnPressed: {
                        if(cancelBtn.focus === true){
                            cancelBtn.clicked()
                        }
                    }
                }

                Button {
                    id: confirmBtn
                    Accessible.name: "kylin-aiassistant_Knowledge_newFileFolderWindow_confirmBtn"
                    text: qsTr("Confirm")
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    highlighted: true
                    visible: !isFileNameRepeat
                    focusPolicy: Qt.NoFocus

                    property bool isFileNameRepeat: false

                    Keys.onReturnPressed: {
                        if(confirmBtn.focus === true){
                            confirmBtn.clicked() // 正常状态：触发可用的确定按钮
                        }
                    }

                    onClicked: {
                        if(lineEdit.text.length === 0 || lineEdit.text.trim().length === 0){
                            //lenthLimittext.text = qsTr("名称不能为空")
                            //return
                        }

                        var repeatFlag = false

                        if(windowType === 1){ // 新建文件夹
                            for (let i = 0; i <= alreadyCreateFolderName.length; i++) {
                                if(alreadyCreateFolderName[i] === lineEdit.text){
                                    repeatFlag = true
                                    break
                                }
                            }
                        }

                        if(windowType === 2){ // 重命名文件/文件夹
                            for (let i = 0; i <= alreadyCreateFolderName.length; i++) {
                                if(alreadyCreateFolderName[i] === lineEdit.text){
                                    repeatFlag = true
                                    break
                                }
                            }

                            for (let j = 0; j <= alreadyCreateFileName.length; j++) {
                                if(getFileNamePrefix(alreadyCreateFileName[j]) === lineEdit.text){
                                    repeatFlag = true
                                    break
                                }
                            }
                        }

                        if(repeatFlag){
                            lenthLimittext.text = qsTr("A file or folder with the same name already exists. Please use a different name.")
                            confirmBtn.isFileNameRepeat = true
                            return
                        }
                        if(windowType === 1){
                            if(lineEdit.text.length === 0 || lineEdit.text.trim().length === 0){
                                // 若文件夹名称为空，则传默认的名称
                                confirmed(newFolderName)
                            }
                            confirmed(lineEdit.text)
                        }else{
                            // 若文件名称为空，则传传进来的文件名，相当与修改为原来的名字
                            if(lineEdit.text.length === 0 || lineEdit.text.trim().length === 0){
                                confirmed(getFileNamePrefix(renameFileName) + fileNamePrefix)
                            }
                            confirmed(lineEdit.text + fileNamePrefix)
                        }
                        hideDialog();
                    }
                }

                Button {
                    id: confirmBtn1
                    text: qsTr("Confirm")
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 36
                    highlighted: true
                    enabled: false // 当文件名不重复时才可用
                    visible: confirmBtn.isFileNameRepeat
                    focusPolicy: Qt.NoFocus
                }
            }
        }
    }
}
