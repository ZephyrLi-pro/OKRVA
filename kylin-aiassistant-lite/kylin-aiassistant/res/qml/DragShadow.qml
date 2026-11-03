import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import ButtonState 1.0
import "."

// 遮罩层
Rectangle {
    id: dragShadow
    Accessible.name: "kylin-aiassistant_main_dragShadow"
    anchors.fill: parent
    color: "white"

    visible: false // 初始时遮罩层不可见

    z: 100 // 确保遮罩层在其他组件之上

    property bool effectiveImageSupport: ButtonState.isTaskMode || isSupportImageProcessing
    property bool effectiveDocumentSupport: isSupportDocumentQA

    Rectangle {
        height: dragShadowColumn.height
        width: parent.width
        anchors.centerIn: parent

        Column {
            id: dragShadowColumn
            width: parent.width
            spacing: 4

            Item {
                height: 32
                width: parent.width
                Image {
                    height: 32
                    width: 32
                    anchors.centerIn: parent
                    source: "qrc:/icon/icon/drag-drop-file.svg"
                    horizontalAlignment: Qt.AlignHCenter
                }
            }

            // 获取系统字体大小和格式
            Label {
                id: standardLabel
                visible: false
            }

            Label {
                id: alertText
                Accessible.name: "kylin-aiassistant_main_alertText"
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
                text: qsTr("Drop files here")
                font.pointSize: standardLabel.font.pointSize
                font.family: standardLabel.font.family
                color: Qt.rgba(0, 0, 0, 0.85)
            }

            Label {
                horizontalAlignment: Qt.AlignHCenter
                width: parent.width
                text: qsTr("Number of files: no more than 1")
                font.pointSize: standardLabel.font.pointSize - 2
                font.family: standardLabel.font.family
                color: Qt.rgba(0, 0, 0, 0.35)
            }
            Label {
                horizontalAlignment: Qt.AlignHCenter
                width: parent.width
                font.pointSize: standardLabel.font.pointSize - 2
                font.family: standardLabel.font.family
                color: Qt.rgba(0, 0, 0, 0.35)
                text: {
                    if (dragShadow.effectiveDocumentSupport && dragShadow.effectiveImageSupport){
                        return qsTr("File size: ") + qsTr("Document ≤ 50MB; ") + qsTr("Image ≤ 20MB")
                    } else if (!dragShadow.effectiveDocumentSupport && dragShadow.effectiveImageSupport) {
                        return qsTr("File size: ") + qsTr("Image ≤ 20MB")
                    } else if (dragShadow.effectiveDocumentSupport && !dragShadow.effectiveImageSupport) {
                        return qsTr("File size: ") + qsTr("Document ≤ 50MB; ")
                    }
                    return ""
                }
            }
            Label {
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                leftPadding: 20
                rightPadding: 20
                font.pointSize: standardLabel.font.pointSize - 2
                font.family: standardLabel.font.family
                color: Qt.rgba(0, 0, 0, 0.35)
                text: {
                    var pdfFormat = globals.isPdfSupported ? "pdf、" : ""
                    if (dragShadow.effectiveDocumentSupport && dragShadow.effectiveImageSupport){
                        return qsTr("Supported formats: ") + "txt、doc、docx、" + pdfFormat + "wps、pptx、ppt、ofd、jpg、jpeg、jpe、png、webp、tiff、bmp、ppm、pgm、pbm"
                    } else if (!dragShadow.effectiveDocumentSupport && dragShadow.effectiveImageSupport) {
                        return qsTr("Supported formats: ") + "jpg、jpeg、jpe、png、webp、tiff、bmp、ppm、pgm、pbm"
                    } else if (dragShadow.effectiveDocumentSupport && !dragShadow.effectiveImageSupport) {
                        return qsTr("Supported formats: ") + "txt、doc、docx、" + pdfFormat + "wps、pptx、ppt、ofd"
                    }
                    return ""
                }
            }

            // 空白占位块，确保图标整个窗口居中
            Rectangle {
                height: 50
                width: parent.width
                color: "white"
            }
        }
    }
}
