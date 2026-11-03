import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: targetDocument
    
    property string iconPath: ""
    property string text: ""
    
    signal clicked()

    function clear () {
        iconPath = ""
        text = ""
    }

    Rectangle {
        clip: true
        color: "#0d262626"
        anchors.fill: parent
        anchors.topMargin: 0
        anchors.bottomMargin: 8
        radius: 6

        RowLayout {
            spacing: 8
            anchors.fill: parent
            Layout.alignment: Qt.AlignCenter
            Image {
                Layout.leftMargin: 8
                width: 16
                height: 16
                source: "qrc:/icon/icon/quote.svg"
                opacity: 0.85
            }
            Image {
                source: targetDocument.iconPath
                sourceSize.height: 16
                sourceSize.width: 16
            }
            Label {
                id: targetDocumentText
                text: targetDocument.text
                Layout.fillWidth: true
                clip: true
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                color: Qt.rgba(0, 0, 0, 0.85)

                // 鼠标区域用于检测悬停
                MouseArea {
                    id: targetDocumentTextMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
                // 悬浮提示 - 只在文字被省略时显示
                ToolTip {
                    visible: targetDocumentTextMouseArea.containsMouse && (targetDocumentText.implicitWidth > targetDocumentText.width)
                    text: targetDocumentText.text
                    delay: Qt.styleHints.mousePressAndHoldInterval
                    property var maxWidth: 400
                    property var wrapMode: Text.WrapAnywhere
                }
            }

            Rectangle{
                width: 24
                height: 24
                Layout.rightMargin: 8
                color: "transparent"
                radius: 6

                Image {
                    id: targetDocumentCloseBtn
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/close-black.svg"
                    anchors.centerIn: parent
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true

                    onEntered: {
                        parent.color = Qt.rgba(0, 0, 0, 0.15)
                    }
                    onExited: {
                        parent.color = "transparent"
                    }
                    onPressed: {
                        parent.color = Qt.rgba(0, 0, 0, 0.2)
                    }
                    onReleased: {
                        parent.color = "transparent"
                    }
                    onClicked: {
                        targetDocument.clicked()
                    }
                }
            }
        }
    }
}

