import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."

Rectangle {
    id: introductionViewWindow
    visible: false
    color: "transparent"

    signal closeIntroductionView()

    function closeView() {
        introductionViewWindow.visible = false
        closeIntroductionView()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        IntroductionContent {
            id: content
            Layout.fillWidth: true
            Layout.fillHeight: true
            onOpenModelsPage: {
                introductionViewWindow.closeView()
                if (root.mychatRef) {
                    root.mychatRef.openModelsPage()
                }
            }
            onCloseWindow: {
                introductionViewWindow.closeView()
            }
            onQuitApplication: {
                Qt.quit()
            }
        }
    }

    Component.onCompleted: {
        // 窗口初始化
        windowCtrl.clearWindowStyle(introductionViewWindow)
    }
}
