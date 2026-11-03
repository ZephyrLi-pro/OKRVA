import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."

Rectangle {
    id: introsuctionContentRec
    color: "transparent"

    signal openModelsPage()
    signal closeWindow()
    signal quitApplication()

    property int currentPage: 0  // 0: agreement, 1: shortcuts, 2: addModel

    // 页面枚举
    property int pageAgreement: 0
    property int pageShortcuts: 1
    property int pageAddModel: 2

    readonly property int contentHorizontalMargin: 0
    readonly property int buttonBottomMargin: 24
    readonly property int guidePageEdgeMargin: 72
    readonly property int shortcutsPageButtonBottomMargin: guidePageEdgeMargin
    readonly property int shortcutsPageButtonRightMargin: guidePageEdgeMargin
    readonly property int agreementPageTopMargin: 24
    readonly property int agreementPageHorizontalMargin: guidePageEdgeMargin
    readonly property int agreementPageButtonBottomMargin: guidePageEdgeMargin
    readonly property int agreementPageButtonRightMargin: guidePageEdgeMargin
    readonly property int shortcutsPageTopMargin: guidePageEdgeMargin
    readonly property int addModelPageTopMargin: guidePageEdgeMargin
    readonly property int addModelPageHorizontalMargin: guidePageEdgeMargin
    readonly property int addModelPageButtonBottomMargin: guidePageEdgeMargin
    readonly property int addModelPageButtonRightMargin: guidePageEdgeMargin
    readonly property int addModelPageButtonLeftMargin: guidePageEdgeMargin
    readonly property int guidePageContentButtonSpacing: 16
    readonly property int addModelWelcomePanelBottomMargin: 246
    readonly property int addModelWelcomePanelBottomMarginInPage: addModelWelcomePanelBottomMargin
            - addModelPageButtonBottomMargin
            - guidePrimaryButtonHeight
            - guidePageContentButtonSpacing
    readonly property int systemFontSize: (typeof globals !== "undefined") ? globals.fontsize : 11
    readonly property string systemFontFamily: Qt.application.font.family
    readonly property int guidePrimaryButtonVerticalPadding: 1
    readonly property int guidePrimaryButtonWidth: 96
    readonly property int guidePrimaryButtonHorizontalPadding: 8
    readonly property int guidePrimaryButtonTextWidth: guidePrimaryButtonWidth - guidePrimaryButtonHorizontalPadding * 2
    readonly property int guidePrimaryButtonHeight: Math.max(
                36,
                guideButtonFontMetrics.height + guidePrimaryButtonVerticalPadding * 2)

    Component.onCompleted: {
        showAgreement()
    }

    function showAgreement() {
        currentPage = pageAgreement
    }

    function showFirst() {
        currentPage = pageShortcuts
    }

    function showSecond() {
        currentPage = pageAddModel
    }

    function updateConfig(key, value) {
        msgPane.updateConfig(key, value)
    }

    // 使用Item作为居中容器
    Item {
        anchors.fill: parent

        FontMetrics {
            id: guideButtonFontMetrics
            font.pointSize: systemFontSize
            font.family: systemFontFamily
        }

        ColumnLayout {
            id: mainLayout
            anchors.fill: parent
            anchors.leftMargin: contentHorizontalMargin
            anchors.rightMargin: contentHorizontalMargin
            anchors.topMargin: {
                if (currentPage === pageAgreement)
                    return agreementPageTopMargin
                if (currentPage === pageShortcuts)
                    return shortcutsPageTopMargin
                if (currentPage === pageAddModel)
                    return addModelPageTopMargin
                return 24
            }
            anchors.bottomMargin: {
                if (currentPage === pageShortcuts)
                    return shortcutsPageButtonBottomMargin
                if (currentPage === pageAgreement)
                    return agreementPageButtonBottomMargin
                if (currentPage === pageAddModel)
                    return addModelPageButtonBottomMargin
                return buttonBottomMargin
            }
            spacing: guidePageContentButtonSpacing

            // 页面容器
            Item {
                id: pageContainer
                Layout.fillWidth: true
                Layout.fillHeight: currentPage === pageAddModel
                        || currentPage === pageShortcuts
                        || currentPage === pageAgreement
                Layout.preferredHeight: 1
                Layout.alignment: Qt.AlignTop
                Layout.leftMargin: {
                    if (currentPage === pageAgreement || currentPage === pageAddModel || currentPage === pageShortcuts)
                        return agreementPageHorizontalMargin
                    return 24
                }
                Layout.rightMargin: {
                    if (currentPage === pageAgreement || currentPage === pageAddModel || currentPage === pageShortcuts)
                        return agreementPageHorizontalMargin
                    return 24
                }

                // 协议页面
                AgreementPage {
                    id: agreementPage
                    anchors.fill: parent
                    visible: currentPage === pageAgreement
                    onAgreeClicked: {
                        updateConfig("is-first-install", false)
                        updateConfig("is-first-install-50", false)
                        showFirst()
                    }
                    onDisagreeClicked: {
                        quitApplication()
                    }
                }

                // 快捷键介绍页面
                ShortcutsPage {
                    id: shortcutsPage
                    anchors.fill: parent
                    visible: currentPage === pageShortcuts
                    onNextClicked: {
                        showSecond()
                    }
                }

                // 添加模型页面
                AddModelPage {
                    id: addModelPage
                    anchors.fill: parent
                    visible: currentPage === pageAddModel
                    welcomePanelBottomMargin: introsuctionContentRec.addModelWelcomePanelBottomMarginInPage
                    onSkipClicked: {
                        closeWindow()
                    }
                    onPreviousClicked: {
                        showFirst()
                    }
                    onConfigureClicked: {
                        if (windowCtrl.winMode === 2) {
                            windowCtrl.winMode = 3
                        }
                        openModelsPage()
                    }
                }
            }

            // 底部按钮区域（主操作按钮右对齐锚定，三页位置一致）
            Item {
                id: guideButtonBar
                Layout.fillWidth: true
                Layout.preferredHeight: guidePrimaryButtonHeight
                visible: !((currentPage === pageAgreement || currentPage === pageShortcuts || currentPage === pageAddModel) && windowCtrl.winMode === 2)
                Layout.topMargin: (currentPage === pageShortcuts || currentPage === pageAgreement || currentPage === pageAddModel) ? 0 : 24
                Layout.rightMargin: {
                    if (currentPage === pageShortcuts)
                        return addModelPageButtonRightMargin
                    if (currentPage === pageAgreement)
                        return agreementPageButtonRightMargin
                    if (currentPage === pageAddModel)
                        return addModelPageButtonRightMargin
                    return 24
                }
                Layout.leftMargin: {
                    if (currentPage === pageAgreement)
                        return agreementPageHorizontalMargin
                    if (currentPage === pageAddModel || currentPage === pageShortcuts)
                        return addModelPageButtonLeftMargin
                    return 24
                }

                readonly property int guideButtonSpacing: 8

                // 跳过（关闭新手引导）；模型添加页左侧
                Button {
                    id: acceptBtn
                    visible: currentPage === pageAddModel
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Skip")
                    focusPolicy: Qt.NoFocus
                    highlighted: false
                    width: guidePrimaryButtonWidth
                    height: guidePrimaryButtonHeight
                    background: Rectangle {
                        color: {
                            if (acceptBtn.pressed)
                                return "#B9B9B9"
                            if (acceptBtn.hovered)
                                return "#DCDCDC"
                            return "#E6E6E6"
                        }
                        radius: 6
                        border.width: 1
                        border.color: "transparent"
                    }
                    contentItem: Label {
                        id: acceptBtnLabel
                        width: guidePrimaryButtonTextWidth
                        anchors.centerIn: parent
                        text: acceptBtn.text
                        font.pointSize: systemFontSize
                        font.family: systemFontFamily
                        color: "#333333"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    ToolTip {
                        delay: 500
                        visible: acceptBtn.hovered && acceptBtnLabel.truncated
                        text: acceptBtn.text
                    }
                    onClicked: {
                        closeWindow()
                    }
                }

                // 同意并继续按钮
                Button {
                    id: agreeBtn
                    visible: currentPage === pageAgreement && windowCtrl.winMode !== 2
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("agree and continue")
                    z: 3
                    focusPolicy: Qt.NoFocus
                    highlighted: false
                    width: guidePrimaryButtonWidth
                    height: guidePrimaryButtonHeight
                    background: Rectangle {
                        color: "#3790FA"
                        radius: 6
                        border.width: 1
                        border.color: "transparent"
                    }
                    contentItem: Label {
                        id: agreeBtnLabel
                        width: guidePrimaryButtonTextWidth
                        anchors.centerIn: parent
                        text: agreeBtn.text
                        font.pointSize: systemFontSize
                        font.family: systemFontFamily
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    ToolTip {
                        delay: 500
                        visible: agreeBtn.hovered && agreeBtnLabel.truncated
                        text: agreeBtn.text
                    }
                    onClicked: {
                        updateConfig("is-first-install", false)
                        updateConfig("is-first-install-50", false)
                        showFirst()
                    }
                }

                // 主操作：下一步 / 立即配置（同一锚点与样式，布局与模型页一致）
                Button {
                    id: guidePrimaryBtn
                    visible: currentPage === pageShortcuts || currentPage === pageAddModel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: currentPage === pageShortcuts
                          ? qsTranslate("IntroductionContent", "Next")
                          : qsTr("Configure now")
                    z: 3
                    focusPolicy: Qt.NoFocus
                    highlighted: false
                    width: guidePrimaryButtonWidth
                    height: guidePrimaryButtonHeight
                    background: Rectangle {
                        color: "#3790FA"
                        radius: 6
                        border.width: 1
                        border.color: "transparent"
                    }
                    contentItem: Label {
                        id: guidePrimaryBtnLabel
                        width: guidePrimaryButtonTextWidth
                        anchors.centerIn: parent
                        text: guidePrimaryBtn.text
                        font.pointSize: systemFontSize
                        font.family: systemFontFamily
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    ToolTip {
                        delay: 500
                        visible: guidePrimaryBtn.hovered && guidePrimaryBtnLabel.truncated
                        text: guidePrimaryBtn.text
                    }
                    onClicked: {
                        if (currentPage === pageShortcuts)
                            showSecond()
                        else
                            openModelsPage()
                    }
                }

                // 不同意按钮
                Button {
                    id: disagreeBtn
                    visible: currentPage === pageAgreement && windowCtrl.winMode !== 2
                    anchors.right: agreeBtn.left
                    anchors.rightMargin: guideButtonBar.guideButtonSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("disagree")
                    focusPolicy: Qt.NoFocus
                    highlighted: false
                    width: guidePrimaryButtonWidth
                    height: guidePrimaryButtonHeight
                    background: Rectangle {
                        color: {
                            if (disagreeBtn.pressed)
                                return "#B9B9B9"
                            if (disagreeBtn.hovered)
                                return "#DCDCDC"
                            return "#E6E6E6"
                        }
                        radius: 6
                        border.width: 1
                        border.color: "transparent"
                    }
                    contentItem: Label {
                        id: disagreeBtnLabel
                        width: guidePrimaryButtonTextWidth
                        anchors.centerIn: parent
                        text: disagreeBtn.text
                        font.pointSize: systemFontSize
                        font.family: systemFontFamily
                        color: "#333333"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    ToolTip {
                        delay: 500
                        visible: disagreeBtn.hovered && disagreeBtnLabel.truncated
                        text: disagreeBtn.text
                    }
                    onClicked: {
                        updateConfig("is-first-install", true)
                        quitApplication()
                    }
                }

                // 上一页按钮（快捷键页透明占位，保证「下一步」与「立即配置」水平位置一致）
                Button {
                    id: previousBtn
                    visible: currentPage === pageAddModel || currentPage === pageShortcuts
                    enabled: currentPage === pageAddModel
                    opacity: enabled ? 1 : 0
                    anchors.right: guidePrimaryBtn.left
                    anchors.rightMargin: guideButtonBar.guideButtonSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Previous")
                    focusPolicy: Qt.NoFocus
                    highlighted: false
                    width: guidePrimaryButtonWidth
                    height: guidePrimaryButtonHeight
                    background: Rectangle {
                        color: {
                            if (previousBtn.pressed)
                                return "#B9B9B9"
                            if (previousBtn.hovered)
                                return "#DCDCDC"
                            return "#E6E6E6"
                        }
                        radius: 6
                        border.width: 1
                        border.color: "transparent"
                    }
                    contentItem: Label {
                        id: previousBtnLabel
                        width: guidePrimaryButtonTextWidth
                        anchors.centerIn: parent
                        text: previousBtn.text
                        font.pointSize: systemFontSize
                        font.family: systemFontFamily
                        color: "#333333"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    ToolTip {
                        delay: 500
                        visible: previousBtn.hovered && previousBtnLabel.truncated
                        text: previousBtn.text
                    }
                    onClicked: {
                        showFirst()
                    }
                }
            }
        }
    }
}
