import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12

import "."

// 模型角标
Rectangle{
    id: modelLabelReason
    Accessible.name: "kylin-aiassistant_Chat_modelLabel"
    width: modelLabelText.width
    height: 18
    property string modelVender: ""

    //anchors.right: messageRectangle.right
    color: "transparent"
    //设置左上右下不同圆角值
    Canvas {
        id: customRectCanvas
        Accessible.name: "kylin-aiassistant_Chat_customRectCanvas"
        width: modelLabelText.width
        height: 18

        onPaint: {
            var ctx = customRectCanvas.getContext('2d');
            ctx.clearRect(0, 0, customRectCanvas.width, customRectCanvas.height);
            var topLeftRadius = 0;
            var topRightRadius = 8;
            var bottomRightRadius =0;
            var bottomLeftRadius = 8;

            ctx.beginPath();
            ctx.moveTo(topLeftRadius, 0);
            ctx.lineTo(customRectCanvas.width - topRightRadius, 0);
            ctx.arc(customRectCanvas.width - topRightRadius, topRightRadius, topRightRadius, 1.5 * Math.PI, 2 * Math.PI);
            ctx.lineTo(customRectCanvas.width, customRectCanvas.height - bottomRightRadius);
            ctx.arc(customRectCanvas.width - bottomRightRadius, customRectCanvas.height - bottomRightRadius, bottomRightRadius, 0, 0.5 * Math.PI);
            ctx.lineTo(bottomLeftRadius, customRectCanvas.height);
            ctx.arc(bottomLeftRadius, customRectCanvas.height - bottomLeftRadius, bottomLeftRadius, 0.5 * Math.PI, Math.PI);
            ctx.lineTo(0, topLeftRadius);
            ctx.arc(0, topLeftRadius, topLeftRadius, Math.PI, 1.5 * Math.PI);
            ctx.closePath();
            ctx.fillStyle = "rgba(0, 0, 0, 0.03)";
            ctx.fill();
        }
    }

    Label {
        id: modelLabelText
        Accessible.name: "kylin-aiassistant_Chat_modelLabelText"
        text: modelVender
        font.pointSize: globals.fontsize - 4
        topPadding: 4
        bottomPadding: 4
        rightPadding: 8
        leftPadding: 8
        color: Qt.rgba(0/255, 0/255, 0/255, 0.3)
        anchors.verticalCenter: parent.verticalCenter
    }
}
