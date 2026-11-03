import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// 文件上传小预览窗口
Rectangle{
    id: root
    Accessible.name: "kylin-aiassistant_UploadedItem_root"
    visible: name !== ""
    radius: 6
    color: Qt.rgba(0/255, 0/255, 0/255, 0.03)
    width: itemWidth
    height: uploadedItemPreviewRow.height
    property int itemWidth: 172
    property string name: ""
    // 由 name 解析出的文件类型，如 "pdf"、"txt"
    property string fileType: {
        if (!name || name === "")
            return ""
        var dotIndex = name.lastIndexOf(".")
        if (dotIndex <= 0 || dotIndex === name.length - 1)
            return ""
        return name.substring(dotIndex + 1)
    }
    property string icon: ""
    property string size: ""
    property string path: ""
    property string uuid: ""
    property bool isDir: false
    property string childCount: ""
    property bool openable: true

    property bool removeable: true  //悬浮是否显示x按钮

    // 核心：统一管理hover状态
    property bool isHover: false

    // 点击关闭按钮，发送删除信号
    signal remove()
    // 点击整个框体，发送打开信号
    signal opened()

    MouseArea {
        id: rootMouseArea
        anchors.fill: parent
        hoverEnabled: true
        // 关键：事件透传，避免拦截子级MouseArea
        propagateComposedEvents: true
        onClicked: {
            if (!openable || isDir) {
                mouse.accepted = false;
                return;
            }

            root.opened();
            console.log("********uploadeditem.width********" + root.width)
            console.log("********uploadeditem.height********" + root.height)
        }

        onEntered: {
            root.isHover = true;
            root.color = Qt.rgba(0/255, 0/255, 0/255, 0.05);
        }

        onExited: {
            // 关键：鼠标在关闭按钮内时，不修改hover状态
            if (!documentPreviewButtonImageMouseArea.containsMouse) {
                root.isHover = false;
                root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03);
            }
        }

        // 鼠标移动时动态判断：避免快速移动导致的状态闪烁
        onPositionChanged: {
            if (containsMouse && !documentPreviewButtonImageMouseArea.containsMouse) {
                root.isHover = true;
            }
        }
    }

    Row{
        id: uploadedItemPreviewRow
        Accessible.name: "kylin-aiassistant_Chat_uploadedItemPreviewRow"
        height: uploadedItemDetailsRow.height + topPadding + bottomPadding
        width: root.itemWidth
        topPadding: 5
        bottomPadding: 5
        leftPadding: 8
        rightPadding: 8
        spacing: 4

        Rectangle{
            id: uploadedItemIconPreviewRet
            Accessible.name: "kylin-aiassistant_Chat_uploadedItemIconPreviewRet"
            width: 32
            height: parent.height - parent.topPadding - parent.bottomPadding
            color: "transparent"

            Image {
                id: documentIconPreviewImage
                Accessible.name: "kylin-aiassistant_Chat_documentIconPreviewImage"
                source: root.icon
                anchors.centerIn: parent
                height: 32
                width: 32
            }
        }

        Column{
            id: uploadedItemDetailsRow
            Accessible.name: "kylin-aiassistant_Chat_uploadedItemDetailsRow"
            width: parent.width - uploadedItemIconPreviewRet.width - parent.leftPadding - parent.rightPadding

            // 文件名
            Label {
                id: documentNameText
                Accessible.name: "kylin-aiassistant_Chat_documentNameText"
                width: parent.width
                text: name
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                font.pointSize: globals.fontsize - 2

                // 鼠标区域仅用于Tooltip，不再修改按钮可见性
                MouseArea {
                    id: documentNameTextMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    propagateComposedEvents: true
                    onClicked: mouse.accepted = false
                    onEntered: {
                        root.isHover = true;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
                    }
                    onExited: {
                        root.isHover = false;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
                    }
                }

                ToolTip {
                    id: documentNameTextTooltip
                    visible: documentNameTextMouseArea.containsMouse && (documentNameText.implicitWidth > documentNameText.width)
                    text: documentNameText.text
                    delay: Qt.styleHints.mousePressAndHoldInterval
                    property var maxWidth: 400
                    property var wrapMode: Text.WrapAnywhere
                }
            }

            // 文件大小
            Label {
                id:documentSizeText
                Accessible.name: "kylin-aiassistant_Chat_documentSizeText"
                text: fileType + "·" + root.size
                width: parent.width
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                font.pointSize: globals.fontsize - 2
                color: Qt.rgba(38/255, 38/255, 38/255, 0.3)
                visible: !isDir

                // 鼠标区域仅用于Tooltip
                MouseArea {
                    id: documentSizeTextMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    propagateComposedEvents: true
                    onClicked: mouse.accepted = false
                    onEntered: {
                        root.isHover = true;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
                    }
                    onExited: {
                        root.isHover = false;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
                    }
                }

                ToolTip {
                    id: documentSizeTextTooltip
                    visible: documentSizeTextMouseArea.containsMouse && (documentSizeText.implicitWidth > documentSizeText.width)
                    text: documentSizeText.text
                    delay: Qt.styleHints.mousePressAndHoldInterval
                    property var maxWidth: 400
                    property var wrapMode: Text.WrapAnywhere
                }
            }

            // 文件夹子文件数量
            Label {
                id:dirChildCountext
                Accessible.name: "kylin-aiassistant_Chat_dirChildCountext"
                text: "包含 " + root.childCount + "个文件"
                width: parent.width
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                font.pointSize: globals.fontsize - 2
                color: Qt.rgba(38/255, 38/255, 38/255, 0.3)
                visible: isDir

                // 鼠标区域仅用于Tooltip
                MouseArea {
                    id: dirChildCountextMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    propagateComposedEvents: true
                    onClicked: mouse.accepted = false
                    onEntered: {
                        root.isHover = true;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.05)
                    }
                    onExited: {
                        root.isHover = false;
                        root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03)
                    }
                }

                ToolTip {
                    id: dirChildCountextTooltip
                    visible: dirChildCountextMouseArea.containsMouse && (dirChildCountext.implicitWidth > dirChildCountext.width)
                    text: dirChildCountext.text
                    delay: Qt.styleHints.mousePressAndHoldInterval
                    property var maxWidth: 400
                    property var wrapMode: Text.WrapAnywhere
                }
            }
        }
    }

    // 右上角x号按钮
    Image {
        id: documentPreviewButtonImage
        source: "qrc:/icon/icon/closeDoc.svg"
        anchors.top: root.top
        anchors.right: root.right
        // 关键：仅通过根hover状态+removeable控制可见性，避免多源修改
        visible: root.removeable && root.isHover

        MouseArea {
            id: documentPreviewButtonImageMouseArea
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true

            onEntered: {
                documentPreviewButtonImage.source = "qrc:/icon/icon/closeDoc_hover.svg"
                // 强制保持hover状态，防止根节点触发exited
                root.isHover = true;
                root.color = Qt.rgba(0/255, 0/255, 0/255, 0.05);
            }

            onExited: {
                documentPreviewButtonImage.source = "qrc:/icon/icon/closeDoc.svg"
                // 仅当根节点也不在hover时，才恢复状态
                if (!rootMouseArea.containsMouse) {
                    root.isHover = false;
                    root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03);
                }
            }

            onPressed: {
                documentPreviewButtonImage.source = "qrc:/icon/icon/closeDoc_active.svg"
            }

            onReleased: {
                documentPreviewButtonImage.source = "qrc:/icon/icon/closeDoc.svg"
            }

            onClicked: {
                root.remove();
                // 点击后重置状态
                root.isHover = false;
                root.color = Qt.rgba(0/255, 0/255, 0/255, 0.03);
            }
        }
    }
}
