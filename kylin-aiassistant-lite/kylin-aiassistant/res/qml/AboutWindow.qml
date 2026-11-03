import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import "."

Window {
    id: aboutPage
    width: 176 + aboutPageText.width
    height: 124 + image.height + aboutPageText.height
    visible: aboutPageVisible
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"

    function closeAboutPage() {
        aboutPageVisible = false
    }

    property int fontSize: Qt.application.font.pointSize
    property string fontFamily: Qt.application.font.family

    property point startDragPos: Qt.point(0, 0)
    property int shakeOriginX: 0

    SequentialAnimation {
        id: shakeAnimation
        loops: 1
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX - 10; duration: 55;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX + 10; duration: 80;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX - 7;  duration: 80;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX + 7;  duration: 80;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX - 5;  duration: 80;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: aboutPage; property: "x"; to: aboutPage.shakeOriginX;     duration: 55;  easing.type: Easing.InOutQuad }
    }

    Component.onCompleted: {
        windowCtrl.clearWindowStyle(aboutPage)
    }

    function centerOnAssistantWindow() {
        console.log("===centerOnAssistantWindow===")
        if (root) {
            console.log("===centerOnAssistantWindow===root" + root.objectName + "aboutPage.width" + aboutPage.width + " height"+ aboutPage.height)
            x = root.x + (root.width - width) / 2
            y = root.y + (root.height - height) / 2
            return
        }
    }

    //监听mainWnd的信号
    Connections{
        target: mainWnd
        onSigShowAbout: function(){
            aboutPageVisible = true
        }
        function onSigActivateAboutWindow(){
            aboutPage.requestActivate()
            aboutPage.shakeOriginX = aboutPage.x
            shakeAnimation.restart()
        }
    }

    onVisibleChanged: {
        if (visible) {
            requestActivate()
            Qt.callLater(function() {
                centerOnAssistantWindow()
            })
        } else {
            mainWnd.setAboutWindowOpen(false)
        }
    }

    // 字体变化会影响窗口尺寸，重新居中
    onWidthChanged: {
        Qt.callLater(function() {
            centerOnAssistantWindow()
        })
    }

    onHeightChanged: {
        Qt.callLater(function() {
            centerOnAssistantWindow()
        })
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "#FFFFFF"

        Rectangle {
            id: dragArea
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: 40
            color: "transparent"

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    startDragPos = Qt.point(mouse.x, mouse.y)
                }
                onPositionChanged: {
                    aboutPage.startSystemMove()
                }
            }
        }

        Button {
            id: closeButton
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.top: parent.top
            anchors.topMargin: 4
            width: 32
            height: 32
            icon.name: "window-close-symbolic"
            focusPolicy: Qt.NoFocus

            flat: true
            property string buttonType: "windowTitleBarCloseButton"

            ToolTip {
                id: closeButtonTooltip
                text: qsTr("Close")
                delay: Qt.styleHints.mousePressAndHoldInterval
            }

            onHoveredChanged: {
                if(hovered)
                    closeButtonTooltip.visible = true
                else
                    closeButtonTooltip.visible = false
            }

            onClicked: closeAboutPage()

            Keys.onReturnPressed: {
                if(closeButton.focus === true){
                    closeButton.clicked()
                }
            }

            Accessible.name: "kylin-aiassistant_Button_close"
        }

        Image {
            id: image
            source: "qrc:/icon/icon/kylin-ai-assistant-new.png"
            anchors.top: parent.top
            anchors.topMargin: 60
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Column {
            id: aboutPageText
            anchors.top: image.bottom
            anchors.topMargin: 24
            spacing: 16
            anchors.horizontalCenter: parent.horizontalCenter

            Label {
                Accessible.name: "kylin-aiassistant_Text_appName"
                text: qsTr("Qi Ling")
                font.family: fontFamily
                font.pointSize: fontSize + 2
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#262626"
            }

            Label {
                Accessible.name: "kylin-aiassistant_Text_version"
                text: qsTr("Version:") + " " + mainWnd.getAiAssistantVersion()
                font.family: fontFamily
                font.pointSize: fontSize
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#262626"
                opacity: 0.7
            }

            Item {
                id: supportItem
                anchors.horizontalCenter: parent.horizontalCenter
                width: childrenRect.width
                height: childrenRect.height
                Label {
                    id: emailText
                    Accessible.name: "kylin-aiassistant_Text_email"
                    text: qsTr("Service & Support:") + " <u>support@kylinos.cn</u>"
                    font.family: fontFamily
                    font.pointSize: fontSize
                    color: "#262626"
                    opacity: 0.7
                    textFormat: Text.RichText
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Qt.openUrlExternally("mailto:support@kylinos.cn")
                        }
                    }
                }
            }
        }
    }
}
