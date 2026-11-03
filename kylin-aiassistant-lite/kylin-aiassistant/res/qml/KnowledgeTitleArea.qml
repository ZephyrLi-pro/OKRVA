import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Row{
    id: knowledgeTitleAreaRet
    height: 170

    property int paddingSize: 24

    Column{
        id: knowledgeTitleArea
        Layout.fillWidth: true
        spacing: 6

        Rectangle{
            width: 64
            height: 83
            anchors.horizontalCenter: parent.horizontalCenter
            color: "transparent"
            Image {
                id: knowledgeImage
                source: "qrc:/icon/icon/knowledge-show.svg"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Image {
                id: textureImage
                source: "qrc:/icon/icon/texture.svg"
                smooth: true  // 抗锯齿
                width: 792
                height: 402
                anchors.centerIn: parent
                z: -1  // 层级设为-1，确保在所有子元素下方
            }
        }

        Label{
            id: knowledgeLabel
            text: qsTr("Personal Knowledge Base")
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            wrapMode: Text.NoWrap
            font.bold: true
            font.family: root.fontFamily
            font.pointSize: knowledgeDescriptionLabel.font.pointSize + 6
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Label{
            id: knowledgeDescriptionLabel
            textFormat: Text.RichText
            text: qsTr("Upload and manage your materials; AI answers accurately from them.") + "<a href='learn' style='color:#3790FA;text-decoration:none'>" + qsTr("Learn more") + "</a>"
            horizontalAlignment: Text.AlignHCenter
            //elide: Text.ElideRight
            wrapMode: Text.WordWrap
            width: knowledgeTitleAreaRet.width
            color: Qt.rgba(0, 0, 0, 0.6)
            anchors.horizontalCenter: parent.horizontalCenter

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onEntered: {
                    knowledgeDescriptionLabel.text = qsTr("Upload and manage your materials; AI answers accurately from them.") + "<a href='learn' style='color:#3489EE;text-decoration:none'>" + qsTr("Learn more") + "</a>"
                }

                onExited: {
                    knowledgeDescriptionLabel.text = qsTr("Upload and manage your materials; AI answers accurately from them.") + "<a href='learn' style='color:#3790FA;text-decoration:none'>" + qsTr("Learn more") + "</a>"
                }

                onPressed: {
                    knowledgeDescriptionLabel.text = qsTr("Upload and manage your materials; AI answers accurately from them.") + "<a href='learn' style='color:#2C73C8;text-decoration:none'>" + qsTr("Learn more") + "</a>"
                }

                onReleased: {
                    knowledgeDescriptionLabel.text = qsTr("Upload and manage your materials; AI answers accurately from them.") + "<a href='learn' style='color:#3489EE;text-decoration:none'>" + qsTr("Learn more") + "</a>"
                }

                onClicked: {
                    showLearnMoreWindow()
                }
            }
        }


        Rectangle{
            width: 64
            height: 69 - knowledgeLabel.height - knowledgeDescriptionLabel.height
            color: "transparent"
        }
    }
    // 了解一下弹窗相对主窗口居中
    LearnMoreWindow {
        id: learnMoreWindow
        mainWindow: root

        // 当弹窗消失时，改变“了解一下”超链接的悬浮颜色，解决三态问题。
        onWindowClosed: {
            knowledgeDescriptionLabel.linkHovered("")
        }
    }

    function showLearnMoreWindow() {
        learnMoreWindow.showCentered()
    }
}


