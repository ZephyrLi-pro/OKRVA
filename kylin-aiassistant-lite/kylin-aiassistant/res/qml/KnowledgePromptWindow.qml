import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."

Window {
    id: promptModelDialog
    width: 376
    height: promptContentRec.height
    maximumHeight: promptContentRec.height
    maximumWidth: width
    minimumHeight: promptContentRec.height
    minimumWidth: width
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    color: "transparent"
    visible: false

    property bool isReplaced: false
    property int fileCounts: 0
    property string fileName: ""     // 外部传入弹窗提示文本("上瘾.pdf")
    property string typeParam: "file" // 外部传入类型（"file" / "folder"）


    signal confirmed(bool isAllUsed)
    signal cancel(bool isAllUsedChecked)


    property string iconSource: {
        if (typeParam !== "file") {
            return "qrc:/icon/icon/insync-folder.png";
        }

        if (fileName.length === 0) {
            return "qrc:/icon/icon/text-plain.svg";
        }

        var dotIndex = fileName.lastIndexOf(".");
        var fileExt = "";
        if (dotIndex !== -1 && dotIndex < fileName.length - 1) {
            fileExt = fileName.substring(dotIndex + 1).toLowerCase();
        }

        switch (fileExt) {
            case "pdf":
                return "qrc:/icon/icon/application-x-pdf.svg";
            case "ppt":
            case "pptx":
                return "qrc:/icon/icon/application-mspowerpoint.svg";
            case "doc":
            case "docx":
            case "wps":
                return "qrc:/icon/icon/application-msword.svg";
            case "ofd":
                return "qrc:/icon/icon/application-ofd.svg";
            case "txt":
                return "qrc:/icon/icon/text-plain.svg";
            default:
                return "qrc:/icon/icon/text-plain.svg";
        }
    }

    function closeDialog() {
        visible = false
        isReplaced = false
    }

    function showDialog() {
        visible = true
    }

    Rectangle {
        id: promptContentRec
        radius: 12
        color: "#FFFFFF"
        width: 376
        height: promptContentColumnLayout.height + promptContentRow.topPadding + promptContentRow.bottomPadding
        focus: true

        Keys.onEscapePressed: {
            closeDialog()
        }

        Keys.onEnterPressed: {
            replaceBtn.clicked()
        }

        Keys.onReturnPressed: {
            // 默认按回车键响应确认按钮
            console.log("*****Return*******")
            if (replaceBtn.visible) {
                replaceBtn.clicked()
            }
        }

        Rectangle {
            id: titleBar
            height: 40
            color: "transparent"//"#FFFFFF"
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
                    promptModelDialog.startSystemMove()
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
                property string buttonType: "windowTitleBarCloseButton"

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
                    closeDialog()
                }

                Keys.onReturnPressed: {
                    if(closeButton.focus === true){
                        closeButton.clicked()
                    }
                }
            }
        }

        Row {
            id: promptContentRow
            width: promptContentRec.width
            height: promptContentColumnLayout.height + promptContentRow.topPadding + promptContentRow.bottomPadding
            anchors.topMargin: titleBar.height
            topPadding: titleBar.height
            leftPadding: 24
            rightPadding: 24
            bottomPadding: 24

            ColumnLayout {
                id: promptContentColumnLayout
                width: promptContentRow.width - promptContentRow.leftPadding - promptContentRow.rightPadding
                spacing: 0
                Row {
                    id: tipContentRow
                    width: parent.width

                    Rectangle {
                        id: tipIconRec
                        width: 48
                        height: 48
                        color: "transparent"
                        radius: 0
                        // 核心绑定：将自身垂直中心绑定到 Row 的垂直中心
                        anchors.verticalCenter: tipContentRow.verticalCenter
                        Image {
                            id: tipIcon
                            width: 32
                            height: 32
                            anchors.centerIn: parent
                            source: (typeParam === "folder") ? "qrc:/icon/icon/insync-folder.png" : iconSource
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    Rectangle {
                        id: tipTextRec
                        width: 280
                        height: tipContentText.lineCount === 3 ? 90 : 74
                        color: "transparent"
                        onHeightChanged: {
                            console.log("阿基搜大家啊死哦大家阿斯顿",tipTextRec.height)
                        }

                        // 存储计算后的显示文本（普通属性，不是绑定）
                        property string displayText: ""
                        property string textPrefix: qsTr("This location already contains a file named \"")
                        property string textSuffix: qsTr("\". Are you sure you want to replace it?")

                        // 隐藏的测量用 Text（与显示元素完全独立）
                        Label {
                            id: measureText
                            visible: false
                            width: 280
                            font.family: Qt.application.font.family
                            wrapMode: Text.WrapAnywhere
                        }

                        // 计算函数：只在需要时调用，不是响应式绑定
                        function calculateDisplayText() {
                            var fullText = textPrefix + fileName + textSuffix;

                            // 用隐藏元素测量完整文本
                            measureText.text = fullText;
                            console.log("measureText.lineCount:", measureText.lineCount);

                            if (measureText.lineCount <= 3) {
                                // 不超过3行，直接使用原文
                                displayText = fullText;
                                return;
                            }

                            // 超过3行，需要截断 fileName
                            var name = fileName;
                            for (var i = 1; i < name.length; i++) {
                                var truncated = name.substring(0, name.length - i) + "...";
                                measureText.text = textPrefix + truncated + textSuffix;
                                if (measureText.lineCount <= 3) {
                                    displayText = textPrefix + truncated + textSuffix;
                                    return;
                                }
                            }
                            displayText = textPrefix + "..." + textSuffix;
                        }

                        // 只在特定时机触发计算
                        Component.onCompleted: calculateDisplayText()

                        Connections {
                            target: promptModelDialog
                            onFileNameChanged: function() {
                                tipTextRec.calculateDisplayText();
                            }
                        }

                        Label {
                            id: tipContentText
                            width: 280
                            text: tipTextRec.displayText  // 只读取，不修改
                            font.family: Qt.application.font.family
                            color: "#262626"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideNone
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 3
                        }
                    }
                }

                Item {
                    id: elasticItem
                    width: parent.width
                    height: 18
                }

                RowLayout {
                    id: bottomRow
                    width: parent.width
                    height: 68

                    Row {
                        id: allAppRow
                        visible: !isReplaced && fileCounts > 1
                        height: 20
                        spacing: 8

                        CheckBox {
                            id: allAppCheckBox
                            width: 16
                            height: 16
                            checked: false
                            anchors.verticalCenter: parent.verticalCenter
                            Keys.onReturnPressed: {
                                if(allAppCheckBox.focus === true){
                                    allAppCheckBox.checked = !allAppCheckBox.checked
                                }
                            }
                            indicator: Item {
                                id: selectAllCheckBoxItem
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                Image {
                                    id: selectAllCheckBoxImage
                                    source: allAppCheckBox.checked ?
                                                "qrc:/icon/icon/sidebar/CheckBox-default.svg" :
                                                "qrc:/icon/icon/sidebar/noCheck-default.svg"
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
                                    if(allAppCheckBox.checked){
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
                                    if(allAppCheckBox.checked){
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
                                    if(allAppCheckBox.checked){
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
                                    if(allAppCheckBox.checked){
                                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                                    }else{
                                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                                    }
                                }
                                onClicked: {
                                    allAppCheckBox.checked = !allAppCheckBox.checked
                                }
                            }
                        }

                        Label {
                            id: allAppCheckText
                            width: 56
                            height: 20
                            text: qsTr("Apply to All")
                            font.family: Qt.application.font.family
                            color: "#262626"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                        }
                    }

                    // 用于显示占位，永不展示
                    Row {
                        id: elasticRow
                        visible: !isReplaced && fileCounts <= 1
                        height: 20
                        spacing: 8
                        opacity: 0
                        CheckBox {
                            id: elasticCheckBox
                            width: 16
                            height: 16
                            checked: false
                            opacity: 0
                        }

                        Label {
                            id: elasticText
                            width: 56
                            height: 20
                            text: qsTr("Apply to All")
                            font.family: Qt.application.font.family
                            color: "#262626"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            opacity: 0
                        }
                    }

                    Rectangle {
                        width: bottomRow.width - allAppRow.width - actionBtnRow.width - 8
                        height: 20
                        color: "transparent"
                    }

                    Row {
                        id: actionBtnRow
                        spacing: 8

                        Button {
                            id: cancelBtn
                            text: qsTr("Cancel")
                            width: 88
                            height: 36
                            visible: !isReplaced
                            focusPolicy: Qt.NoFocus
                            Accessible.name: "customDialog_Button_cancel"
                            onClicked: {
                                cancel(allAppCheckBox.checked)
                                //closeDialog()
                            }
                            Keys.onReturnPressed: {
                                if(cancelBtn.focus === true){
                                    cancelBtn.clicked()
                                }
                            }
                        }

                        Button {
                            id: replaceBtn
                            visible: !isReplaced
                            text: qsTr("Replace")
                            width: 88
                            height: 36
                            highlighted:false
                            focusPolicy: Qt.NoFocus
                            Accessible.name: "customDialog_Button_replaceConfirm"
                            onClicked: {
                                if (!isReplaced) {
                                    isReplaced = true
                                }
                                confirmed(allAppCheckBox.checked)
                            }
                            Keys.onReturnPressed: {
                                if(replaceBtn.focus === true){
                                    replaceBtn.clicked()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
