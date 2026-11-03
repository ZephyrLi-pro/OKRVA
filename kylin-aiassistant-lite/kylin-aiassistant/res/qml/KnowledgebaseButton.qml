import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import ButtonState 1.0
import QtQuick.Window 2.12
import "."

// 知识库按钮
Rectangle {
    id: knowledgebaseButtonRectangle
    Accessible.name: "kylin-aiassistant_Chat_knowledgebaseButton"
    width: 32
    height: 32
    radius: 6
    color: "transparent"
    opacity: btnEnabled ? 1.0 : 0.4
    visible: !ButtonState.isTaskMode

    property bool isActive: false
    // 当选中任何指令时禁用知识库按钮
    property bool btnEnabled: globals.isKnowledgeBaseSupported && !hasActiveCommand

    property bool isWindowActive: Window.window ? Window.window.active : true     // 窗口是否失焦
    // 窗口失焦后模型切换列表关闭
    onIsWindowActiveChanged: {
        if (!isWindowActive && selectKBFilePopup.visible) {
            console.log("selectKBFilePopup closed")
            selectKBFilePopup.close()
        }
    }


    signal showKnowledgebaseButtonRectangleTransparentRet(var flag)

    ToolTip {
        id: knowledgeBaseButtonTextToolTip
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: qsTr("Knowledge base")
        property var maxWidth: 400
        property var wrapMode: Text.WrapAnywhere
        // 与侧栏知识库入口一致：置灰时仍可悬浮显示说明（不依赖 enabled）
        visible: knowledgebaseButtonMouseArea.containsMouse && !knowledgebaseButtonMouseArea.isChildVisible
    }

    onIsActiveChanged: {
        if (!btnEnabled){
            return
        }

        if (isActive === true){
            knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
            knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active.svg"
            // 置灰联网搜索按钮
            ButtonState.storageWebSearchEnabled = ButtonState.webSearchEnabled
            ButtonState.webSearchEnabled = false
        } else {
            knowledgebaseButtonRectangle.color = "transparent"
            knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase.svg"
            // 恢复联网搜索按钮
            ButtonState.webSearchEnabled = ButtonState.storageWebSearchEnabled
            ButtonState.storageWebSearchEnabled = null
        }
    }



    RowLayout {
        anchors.fill: parent
        Image {
            id: knowledgebaseImage
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            source: "qrc:/icon/icon/icon_knowledgebase.svg"
        }
    }

    MouseArea {
        id: knowledgebaseButtonMouseArea
        Accessible.name: "kylin-aiassistant_Chat_knowledgebaseButtonMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        property bool isChildVisible:selectKBFilePopup.visible

        onIsChildVisibleChanged: {
            if (isChildVisible){
                if (knowledgebaseButtonRectangle.isActive){
                    knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
                    knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active_press.svg"
                } else {
                    knowledgebaseButtonRectangle.color = "#33000000"
                }
            } else {
                if (knowledgebaseButtonRectangle.isActive) {
                    knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                    knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active.svg"
                } else {
                    knowledgebaseButtonRectangle.color = "transparent"
                }
            }
        }

        onEntered: {
            if (!knowledgebaseButtonRectangle.btnEnabled || isChildVisible){
                return
            }

            if (knowledgebaseButtonRectangle.isActive) {
                knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.15)
                knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active_hover.svg"
            } else {
                knowledgebaseButtonRectangle.color = "#26000000"
            }
        }
        onExited: {
            if (!knowledgebaseButtonRectangle.btnEnabled || isChildVisible){
                return
            }

            if (knowledgebaseButtonRectangle.isActive) {
                knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active.svg"
            } else {
                knowledgebaseButtonRectangle.color = "transparent"
            }
        }
        onPressed: {
            if (!knowledgebaseButtonRectangle.btnEnabled || isChildVisible){
                return
            }

            if (knowledgebaseButtonRectangle.isActive) {
                knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.2)
                knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active_press.svg"
            } else {
                knowledgebaseButtonRectangle.color = "#33000000"
            }
        }
        onReleased: {
            if (!knowledgebaseButtonRectangle.btnEnabled || isChildVisible){
                return
            }

            if (knowledgebaseButtonRectangle.isActive) {
                knowledgebaseButtonRectangle.color = Qt.rgba(55/255, 144/255, 250/255, 0.1)
                knowledgebaseImage.source = "qrc:/icon/icon/icon_knowledgebase_active.svg"
            } else {
                knowledgebaseButtonRectangle.color = "transparent"
            }
        }
        onClicked: {
            if (!knowledgebaseButtonRectangle.btnEnabled){
                return
            }

            // 弹出文件选择框
            // 设置默认选择文件
            var paths = [];
            if (uploadedItemModel.chatType === "knowledgeBaseFile") {
                selectKBFilePopup.isSelectedAllFiles = false
                for (var i = 0; i < uploadedItemModel.count; i++) {
                    var item = uploadedItemModel.get(i);
                    if (item && item.path) {
                        paths.push(item.path);
                    }
                }
            } else if (uploadedItemModel.chatType === "knowledgeBase"){
                selectKBFilePopup.isSelectedAllFiles = true
            } else {
                selectKBFilePopup.isSelectedAllFiles = false
            }
            selectKBFilePopup.selectedFilesPath = paths
            showKnowledgebaseButtonRectangleTransparentRet(true)
            selectKBFilePopup.visible = true
        }
    }
}
