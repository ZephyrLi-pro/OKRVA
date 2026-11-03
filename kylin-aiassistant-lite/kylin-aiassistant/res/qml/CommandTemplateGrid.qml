import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

Rectangle {
    id:commandEditingRectangle
    Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandEditingRectangle"
    visible: true
    width: 496
    height: Math.min(gridView.contentHeight + commandEditingRow.topPadding + commandEditingRow.bottomPadding, maxHeight)  // 最多显示
    radius: 8
    color: "white"

    property int maxHeight: 332 // 9 * (28 + 8) + 4 + 4

    function getTruncateCommandLabel(text, maxChars) {
        let totalUnits = 0;
        let truncatedText = "";

        for (let i = 0; i < text.length; i++) {
            let currentChar = text.charAt(i);

            // 判断是否为中文字符
            if (currentChar >= '\u4E00' && currentChar <= '\u9FFF') {
                // 中文字符占用 2 个单位
                totalUnits += 2;
            } else {
                // 英文或其他字符占用 1 个单位
                totalUnits += 1;
            }

            // 如果总单位数超过了允许的最大值，则截断
            if (totalUnits > maxChars * 2) {
                truncatedText += "...";
                break;
            }

            // 追加当前字符
            truncatedText += currentChar;
        }

        return truncatedText;
    }

    Row {
        id: commandEditingRow
        Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandEditingRow"
        anchors.fill: parent
        leftPadding: 4
        rightPadding: 4
        topPadding: 4
        bottomPadding: 4

        Flickable {
            id: commandFlickable
            Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandFlickable"
            width: parent.width
            height: Math.min(gridView.contentHeight + commandEditingRow.topPadding + commandEditingRow.bottomPadding, maxHeight)  // 最多显示
            contentHeight: gridView.contentHeight  + commandEditingRow.topPadding + commandEditingRow.bottomPadding
            boundsBehavior: Flickable.StopAtBounds  // 防止过度滚动
            clip: true

            ScrollBar.vertical: ScrollBar {
                id: scrollBar
                Accessible.name: "kylin-aiassistant_CommandTemplateGrid_scrollBar"
                policy: ScrollBar.AlwaysOn
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.bottom: parent.bottom

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: "#26262626"
                }

                visible: gridView.contentHeight > commandFlickable.height
                size: commandFlickable.height / gridView.contentHeight
                interactive: true
            }

            QtObject {
                id: settings
                Accessible.name: "kylin-aiassistant_CommandTemplateGrid_settings"

                // 网格单元的大小
                property int cellWidth: 160
                property int cellHeight: 36

                // 指令模板区域的大小
                property int iconWidth: 152
                property int iconHeight: 28

                property int dragIndex: -1
                property bool dragBehavior: false
            }

            Item {
                id: scrollView
                Accessible.name: "kylin-aiassistant_CommandTemplateGrid_scrollView"
                width: parent.width - commandEditingRow.leftPadding
//                height: parent.height - commandEditingRow.topPadding - commandEditingRow.bottomPadding
//                clip: true
                height: parent.height

                GridView {
                    id: gridView
                    Accessible.name: "kylin-aiassistant_CommandTemplateGrid_gridView"
                    anchors.fill: parent
                    cellWidth: settings.cellWidth
                    cellHeight: settings.cellHeight

                    move: Transition {
                        NumberAnimation { properties: "x"; duration: 100; easing.type: Easing.OutCubic }
                        NumberAnimation { properties: "y"; duration: 100; easing.type: Easing.OutCubic }
                    }
                    moveDisplaced: Transition {
                        NumberAnimation { properties: "x"; duration: 300; easing.type: Easing.OutCubic }
                        NumberAnimation { properties: "y"; duration: 100; easing.type: Easing.OutCubic }
                    }

                    model: commandTemplateListModel


                    delegate: Item {
                        width: settings.cellWidth
                        height: settings.cellHeight
                        z: mouseArea.pressed ? 1000 : 1

                        Rectangle {
                            id: commandRectangleArea
                            Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandRectangleArea"
                            anchors.centerIn: parent  // 图标区域居中显示在网格单元中
                            width: settings.iconWidth
                            height: settings.iconHeight
                            radius: 4

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                drag.target: parent
                                propagateComposedEvents: true

                                onEntered: {
                                    commandRectangle.color = "#3489EE";
                                    commandNameText.color = "#FFFFFF";
                                    commandDragIcon.source = "qrc:/icon/icon/setting/drag-white.svg";
                                    commandDeleteIcon.source = "qrc:/icon/icon/setting/delete-white.svg";
                                    settings.dragBehavior = false;
                                }
                                onExited: {
                                    commandRectangle.color = "#1a3790FA";
                                    commandNameText.color = "#3790FA";
                                    commandDragIcon.source = "qrc:/icon/icon/setting/drag-highlight.svg";
                                    commandDeleteIcon.source = "qrc:/icon/icon/setting/delete-highlight.svg";
                                    settings.dragBehavior = false;
                                }
                            }

                            Rectangle {
                                id: commandRectangle
                                Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandRectangle"
                                width: settings.iconWidth
                                height: settings.iconHeight
                                radius: 4
                                color: "#1a3790FA"

                                Behavior on x { enabled: settings.dragBehavior; NumberAnimation { duration: 200 } }
                                Behavior on y { enabled: settings.dragBehavior; NumberAnimation { duration: 200 } }

                                Row {
                                    id: commandRow
                                    Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandRow"
                                    anchors.fill: parent
                                    leftPadding: 4
                                    rightPadding: 4
                                    spacing: 6

                                    // 拖拽图标
                                    Image {
                                        id: commandDragIcon
                                        Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandDragIcon"
                                        source: "qrc:/icon/icon/setting/drag-highlight.svg"
                                        width: 16
                                        height: 16
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    // 指令名称
                                    Label {
                                        id: commandNameText;
                                        Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandNameText"
                                        text: getTruncateCommandLabel(model.name, 6);
                                        color: "#3790FA"
                                        width: commandRow.width - commandDeleteIcon.width - commandRow.leftPadding - commandRow.rightPadding - commandDragIcon.width - commandRow.spacing * 2
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // 删除图标，仅自定义指令显示
                                    Image {
                                        id: commandDeleteIcon
                                        Accessible.name: "kylin-aiassistant_CommandTemplateGrid_commandDeleteIcon"
                                        source: "qrc:/icon/icon/setting/delete-highlight.svg"
                                        visible: model.commandType === 2  // 仅在编辑模式下且为自定义指令时显示
                                        width: 16
                                        height: 16
                                        anchors.verticalCenter: parent.verticalCenter

                                        MouseArea {
                                            anchors.fill: parent

                                            onClicked: {
                                                // console.log("gridView.contentHeight" + gridView.contentHeight + "gridView.height" + gridView.height + "commandEditingRectangle" + commandEditingRectangle.height + "scrollView" + scrollView.height)
                                                // console.log("gridView.width" + gridView.width +  "commandEditingRectangle" + commandEditingRectangle.width + commandTemplateListModel.rowCount())
                                                 console.log("Deleting command:", model.name);

                                                if (model.commandType === 2) {
                                                    commandTemplateListModel.removeData(model.actorId, model.name);
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: mouseArea
                                    Accessible.name: "kylin-aiassistant_CommandTemplateGrid_mouseArea"
                                    anchors.fill: parent
                                    drag.target: parent
                                    propagateComposedEvents: true

                                    onPressed: {
                                        commandRectangle.color = "#2C73C8";
                                        commandNameText.color = "#FFFFFF";
                                        commandDragIcon.source = "qrc:/icon/icon/setting/drag-white.svg";
                                        commandDeleteIcon.source = "qrc:/icon/icon/setting/delete-white.svg";

                                        settings.dragBehavior = false;
                                        var pos = gridView.mapFromItem(commandRectangle, 0, 0);
                                        settings.dragIndex = model.index;
                                        commandRectangle.parent = gridView;
                                        commandRectangle.x = pos.x;
                                        commandRectangle.y = pos.y;
                                    }

                                    onReleased: {
                                        commandRectangle.color = "#1a3790FA";
                                        commandNameText.color = "#3790FA";
                                        commandDragIcon.source = "qrc:/icon/icon/setting/drag-highlight.svg";
                                        commandDeleteIcon.source = "qrc:/icon/icon/setting/delete-highlight.svg";

                                        settings.dragIndex = -1;
                                        var pos = gridView.mapToItem(commandRectangleArea, commandRectangle.x, commandRectangle.y);
                                        commandRectangle.parent = commandRectangleArea;
                                        commandRectangle.x = pos.x;
                                        commandRectangle.y = pos.y;
                                        settings.dragBehavior = true;
                                        commandRectangle.x = 0;
                                        commandRectangle.y = 0;
                                    }

                                    onPositionChanged: {
                                        // 获取Flickable的可视区域
                                        var minX = 0;
                                        var maxX = commandFlickable.width - commandRectangle.width - 16;
                                        var minY = 0;
                                        var maxY = commandFlickable.contentHeight - commandRectangle.height - 8;

                                        // 检测是否超出上或下边界，触发自动滚动
                                        var scrolledDistance = commandFlickable.contentY;
                                        var scrollSpeed = 5;  // 设置自动滚动的速度（可以根据需要调整）
                                        var shangy = minY + 16 + scrolledDistance;
                                        var xiay = minY + commandFlickable.height + scrolledDistance - commandRectangle.height - 16 - 36;

//                                        console.log("=========================scrolledDistance" + scrolledDistance + "==========commandFlickable.contentHeight" + commandFlickable.contentHeight + "============ commandFlickable.height" + commandFlickable.height );
//                                        console.log("======================上边界" + shangy + "===============y" + commandRectangle.y);
//                                        console.log("======================下边界" + xiay + "=================y" + commandRectangle.y);

                                        // 向上拖拽超过上边界时滚动
                                        if (commandRectangle.y < minY + 16 + scrolledDistance) {  // // 如果达到当前可见区域的上边界
                                            console.log("11111111111111向上超出边界")
                                            commandFlickable.contentY = Math.max(0, commandFlickable.contentY - scrollSpeed);
                                        }
                                        // 向下拖拽超过下边界时滚动
                                        else if (commandRectangle.y > minY + commandFlickable.height + scrolledDistance - commandRectangle.height - 16) {  // 如果达到当前可见区域的下边界
                                            console.log("22222222222222向下超出边界")
                                            commandFlickable.contentY = Math.min(commandFlickable.contentHeight - commandFlickable.height, commandFlickable.contentY + scrollSpeed);
                                        }

//                                        console.log("-----------commandFlickable.contentheight" + commandFlickable.contentHeight +"------------commandFlickable.height" +commandFlickable.height +
//                                                    "-----------commandRectangle.x" + commandRectangle.x + "--------------commandRectangle.y"  + commandRectangle.y)
//                                        console.log("===============maxX" + maxX + "===============maxY" + maxY)

                                        // 限制拖拽的范围
                                        commandRectangle.x = Math.max(minX, Math.min(commandRectangle.x, maxX));
                                        commandRectangle.y = Math.max(minY, Math.min(commandRectangle.y, maxY));

                                        var pos = gridView.mapFromItem(commandRectangle, 0, 0);
                                        var idx = gridView.indexAt(pos.x, pos.y);
                                        if (idx > -1 && idx < gridView.count) {
                                            commandTemplateListModel.moveCommand(settings.dragIndex,idx);
                                            settings.dragIndex = idx;
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

}
