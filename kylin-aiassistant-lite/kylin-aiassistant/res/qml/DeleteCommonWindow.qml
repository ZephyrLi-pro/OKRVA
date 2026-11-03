import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import "."

Window {
    id: deleteCommonWindow
    width: windowWidth
    height: windowRow.height
    maximumHeight: windowRow.height
    maximumWidth: width
    minimumHeight: windowRow.height
    minimumWidth: width
    title: " "
    flags: Qt.Dialog
    modality: Qt.WindowModal
    visible: false
    color: "#FFFFFF"

    property var selectedOptions: []

    //窗口宽度配置
    property int windowWidth: 376  // 默认宽度376
    
    // 图标配置
    property string iconSource: "qrc:/icon/icon/dialog-question.svg"  // 图标路径，可传入
    
    // 文本配置
    property string questionText: ""  // 问题文本（必要）
    property string infoText: ""  // 提示文本（可选，灰色次要文本）
    
    // 按钮配置
    property string confirmButtonText: qsTr("Delete")  // 确认按钮文本
    property string cancelButtonText: qsTr("Cancel")  // 取消按钮文本
    property bool highlightConfirmButton: false  // 是否高亮确认按钮
    property bool highlightCancelButton: false  // 是否高亮取消按钮
    
    // 不再提示复选框配置
    property bool showNoRemindCheckBox: false  // 是否显示"不再提示"复选框
    property string noRemindText: qsTr("Don't remind me again")  // "不再提示"复选框文本
    property bool noRemindChecked: false  // "不再提示"复选框状态

    Component.onCompleted: {
        windowCtrl.clearWindowStyle(deleteCommonWindow)
    }
    
    signal confirmed(var selectedOptions, bool noRemindChecked)
    signal canceled()
    signal dialogClosed()

    // 与主助手窗口几何同步（main.cpp 注入 windowCtrl），独立 Window 勿依赖 parent 链
    function centerOnAssistantWindow() {
        console.log("===centerOnAssistantWindow===")
        if (root) {
            console.log("===centerOnAssistantWindow===root" + root.objectName + "deleteCommonWindow.width" + deleteCommonWindow.width + " height" + deleteCommonWindow.height)
            x = root.x + (root.width - deleteCommonWindow.width) / 2
            y = root.y + (root.height - deleteCommonWindow.height) / 2
            return
        }
    }

    function showDialog() {
        visible = true
        console.log("***********showdialog" + mainColumnLayout.height)
        requestActivate() // 请求激活窗口，确保显示在最前面
        // 布局完成后再居中，避免高度未算准；再抢焦点以便回车响应
        Qt.callLater(function() {
            centerOnAssistantWindow()
            windowRow.forceActiveFocus()
        })
    }

    function hideDialog() {
        visible = false
        dialogClosed()
    }

    // 字体变化会影响窗口尺寸，重新居中
    onWidthChanged: {
        Qt.callLater(function() {
            centerOnAssistantWindow()
        })
    }

    onHeightChanged: {
        Qt.callLater(function() {
            centerOnAssistantWindow()
        })
    }

    Row {
        id: windowRow
        topPadding: 0
        leftPadding: 0
        rightPadding: 0
        bottomPadding: 24
        width: windowWidth
        height: mainColumnLayout.height + windowRow.bottomPadding
        focus: true

        Keys.onEscapePressed: {
            console.log("*****Escape*******")
            canceled();
            hideDialog();
        }

        Keys.onReturnPressed: {
            // 默认按回车键响应确认按钮
            console.log("*****Return*******")
            if (confirmBtn.visible) {
                confirmBtn.clicked()
            }
        }

        Keys.onEnterPressed: {
            console.log("*****Enter*******")
            // 小键盘回车键也响应确认按钮
            if (confirmBtn.visible) {
                confirmBtn.clicked()
            }
        }

        ColumnLayout {
            id: mainColumnLayout
            width: windowRow.width - windowRow.leftPadding - windowRow.rightPadding
            spacing: 0

            // 标题栏区域
            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                height: 40
                color: "transparent"

                // 鼠标拖动区域，需要放在前面，不然关闭按钮触发不了click
                MouseArea {
                    anchors.fill: parent
                    property point startDragPos: Qt.point(0, 0)

                    onPressed: {
                        startDragPos = Qt.point(mouse.x, mouse.y)
                    }

                    onPositionChanged: {
                        var delta = Qt.point(mouse.x - startDragPos.x, mouse.y - startDragPos.y)
                        deleteCommonWindow.x += delta.x
                        deleteCommonWindow.y += delta.y
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                    }

                    // 关闭按钮
                    Button {
                        id: closeButton
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 32
                        Layout.minimumHeight: 32
                        Layout.minimumWidth: 32
                        Layout.maximumHeight: 32
                        Layout.maximumWidth: 32
                        Layout.rightMargin: 4
                        flat: true
                        icon.name: "window-close-symbolic"
                        focusPolicy: Qt.NoFocus

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
                            canceled();
                            hideDialog();
                        }

                        Keys.onReturnPressed: {
                            if(closeButton.focus === true){
                                closeButton.clicked()
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                height: 8
            }

            // 内容区域（左侧图标，右侧问题文本和提示文本，支持换行，自适应，上边距8px）
            RowLayout {
                id: contentRowLayout
                Layout.fillWidth: true
                Layout.topMargin: 0
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 4

                // 左侧图标
                Image {
                    id: tipImage
                    source: deleteCommonWindow.iconSource
                    width: 16
                    height: 16
                    sourceSize.width: 16
                    sourceSize.height: 16
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 4
                }

                // 右侧文本区域
                Column {
                    id: tipTextColumn
                    Layout.fillWidth: true
                    spacing: 0

                    // 问题文本
                    Label {
                        id: questionLabelText
                        width: tipTextColumn.width
                        text: deleteCommonWindow.questionText
                        wrapMode: Text.WrapAnywhere
                        color: Qt.rgba(0, 0, 0, 0.85)
                    }

                    // 问题文本和提示文本之间的间距
                    Rectangle {
                        id: spacingRect
                        width: 1
                        height: 10
                        visible: deleteCommonWindow.infoText !== ""
                        color: "transparent"
                    }

                    // 提示文本
                    Label {
                        id: infoLabelText
                        width: tipTextColumn.width
                        text: deleteCommonWindow.infoText
                        visible: deleteCommonWindow.infoText !== ""
                        color: Qt.rgba(0, 0, 0, 0.6)
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                height: deleteCommonWindow.infoText !== "" ? 32 : 40
            }

            // 按钮区域（左侧可能有不再提示复选框，右侧取消和确认按钮）
            RowLayout {
                id: bottomButtonRowLayout
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 8

                // 不再提示复选框
                CheckBox {
                    id: noRemindCheckBox
                    visible: deleteCommonWindow.showNoRemindCheckBox
                    checked: deleteCommonWindow.noRemindChecked
                    text: deleteCommonWindow.noRemindText
                    Layout.fillWidth: true
                    Layout.preferredWidth: visible ? implicitWidth : 0

                    contentItem: Label {
                        id: noRemindTextItem
                        text: parent.text
                        font: parent.font
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        width: parent.width
                        leftPadding: 20
                        color: Qt.rgba(0, 0, 0, 0.85)
                    }

                    ToolTip {
                        id: noRemindTooltip
                        parent: parent
                        text: parent.text
                        delay: Qt.styleHints.mousePressAndHoldInterval
                    }

                    onHoveredChanged: {
                        if(hovered && noRemindMetrics.implicitWidth > (noRemindTextItem.width - 28))
                            noRemindTooltip.visible = true
                        else
                            noRemindTooltip.visible = false
                    }

                    Label {
                        id: noRemindMetrics
                        text: deleteCommonWindow.noRemindText
                        visible: false
                    }

                    onCheckedChanged: {
                        deleteCommonWindow.noRemindChecked = checked
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // 取消按钮
                Button {
                    id: cancelBtn
                    Accessible.name: "kylin-aiassistant_DeleteCommonWindow_cancelBtn"
                    text: deleteCommonWindow.cancelButtonText
                    visible: deleteCommonWindow.cancelButtonText !== ""
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 36
                    focusPolicy: Qt.NoFocus

                    highlighted: deleteCommonWindow.highlightCancelButton ? true : false
                    property int maxWidth: 88

                    ToolTip {
                        id: cancelBtnTextTooltip
                        parent: parent
                        text: parent.text
                        delay: Qt.styleHints.mousePressAndHoldInterval
                    }

                    onClicked: {
                        canceled();
                        hideDialog();
                    }
                }

                // 确认按钮
                Button {
                    id: confirmBtn
                    Accessible.name: "kylin-aiassistant_DeleteCommonWindow_confirmBtn"
                    text: deleteCommonWindow.confirmButtonText
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 36
    //                highlighted: deleteCommonWindow.highlightConfirmButton ? true : false
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    property int maxWidth: 88

                    ToolTip {
                        id: confirmBtnTextTooltip
                        parent: parent
                        text: parent.text
                        delay: Qt.styleHints.mousePressAndHoldInterval
                    }

                    onClicked: {
                        confirmed(selectedOptions, noRemindCheckBox.checked);
                        hideDialog();
                    }
                }
            }

        }
    }
}
