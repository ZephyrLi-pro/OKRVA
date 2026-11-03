import QtQuick 2.12
import QtQuick.Layouts 1.12

Item {
    id: knowledgeShowPageWrapper
    property alias knowledgeShowPage: knowledgeShowPage
    
    KnowledgeShowPage{
        id: knowledgeShowPage
        anchors.fill: parent
        visible: false
        showDragShadowFlag: false
        uploadFileUrl: ""
        onShowKnowledgeTip: function(tipImageSource, tipText){
            console.log("onShowNotMoveTip====", tipImageSource,tipText)
            // 通过作用域链访问 root (ApplicationWindow)
            // root 是在 main.qml 中定义的 ApplicationWindow 的 id
//            if (typeof root !== "undefined" && root) {
//                root.commonTipsText = tipText
//                root.commonTipsSources = tipImageSource
//                if (root.commonTips) {
//                    root.commonTips.visible = true
//                }
//                if (root.copyTipCloseTimer) {
//                    root.copyTipCloseTimer.running = true
//                }
//            }
        }
        onChangeTransparentKnowledgeShow: function(flag){
            if (typeof root !== "undefined" && root && typeof root.changeTrasnparent === "function") {
                root.changeTrasnparent(flag)
            }
        }
    }
}

