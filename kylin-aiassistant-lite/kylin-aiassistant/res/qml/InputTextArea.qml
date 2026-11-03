import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."

ScrollView {
    id: inputTextEditScrollView
    Layout.fillWidth: true
    Layout.maximumHeight: v_lineText.height + warningText.height
    property string placeholderText: ""
    property string realText: inputTextEdit.text
    property int maximumTextLength: 1000

    signal enterPressed()
    function clearInput() {
        inputTextEdit.text = ""
    }

    // 隐藏水平滚动条，垂直滚动条按需显示
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AlwaysOff

    // 监听内容高度变化，自动滚动到底部
    onContentHeightChanged: {
        if (contentItem && contentItem.contentHeight > contentItem.height) {
            Qt.callLater(function() {
                contentItem.contentY = contentItem.contentHeight - contentItem.height
            })
        }
    }

    Column{
        id: textColumn
        anchors.fill: parent
        spacing: 0
        TextArea {
            id: inputTextEdit
            placeholderText: placeholderText
            wrapMode: TextArea.WrapAnywhere
            selectByMouse: true
            // 限制文本宽度与 ScrollView 一致（确保换行基于容器宽度）
            width: parent.width
            topPadding: 9
            bottomPadding: 9
            leftPadding: 0
            rightPadding: 10
            background: Rectangle {
                color: "transparent"  // 透明背景
                border.width: 0        // 无边框
            }

            onTextChanged: {
                // 当文本长度超过最大限制时，截断到最大长度
                if (text.length > maximumTextLength) {
                    text = text.substring(0, maximumTextLength)
                    // 光标定位到末尾
                    cursorPosition = maximumTextLength
                }
                // 文本变化后，滚动到底部
                scrollToBottom()
            }

            // 监听文本变化和光标位置变化
            onFontChanged: updateScrollBarPolicy()
            Component.onCompleted:{
                updateScrollBarPolicy()
            }
            onLineCountChanged: {
                updateScrollBarPolicy()
                // 行数变化时也滚动到底部
                scrollToBottom()
            }

            function updateScrollBarPolicy() {
                // 超过5行时显示滚动条
                inputTextEditScrollView.ScrollBar.vertical.policy =
                        inputTextEdit.lineCount > 3 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                return
            }

            // 滚动到底部
            function scrollToBottom() {
                Qt.callLater(function() {
                    // 直接操作 ScrollView 内部的 Flickable
                    var flickable = inputTextEditScrollView.contentItem
                    if (flickable) {
                        var targetY = flickable.contentHeight - flickable.height
                        if (targetY > 0 && Math.abs(flickable.contentY - targetY) > 1) {
                            flickable.contentY = targetY
                        }
                    }
                })
            }


            //永不展示，用于计算3行文字占用的高度
            TextArea{
                id:v_lineText
                text: "a\n a\n a"
                visible: false
                wrapMode: TextArea.WrapAnywhere
                selectByMouse: true
                width: parent.width
                topPadding: 9
                bottomPadding: 9
                leftPadding: 0
                rightPadding: 10
                background: Rectangle {
                    color: "transparent"  // 透明背景
                    border.width: 0        // 无边框
                }
            }

            Keys.onPressed: {

                if (event.key === Qt.Key_Tab) {
                    // 插入一个制表符（Tab）
                    var cursor = inputTextEdit.cursorPosition
                    inputTextEdit.text = inputTextEdit.text.substring(0, cursor) + " " + inputTextEdit.text.substring(cursor)
                    inputTextEdit.cursorPosition = cursor + 1  // 移动光标到插入的 Tab 后
                    event.accepted = true  // 阻止默认的 Tab 键行为
                }

                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                    // Ctrl+Enter/Alt+Enter/Shift+Enter - 换行
                    if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier)) {
                        var cursorPos = inputTextEdit.cursorPosition;
                        inputTextEdit.text = inputTextEdit.text.substring(0, cursorPos) + "\n" + inputTextEdit.text.substring(cursorPos);
                        inputTextEdit.cursorPosition = cursorPos + 1;
                        event.accepted = true;
                        return;
                    }
                    enterPressed()

                    event.accepted = true;
                }
            }
        }

        // 字数上限提示
        Label {
            id: warningText
            width: parent.width
            text: maximumTextLength.toString() + "/" + maximumTextLength.toString()
            color: "red"
            horizontalAlignment: Qt.AlignRight
            visible: (realText.length >= maximumTextLength)
        }
    }
}
