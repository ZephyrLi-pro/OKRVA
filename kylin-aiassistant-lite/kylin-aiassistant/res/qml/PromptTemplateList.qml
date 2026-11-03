import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

Rectangle {
    id:promptRectangle
    Accessible.name: "kylin-aiassistant_PromptTemplateList_promptRectangle"
    visible: true
    width: 496
    height: listView.contentHeight
    color: "#F5F5F5"
//    radius: 6
    antialiasing: true // 启用抗锯齿

    property int maxHeight: 332 // 9 * (28 + 8) + 4 + 4

    Component {
        id: listDelegate

        Item {
            id: delegateItem
            Accessible.name: "kylin-aiassistant_PromptTemplateList_delegateItem"
            width: listItem.width
            height: listItem.height
            antialiasing: true

            Rectangle {
                id: listItem
                Accessible.name: "kylin-aiassistant_PromptTemplateList_listItem"
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                width: listView.width
                height: 44
                radius: 6
                antialiasing: true // 启用抗锯齿

                MouseArea {
                    id: mouseArea
                    Accessible.name: "kylin-aiassistant_PromptTemplateList_mouseArea"
                    anchors.fill: parent
                    enabled: true
                    hoverEnabled: true
                    propagateComposedEvents: true

                    onEntered: {
                        if (model.commandType === 1)
                        { // 预设只读提示词
                            defaultPromptBtn.visible = true
                            viewPromptBtn.visible = true
                            viewPromptImage.visible = true
                            customPromptBtn.visible = false
                            editPromptBtn.visible = false
                            editPromptImage.visible = false
                            deletePromptBtn.visible = false
                            deletePromptImage.visible = false

                        } else if (model.commandType === 2)
                        { // 自定义提示词
                            defaultPromptBtn.visible = false
                            viewPromptBtn.visible = false
                            viewPromptImage.visible = false
                            viewPromptBtn.color = "#transparent"
                            customPromptBtn.visible = true
                            editPromptBtn.visible = true
                            editPromptImage.visible = true
                            deletePromptBtn.visible = true
                            deletePromptImage.visible = true
                        } else
                        { // 预设不可见提示词
                            defaultPromptBtn.visible = false
                            viewPromptBtn.visible = false
                            viewPromptImage.visible = false
                            viewPromptBtn.color = "transparent"
                            customPromptBtn.visible = false
                            editPromptBtn.visible = false
                            editPromptImage.visible = false
                            editPromptBtn.color = "transparent"
                            deletePromptBtn.visible = false
                            deletePromptImage.visible = false
                            deletePromptBtn.color = "transparent"
                        }
                    }

                    onExited: {
                        defaultPromptBtn.visible = false
                        viewPromptBtn.visible = false
                        viewPromptImage.visible = false
                        viewPromptBtn.color = "transparent"
                        customPromptBtn.visible = false
                        editPromptBtn.visible = false
                        editPromptImage.visible = false
                        editPromptBtn.color = "transparent"
                        deletePromptBtn.visible = false
                        deletePromptImage.visible = false
                        deletePromptBtn.color = "transparent"

                    }

                    // 为了处理内部按钮无法触发鼠标事件的问题，将整个row加到mouseArea
                    Row {
                        id: listItemRow
                        Accessible.name: "kylin-aiassistant_PromptTemplateList_listItemRow"
                        anchors.fill: parent
                        leftPadding: 12
                        rightPadding: 0
                        spacing: 8

                        // 提示词图标
                        Image {
                            id: promptIcon
                            Accessible.name: "kylin-aiassistant_PromptTemplateList_promptIcon"
                            source: model.iconPath
                            sourceSize.width: 16
                            sourceSize.height: 16
                            smooth: true
                            width: 16
                            height: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // 提示词名称
                        Label {
                            id: promptNameText;
                            Accessible.name: "kylin-aiassistant_PromptTemplateList_promptNameText"
                            text: model.name
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.85);
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // 提示词内容
                        Label {
                            id: promptContentText;
                            Accessible.name: "kylin-aiassistant_PromptTemplateList_promptContentText"
                            text: model.commandType === 2 ? qsTr("User created prompt word template") : model.description.replace(/\n/g, "")
                            font.pointSize: globals.fontsize - 2
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.6);
                            width: listItem.width - listItemRow.leftPadding - listItemRow.rightPadding - promptIcon.width - promptNameText.implicitWidth - 2 * listItemRow.spacing
                                    - (defaultPromptBtn.visible ? defaultPromptBtn.width + 8 : 0)
                                    - (customPromptBtn.visible ?  customPromptBtn.width + 8 : 0)
                                    - (!defaultPromptBtn.visible && !customPromptBtn.visible ? 12 : 0) // 如果按钮不可见，则再减去 12

                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter

                        }

                        Row {
                            id: defaultPromptBtn
                            Accessible.name: "kylin-aiassistant_PromptTemplateList_defaultPromptBtn"
                            width: viewPromptBtn.width + defaultPromptBtn.spacing + defaultPromptBtn.leftPadding + defaultPromptBtn.rightPadding
                            height: 44
                            leftPadding: 0
                            rightPadding: 4
                            spacing: 0
                            visible: false

                            // 查看预设提示词
                            Rectangle {
                                id: viewPromptBtn
                                Accessible.name: "kylin-aiassistant_PromptTemplateList_viewPromptBtn"
                                width: 36
                                height: 36
                                radius: 6
                                visible: false
                                color: "transparent"
                                anchors.verticalCenter: parent.verticalCenter
                                ToolTip {
                                    text: qsTr("View")
                                    visible: viewPromptBtnMouseArea.containsMouse && !viewPromptBtnMouseArea.pressed
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                }

                                Image {
                                    id: viewPromptImage
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_viewPromptImage"
                                    visible: false
                                    source: "qrc:/icon/icon/setting/view.svg"
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    sourceSize.width: 16
                                    sourceSize.height: 16
                                    smooth: true  // 开启抗锯齿
                                    width: 16
                                    height: 16
                                }

                                MouseArea {
                                    id: viewPromptBtnMouseArea
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_viewPromptBtnMouseArea"
                                    anchors.fill: parent
                                    enabled: true
                                    hoverEnabled: true

                                    onEntered: {
                                        viewPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }
                                    onExited: {
                                        viewPromptBtn.color = "transparent"
                                    }
                                    onPressed: {
                                        viewPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.2)
                                    }
                                    onReleased: {
                                        viewPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }

                                    onClicked: {
                                        if(viewPromptBtn.visible){
                                            console.log("viewButton click " + model.name + model.actorId)
                                            commandPage.onCommandClicked(model.commandType, model.actorId, model.name, model.content)

                                        }
                                    }
                                }
                            }

                        }

                        Row {
                            id: customPromptBtn
                            Accessible.name: "kylin-aiassistant_PromptTemplateList_customPromptBtn"
                            width: editPromptBtn.width + deletePromptBtn.width + customPromptBtn.spacing + customPromptBtn.leftPadding + customPromptBtn.rightPadding
                            height: 44
                            leftPadding: 0
                            rightPadding: 4
                            spacing: 4
                            visible: false

                            // 编辑自定义提示词
                            Rectangle {
                                id: editPromptBtn
                                Accessible.name: "kylin-aiassistant_PromptTemplateList_editPromptBtn"
                                width: 36
                                height: 36
                                radius: 6
                                visible: false
                                color: "transparent"
                                anchors.verticalCenter: parent.verticalCenter
                                ToolTip {
                                    text: qsTr("Edit")
                                    visible: editPromptBtnMouseArea.containsMouse && !editPromptBtnMouseArea.pressed
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                }

                                Image {
                                    id: editPromptImage
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_editPromptImage"
                                    visible: false
                                    source: "qrc:/icon/icon/setting/edit.svg"
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    sourceSize.width: 16
                                    sourceSize.height: 16
                                    smooth: true  // 开启抗锯齿
                                    width: 16
                                    height: 16
                                }

                                MouseArea {
                                    id: editPromptBtnMouseArea
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_editPromptBtnMouseArea"
                                    anchors.fill: parent
                                    enabled: true
                                    hoverEnabled: true

                                    onEntered: {
                                        editPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }
                                    onExited: {
                                        editPromptBtn.color = "transparent"
                                    }
                                    onPressed: {
                                        editPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.2)
                                    }
                                    onReleased: {
                                        editPromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }

                                    onClicked: {
                                        if(editPromptBtn.visible){
                                            console.log("editButton click " + model.name + model.actorId)
                                            commandPage.onCommandClicked(model.commandType, model.actorId, model.name, model.content)

                                        }
                                    }
                                }
                            }

                            // 删除自定义提示词
                            Rectangle {
                                id: deletePromptBtn
                                Accessible.name: "kylin-aiassistant_PromptTemplateList_deletePromptBtn"
                                width: 36
                                height: 36
                                radius: 6
                                visible: false
                                color: "transparent"
                                anchors.verticalCenter: parent.verticalCenter
                                ToolTip {
                                    text: qsTr("Delete")
                                    visible: deletePromptBtnMouseArea.containsMouse && !deletePromptBtnMouseArea.pressed
                                    delay: Qt.styleHints.mousePressAndHoldInterval
                                }


                                Image {
                                    id: deletePromptImage
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_deletePromptImage"
                                    visible: false
                                    source: "qrc:/icon/icon/setting/delete-black.svg"
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    sourceSize.width: 16
                                    sourceSize.height: 16
                                    smooth: true  // 开启抗锯齿
                                    width: 16
                                    height: 16
                                }

                                MouseArea {
                                    id: deletePromptBtnMouseArea
                                    Accessible.name: "kylin-aiassistant_PromptTemplateList_deletePromptBtnMouseArea"
                                    anchors.fill: parent
                                    enabled: true
                                    hoverEnabled: true

                                    onEntered: {
                                        deletePromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }
                                    onExited: {
                                        deletePromptBtn.color = "transparent"
                                    }
                                    onPressed: {
                                        deletePromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.2)
                                    }
                                    onReleased: {
                                        deletePromptBtn.color = Qt.rgba(0/255, 0/255, 0/255, 0.15)
                                    }

                                    onClicked: {
                                        if(deletePromptBtn.visible){
                                            console.log("deleteButton click " + model.name + model.actorId + listView.contentHeight)
                                            commandPage.showConfirmMessage(model.actorId, model.name)

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

    Item {
        id:rootItem
        Accessible.name: "kylin-aiassistant_PromptTemplateList_rootItem"
        anchors.fill: parent

        ListView {
            id: listView
            Accessible.name: "kylin-aiassistant_PromptTemplateList_listView"
            anchors.fill: parent
            interactive: false
            delegate: listDelegate
            model: commandTemplateListModel
            spacing: 4
        }
    }

    Connections {
        target: listView
        onContentHeightChanged: {
            commandPage.onContentHeightChanged(listView.contentHeight);
            console.log("ListView contentHeight changed to::", listView.contentHeight);
        }
    }
}
