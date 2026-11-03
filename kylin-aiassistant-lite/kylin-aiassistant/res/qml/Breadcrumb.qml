import QtQuick 2.12
import QtQuick.Controls 2.12

Row {
    id: breadcrumb
    spacing: 5

    property var paths: [""]
    signal pathClicked(int index)
    
    Repeater {
        model: breadcrumb.paths
        delegate: Row {
            spacing: 5
            Label {
                text: modelData
                MouseArea {
                    anchors.fill: parent
                    onClicked: () => breadcrumb.pathClicked(index)
                }
            }

            Label {
                visible: index < breadcrumb.paths.length - 1
                text: ">"
            }
        }
    }

    function addPath(name) {
        paths.push(name)
        paths = paths.slice() // 触发更新
    }

    function goBack() {
        if (paths.length > 1) {
            paths.pop()
            paths = paths.slice()
            pathClicked(paths.length - 1)
        }
    }
}
