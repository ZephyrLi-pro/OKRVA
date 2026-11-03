import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."


Rectangle {
    id: progressBar

    // 暴露的属性：当前进度百分比（0-100）
    property real progress: 50
    // 进度条填充颜色（可自定义）
    property color fillColor: Qt.rgba(55/255, 144/255, 250/255, 1)
    // 动画时长（毫秒）
    property int animationDuration: 100
    property int progressBarHeight: 8
    property int progressBarWidth: 0

    height: progressBarHeight
    width: progressBarWidth
    // 背景色（未填充部分）
    color: "#EEEEEE"
    // 圆角效果
    radius: height / 2

    // 填充进度的前景条
    Rectangle {
        id: fillBar
        height: parent.height
        // 宽度根据百分比计算（限制在0到父组件宽度之间）
        width: progress === 0 ? 0 : Math.max(parent.height, Math.min(parent.width, parent.progress / 100 * parent.width))
        color: parent.fillColor
        radius: parent.radius

        // 进度变化时的动画效果
//        Behavior on width {
//            NumberAnimation {
//                duration: parent.animationDuration
//                easing.type: Easing.InOutQuad
//            }
//        }
    }
}
