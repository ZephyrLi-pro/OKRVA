import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Item {
    id: knowledgeListViewFolderItem
    height: knowledgeTitleMetrics.implicitHeight > 32 ? knowledgeTitleMetrics.implicitHeight + 10 : 32

    property int padingSize: 0
    property int itemHeight: knowledgeTitleMetrics.implicitHeight > 32 ? knowledgeTitleMetrics.implicitHeight + 10 : 32

    property string fileName:  ""
    property string filePath:  ""
    property var fileIcon: null
    property bool isEdit: false


    signal requestToLoadDir()

    Rectangle{
        id: recorderRet
        width: parent.width
        height: itemHeight
        radius: 4
        color: "transparent"
        anchors.right:  parent.right
        anchors.rightMargin: padingSize
        anchors.left: parent.left
        anchors.leftMargin: padingSize

        Row{
            id: recorderRowItem
            width: parent.width
            height: itemHeight
            spacing: 0
            Rectangle {
                id: recorder
                width: parent.width - recorderRowItem.spacing
                height: itemHeight
                radius: 4
                color: "transparent"
                MouseArea {
                    id: recorderMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !isEdit
                    onEntered: {
                        recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                    }
                    onExited: {
                        recorder.color = "transparent";
                    }
                    onPressed: {
                    }
                    onReleased: {
                    }
                    onClicked: {
                    }
                    onDoubleClicked: () => requestToLoadDir()
                }

                Row {
                    id: recorderRow
                    width: parent.width - recorderRow.leftPadding
                    height: itemHeight
                    spacing: 0
                    leftPadding: 0
                    anchors.verticalCenter: parent.verticalCenter

                    // 文件类型图标
                    Rectangle {
                        id: fileIconRectangle
                        width: 28
                        height: itemHeight
                        color: "transparent"
                        visible: true
                        Image{
                            id: fileIconImage
                            anchors.centerIn: parent
                            source: "qrc:/icon/icon/insync-folder.png"
                            width: 16
                            height: 16
                        }
                    }

                    Label {
                        id: dialogueTitle
                        height: dialogueTitle.implicitHeight
                        width: parent.width - fileIconRectangle.width
                        text: fileName
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: Text.AlignVCenter
                        // 1. 核心：判断是否显示省略号
                        property bool isElided: dialogueTitleMertics.width > dialogueTitle.width

                        // 2. 悬浮触发Tooltip的关键：鼠标是否悬停
                        property bool isHovered: false

                        // 悬浮提示框
                        ToolTip {
                            id: dialogueTitleTooltip
                            visible: dialogueTitle.isElided && dialogueTitle.isHovered
                            delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                            text: fileName
                            property var maxWidth: 400
                            property var wrapMode: Text.WrapAnywhere
                        }

                        MouseArea {
                            id: dialogueTitleMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: !isEdit
                            onEntered: {
                                recorder.color = Qt.rgba(220/255, 220/255, 220/255, 1)
                                dialogueTitle.isHovered = true
                            }
                            onExited: {
                                recorder.color = "transparent";
                                dialogueTitle.isHovered = false
                            }
                            onPressed: {
                                dialogueTitle.isHovered = false
                            }
                            onReleased: {
                                dialogueTitle.isHovered = false
                            }
                            onClicked: {
                            }
                            onDoubleClicked: () => requestToLoadDir()
                        }
                    }

                    Label {
                        id: dialogueTitleMertics
                        height: dialogueTitle.implicitHeight
                        text: fileName
                        visible: false
                    }

                    Label {
                        id: knowledgeTitleMetrics
                        visible: false
                        text: "用于计算汉字高度"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
