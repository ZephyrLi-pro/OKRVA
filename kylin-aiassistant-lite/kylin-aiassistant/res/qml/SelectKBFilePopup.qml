import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12

Popup {
    id: kbPopup
    width: 306
    height: mainColumn.implicitHeight + 16 // 最大356
    x: posX
    y: -height - 8
    visible: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    clip: false

    property int posX
    property var selectedFilesPath: [] // 默认选中文件
    property bool isSelectedAllFiles: false // 是否全选个人知识库
    property int rootFolderId: 0  // 根目录的ID
    property var latestFiles: []  // 存储最新的5个文件
    property bool hasFiles: false  // 新增：是否有文件的标志

    signal confirmed(string selectedFilesJson)
    signal closeKbPopupTransparentRet(var flag)

    onVisibleChanged: {
        if (visible) {
            // 延迟调用加载文件函数，避免UI与后端同时执行导致的卡顿
            Qt.callLater(function(){
                loadLatestFiles()
            })
        }
    }

    // 加载最新文件函数
    function loadLatestFiles() {
        console.log("开始加载最新文件...")
        if (!kbDatabase) {
            console.error("知识库数据库未初始化")
            return
        }

        // 清空之前的列表
        latestFiles = []
        hasFiles = false  // 重置标志

        try {
            // 获取根目录的最新5个文件
            var files = kbDatabase.getLatestFilesByFolderIdRecursively(rootFolderId, 5)
            console.log("数据库返回的文件数量:", files.length)

            // 添加调试：打印每个文件的详细信息
            for (var i = 0; i < files.length; i++) {
                console.log("文件", i, "原始数据:", JSON.stringify(files[i]))
            }

            for (var j = 0; j < files.length; j++) {
                var fileItem = files[j]

                // 添加更多调试
                console.log("处理文件", j, ":")
                console.log("  documentPath:", fileItem.documentPath)
                console.log("  status:", fileItem.status)
                console.log("  isDir:", fileItem.isDir)
                let childCount = 0;
                if (fileItem.isDir){
                    // 根据目录获取子文件数量
                    childCount = msgPane.getKnowledgeFileCountByFilepath(fileItem.documentTargetPath)
                }

                var fileData = {
                    documentName: fileItem.documentName || "",
                    documentPath: fileItem.documentPath || "",
                    documentTargetPath: fileItem.documentTargetPath || "",
                    documentSize: fileItem.documentSize || "",
                    createTime: fileItem.createTime || "",
                    isDir: fileItem.isDir || false,  // 确保是布尔值
                    status: fileItem.status || 0,
                    uuid: fileItem.uuid || "",
                    fileIcon: fileItem.fileIcon,
                    isEnabled: childCount > 0 || !fileItem.isDir ? true : false
                }

                console.log("  处理后文件名:", fileData.documentName)

                latestFiles.push(fileData)
            }

            console.log("最终加载了", latestFiles.length, "个最新文件")

            // 更新文件标志
            hasFiles = latestFiles.length > 0

            // 强制触发Repeater更新
            intelligentModelRepeater.model = 0
            intelligentModelRepeater.model = latestFiles

        } catch (error) {
            console.error("加载文件时出错:", error)
            console.error(error.stack)
        }
    }

    // 打开从知识库选择文件弹窗
    function showKBFileMoreWindow() {
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/KnowledgeFileSelectMoreWindow.qml");
        if (dialogComponent.status === Component.Ready) {
            let dialog = dialogComponent.createObject(root, {
                                                          isSupportAllFile: true,
                                                          currentFilesPath: selectedFilesPath,
                                                          isSelectedRootPath: isSelectedAllFiles
                                                      }); // 父级设为顶层覆盖层
            if (dialog) {
                dialog.confirmed.connect(function(fileInfo) {
                    kbPopup.confirmed(fileInfo);
                    dialog.destroy()
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

    contentItem: Column {
        id: mainColumn
        spacing: 10
        width: parent.width

        // 当有文件时显示正常内容
        Column {
            id: normalContent
            spacing: 10
            width: parent.width
            visible: hasFiles  // 使用标志位
            rightPadding: 8
            leftPadding: 8

            // 引用全量知识库
            Label {
                text: qsTr("Reference all knowledge base")
                color: Qt.rgba(0, 0, 0, 0.35)
            }

            Rectangle {
                id: allKBFileRectangle
                width: parent.width - 16
                height: 36
                radius: 6

                Row {
                    anchors.fill: parent
                    spacing: 8
                    leftPadding: 8
                    rightPadding: 8

                    // 选项图标
                    Rectangle {
                        width: 16
                        height: 16
                        color: "transparent"
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            anchors.fill: parent
                            source: "qrc:/icon/icon/knowledge.svg"
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    // 选项名称
                    Label {
                        text: qsTr("Personal Knowledge Base")
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        width: 208
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        allKBFileRectangle.color = Qt.rgba(0, 0, 0, 0.15)
                    }
                    onExited: {
                        allKBFileRectangle.color = "transparent"
                    }
                    onPressed: {
                        allKBFileRectangle.color = Qt.rgba(0, 0, 0, 0.2)
                    }
                    onReleased: {
                        allKBFileRectangle.color = "transparent"
                    }
                    onClicked: {
                        kbPopup.visible = false
                        closeKbPopupTransparentRet(false)
                        processFileSelection(-1)  // -1表示选择整个知识库
                    }
                }
            }

            Label {
                text: qsTr("Select Knowledge Base file")
                color: Qt.rgba(0, 0, 0, 0.35)
            }

            Column {
                id: intelligentModelColumn
                width: parent.width
                spacing: 2

                // 显示最新的5个文件
                Repeater {
                    id: intelligentModelRepeater
                    model: latestFiles

                    delegate: Rectangle {
                        id: kbFileRectangle
                        width: parent.width - 16
                        height: 36
                        radius: 6
                        color: "transparent"

                        Row {
                            anchors.fill: parent
                            spacing: 8
                            leftPadding: 8
                            rightPadding: 8

                            // 文件图标
                            Rectangle {
                                width: 16
                                height: 16
                                color: "transparent"
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    anchors.fill: parent
                                    source: modelData.fileIcon
                                    fillMode: Image.PreserveAspectFit
                                    opacity: modelData.isEnabled ? 1.0 : 0.35
                                }
                            }

                            // 文件名称
                            Label {
                                id: documentNameLabel
                                text: modelData.documentName
                                color: modelData.isEnabled ? Qt.rgba(0, 0, 0, 0.85) : Qt.rgba(0, 0, 0, 0.35)
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                width: parent.width - 36
                                ToolTip {
                                    visible: documentNameLabel.truncated && intelligentMouseArea.containsMouse
                                    text: modelData.documentName
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                    property var maxWidth: 400
                                    property var wrapMode: Text.WrapAnywhere
                                }
                            }
                        }

                        MouseArea {
                            id: intelligentMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: modelData.isEnabled
                            onEntered: {
                                kbFileRectangle.color = Qt.rgba(0, 0, 0, 0.15)
                            }
                            onExited: {
                                kbFileRectangle.color = "transparent"
                            }
                            onPressed: {
                                kbFileRectangle.color = Qt.rgba(0, 0, 0, 0.2)
                            }
                            onReleased: {
                                kbFileRectangle.color = "transparent"
                            }
                            onClicked: {
                                kbPopup.visible = false
                                closeKbPopupTransparentRet(false)
                                processFileSelection(index)
                            }
                        }
                    }
                }

                Rectangle {
                    id: kbMoreFileRectangle
                    width: parent.width - 16
                    height: 36
                    radius: 6
                    Row {
                        anchors.fill: parent
                        spacing: 8
                        leftPadding: 8
                        rightPadding: 8

                        // 更多图标
                        Rectangle {
                            width: 16
                            height: 16
                            color: "transparent"
                            anchors.verticalCenter: parent.verticalCenter

                            Image {
                                anchors.fill: parent
                                source: "qrc:/html/icon/setting/view-more-horizontal.svg"
                                fillMode: Image.PreserveAspectFit
                            }
                        }

                        // 更多文件选项
                        Label {
                            text: qsTr("Select multiple files")
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            width: parent.width - 48
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            kbMoreFileRectangle.color = Qt.rgba(0, 0, 0, 0.15)
                        }
                        onExited: {
                            kbMoreFileRectangle.color = "transparent"
                        }
                        onPressed: {
                            kbMoreFileRectangle.color = Qt.rgba(0, 0, 0, 0.2)
                        }
                        onReleased: {
                            kbMoreFileRectangle.color = "transparent"
                        }
                        onClicked: {
                            kbPopup.visible = false
                            closeKbPopupTransparentRet(false)
                            showKBFileMoreWindow()
                        }
                    }
                }
            }
        }

        // 当没有文件时显示的图片
        Item {
            id: emptyContent
            width: parent.width
            height: 340
            visible: !hasFiles  // 使用标志位

            Column {
                id: emptyColumn
                anchors.centerIn: parent
                spacing: 4
                width: 258  // 固定宽度，确保内容居中

                Image {
                    id: emptyImage
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "qrc:/icon/icon/kbFile.svg"
                    width: 56
                    height: 56
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    text: qsTr("Upload files to Personal Knowledge Base")
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    text: qsTr("Help you manage personal materials and perform intelligent Q&A based on uploaded files. Add documents to make your assistant understand you better")
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    color: Qt.rgba(0, 0, 0, 0.35)
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: globals.fontsize - 2
                    lineHeight: 1.2
                }

                Item {
                    width: 1
                    height: 16  // 按钮与上面label的间距
                }
                Button {
                    text: qsTr("Add File")
                    icon.name: "list-add-symbolic"
                    property var buttonType: "importButton"
                    anchors.horizontalCenter: parent.horizontalCenter
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        closeKbPopupTransparentRet(false)
                        kbPopup.visible = false
                        Qt.callLater(() => { moveTimer.start() });
                    }
                }
            }
        }
    }

    Timer {
        id: moveTimer
        interval: 200
        running: false
        repeat: false
        onTriggered: {
            root.onChangeWindowPage(5)
        }
    }

    background: Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"  // 背景颜色
        radius: 8  // 圆角
        border.color: Qt.rgba(0, 0, 0, 0.1)  // 很淡的边框
        border.width: 1  // 边框宽度
    }

    // 处理文件选择
    function processFileSelection(index) {
        var selectedFiles = [];
        var fileInfo = {};

        // 选择单个文件
        if (index === -1) {
            fileInfo.documentName = "";
            fileInfo.documentTargetPath = "";
            fileInfo.documentSize = "";
            fileInfo.fileIcon = "";
            fileInfo.uuid = "";
            fileInfo.isDir = false;
        } else {
            var fileData = latestFiles[index]
            fileInfo.documentName = fileData.documentName || "";
            fileInfo.documentTargetPath = fileData.documentTargetPath || "";
            fileInfo.documentSize = fileData.documentSize || "";
            fileInfo.fileIcon = fileData.fileIcon || "";
            fileInfo.uuid = fileData.uuid || "";
            fileInfo.isDir = fileData.isDir || false;
        }

        selectedFiles.push(fileInfo);

        var jsonData = {
            msgType: "knowledgeFilesSelected",
            msgBody: selectedFiles
        };
        var jsonString = JSON.stringify(jsonData);

        confirmed(jsonString)
        console.log("成功发送", selectedFiles.length, "个文件的选择信息", jsonString);
    }
}
