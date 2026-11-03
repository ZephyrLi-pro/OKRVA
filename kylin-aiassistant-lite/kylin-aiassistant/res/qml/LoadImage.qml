import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

Rectangle {
    id: loadImageRect
    anchors.fill: parent
    color: "white"
    z: 100

    RowLayout {
        anchors.fill: parent
        Layout.alignment: Qt.AlignCenter

        ColumnLayout {
            id: loadContentLayout
            Layout.alignment: Qt.AlignCenter
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                id: loadImageLoader
                sourceComponent: AnimatedImage {
                    id: loadImageAnimation
                    source: "qrc:/icon/icon/spark.svg"
                    visible: true
                }
                Layout.alignment: Qt.AlignHCenter
                active: loadImageRect.visible
            }

            Label {
                id: loadText
                text: qsTr("Qi Ling is being prepared...")
                font.bold: true
                font.family: root.fontFamily
                wrapMode: Text.WordWrap
                color: "black"
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Label {
                id: loadDescriText
                text: qsTr("The AI service is being initialized to provide you with a more intelligent and efficient Qi Ling experience.")
                font.pointSize: globals.fontsize - 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: parent.width
                leftPadding: 20
                rightPadding: 20
                Layout.fillWidth: true
            }
        }
    }
    // 改进的鼠标区域配置
    MouseArea {
        id: maskMouseArea
        anchors.fill: parent
        enabled: true
        propagateComposedEvents: false // 阻止事件传播到下层
        acceptedButtons: Qt.AllButtons // 接受所有按钮事件
        hoverEnabled: true // 启用悬停检测

        // 捕获所有鼠标事件但不做任何处理
        onPressed: mouse.accepted = true
        onClicked: {}
        onReleased: {}
        onDoubleClicked: {}
        onWheel: {}
    }
}
