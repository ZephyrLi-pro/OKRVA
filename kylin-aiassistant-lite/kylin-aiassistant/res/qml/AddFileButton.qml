import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Dialogs 1.3
import ButtonState 1.0
import "."

// 上传文件按钮
Rectangle {
    id: addFileButtonRectangle
    Accessible.name: "kylin-aiassistant_Chat_addFileButton"
    width: 32
    height: 32
    radius: 6
    property bool effectiveImageSupport: ButtonState.isTaskMode || isSupportImageProcessing
    property bool effectiveDocumentSupport: isSupportDocumentQA
    visible: (effectiveDocumentSupport || effectiveImageSupport)
    color: "transparent"
    // 当选中非"生成待办"的其他指令时置灰；勿用 Item.enabled，否则 MouseArea 无法悬停、无 tooltip（与侧栏知识库一致）
    property bool fileButtonInteractive: (!hasActiveCommand || isTodoCommand)
    opacity: fileButtonInteractive ? 1.0 : 0.4

    // 是否选中了"生成待办"指令
    property bool isTodoCommand: false

    function currentUploadType() {
        // 0: 图片和文档，1: 仅图片，2: 仅文档
        if (isTodoCommand) {
            return 2
        }
        if (effectiveDocumentSupport && effectiveImageSupport) {
            return 0
        }
        if (!effectiveDocumentSupport && effectiveImageSupport) {
            return 1
        }
        if (effectiveDocumentSupport && !effectiveImageSupport) {
            return 2
        }
        return -1
    }

    function imageNameFilters() {
        return [
            "*.bmp *.jpe *.jpeg *.jpg *.pbm *.pgm *.png *.ppm *.tiff *.webp",
            "*.bmp",
            "*.jpe *.jpeg *.jpg",
            "*.pbm",
            "*.pgm",
            "*.png",
            "*.ppm",
            "*.tiff",
            "*.webp"
        ]
    }

    function documentNameFilters() {
        if (globals.isPdfSupported) {
            return [
                "*.txt *.doc *.docx *.pdf *.wps *.pptx *.ppt *.ofd",
                "*.txt",
                "*.doc *.docx",
                "*.pdf",
                "*.wps",
                "*.pptx *.ppt",
                "*.ofd"
            ]
        }
        return [
            "*.txt *.doc *.docx *.wps *.pptx *.ppt *.ofd",
            "*.txt",
            "*.doc *.docx",
            "*.wps",
            "*.pptx *.ppt",
            "*.ofd"
        ]
    }

    function allSupportedNameFilters() {
        if (globals.isPdfSupported) {
            return [
                "*.bmp *.jpe *.jpeg *.jpg *.pbm *.pgm *.png *.ppm *.tiff *.webp *.txt *.doc *.docx *.pdf *.wps *.pptx *.ppt *.ofd",
                "*.bmp",
                "*.jpe *.jpeg *.jpg",
                "*.pbm",
                "*.pgm",
                "*.png",
                "*.ppm",
                "*.tiff",
                "*.webp",
                "*.txt",
                "*.doc *.docx",
                "*.pdf",
                "*.wps",
                "*.pptx *.ppt",
                "*.ofd"
            ]
        }
        return [
            "*.bmp *.jpe *.jpeg *.jpg *.pbm *.pgm *.png *.ppm *.tiff *.webp *.txt *.doc *.docx *.wps *.pptx *.ppt *.ofd",
            "*.bmp",
            "*.jpe *.jpeg *.jpg",
            "*.pbm",
            "*.pgm",
            "*.png",
            "*.ppm",
            "*.tiff",
            "*.webp",
            "*.txt",
            "*.doc *.docx",
            "*.wps",
            "*.pptx *.ppt",
            "*.ofd"
        ]
    }

    function nameFiltersForUploadType(typeNum) {
        if (typeNum === 1) {
            return imageNameFilters()
        }
        if (typeNum === 2) {
            return documentNameFilters()
        }
        if (typeNum === 0) {
            return allSupportedNameFilters()
        }
        return []
    }

    function handleSelectedFileUrl(selectedFileUrl) {
        if (!selectedFileUrl || selectedFileUrl === "") {
            return
        }

        var path = selectedFileUrl.toString()
        // 去掉 file:// 前缀并解码，转换为本地绝对路径（C++ 侧会再 canonical 一次）
        if (path.indexOf("file://") === 0) {
            path = path.substring(7)
            path = decodeURIComponent(path)
        }

        if (!path || path === "") {
            return
        }

        var fileExtension = path.substring(path.lastIndexOf('.') + 1).toLowerCase()
        var imageFormats = ["bmp", "jpe", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tiff", "webp"]
        // 根据PDF支持情况动态构建文档格式列表
        var docFormats = globals.isPdfSupported ?
            ["txt", "doc", "docx", "pdf", "wps", "pptx", "ppt", "ofd"] :
            ["txt", "doc", "docx", "wps", "pptx", "ppt", "ofd"]

        // 如果选中了"生成待办"指令，只允许上传文档格式
        if (isTodoCommand && imageFormats.indexOf(fileExtension) !== -1) {
            console.log("生成待办指令模式下不支持上传图片格式")
            return
        }

        if (imageFormats.indexOf(fileExtension) !== -1) {
            if (!addFileButtonRectangle.effectiveImageSupport) {
                return
            }
            msgPane.onShowImage(path, 1)
        } else if (docFormats.indexOf(fileExtension) !== -1) {
            if (!addFileButtonRectangle.effectiveDocumentSupport) {
                return
            }
            msgPane.onShowDocument(path)
        }
    }

    // Tolltip内容，根据能力展示
    property string text: {
        // 如果选中了"生成待办"指令，只支持文档格式
        if (isTodoCommand) {
            return qsTr("Add file (1 item;") + qsTr("Document ≤ 50MB") + qsTr(")")
        } else if (effectiveDocumentSupport && effectiveImageSupport) {
            return qsTr("Add file (1 item;") + qsTr("Document ≤ 50MB;") + qsTr("Image ≤ 20MB") + qsTr(")")
        } else if (!effectiveDocumentSupport && effectiveImageSupport) {
            return qsTr("Add file (1 item;") + qsTr("Image ≤ 20MB") + qsTr(")")
        } else if (effectiveDocumentSupport && !effectiveImageSupport) {
            return qsTr("Add file (1 item;") + qsTr("Document ≤ 50MB") + qsTr(")")
        }
        return ""
    }

    property bool isActive: false

    onIsActiveChanged: {
        if (isActive === true){
            addFileButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
            addFileImage.source = "qrc:/icon/icon/icon_attachment.svg"
            // 置灰联网搜索按钮
            ButtonState.storageWebSearchEnabled = ButtonState.webSearchEnabled
            ButtonState.webSearchEnabled = false
        } else {
            addFileButtonRectangle.color = "transparent"
            addFileImage.source = "qrc:/icon/icon/insert-link.svg"
            // 恢复联网搜索按钮
            ButtonState.webSearchEnabled = ButtonState.storageWebSearchEnabled
            ButtonState.storageWebSearchEnabled = null
        }
    }

    ToolTip {
        id: addFileButtonToolTip
        delay: Qt.styleHints.mousePressAndHoldInterval
        visible: addFileButtonMouseArea.containsMouse
        property var maxWidth: 250  // 设置固定宽度并微调,text放入contentItem位置居中
        text: addFileButtonRectangle.text
        property var wrapMode: Text.WrapAnywhere
    }

    RowLayout {
        anchors.fill: parent
        Image {
            id: addFileImage
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            source: "qrc:/icon/icon/insert-link.svg"
        }
    }

    MouseArea {
        id: addFileButtonMouseArea
        Accessible.name: "kylin-aiassistant_Chat_addFileButtonMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            if (!addFileButtonRectangle.fileButtonInteractive) {
                return
            }
            if (addFileButtonRectangle.isActive) {
                addFileButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.15)
                addFileImage.source = "qrc:/icon/icon/icon_attachment_hover.svg"

            } else {
                addFileButtonRectangle.color = "#26000000"
            }
        }
        onExited: {
            if (!addFileButtonRectangle.fileButtonInteractive) {
                return
            }
            if (addFileButtonRectangle.isActive) {
                addFileButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                addFileImage.source = "qrc:/icon/icon/icon_attachment.svg"

            } else {
                addFileButtonRectangle.color = "transparent"
            }
        }
        onPressed: {
            if (!addFileButtonRectangle.fileButtonInteractive) {
                return
            }
            if (addFileButtonRectangle.isActive) {
                addFileButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
                addFileImage.source = "qrc:/icon/icon/icon_attachment_press.svg"

            } else {
                addFileButtonRectangle.color = "#33000000"
            }
            onCloseCommand()
        }
        onReleased: {
            if (!addFileButtonRectangle.fileButtonInteractive) {
                return
            }
            if (addFileButtonRectangle.isActive) {
                addFileButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                addFileImage.source = "qrc:/icon/icon/icon_attachment.svg"
            } else {
                addFileButtonRectangle.color = "transparent"
            }
        }
        onClicked: {
            if (!addFileButtonRectangle.fileButtonInteractive) {
                return
            }
            var typeNum = addFileButtonRectangle.currentUploadType()
            if (typeNum < 0) {
                return
            }

            msgPane.onSendDataCollect("add_file_click_event", "chatPage")

            // 使用 QtQuick.Dialogs.FileDialog 打开文件选择对话框
            var dialog = uploadFileDialogComponent.createObject(addFileButtonRectangle, {
                "nameFilters": addFileButtonRectangle.nameFiltersForUploadType(typeNum)
            })
            if (dialog) {
                dialog.open()
            }
        }
    }

    // 输入框上传文件的文件选择对话框（使用 QtQuick.Dialogs.FileDialog）
    Component {
        id: uploadFileDialogComponent

        FileDialog {
            selectExisting: true
            selectMultiple: false
            folder: shortcuts.desktop

            onAccepted: {
                addFileButtonRectangle.handleSelectedFileUrl(fileUrl)
                destroy()
            }

            onRejected: {
                destroy()
            }
        }
    }
}
