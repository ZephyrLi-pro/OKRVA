import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import "."

// 设置页面问答指令内容显示区域
Rectangle {
    id: settingChatCommandPageRet
    color: "#FFFFFF"

    // 使用C++实现的指令模型
    property var commandModel: typeof commandInstructionModel !== "undefined" ? commandInstructionModel : null

    signal changeTransparentCommandPage(var flag)
    
    // 打开添加指令弹窗
    function openAddCommandDialog() {
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/CommandInstructionDialog.qml");
        if (dialogComponent.status === Component.Ready) {
            // Window 类型可以传 null 作为父对象

            let dialog = dialogComponent.createObject(root);
            if (dialog) {
                dialog.openForAdd();
                dialog.accepted.connect(function(name, summary, description) {
                    if (commandModel) {
                        commandModel.addCommand(name, summary, description)
                    }
                    dialog.close();
                });
                dialog.showDialog();
            } else {
                console.error("创建添加指令弹窗失败：", dialogComponent.errorString());
            }
        } else {
            console.error("加载添加指令弹窗组件失败：", dialogComponent.errorString());
        }
    }
    
    // 打开编辑指令弹窗
    function openEditCommandDialog(uuid, name, summary, description) {
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/CommandInstructionDialog.qml");
        if (dialogComponent.status === Component.Ready) {
            // Window 类型可以传 null 作为父对象
            let dialog = dialogComponent.createObject(root);
            if (dialog) {
                dialog.openForEdit(uuid, name, summary, description);
                dialog.accepted.connect(function(name, summary, description) {
                    if (commandModel) {
                        commandModel.updateCommand(uuid, name, summary, description)
                    }
                    dialog.close();
                });
                dialog.showDialog();
            } else {
                console.error("创建编辑指令弹窗失败：", dialogComponent.errorString());
            }
        } else {
            console.error("加载编辑指令弹窗组件失败：", dialogComponent.errorString());
        }
    }
    
    // 删除确认对话框 - 使用 DeleteCommonWindow
    function openDeleteConfirmDialog(name, uuid) {
        let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
        if (dialogComponent.status === Component.Ready) {
            let dialog = dialogComponent.createObject(root);
            dialog.selectedOptions = [uuid]
            dialog.questionText = qsTr("After deleting the prompt template, it cannot be restored. Are you sure you want to delete?")
            dialog.infoText = ""
            dialog.confirmButtonText = qsTr("Delete")
            dialog.cancelButtonText = qsTr("Cancel")
            dialog.highlightConfirmButton = true
            dialog.highlightCancelButton = false
            dialog.showNoRemindCheckBox = false
            if (dialog) {
                dialog.confirmed.connect(function(options, noRemindChecked) {
                    if (commandModel && options.length > 0) {
                        commandModel.removeCommand(options[0])
                    }
                    dialog.destroy();
                });
                
                dialog.canceled.connect(function() {
                    dialog.destroy();
                });
                
                dialog.showDialog();
            } else {
                console.error("创建删除确认对话框失败：", dialogComponent.errorString());
            }
        } else {
            console.error("加载删除确认对话框组件失败：", dialogComponent.errorString());
        }
    }

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 6
        anchors.topMargin: 0
        anchors.bottomMargin: 0
        spacing: 0

        // 标题、描述和添加按钮区域
        RowLayout {
            id: titleAndButtonRow
            Layout.fillWidth: true
            Layout.topMargin: 0
            Layout.bottomMargin: 0
            Layout.rightMargin: 16
            spacing: 0

            // 标题和描述列布局
            ColumnLayout {
                id: titleColumn
                spacing: 0
                Layout.leftMargin: 0
                Layout.fillWidth: true
                //Layout.maximumWidth: parent.width - addButton.implicitWidth - 20

                Label {
                    id: titleLabel
                    text: qsTr("Command Template")
                    font.pointSize: globals.fontsize + 2
                    font.family: root.fontFamily
                    color: "#333333"
                    Layout.topMargin: 0
                    Layout.bottomMargin: 0
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Label {
                    id: tipLabel
                    text: qsTr("Sorting will affect the order of commands at the bottom of the Q&A function")
                    font.pointSize: globals.fontsize - 2
                    font.family: root.fontFamily
                    color: "#666666"
                    Layout.topMargin: 8
                    Layout.bottomMargin: 0
                    Layout.leftMargin: 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // 添加按钮
            ToolButton {
                id: addButton
                text: qsTr("Add")
                icon.source: commandListView.count > 100 ? "qrc:/icon/icon/setting/modelAdd-hover.svg" : "qrc:/icon/icon/setting/modelAdd.svg"
                icon.width: 12
                icon.height: 12
                display: AbstractButton.TextBesideIcon
                Layout.alignment: Qt.AlignVCenter

                // 悬浮提示框
                ToolTip {
                    id: deleteDialogueImageTooltip
                    visible: addButton.hovered && commandListView.count > 100   // 默认不显示
                    delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                    text: qsTr("The number of instruction templates cannot exceed 100")
                    property var maxWidth: 400
                    property var wrapMode: Text.WrapAnywhere
                }

                contentItem: Row {
                    spacing: 4
                    Image {
                        id: addIcon
                        source: (commandListView.count > 100 || addButton.hovered) ? "qrc:/icon/icon/setting/modelAdd-hover.svg" : "qrc:/icon/icon/setting/modelAdd.svg"
                        sourceSize.width: 12
                        sourceSize.height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: commandListView.count > 100 ? 0.45 : 1
                    }
                    Label {
                        text: addButton.text
                        color: commandListView.count > 100 ? Qt.rgba(55/255, 144/255, 250/255, 0.45) : addButton.hovered ? "#3790FA" : Qt.rgba(0, 0, 0, 0.85)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                background: Rectangle {
                    color: "transparent"
                    border.color: "transparent"
                }

                onClicked: {
                    if(commandListView.count > 100){
                        return
                    }
                    openAddCommandDialog()
                }
            }
        }

        // 指令列表
        ListView {
            id: commandListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 16
            clip: true
            model: commandModel
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: commandListView.contentHeight > commandListView.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                anchors.right: parent.right
                anchors.rightMargin: 0
            }

            delegate: CommandTemplateItemDelegate {
                width: commandListView.width - 20
                commandType: model.isDefault ? 1 : 2  // 1: 默认指令(只读), 2: 自定义指令(可编辑)
                actorId: 0  // 不再使用actorId
                name: model.name
                content: model.description
                description: model.summary || qsTr("Prompt template created by user")
                iconPath: {
                    if (model.isDefault) {
                        if (model.uuid === "550e8400-e29b-41d4-a716-446655440001") {
                            return "qrc:/html/icon/setting/todo-generate.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440002") {
                            return "qrc:/html/icon/setting/system-QA.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440003") {
                            return "qrc:/icon/icon/setting/text-expend.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440004") {
                            return "qrc:/icon/icon/setting/text-polish.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440005") {
                            return "qrc:/icon/icon/setting/translate.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440006") {
                            return "qrc:/icon/icon/setting/weekly-report.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440007") {
                            return "qrc:/icon/icon/setting/text-proofread.svg"
                        } else if (model.uuid === "550e8400-e29b-41d4-a716-446655440008") {
                            return "qrc:/icon/icon/setting/summary.svg"
                        }
                    }
                    return "qrc:/icon/icon/setting/prompt.svg"
                }
                isFirst: index === 0
                isLast: index === commandListView.count - 1

                onEditClicked: {
                    if (commandModel && !model.isDefault) {
                        var cmd = commandModel.getCommand(model.uuid)
                        if (cmd && cmd.uuid) {
                            openEditCommandDialog(cmd.uuid, cmd.name, cmd.summary || "", cmd.description)
                        }
                    }
                }

                onDeleteClicked: {
                    if (!model.isDefault) {
                        openDeleteConfirmDialog(model.name, model.uuid)
                    }
                }

                onMoveUp: {
                    if (commandModel && index > 0) {
                        commandModel.moveCommand(index, index - 1)
                    }
                }

                onMoveDown: {
                    if (commandModel && index < commandListView.count - 1) {
                        commandModel.moveCommand(index, index + 1)
                    }
                }

                onChangeTransparentCommandItemDelegate: function(flag){
                    changeTransparentCommandPage(flag)
                }
            }
        }
    }
}
