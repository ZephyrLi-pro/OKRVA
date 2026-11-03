import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Layouts 1.12
import TextEditMenu 1.0
import "."


Rectangle{
    id: titleOperatorAreaRet
    height: 36
    visible: filteredListModel.count !== 0

    property int operateChooseIndex: 1
    property int totelOperateCollectedCount: 0 // 全部收藏数量
    property int maxOperateCollectedCount: 0  // 最大收藏数量
    property int editOperateCollectedCount: 0 // 要操作收藏的数量
    property string tipTextStr: ""
    property int checkedCollectEditOperatorCount: 0
    property int editOperatorCount: 0

    signal batchCancelCollect(var selectedOptions, int operateType)

    onEditOperatorCountChanged: {
        if(checkedCollectEditOperatorCount === editOperatorCount){
            collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
        }else{
            collectImage.source = "qrc:/icon/icon/collect-black.svg"
        }
    }

    onCheckedCollectEditOperatorCountChanged: {
        if(checkedCollectEditOperatorCount === editOperatorCount){
            collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
        }else{
            collectImage.source = "qrc:/icon/icon/collect-black.svg"
        }
    }

    function changeSelectAllCheckBox(flag){
        selectAllCheckBox.checked = flag
    }

    function changeSelectAllCheckBoxImageSoucre(source){
        selectAllCheckBoxImage.source = source
    }

    Rectangle{
        id: lineRet
        color: "#1a000000"
        width: titleOperatorAreaRet.width - 48
        height: 1
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 24
        z: 5
    }

    // 历史对话标题操作选择，取消，删除区域
    Row{
        id: titleOperatorRow
        anchors.fill: parent
        leftPadding: 24
        rightPadding: 24
        //bottomPadding: 10
        spacing: 20

        // 历史对话标题区域
        Rectangle {
            id: titleOperatorRet
            color: "#FFFFFF"
            width: 16 + selectAllCheckBoxLabel.implicitWidth
            height: 36
            visible: filteredListModel.count !== 0
            Layout.alignment: Qt.AlignLeft
            Layout.leftMargin: 36
            Row {
                id: selectAllRow
                spacing: 6
                leftPadding: 8
                height: 36
                CheckBox {
                    id: selectAllCheckBox
                    checked: historyShowPageRet.selectAllButtonChecked
                    height: 36
                    Label {
                        id:selectAllCheckBoxLabel
                        anchors.left: selectAllCheckBox.indicator.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: selectAllCheckBox.verticalCenter
                        width: selectAllCheckBoxLabel.implicitWidth
                        topPadding: -2
                        text: qsTr("History")
                        font.bold: true
                        font.pointSize: globals.fontsize
                        font.family: root.fontFamily
                        color: "black"// 设置文本颜色
                    }
                    onCheckedChanged: historyShowPageRet.selectAllButtonChecked = checked
                    indicator: Item {
                        id: selectAllCheckBoxItem
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        Image {
                            id: selectAllCheckBoxImage
                            source: historyShowPageRet.selectAllButtonChecked ?
                                        "qrc:/icon/icon/sidebar/CheckBox-default.svg" :
                                        "qrc:/icon/icon/sidebar/noCheck-default.svg"
                        }
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                // 按钮三态
                onEntered: {
                    if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check.svg")){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check-hover.svg"
                        return
                    }
                    if(selectAllCheckBox.checked){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-hover.svg";
                    }else{
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-hover.svg";
                    }

                }
                onExited: {
                    if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-hover.svg")){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check.svg"
                        return
                    }
                    if(selectAllCheckBox.checked){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                    }else{
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                    }
                }
                onPressed: {
                    if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-hover.svg")){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/part-check-click.svg"
                        return
                    }
                    if(selectAllCheckBox.checked){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg";
                    }else{
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-click.svg";
                    }
                }
                onReleased: {
                    if(String(selectAllCheckBoxImage.source) === String("qrc:/icon/icon/part-check-click.svg")){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-click.svg"
                        return
                    }
                    if(selectAllCheckBox.checked){
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/noCheck-default.svg";
                    }else{
                        selectAllCheckBoxImage.source = "qrc:/icon/icon/sidebar/CheckBox-default.svg";
                    }
                }
                onClicked: {
                    //console.log("selectAllButton click")
                    // 用于判断历史记录复选框是否是全选状态 true:没有全部选中  false:全部选中
                    var checkedFlag = false
                    for (var i = 0; i < filteredListModel.count; ++i) {
                        var itemModel = filteredListModel.get(i);
                        // 跳过日期分隔项
                        if(itemModel.dateDelegateText !== ""){
                            continue
                        }
                        if(!itemModel.checked){
                            checkedFlag = true
                            break
                        }
                    }
                    // 若没有全部选中则全选;若已经全选,点击按钮则全不选
                    // 使用 setProperty 方法更新 model，确保所有项目（包括未渲染的）都被更新
                    for (var j = 0; j < filteredListModel.count; ++j) {
                        var item = filteredListModel.get(j);
                        // 跳过日期分隔项
                        if(item.dateDelegateText !== ""){
                            continue
                        }
                        // 使用 setProperty 方法更新，而不是直接修改属性
                        // 这样可以确保 ListView 的所有项目（包括虚拟化的未渲染项目）都被正确更新
                        filteredListModel.setProperty(j, "checked", checkedFlag)
                    }
                    // 修改全选按钮勾选状态
                    if(checkedFlag){
                        //selectAllButtonChecked.set(0, { "boolValue": true })
                        historyShowPageRet.selectAllButtonChecked = true
                        selectAllCheckBox.checked = true
                    }else{
                        //selectAllButtonChecked.set(0, { "boolValue": false })
                        historyShowPageRet.selectAllButtonChecked = false
                        selectAllCheckBox.checked = false
                    }
                    
                    // 手动重新统计所有选中项目的数量（因为虚拟化的 delegate 不会触发 onCheckedChanged）
                    // 调用 HistoryShowPage 的统一统计函数
                    historyShowPageRet.updateEditCount()
                }
            }
        }

        Rectangle{
            id: sumLabelRet
            height: 36
            width: sumLabel.implicitWidth
            visible: isEditFlag
            Label{
                id: sumLabel
                text: qsTr("(Choose ") + (editOperatorCount/* - dateDelegateTextCount*/) + qsTr(" item)")
                anchors.verticalCenter: parent.verticalCenter
                width: sumLabel.implicitWidth
                color: "#000000"
            }
        }

        // 弹簧
        Rectangle{
            id: elasticityOperatorRet
            width: titleOperatorRow.width - titleOperatorRet.width - cancelLabelRet.width - deleteLabelRet.width -
                   titleOperatorRow.spacing * 5 - sumLabelRet.width - collectLabelRet.width -
                   titleOperatorRow.leftPadding - titleOperatorRow.rightPadding - 10 // -10作为右边距
            height: 36
            color: "transparent"
        }

        Rectangle{
            id: cancelLabelRet
            height: 36
            width: cancelRow.width
            visible: isEditFlag
            onVisibleChanged: {
                if (visible) {
                    cancelImage.source = "qrc:/icon/icon/close-black.svg";
                    cancelLabel.color = "#000000";
                    cancelLabelRetMouseArea.hoverEnabled = true
                }
            }
            // 悬浮提示框
            ToolTip {
                id: cancelLabelRetTooltip
                visible: cancelLabelRetMouseArea.containsMouse && root.winMode === 2
                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                text: qsTr("Cancel")
                property var maxWidth: 400
                property var wrapMode: Text.WrapAnywhere
            }
            Row{
                id: cancelRow
                width: cancelImage.width + (cancelLabel.visible ? (cancelLabel.implicitWidth  + cancelRow.spacing) : 0)
                height: parent.height
                spacing: 6
                Image {
                    id: cancelImage
                    source: "qrc:/icon/icon/close-black.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: cancelLabel
                    text: qsTr("Cancel")
                    anchors.verticalCenter: parent.verticalCenter
                    width: cancelLabel.implicitWidth
                    color: "#000000"
                    visible: root.winMode === 2 ? false : true
                }
            }
            MouseArea{
                id: cancelLabelRetMouseArea
                anchors.fill: parent
                //hoverEnabled: true
                onEntered: {
                    cancelImage.source = "qrc:/icon/icon/close-blue-hover.svg"
                    cancelLabel.color = "#3489EE";
                }
                onExited: {
                    cancelImage.source = "qrc:/icon/icon/close-black.svg"
                    cancelLabel.color = "#000000";
                }
                onPressed: {
                    cancelImage.source = "qrc:/icon/icon/close-blue-click.svg"
                    cancelLabel.color = "#2C73C8";
                }
                onReleased: {
                    cancelImage.source = "qrc:/icon/icon/close-black.svg"
                    cancelLabel.color = "#000000";
                }
                onClicked: {
                    cancelAllChoose()
                }
            }

            Rectangle{
                width: 1
                height: 16
                color: Qt.rgba(216/255, 216/255, 216/255, 1)
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.right
                anchors.leftMargin: 10
            }

        }

        Rectangle{
            id: collectLabelRet
            height: 36
            width: collectRow.width
            visible: isEditFlag
            opacity: (operateChooseIndex === 1 ? (totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount ? 0.5 : 1) : 1)
            onVisibleChanged: {
                if (visible) {
                    if(operateChooseIndex === 1){
                        collectImage.source = "qrc:/icon/icon/collect-black.svg";
                    }else{
                        collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                    }
                    collectLabel.color = "#000000";
                    collectLabelRetMouseArea.hoverEnabled = true
                }
            }
            // 悬浮提示框
            ToolTip {
                id: collectLabelRetTooltip
                visible: false//collectLabelRetMouseArea.containsMouse && (root.winMode === 2 || totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount)
                delay: Qt.styleHints.mousePressAndHoldInterval
                text: qsTr("Collect")
                property var maxWidth: 400
                property var wrapMode: Text.WordWrap
            }
            ToolTip {
                id: unsaveTooltip
                visible: false//collectLabelRetMouseArea.containsMouse && (root.winMode === 2 || totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount)
                delay: Qt.styleHints.mousePressAndHoldInterval
                text: qsTr("Unsave")
                property var maxWidth: 400
                property var wrapMode: Text.WordWrap
            }
            ToolTip {
                id: noCollectedTooltip
                visible: false//collectLabelRetMouseArea.containsMouse && (root.winMode === 2 || totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount)
                delay: Qt.styleHints.mousePressAndHoldInterval
                text: qsTr("The number of collections cannot exceed 50")
                property var maxWidth: 400
                property var wrapMode: Text.WordWrap
            }
            Row{
                id: collectRow
                width: collectImage.width + (collectLabel.visible ? (collectLabel.implicitWidth + collectRow.spacing) : 0 )
                height: parent.height
                spacing: 6
                Image {
                    id: collectImage
                    source: operateChooseIndex === 1 ? editOperatorCount === checkedCollectEditOperatorCount ? "qrc:/icon/icon/collected-button-click.svg" : "qrc:/icon/icon/collect-black.svg" : "qrc:/icon/icon/collected-button-click.svg"
                    anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                }
                Label{
                    id: collectLabel
                    text: operateChooseIndex === 1 ? editOperatorCount === checkedCollectEditOperatorCount ? qsTr("Unsave") : qsTr("Collect") : qsTr("Unsave")
                    anchors.verticalCenter: parent.verticalCenter
                    width: collectLabel.implicitWidth
                    color: "#000000"
                    visible: root.winMode === 2 ? false : true
                }
            }
            MouseArea{
                id: collectLabelRetMouseArea
                anchors.fill: parent
                //hoverEnabled: true
                onEntered: {
                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount || root.winMode === 2){
                        if(operateChooseIndex !== 1){
                            //tipTextStr = qsTr("Unsave")
                            unsaveTooltip.visible = true
                        }else{
                            if(editOperatorCount === checkedCollectEditOperatorCount){
                                unsaveTooltip.visible = true
                            }else{
                                if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount){
                                    noCollectedTooltip.visible = true
                                    //tipTextStr = qsTr("The number of collections cannot exceed 50")
                                }else{
                                    //tipTextStr = qsTr("Collect")
                                    collectLabelRetTooltip.visible = true
                                }
                            }
                        }
                        //collectLabelRetTooltip.visible = true
                    }

                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount && operateChooseIndex === 1){
                        return
                    }
                    if(operateChooseIndex === 1){
                        if(editOperatorCount === checkedCollectEditOperatorCount){
                            collectImage.source = "qrc:/icon/icon/collected-button-hover.svg"
                        }else{
                            collectImage.source = "qrc:/icon/icon/collect-hover-blue.svg"
                        }
                    }else{
                        collectImage.source = "qrc:/icon/icon/collected-button-hover.svg"
                    }
                    collectLabel.color = "#3489EE"
                }
                onExited: {
                    collectLabelRetTooltip.visible = false
                    noCollectedTooltip.visible = false
                    unsaveTooltip.visible = false
                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount && operateChooseIndex === 1){
                        return
                    }
                    if(operateChooseIndex === 1){
                        if(editOperatorCount === checkedCollectEditOperatorCount){
                            collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                        }else{
                            collectImage.source = "qrc:/icon/icon/collect-black.svg"
                        }
                    }else{
                        collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                    }
                    collectLabel.color = "#000000"
                }
                onPressed: {
                    collectLabelRetTooltip.visible = false
                    noCollectedTooltip.visible = false
                    unsaveTooltip.visible = false
                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount && operateChooseIndex === 1){
                        return
                    }
                    if(operateChooseIndex === 1){
                        if(editOperatorCount === checkedCollectEditOperatorCount){
                            collectImage.source = "qrc:/icon/icon/collected-button.svg"
                        }else{
                            collectImage.source = "qrc:/icon/icon/collect-click-blue.svg"
                        }
                    }else{
                        collectImage.source = "qrc:/icon/icon/collected-button.svg"
                    }
                    collectLabel.color = "#2C73C8"
                }
                onReleased: {
                    collectLabelRetTooltip.visible = false
                    noCollectedTooltip.visible = false
                    unsaveTooltip.visible = false
                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount && operateChooseIndex === 1){
                        return
                    }
                    if(operateChooseIndex === 1){
                        if(editOperatorCount === checkedCollectEditOperatorCount){
                            collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                        }else{
                            collectImage.source = "qrc:/icon/icon/collect-black.svg"
                        }
                    }else{
                        collectImage.source = "qrc:/icon/icon/collected-button-click.svg"
                    }
                    collectLabel.color = "#000000"
                }
                onClicked: {
                    if(editOperatorCount === checkedCollectEditOperatorCount){
                        var selectedOptions = [];
                        // 获取所有选中的历史记录
                        for (var i = 0; i < filteredListModel.count; i++) {
                            if (filteredListModel.get(i).checked) {
                                if(filteredListModel.get(i).sessionID !== null &&　filteredListModel.get(i).sessionID !== "" && filteredListModel.get(i).sessionID !== undefined){
                                    selectedOptions.push(filteredListModel.get(i).sessionID);
                                }
                            }
                        }
                        // 没有选中的历史记录返回,不做操作
                        if(selectedOptions.length < 1){
                            return
                        }

                        var operateType = 2 // 取消收藏
                        batchCancelCollect(selectedOptions, operateType)
                        return
                    }

                    if(totelOperateCollectedCount + editOperateCollectedCount > maxOperateCollectedCount && operateChooseIndex === 1){
                        return
                    }
                    var selectedOptions = [];
                    // 获取所有选中的历史记录
                    for (var i = 0; i < filteredListModel.count; i++) {
                        if (filteredListModel.get(i).checked) {
                            if(filteredListModel.get(i).sessionID !== null &&　filteredListModel.get(i).sessionID !== "" && filteredListModel.get(i).sessionID !== undefined){
                                selectedOptions.push(filteredListModel.get(i).sessionID);
                            }
                        }
                    }
                    // 没有选中的历史记录返回,不做操作
                    if(selectedOptions.length < 1){
                        return
                    }
                    console.log("Selected options:", selectedOptions);

                    var operateType = operateChooseIndex === 1 ? 1 : 2
                    batchCancelCollect(selectedOptions, operateType)
                }
            }
        }

        Rectangle{
            id: deleteLabelRet
            height: 36
            width: deleteRow.width
            visible: isEditFlag
            onVisibleChanged: {
                if (visible) {
                    deleteImage.source = "qrc:/icon/icon/delete-blue.svg";
                    deleteLabel.color = "#000000";
                    deleteLabelRetMouseArea.hoverEnabled = true
                }
            }
            // 悬浮提示框
            ToolTip {
                id: deleteLabelRetTooltip
                visible: deleteLabelRetMouseArea.containsMouse && root.winMode === 2
                delay: Qt.styleHints.mousePressAndHoldInterval       // 延迟显示提升用户体验
                text: qsTr("Delete")
                property var maxWidth: 400
                property var wrapMode: Text.WrapAnywhere
            }
            Row{
                id: deleteRow
                width: deleteImage.width + (deleteLabel.visible ? (deleteLabel.implicitWidth + deleteRow.spacing) : 0 )
                height: parent.height
                spacing: 6
                Image {
                    id: deleteImage
                    source: "qrc:/icon/icon/delete-blue.svg"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label{
                    id: deleteLabel
                    text: qsTr("Delete")
                    anchors.verticalCenter: parent.verticalCenter
                    width: deleteLabel.implicitWidth
                    color: "#000000"
                    visible: root.winMode === 2 ? false : true
                }
            }
            MouseArea{
                id: deleteLabelRetMouseArea
                anchors.fill: parent
                //hoverEnabled: true
                onEntered: {
                    deleteImage.source = "qrc:/icon/icon/delete-red-hover.svg"
                    deleteLabel.color = "#E93C3C"
                }
                onExited: {
                    deleteImage.source = "qrc:/icon/icon/delete-blue.svg"
                    deleteLabel.color = "#000000"
                }
                onPressed: {
                    deleteImage.source = "qrc:/icon/icon/delete-red-click.svg"
                    deleteLabel.color = "#C63432"
                }
                onReleased: {
                    deleteImage.source = "qrc:/icon/icon/delete-blue.svg"
                    deleteLabel.color = "#000000"
                }
                onClicked: {
                    var selectedOptions = [];
                    // 获取所有选中的历史记录
                    for (var i = 0; i < filteredListModel.count; i++) {
                        if (filteredListModel.get(i).checked) {
                            if(filteredListModel.get(i).sessionID !== null &&　filteredListModel.get(i).sessionID !== "" && filteredListModel.get(i).sessionID !== undefined){
                                selectedOptions.push(filteredListModel.get(i).sessionID);
                            }
                        }
                    }
                    // 没有选中的历史记录返回,不做操作
                    if(selectedOptions.length < 1){
                        return
                    }
                    console.log("Selected options:", JSON.stringify(selectedOptions));

                    let dialogComponent = Qt.createComponent("qrc:/qml/qml/DeleteCommonWindow.qml");
                    if (dialogComponent.status === Component.Ready) {
                        let dialog = dialogComponent.createObject(root); // 父级设为顶层覆盖层
                        dialog.selectedOptions = selectedOptions
                        dialog.questionText = qsTr("Delete is irreversible. Continue?")
                        dialog.infoText = ""
                        dialog.confirmButtonText = qsTr("Delete")
                        dialog.cancelButtonText = qsTr("Cancel")
                        dialog.highlightConfirmButton = true
                        dialog.highlightCancelButton = false
                        dialog.showNoRemindCheckBox = false
                        if (dialog) {
                            dialog.confirmed.connect(function(options, noRemindChecked) {
                               msgPane.deleteSessionList(selectedOptions)
                               dialog.destroy();
                            });

                            dialog.canceled.connect(function() {
                               dialog.destroy();
                            });

                            dialog.showDialog();
                        } else {
                            console.error("创建弹窗失败：", dialogComponent.errorString());
                        }
                    } else {
                        console.error("加载弹窗组件失败：", dialogComponent.errorString());
                    }
                }
            }
        }
    }
}
