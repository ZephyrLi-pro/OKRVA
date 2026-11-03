import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
Rectangle {
    id: commandPageRectangle
    Accessible.name: "kylin-aiassistant_CommandTemplateFlow_commandPageRectangle"
    width: 496
    height: commandFlickable.height
    color: "white"
    radius: 8

    property int maxHeight: 340

    Flickable{
        id:commandFlickable
        anchors.top: parent.top
        width:commandPageRectangle.width
        height: Math.min(commandRepeater.implicitHeight, maxHeight)  // 最多显示
        contentHeight: commandRepeater.implicitHeight
        boundsBehavior: Flickable.StopAtBounds  // 防止过度滚动
        clip: true

        // 自定义垂直滚动条
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOff  // 仅在需要时显示
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.bottom: parent.bottom

            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: "#26262626"
            }
            size: commandFlickable.height / commandRepeater.implicitHeight
        }
        property bool isCommandContentOverflowing: false

        onContentHeightChanged: {
//            console.log("contentHeight" + contentHeight + "height" +maxHeight)
            // 判断内容是否超出容器高度，超出显示滚动条
            isCommandContentOverflowing = contentHeight > maxHeight
            if(isCommandContentOverflowing){
                commandFlickable.ScrollBar.vertical.policy = ScrollBar.AlwaysOn
            }else{
                commandFlickable.ScrollBar.vertical.policy = ScrollBar.AlwaysOff
            }
        }
        Flow{
            id:commandRepeater
            Accessible.name: "kylin-aiassistant_CommandTemplateFlow_commandRepeater"
            width:commandPageRectangle.width - 8  // 减去滚动条占据空间
            height: implicitHeight
            clip: false
            spacing: 8
            leftPadding: 8
            rightPadding: 8
            topPadding: 8
            bottomPadding: 8

            Repeater {
                model: commandTemplateListModel;
                delegate: Rectangle {
                    id: commandRectangle;
                    Accessible.name: "kylin-aiassistant_CommandTemplateFlow_commandRectangle"
                    width: commandName.implicitWidth + commandRow.leftPadding + commandRow.rightPadding;
                    height:28;
                    color: commandType === 3 ? "#14000000" : "#1a3790FA"
                    radius: 4

                    MouseArea {
                        anchors.fill: parent
                        enabled: commandType !== 3  // 默认状态下，仅自定义指令时可点击,编辑状态下都可以点击
                        // 按钮三态
                        hoverEnabled: true
                        onEntered: {
                            commandRectangle.color = "#3489EE";
                            commandName.color = "#FFFFFF";
                        }
                        onExited: {
                            commandRectangle.color = "#1a3790FA";
                            commandName.color = "#3790FA";
                        }
                        onPressed: {
                            commandRectangle.color = "#2C73C8";
                            commandName.color = "#FFFFFF";
                        }
                        onReleased: {
                            commandRectangle.color = "#1a3790FA";
                            commandName.color = "#3790FA";
                        }
                        onClicked: {
//                            console.log("Clicked on", commandName.text);
                            if (commandType !== 3) {
                                commandPage.onCommandClicked(commandType, actorId, name, content);
                            }

                        }
                    }
                    Row {
                        id: commandRow
                        Accessible.name: "kylin-aiassistant_CommandTemplateFlow_commandRow"
                        anchors.centerIn: parent
                        leftPadding: 8
                        rightPadding: 8

                        // 指令名称
                        Label {
                            id: commandName;
                            Accessible.name: "kylin-aiassistant_CommandTemplateFlow_commandName"
                            text: name;
                            color: commandType === 3 ? "#AFAFAF" : "#3790FA"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                    }


                }

            }

            Rectangle {
                id: addButtonRectangle
                Accessible.name: "kylin-aiassistant_CommandTemplateFlow_addButtonRectangle"
                radius: 4;
                height: 28;
                width: 72;
                color: "#1a3790FA";

                Row {
                    anchors.centerIn: parent
                    spacing: 4

                    Image {
                        id: addButtonImage
                        Accessible.name: "kylin-aiassistant_CommandTemplateFlow_addButtonImage"
                        width: 16
                        height: 16
                        source: "qrc:/icon/icon/setting/list-add-highlight.svg"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        id: addButtonText
                        text: qsTr("Add")
                        color: "#3790FA";
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    // 按钮三态
                    hoverEnabled: true
                    onEntered: {
                        parent.color = "#3489EE";
                        addButtonText.color = "#FFFFFF";
                        addButtonImage.source = "qrc:/icon/icon/setting/list-add-white.svg";
                    }
                    onExited: {
                        parent.color = "#1a3790FA";
                        addButtonText.color = "#3790FA";
                        addButtonImage.source = "qrc:/icon/icon/setting/list-add-highlight.svg";
                    }
                    onPressed: {
                        parent.color = "#2C73C8";
                        addButtonText.color = "#FFFFFF";
                        addButtonImage.source = "qrc:/icon/icon/setting/list-add-white.svg";
                    }
                    onReleased: {
                        parent.color = "#1a3790FA";
                        addButtonText.color = "#3790FA";
                        addButtonImage.source = "qrc:/icon/icon/setting/list-add-highlight.svg";
                    }
                    onClicked: {
//                        console.log("Add button clicked")
                        commandPage.onAddBtnClicked();
                    }
                }

            }

            Connections
            {
                target: commandPage


            }

          }


    }

}
