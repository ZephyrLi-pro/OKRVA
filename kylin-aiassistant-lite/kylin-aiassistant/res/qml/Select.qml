import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id:selectionRectangle
    Accessible.name: "kylin-aiassistant_Select_selectionRectangle"
    color: "white"
    enabled: true
    width: selectionBarWidth()
    height: 36
    radius: 8
    opacity: opacityValue
    border.width: 1
    border.color: Qt.rgba(0/255, 0/255, 0/255,0.1)
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐

    property bool isMoreButtonPressed: false
    property int activeCommandID: -1
    property string  activeCommand: ""
    property bool isSelectedEmpty: false
    property real opacityValue: 1.0  // 自定义透明度属性
    readonly property int barHorizontalPadding: 4

    signal sigSendButtonText(string currentCommand, int currentCommandID)
    signal sigResizeRect(int width, int height)
    signal sigsettingButtonLeftClicked()
    signal sigDisableMorePage()
    signal sigShowToolTip(bool isShow)
    signal sigHideWindowFinished()
    signal sigGetFocus()
    // 当前指令
    ListModel {
        id: selectCommandListModel
        //总结
        ListElement {
            name:  qsTr("Summarize")
            actorId: 501
            isActive : true
            iconPath: "qrc:/icon/icon/setting/more.svg"
        }
        //解释
        ListElement {
            name:  qsTr("Explain")
            actorId: 502
            isActive : true
            iconPath: "qrc:/icon/icon/setting/more.svg"
        }
        //翻译
        ListElement {
            name:  qsTr("Translate")
            actorId: 503
            isActive : true
            iconPath: "qrc:/icon/icon/setting/more.svg"
        }
        //朗读
        ListElement {
            name:  qsTr("Read")
            actorId: 504
            isActive : true
            iconPath: "qrc:/icon/icon/setting/more.svg"
        }
        //问问
        ListElement {
            name:  qsTr("Ask")
            actorId: 505
            isActive : true
            iconPath: "qrc:/icon/icon/setting/more.svg"
        }
    }

    // 主要布局
    RowLayout{
        anchors.centerIn: parent
        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
        spacing: 4
        opacity: selectionRectangle.opacityValue
        width: menuRect.width + (selectCommandFlow.visible ? spacing + selectCommandFlow.width : 0)
        id: mainLayout
        // 菜单图标
        Rectangle {
            width: 30;
            height: 28;
            color: Qt.rgba(255/255, 255/255, 255/255)
            radius: 4
            id: menuRect
            opacity: selectionRectangle.opacityValue
            Accessible.name: "kylin-aiassistant_Select_menuRect"
            Row {
                anchors.centerIn: parent
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
                Image {
                    id: m_textSendButton
                    Accessible.name: "kylin-aiassistant_Select_textSendButton"
                    width: 24
                    height: 24
                    source: "qrc:/icon/icon/kylin-ai-assistant-new-s.png"
                    fillMode: Image.PreserveAspectFit
                    visible: true

                }
            }
            MouseArea {
                id:rightClickArea
                Accessible.name: "kylin-aiassistant_Select_rightClickArea"
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.RightButton |Qt.LeftButton
                property bool isEntered: false
                onEntered: {
                    parent.color = Qt.rgba(55/255, 144/255, 250/255,0.1);
                    selectionRectangle.sigShowToolTip(true);
                    isEntered = true
                    // 停止动画，恢复显示透明度
                    resetOpacity()
                    // 获取焦点
                    sigGetFocus()
                    // 显示完整版划词条
                    if(selectCommandFlow.visible === false) {
                        selectCommandFlow.visible  = true;
                        selectionRectangle.width = Math.max(selectionBarWidth(), 385 + barHorizontalPadding * 2)
                        selectionRectangle.sigResizeRect(selectionRectangle.width,selectionRectangle.height);

                    }
                }
                onExited: {
                    parent.color = Qt.rgba(255/255, 255/255, 255/255);
                    selectionRectangle.sigShowToolTip(false);
                    isEntered = false
                }
                onPressedChanged: {
                    if(!rightClickArea.pressed) {
                        if(isEntered){
                            // 隐藏更多下拉
                            isMoreButtonPressed = false
                            selectionRectangle.sigResizeRect(selectionRectangle.width,selectionRectangle.height);

                            //显示右键菜单
                            selectionRectangle.sigsettingButtonLeftClicked()
                            parent.color = Qt.rgba(255/255, 255/255, 255/255);
                        }
                    }
                }
            }

        }

        // 指令列表
        Row {
            id:selectCommandFlow
            Accessible.name: "kylin-aiassistant_Select_selectCommandFlow"
            height: 28
            width:selectRepeater.width
            Layout.rightMargin: -6 //此处设置为负值是因为在计算左右间距时会由于spacing和下方的repeater中item的rightmargin重复，补充此值为抵消此间隙过大
            visible: false
            Repeater {
                id:selectRepeater
                Accessible.name: "kylin-aiassistant_Select_selectRepeater"
                visible: true
                model: selectCommandListModel;

                delegate: Rectangle {
                    width:selectRect.width+2
                    height: 28
                    id:selectDelegateRectangle
                    visible: true //只显示前三个
                    enabled: {
                        if(isSelectedEmpty) {
                            return false;
                        }
                        if(!isActive) {
                            return false;
                        }
                        return true;
                    }
                    Accessible.name: "kylin-aiassistant_Select_Rectangle"
                    color: "white"
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter  // 水平和垂直居中对齐
                        id:selectButtonLayout
                        Accessible.name: "kylin-aiassistant_Select_selectButtonLayout"
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        Rectangle{
                            id:selectRect
                            Accessible.name: "kylin-aiassistant_Select_selectRect"
                            //width: selectButtonText.implicitWidth +selectButtonIcon.implicitWidth + textLayout.leftPadding+textLayout.rightPadding;
                            width:textLayout.width
                            height:28;
                            color: "white"
                            radius: 4
                            Row{
                                leftPadding: 4
                                rightPadding: 4
                                anchors.centerIn: parent;
                                id:textLayout
                                Accessible.name: "kylin-aiassistant_Select_textLayout"
                                Item{
                                    id: selectButtonIconItem
                                    width: 28
                                    height: 28
                                    opacity: isSelectedEmpty ? 0.3 : 1.0
                                    Image {
                                        id: selectButtonIcon
                                        Accessible.name: "kylin-aiassistant_Select_commandImage"
                                        source: iconPath
                                        anchors.centerIn: parent
                                        opacity: isActive ? 1 : 0.35
                                    }

                                }

                                Label {
                                    id: selectButtonText;
                                    Accessible.name: "kylin-aiassistant_Select_selectButtonText"
                                    text: getTruncateCommandLabel(name, 8); // 中文不超过 8 个字符，英文不超过 16 个字符
                                    color:{
                                        if(isSelectedEmpty) {
                                            return Qt.rgba(0/255,0/255,0/255,0.35)
                                        }
                                        if(!isActive) {
                                            return Qt.rgba(0/255,0/255,0/255,0.35)
                                        }
                                        return Qt.rgba(0/255, 0/255, 0/255,0.85)
                                    }
                                    width: 28
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            MouseArea {
                                id: selectMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                property bool isEntered: false
                                onEntered: {
                                    parent.color = Qt.rgba(0/255, 0/255, 0/255,0.15)
                                    isEntered = true
                                }
                                onExited: {
                                    parent.color = "white"
                                    isEntered = false
                                }
                                onPressed: {
                                    parent.color = Qt.rgba(0/255, 0/255, 0/255,0.2)
                                }
                                onPressedChanged: {
                                    if(!selectMouseArea.pressed) {
                                        parent.color = "white";
                                        if(isEntered){
                                            selectionRectangle.activeCommand = name
                                            selectionRectangle.activeCommandID = actorId
                                            selectionRectangle.sigSendButtonText(selectionRectangle.activeCommand, selectionRectangle.activeCommandID)
                                        }
                                    }
                                }
                           }

                        }
                    }

                    }
               }
        }
        }
    // 渐变隐藏动画
    PropertyAnimation {
        id: hideAnimation
        target: selectionRectangle
        property: "opacityValue"
        from: 1.0
        to: 0.0
        duration: 2000
        easing.type: Easing.OutInQuad
        onRunningChanged: {
            if (!running && opacityValue === 0.0) {
                // 动画结束后发送信号给 C++ 层，触发窗口隐藏
                sigHideWindowFinished()
            }
        }
    }

    //响应当前qml中sigsendButtonText信号，设置在发送内容后隐藏更多按钮控制面板
    onSigSendButtonText:
    {
        //设置按钮的状态 发送指令后隐藏更多按钮控制面板
        isMoreButtonPressed =false
    }

    function onsigDisableMorePage(){
        isMoreButtonPressed =false
    }

    function onsigDisableSelectStatus(){
        selectionRectangle.isSelectedEmpty = true;
    }

    function onsigEnableSelectStatus(){
        selectionRectangle.isSelectedEmpty = false;
    }

    function onUpdateRectSize() {
        console.log("====onUpdateRectSize====onUpdateRectSize:",menuRect.width)
        selectionRectangle.width = selectionBarWidth()
        selectionRectangle.height = mainLayout.height
        selectionRectangle.sigResizeRect(selectionRectangle.width,selectionRectangle.height);

    }

    function selectionBarWidth() {
        var contentWidth = Math.max(mainLayout.implicitWidth, mainLayout.width)
        return Math.ceil(contentWidth + barHorizontalPadding * 2)
    }

    // 总长度计算
    function calculateTotalWidth() {
        let totalWidth = 0;
        for (let i = 0; i < 3; i++) {
            let item = selectRepeater.itemAt(i);
            if (item) {
                totalWidth += item.width;
            }
        }
        console.log("当前划词指令repeater的宽度为"+totalWidth);
        return totalWidth;
    }

    function onsigUpdateModelAndSelectSize()
    {
        console.log("指令模板模型内容已经更新"+selectionRectangle.width);
        var repeaterTotalWidth = calculateTotalWidth();
        console.log("指令repeater宽度"+selectCommandFlow.width)

    }

    function resetOpacity() {
        console.log("重置透明度数据和透明度变化动画")
        opacityValue = 1.0; // 直接重置自定义透明度属性
        hideAnimation.stop(); // 停止可能正在运行的动画
    }

    function getTruncateCommandLabel(text, maxChars) {
        let totalUnits = 0;
        let truncatedText = "";

        for (let i = 0; i < text.length; i++) {
            let currentChar = text.charAt(i);

            // 判断是否为中文字符
            if (currentChar >= '\u4E00' && currentChar <= '\u9FFF') {
                // 中文字符占用 2 个单位
                totalUnits += 2;
            } else {
                // 英文或其他字符占用 1 个单位
                totalUnits += 1;
            }

            // 如果总单位数超过了允许的最大值，则截断
            if (totalUnits > maxChars * 2) {
                truncatedText += "...";
                break;
            }

            // 追加当前字符
            truncatedText += currentChar;
        }

        return truncatedText;
    }

     // 定义方法：供 C++ 调用启动隐藏动画
    function startHideAnimation() {
        hideAnimation.start()
    }

    // 定义方法：供 C++ 调用启动显示动画
    function startShowAnimation() {
        opacityValue = 0.0
        showAnimation.start()
    }

    // 处理按键按下事件
    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            console.log("Escape键被按下")
            event.accepted = true  // 标记事件已处理
        } else if (event.key === Qt.Key_Space) {
            console.log("空格键被按下")
        }
    }


}

