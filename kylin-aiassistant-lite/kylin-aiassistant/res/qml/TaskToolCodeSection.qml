import QtQuick 2.12
import QtQuick.Controls 2.12

Column {
    id: root
    spacing: 4

    property string title: ""
    property string codeText: ""
    property bool expanded: false

    signal toggleRequested()

    function arrowSource(hovered, expanded) {
        if (hovered) {
            return expanded ? "qrc:/icon/icon/arrow-up-hover.svg"
                            : "qrc:/icon/icon/arrow-down-hover.svg"
        }
        return expanded ? "qrc:/icon/icon/arrow-up.svg"
                        : "qrc:/icon/icon/arrow-down.svg"
    }

    Row {
        id: titleRow
        width: parent.width
        height: 20
        leftPadding: 24
        rightPadding: 16
        spacing: 4

        property bool hovered: false

        Label {
            id: titleLabel
            height: titleRow.height
            text: root.title
            color: titleRow.hovered ? "#3489ED" : "#262626"
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            width: Math.min(implicitWidth,
                            titleRow.width - titleRow.leftPadding - titleRow.rightPadding - 20 - titleRow.spacing)

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: titleRow.hovered = true
                onExited: titleRow.hovered = false
                onClicked: root.toggleRequested()
            }
        }

        Rectangle {
            width: 20
            height: 20
            color: "transparent"

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: root.arrowSource(titleRow.hovered, root.expanded)
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: titleRow.hovered = true
                onExited: titleRow.hovered = false
                onClicked: root.toggleRequested()
            }
        }
    }

    TextArea {
        id: codeTextArea
        x: 24
        width: parent.width - 40
        visible: root.expanded
        topPadding: 8
        leftPadding: 10
        rightPadding: 10
        bottomPadding: 8
        text: root.codeText
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        color: Qt.rgba(0, 0, 0, 0.75)
        font.family: "monospace"
        readOnly: true
        selectByMouse: true
        cursorVisible: false
        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.04)
            radius: 6
        }
    }
}
