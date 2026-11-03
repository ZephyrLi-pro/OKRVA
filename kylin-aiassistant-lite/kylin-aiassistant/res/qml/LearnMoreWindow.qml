import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.12
import "."

Window {
    id: learnMoreWindow
    width: 372
    height: 496
    maximumHeight: height
    maximumWidth: width
    minimumHeight: height
    minimumWidth: width
    modality: Qt.WindowModal
    flags: Qt.Dialog

    // 外部传入的主窗口，用于居中
    property var mainWindow: null
    signal windowClosed;

    Component.onCompleted: {
        windowCtrl.clearWindowStyle(learnMoreWindow)
    }

    function showCentered() {
        if (mainWindow) {
            learnMoreWindow.x = mainWindow.x + (mainWindow.width - width) / 2
            learnMoreWindow.y = mainWindow.y + (mainWindow.height - height) / 2
        }
        learnMoreWindow.visible = true
    }

    function hideDialog() {
        learnMoreWindow.visible = false
        windowClosed()
    }

    onVisibleChanged: {
        learnMoreTextScrollView.ScrollBar.vertical.position = 0
    }

    ColumnLayout {
        anchors.fill: parent
        focus: true
        spacing: 0

        Keys.onEscapePressed: {
            hideDialog()
        }

        Rectangle {
            id: titleBar
            height: 40
            color: "#FFFFFF"
            radius: 12
            Layout.fillWidth: true


            // 鼠标拖动区域，需要放在前面，不然关闭按钮触发不了click
            MouseArea {
                anchors.fill: parent
                property point startDragPos: Qt.point(0, 0)

                onPressed: {
                    startDragPos = Qt.point(mouse.x, mouse.y)
                }

                onPositionChanged: {
                    var delta = Qt.point(mouse.x - startDragPos.x, mouse.y - startDragPos.y)
                    learnMoreWindow.x += delta.x
                    learnMoreWindow.y += delta.y
                }
            }

            Button {
                id: closeButton
                width: 32
                height: 32
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 4
                anchors.rightMargin: 4
                icon.name: "window-close-symbolic"
                focusPolicy: Qt.NoFocus

                flat: true
                property string buttonType: "windowTitleBarCloseButton"

                onHoveredChanged: {
                    if(hovered)
                        closeButtonTooltip.visible = true
                    else
                        closeButtonTooltip.visible = false
                }

                ToolTip {
                    id: closeButtonTooltip
                    text: qsTr("close")
                    delay: Qt.styleHints.mousePressAndHoldInterval
                }

                onClicked: {
                    hideDialog()
                }
            }
        }


        ScrollView {
            id: learnMoreTextScrollView
            Accessible.name: "kylin-aiassistant_LearnMoreWindow_learnMoreTextScrollView"
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 4
            Layout.bottomMargin: 24
            clip: true
            ScrollBar.horizontal.visible : false
            ScrollBar.vertical.visible : implicitContentHeight > learnMoreTextScrollView.height ? true : false

            Column {
                width: parent.width

                Image {
                    id: bannerImage
                    width: 328
                    height: 164
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/icon/icon/knowledgebase/knowledge_file_example.png"
                }

                Rectangle{
                    height: 16
                    width: parent.width
                }

                Text {
                    width: parent.width
                    text: qsTr("What is a personal knowledge base?")
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    font.bold: true
                    color: Qt.rgba(0, 0, 0, 0.85)
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: qsTr("A personal knowledge base is an exclusive space for storing and managing your personal information. You can upload files and let AI answer questions, organize information or generate content based solely on these contents, helping you use your data more accurately and efficiently.")
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    color: Qt.rgba(0, 0, 0, 0.6)
                    wrapMode: Text.WordWrap
                }

                Rectangle{
                    height: 16
                    width: parent.width
                }

                Text {
                    width: parent.width
                    text: qsTr("How to use it?")
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    font.bold: true
                    color: Qt.rgba(0, 0, 0, 0.85)
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: qsTr("1. Upload your document or information to the 「<b>Personal Knowledge Base</b>」.")
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    color: Qt.rgba(0, 0, 0, 0.6)
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: qsTr("2. Click on the 「<b>Knowledge Base</b>」 entrance in the bottom right corner of the input box in the 「<b>Q&amp;A</b>」 module, select the relevant files, and then ask a question. For example, \" What work did I do in February \".")
                    font.pointSize: globals.fontsize
                    font.family: root.fontFamily
                    color: Qt.rgba(0, 0, 0, 0.6)
                    wrapMode: Text.WordWrap
                }
            }
        }

    }
}
