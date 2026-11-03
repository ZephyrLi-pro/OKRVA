import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

Rectangle {
    id: searchTextAreaRet
    height: searchTextAreaRetHeight
    color: "#FFFFFF"
    radius: 6

    property int searchTextAreaRetHeight: 0

    signal searchTextAreaTextChanged(string changeText)

    // 初始化搜索框
    function initSearchTextAreaRet(){
        mouseArea.visible = true
        searchLineHidden.visible = true
        searchLineRow.visible = false
        searchLineRet.border.color = "#1a000000"
        searchLineRet.color = "#1a000000"
        searchLineEdit.text = ""
    }

    Column {
        anchors.fill: parent
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 10

        Rectangle{
            id: searchLineRet
            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRet"
            width: parent.width
            height: 32
            radius: 4
            color: "#1a000000"
            border.color: "#1a000000"
            border.width: 2
            Row{
                id: searchLineRow
                spacing: 0
                width: parent.width - searchLineRet.border.width * 2
                height: parent.height - searchLineRet.border.width * 2
                anchors.centerIn: parent
                visible: false
                Rectangle{
                    id: searchLineRowRet
                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRowRet"
                    width: 5
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    color: "transparent"
                }
                Image{
                    id: searchLineImg
                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineImg"
                    width: 16
                    height: 16
                    source: "qrc:/icon/icon/sidebar/search.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                TextField {
                    id: searchLineEdit
                    Accessible.name: "kylin-aiassistant_HistoryPage_searchLineEdit"
                    placeholderText: qsTr("Search")
                    width: parent.width - searchLineRowRet.width - searchLineImg.width - searchLineRow.spacing - searchLineRet.border.width*2 - (closeImgRet.visible ? closeImgRet.width : 0)
                    selectByMouse: true
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                    anchors.verticalCenter: parent.verticalCenter
                    background: Rectangle {
                        id: searchLineEditRet
                        Accessible.name: "kylin-aiassistant_HistoryPage_searchLineEditRet"
                        radius: 4
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    onFocusChanged: {
                    }
                    onTextChanged: {
                        searchTextAreaTextChanged(searchLineEdit.text)
                    }
                }
                Rectangle{
                    id: closeImgRet
                    Accessible.name: "kylin-aiassistant_HistoryPage_closeImgRet"
                    width: 20
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    visible: searchLineEdit.text !== ""
                    color: "transparent"
                    Image{
                        id: closeImg
                        width: 16
                        height: 16
                        source: "qrc:/icon/icon/close-black.svg"
                        anchors.centerIn: parent
                    }
                    MouseArea{
                        id: closeImgRetMouseArea
                        Accessible.name: "kylin-aiassistant_HistoryPage_closeImgRetMouseArea"
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onEntered: {

                        }
                        onExited: {

                        }
                        onPressed: {

                        }
                        onReleased: {

                        }
                        onClicked: {
                            searchLineEdit.text = ""
                        }
                    }
                }
            }
            RowLayout{
                id: searchLineHidden
                spacing: 0
                width: parent.width
                height: parent.height - 2
                visible: true
                Rectangle{
                    width: searchLineImgHidden.width +　searchLineTextHidden.width + searchLineRowHidden.spacing
                    height: parent.height
                    color: "transparent"
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    Row{
                        id: searchLineRowHidden
                        Accessible.name: "kylin-aiassistant_HistoryPage_searchLineRowHidden"
                        width: parent.width
                        height: parent.height
                        anchors.centerIn: parent
                        spacing: 8
                        Image{
                            id: searchLineImgHidden
                            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineImgHidden"
                            width: 16
                            height: 16
                            source: "qrc:/icon/icon/sidebar/search.svg"
                            sourceSize: Qt.size(16, 16)
                            smooth: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            id: searchLineTextHidden
                            Accessible.name: "kylin-aiassistant_HistoryPage_searchLineTextHidden"
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Search")
                            color: Qt.rgba(0/255, 0/255, 0/255, 0.55)
                        }
                    }
                }
            }
            MouseArea {
                id: mouseArea
                Accessible.name: "kylin-aiassistant_HistoryPage_mouseArea"
                anchors.fill: parent
                propagateComposedEvents: true
                onClicked: {
                    // 隐藏上层遮罩层和MouseArea，显示底层真实输入框
                    searchLineEdit.cursorVisible = true
                    searchLineEdit.cursorPosition = 0
                    searchLineEdit.focus = true
                    searchLineEdit.forceActiveFocus()
                    searchLineHidden.visible = false
                    mouseArea.visible = false
                    searchLineRet.border.color = "#3790FA"
                    searchLineRet.color = "#ffffff"
                    searchLineRow.visible = true
                }
            }
        }
    }
}
