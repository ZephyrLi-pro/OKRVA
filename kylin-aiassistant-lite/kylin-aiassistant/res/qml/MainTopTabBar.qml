import QtQuick 2.12
import QtQuick.Controls 2.12

/**
 * 顶栏 TAB：尺寸与间距按设计稿，三态与 SmallSideBar 图标按钮一致。
 * 左右各 56px 边距，整组 TAB 在可用区域内水平居中。
 * 侧边栏模式（sidebarNarrowLayout）下若总宽度超出可视区域，可左右滑动查看。
 */
Rectangle {
    id: tabBarRoot
    color: "transparent"
    implicitHeight: Math.max(36, tabRow.implicitHeight)

    property int currentTabIndex: 0
    property string fontFamily: ""
    /** 为 true 时（如 winMode===2），仅在 TAB 总宽超出可视区域时可横向滑动，避免窗口模式下误拖。 */
    property bool sidebarNarrowLayout: false

    signal tabClicked(int index, string tabKey)

    readonly property var tabKeys: ["chat", "channels", "capabilities", "tasks", "models"]
    readonly property var tabTexts: [
        qsTr("Chat"),
        qsTr("Channels"),
        qsTr("Capabilities"),
        qsTr("Tasks"),
        qsTr("Models")
    ]
    Item {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 56
        clip: true

        Flickable {
            id: tabFlick
            anchors.fill: parent
            flickableDirection: Flickable.HorizontalFlick
            boundsMovement: Flickable.StopAtBounds
            interactive: tabBarRoot.sidebarNarrowLayout && tabCenterWrap.width > width
            contentWidth: tabCenterWrap.width
            contentHeight: tabRow.height

            Item {
                id: tabCenterWrap
                height: tabRow.height
                width: Math.max(tabFlick.width, tabRow.width)

                Row {
                    id: tabRow
                    x: (tabCenterWrap.width - tabRow.width) / 2
                    spacing: 8

                    Repeater {
                        model: tabTexts.length

                        delegate: Rectangle {
                            id: tabCell
                            width: Math.max(64, tabLabel.implicitWidth)
                            height: Math.max(36, tabLabel.implicitHeight)
                            radius: 6
                            property bool isSel: tabBarRoot.currentTabIndex === index

                            color: {
                                if (isSel)
                                    return "#3790FA"
                                if (tabMouse.pressed)
                                    return "#3790FA"
                                if (tabMouse.containsMouse)
                                    return Qt.rgba(0, 0, 0, 0.05)
                                return "transparent"
                            }

                            Label {
                                id: tabLabel
                                anchors.centerIn: parent
                                text: tabBarRoot.tabTexts[index]
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                topPadding: 2
                                bottomPadding: 2
                                leftPadding: 16
                                rightPadding: 16
                                font.pointSize: globals.fontsize
                                font.family: tabBarRoot.fontFamily !== "" ? tabBarRoot.fontFamily : root.fontFamily
                                color: (tabCell.isSel || tabMouse.pressed) ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.85)
                                elide: Text.ElideNone
                                Accessible.name: text
                            }

                            MouseArea {
                                id: tabMouse
                                z: 1
                                anchors.fill: parent
                                hoverEnabled: true
                            onClicked: {
                                tabBarRoot.tabClicked(index, tabBarRoot.tabKeys[index])
                            }
                            }
                        }
                    }
                }
            }
        }
    }
}
