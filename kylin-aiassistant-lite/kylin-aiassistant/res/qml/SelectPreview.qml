import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: previewRectangle
    Accessible.name: "kylin-aiassistant_SelectPreview_previewRectangle"

    property int  itemMargin: 8

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

    Canvas {
        id: backgroundCanvas
        anchors.fill: parent
        antialiasing: true
        Accessible.name: "kylin-aiassistant_SelectPreview_backgroundCanvas"

        onPaint: {
            var ctx = getContext("2d");

            // 清空画布
            ctx.clearRect(0, 0, width, height);

            // 创建渐变 (从上到下)
            var gradient = ctx.createLinearGradient(0, 0, 0, height);
            gradient.addColorStop(0, "rgba(55, 144, 250, 0.75)");
            gradient.addColorStop(1, "rgba(55, 144, 250, 0.25)");

            // 设置填充样式为渐变
            ctx.fillStyle = gradient;

            // 开始绘制路径
            ctx.beginPath();
            // 左上角圆角
            ctx.moveTo(8, 0);
            ctx.arcTo(0, 0, 0, 8, 8);
            // 左边直线
            ctx.lineTo(0, height);
            // 底边直线
            ctx.lineTo(width, height);
            // 右边直线
            ctx.lineTo(width, 8);
            // 右上角圆角
            ctx.arcTo(width, 0, width - 8, 0, 8);
            // 顶边直线
            ctx.lineTo(8, 0);
            ctx.closePath();
            // 填充路径
            ctx.fill();
        }
    }

    Rectangle {
        id:selectionRectangle
        Accessible.name: "kylin-aiassistant_SelectPreview_selectionRectangle"
        color:"white"
        enabled: true
        width:childrenRect.width
        height: 36
        radius: 8;
        anchors.centerIn: parent

        //主要布局
        RowLayout{
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            // 固定指令（前三个）
            Row {
                id:selectCommandFlow
                Accessible.name: "kylin-aiassistant_SelectPreview_selectCommandFlow"
                height: 28
                width:childrenRect.width
                leftPadding: 4
                rightPadding: 0

                Repeater {
                    id:selectRepeater
                    Accessible.name: "kylin-aiassistant_SelectPreview_selectRepeater"
                    visible: true
                    model: commandTemplateListModel;

                    delegate: Rectangle {
                        width: selectRect.width + 9
                        height: 28
                        visible: index <3 //只显示前三个
                        Accessible.name: "kylin-aiassistant_SelectPreview_Rectangle_1"

                        Row {
                            spacing: 4
                            id:selectButtonLayout
                            Accessible.name: "kylin-aiassistant_SelectPreview_selectButtonLayout"

                            Rectangle{
                                id:selectRect
                                Accessible.name: "kylin-aiassistant_SelectPreview_selectRect"
                                width: selectButtonText.implicitWidth + tttttt.leftPadding+tttttt.rightPadding ;
                                height:28;
//                                color: Qt.rgba(55/255, 144/255, 250/255, 0.1)
                                radius: 4
                                Row{
                                    leftPadding: 4
                                    rightPadding: 4
                                    anchors.centerIn: parent;
                                    id:tttttt
                                    Accessible.name: "kylin-aiassistant_SelectPreview_tttttt"
                                    Label {
                                        id: selectButtonText;
                                        Accessible.name: "kylin-aiassistant_SelectPreview_selectButtonText"
                                        text:getTruncateCommandLabel(name, 6); // 中文不超过 6 个字符，英文不超过 12 个字符
                                        color: Qt.rgba(0/255, 0/255, 0/255);

                                    }
                                }
                            }
                            // 分割线1
                            Rectangle{
                                id:line1
                                Accessible.name: "kylin-aiassistant_SelectPreview_line1"
                                width:1;
                                height:16;
                                color: Qt.rgba(38/255, 38/255, 38/255,0.15);
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
            // 更多按钮
            Rectangle {
                width: 68;
                height: 28;
//                color: Qt.rgba(38/255, 38/255, 38/255, 0.05);
                radius: 4
                id:moreRect
                Accessible.name: "kylin-aiassistant_SelectPreview_moreRect"
                RowLayout {
                    anchors.centerIn: parent
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
                    Image {
                        id: m_moreSvg

                        Accessible.name: "kylin-aiassistant_SelectPreview_m_moreSvg"
                        source: "qrc:/icon/icon/setting/more.svg"
                        visible: true
                    }
                    Label { id: m_totalButton; text: qsTr("More"); color: Qt.rgba(0/255, 0/255, 0/255); Accessible.name: "kylin-aiassistant_SelectPreview_m_totalButton"}

                }
            }
            // 分割线2
            Rectangle{
                id:lineRec2
                Accessible.name: "kylin-aiassistant_SelectPreview_lineRec2"
                width: line2.width + lineRow2.leftPadding + lineRow2.rightPadding;
                height: 16

                Row{
                    id:lineRow2
                    Accessible.name: "kylin-aiassistant_SelectPreview_lineRow2"
                    leftPadding: 4
                    rightPadding: 4
                    anchors.centerIn: parent;

                    Rectangle{
                        id:line2
                        Accessible.name: "kylin-aiassistant_SelectPreview_line2"
                        width:1;
                        height:16;
                        color: Qt.rgba(38/255, 38/255, 38/255,0.15);

                    }
                }
            }
            // 右键菜单按钮
            Rectangle {
                width: 30;
                height: 28;
                color: Qt.rgba(255/255, 255/255, 255/255)
                radius: 4
                id: menuRect
                Accessible.name: "kylin-aiassistant_SelectPreview_menuRect"
                Row {
                    anchors.centerIn: parent
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
                    Image {
                        id: m_textSendButton
                        Accessible.name: "kylin-aiassistant_SelectPreview_m_textSendButton"
                        source: "qrc:/icon/icon/icon_aiassistant.svg"
                        visible: true

                    }
                }

            }

        }
    }
}
