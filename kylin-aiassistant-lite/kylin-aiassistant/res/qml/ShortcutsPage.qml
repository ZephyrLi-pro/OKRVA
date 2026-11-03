import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import "."

Rectangle {
    id: shortcutsRect
    signal nextClicked()

    function applyScribeAutoInvokeRadios(autoInvokeEnabled) {
        autoWakeRadio.isUpdating = true
        shortcutWakeRadio.isUpdating = true
        autoWakeRadio.checked = autoInvokeEnabled
        shortcutWakeRadio.checked = !autoInvokeEnabled
        autoWakeRadio.isUpdating = false
        shortcutWakeRadio.isUpdating = false
    }

    Component.onCompleted: {
        if (typeof globals !== "undefined" && globals !== null)
            shortcutsRect.applyScribeAutoInvokeRadios(globals.getScribeAutoInvoke())
    }

    Connections {
        target: (typeof globals !== "undefined" && globals !== null) ? globals : null
        function onFontsizeChanged() {
            shortcutsRect.shortcutKeyLayoutEpoch++
        }
    }

    Item {
        width: 0
        height: 0
        visible: false

        ButtonGroup {
            id: floatingBarWakeButtons
            exclusive: true
        }
    }

    readonly property bool isSidebarMode: windowCtrl.winMode === 2

    readonly property color cardTitleColor: Qt.rgba(0, 0, 0, 0.85)
    readonly property color guidePageTitleColor: Qt.rgba(0, 0, 0, 0.85)
    readonly property string systemFontFamily: Qt.application.font.family
    readonly property color shortcutKeyTextColor: "#3790FA"
    readonly property color shortcutKeyBoxBorderColor: Qt.rgba(0, 0, 0, 0.12)
    readonly property int cardIllustrationWidth: 412
    readonly property int cardIllustrationHeight: 122
    readonly property double cardDesignWidth: 460
    readonly property double cardDesignHeight: 297.83
    readonly property double cardInnerGap: 56
    readonly property int titleToCardsGap: 62
    readonly property int cardSpacing: 32
    readonly property real systemFontSize: (typeof globals !== "undefined") ? globals.fontsize : 11
    readonly property int cardTitleFontSize: systemFontSize + 2
    readonly property int shortcutKeyFontSize: systemFontSize
    readonly property int shortcutKeyHorizontalPadding: 12
    readonly property int shortcutKeyVerticalPadding: 6
    readonly property int shortcutKeyMinWidth: 52
    property int shortcutKeyLayoutEpoch: 0
    readonly property string floatingBarAutoIllustrationSource: "qrc:/icon/icon/select-words.svg"
    readonly property string floatingBarShortcutIllustrationSource: "qrc:/icon/icon/super-alt-c.svg"
    readonly property real guideContentHeight: shortcutHeaderRow.height + titleToCardsGap + cardDesignHeight

    readonly property int sidebarCardWidth: 354
    readonly property int sidebarCard1Height: 215
    readonly property int sidebarCard2Height: 243
    readonly property int sidebarIconSize: 48
    readonly property int sidebarTitleFontSize: (typeof globals !== "undefined") ? globals.fontsize + 8 : 22
    readonly property int sidebarIconToTitleGap: 16
    readonly property int sidebarTitleToCardsGap: 24
    readonly property int sidebarCardSpacing: 16
    readonly property int sidebarCardPadding: 16
    readonly property int sidebarCardRadius: 8
    readonly property int sidebarCardContentSpacing: 12
    readonly property int sidebarButtonToCardsGap: 24
    readonly property int guidePrimaryButtonWidth: 96
    readonly property int guidePrimaryButtonHeight: Math.max(
                36,
                guideButtonFontMetrics.height + 2)

    FontMetrics {
        id: guideButtonFontMetrics
        font.pointSize: systemFontSize
        font.family: systemFontFamily
    }

    // ==================== 侧边栏模式布局 ====================
    Item {
        visible: isSidebarMode
        anchors.fill: parent

        Item {
            anchors.centerIn: parent
            width: sidebarCardWidth
            height: sidebarContent.height

            ColumnLayout {
                id: sidebarContent
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                spacing: 0

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/icon/icon/guide.svg"
                    sourceSize.width: sidebarIconSize
                    sourceSize.height: sidebarIconSize
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: sidebarIconToTitleGap
                    text: qsTranslate("IntroductionContent", "Beginner's Guide / Usage Tips")
                    font.family: systemFontFamily
                    font.weight: Font.Medium
                    font.pointSize: sidebarTitleFontSize
                    color: guidePageTitleColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: sidebarTitleToCardsGap
                    width: sidebarCardWidth
                    height: sidebarCard1Height
                    radius: sidebarCardRadius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(97 / 255, 53 / 255, 1, 13 / 255)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(53 / 255, 93 / 255, 1, 13 / 255)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: sidebarCardPadding
                        spacing: sidebarCardContentSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Item {
                                Layout.preferredWidth: 24
                                Layout.minimumWidth: 24
                                Layout.maximumWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignTop

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/icon/icon/1.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: qsTranslate("IntroductionContent", "Qi Ling activation / dismissal")
                                font.family: systemFontFamily
                                font.pointSize: cardTitleFontSize
                                font.bold: true
                                color: cardTitleColor
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextMetrics {
                                id: sidebarSuperKeyTextMetrics
                                font.family: systemFontFamily
                                font.pointSize: shortcutsRect.shortcutKeyFontSize
                                text: "Super"
                            }
                            TextMetrics {
                                id: sidebarPlusKeyTextMetrics
                                font.family: systemFontFamily
                                font.pointSize: shortcutsRect.shortcutKeyFontSize
                                text: "+"
                            }
                            TextMetrics {
                                id: sidebarCKeyTextMetrics
                                font.family: systemFontFamily
                                font.pointSize: shortcutsRect.shortcutKeyFontSize
                                text: "C"
                            }

                            Rectangle {
                                Layout.preferredWidth: Math.max(
                                    sidebarSuperKeyTextMetrics.width + shortcutsRect.shortcutKeyHorizontalPadding * 2,
                                    shortcutsRect.shortcutKeyMinWidth)
                                Layout.preferredHeight: sidebarSuperKeyTextMetrics.height + shortcutsRect.shortcutKeyVerticalPadding * 2
                                radius: 8
                                color: "white"
                                border.width: 1
                                border.color: shortcutsRect.shortcutKeyBoxBorderColor
                                Label {
                                    anchors.centerIn: parent
                                    text: "Super"
                                    font.family: systemFontFamily
                                    font.pointSize: shortcutsRect.shortcutKeyFontSize
                                    color: shortcutsRect.shortcutKeyTextColor
                                }
                            }

                            Label {
                                text: "+"
                                font.family: systemFontFamily
                                font.pointSize: shortcutsRect.shortcutKeyFontSize
                                color: cardTitleColor
                                Layout.preferredWidth: sidebarPlusKeyTextMetrics.width
                                Layout.preferredHeight: sidebarPlusKeyTextMetrics.height
                                Layout.alignment: Qt.AlignVCenter
                                padding: 0
                                verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                Layout.preferredWidth: sidebarCKeyTextMetrics.width + shortcutsRect.shortcutKeyHorizontalPadding * 2
                                Layout.preferredHeight: sidebarCKeyTextMetrics.height + shortcutsRect.shortcutKeyVerticalPadding * 2
                                radius: 8
                                color: "white"
                                border.width: 1
                                border.color: shortcutsRect.shortcutKeyBoxBorderColor
                                Label {
                                    anchors.centerIn: parent
                                    text: "C"
                                    font.family: systemFontFamily
                                    font.pointSize: shortcutsRect.shortcutKeyFontSize
                                    color: shortcutsRect.shortcutKeyTextColor
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: Math.min(322, parent.width)
                                height: 96
                                source: "qrc:/icon/icon/super-c.svg"
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: sidebarCardSpacing
                    width: sidebarCardWidth
                    height: sidebarCard2Height
                    radius: sidebarCardRadius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(97 / 255, 53 / 255, 1, 13 / 255)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(53 / 255, 93 / 255, 1, 13 / 255)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: sidebarCardPadding
                        spacing: sidebarCardContentSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Item {
                                Layout.preferredWidth: 24
                                Layout.minimumWidth: 24
                                Layout.maximumWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignTop

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/icon/icon/2.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: qsTranslate("IntroductionContent", "Floating toolbar for selected text")
                                font.family: systemFontFamily
                                font.pointSize: cardTitleFontSize
                                font.bold: true
                                color: cardTitleColor
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            RadioButton {
                                id: autoWakeRadio
                                checked: true
                                hoverEnabled: true
                                text: qsTranslate("IntroductionContent", "Automatic wake-up")
                                Layout.fillWidth: true
                                ButtonGroup.group: floatingBarWakeButtons
                                property bool isUpdating: false

                                indicator: Rectangle {
                                    x: root.leftPadding
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: width
                                    antialiasing: true
                                    radius: width / 2
                                    border.width: autoWakeRadio.checked ? width * 0.25 : 1
                                    border.color: autoWakeRadio.checked ? "#3790FA" : "#CCCCCC"
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width * 0.5
                                        height: width
                                        antialiasing: true
                                        radius: width / 2
                                        color: root.checkedColor
                                        visible: root.checked
                                    }
                                }

                                contentItem: Label {
                                    text: autoWakeRadio.text
                                    font.family: systemFontFamily
                                    font.pointSize: systemFontSize
                                    color: "#333333"
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: autoWakeRadio.indicator.width + autoWakeRadio.spacing
                                    elide: Text.ElideRight
                                }

                                HoverHandler {
                                    id: guideAutoWakeTipHoverHandler
                                }
                                ToolTip.delay: 500
                                ToolTip.visible: guideAutoWakeTipHoverHandler.hovered
                                ToolTip.text: qsTranslate("SettingSelectPage", "After selecting text, the floating bar appears automatically.")

                                onCheckedChanged: {
                                    if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !shortcutWakeRadio.checked)
                                        globals.setScribeAutoInvoke(true)
                                }
                            }

                            RadioButton {
                                id: shortcutWakeRadio
                                checked: false
                                hoverEnabled: true
                                text: qsTranslate("IntroductionContent", "Shortcut wake-up (Super + Alt + C)")
                                Layout.fillWidth: true
                                ButtonGroup.group: floatingBarWakeButtons
                                property bool isUpdating: false

                                indicator: Rectangle {
                                    x: root.leftPadding
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: width
                                    antialiasing: true
                                    radius: width / 2
                                    border.width: shortcutWakeRadio.checked ? width * 0.25 : 1
                                    border.color: shortcutWakeRadio.checked ? "#3790FA" : "#CCCCCC"
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width * 0.5
                                        height: width
                                        antialiasing: true
                                        radius: width / 2
                                        color: root.checkedColor
                                        visible: root.checked
                                    }
                                }

                                contentItem: Label {
                                    id: shortcutWakeLabel
                                    text: shortcutWakeRadio.text
                                    font.family: systemFontFamily
                                    font.pointSize: systemFontSize
                                    color: "#333333"
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: shortcutWakeRadio.indicator.width + shortcutWakeRadio.spacing
                                    elide: Text.ElideRight
                                    wrapMode: Text.NoWrap
                                }

                                onCheckedChanged: {
                                    if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !autoWakeRadio.checked)
                                        globals.setScribeAutoInvoke(false)
                                }

                                TextMetrics {
                                    id: shortcutWakeTextMetrics
                                    font: shortcutWakeRadio.font
                                    text: shortcutWakeRadio.text
                                }

                                ToolTip.delay: 500
                                ToolTip.visible: shortcutWakeRadio.hovered
                                ToolTip.text: (((shortcutWakeLabel.width - shortcutWakeLabel.leftPadding) > 1)
                                    && (shortcutWakeTextMetrics.width > (shortcutWakeLabel.width - shortcutWakeLabel.leftPadding)))
                                    ? shortcutWakeRadio.text
                                    : qsTranslate("SettingSelectPage", "After selecting text, press the shortcut keys (Super + Alt + C) to display the floating bar.")
                            }
                        }

                        Connections {
                            target: (typeof globals !== "undefined" && globals !== null) ? globals : null
                            function onSigScribeAutoInvokeChanged(autoInvokeEnabled) {
                                shortcutsRect.applyScribeAutoInvokeRadios(autoInvokeEnabled)
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Image {
                                id: sidebarFloatingBarIllustration
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                width: Math.min(322, parent.width)
                                height: 96
                                source: autoWakeRadio.checked
                                        ? shortcutsRect.floatingBarAutoIllustrationSource
                                        : shortcutsRect.floatingBarShortcutIllustrationSource
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: sidebarButtonToCardsGap
                }

                Row {
                    Layout.alignment: Qt.AlignRight

                    Button {
                        id: sidebarNextBtn
                        text: qsTranslate("IntroductionContent", "Next")
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
                            anchors.centerIn: parent
                            text: sidebarNextBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            nextClicked()
                        }
                    }
                }
            }
        }
    }

    // ==================== 窗口模式布局（保持原样） ====================
    Item {
        visible: !isSidebarMode
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 标题：灯泡 48×48 + 文案（与协议页 agreementRow 一致）
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: shortcutsRow.height
                Layout.maximumHeight: shortcutsRow.height

                Row {
                    id: shortcutsRow
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 0
                    bottomPadding: titleToCardsGap

                    Row {
                        id: shortcutHeaderRow
                        spacing: 12
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0

                        Image {
                            id: shortcutHeaderRowImage
                            anchors.verticalCenter: parent.verticalCenter
                            width: 48
                            height: 48
                            sourceSize.width: 48
                            sourceSize.height: 48
                            smooth: true
                            source: "qrc:/icon/icon/guide.svg"
                            fillMode: Image.PreserveAspectFit
                        }

                        Label {
                            id: shortcutTitle
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(
                                implicitWidth,
                                Math.max(shortcutsRect.width - 48 - 12 - 32, 100)
                            )
                            text: qsTranslate("IntroductionContent", "Beginner's Guide / Usage Tips")
                            font.family: systemFontFamily
                            font.weight: Font.Medium
                            font.pointSize: (typeof globals !== "undefined") ? globals.fontsize + 16 : 30
                            color: guidePageTitleColor
                            lineHeight: font.pixelSize
                            lineHeightMode: Text.FixedHeight
                            padding: 0
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // 左右双卡片：460 × 297.83，padding 24、gap 56、圆角 8、浅紫→浅蓝渐变
            RowLayout {
                id: cardsRow
                Layout.fillWidth: true
                Layout.preferredHeight: shortcutsRect.cardDesignHeight
                Layout.maximumHeight: shortcutsRect.cardDesignHeight
                Layout.fillHeight: false
                spacing: shortcutsRect.cardSpacing

                // 左卡：小K唤起
                Rectangle {
                    id: leftCard
                    Layout.preferredWidth: shortcutsRect.cardDesignWidth
                    Layout.preferredHeight: shortcutsRect.cardDesignHeight
                    Layout.minimumWidth: shortcutsRect.cardDesignWidth
                    Layout.minimumHeight: shortcutsRect.cardDesignHeight
                    Layout.maximumWidth: shortcutsRect.cardDesignWidth
                    Layout.maximumHeight: shortcutsRect.cardDesignHeight
                    Layout.alignment: Qt.AlignTop
                    radius: 8
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(97 / 255, 53 / 255, 1, 13 / 255)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(53 / 255, 93 / 255, 1, 13 / 255)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: shortcutsRect.cardInnerGap

                        // 标题 1：角标 +「小K唤起 / 隐藏方式」；下行 Super + C 与效果图一致（白底匙形、文案 #3790FA）
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Item {
                                Layout.preferredWidth: 24
                                Layout.minimumWidth: 24
                                Layout.maximumWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignTop

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/icon/icon/1.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTranslate("IntroductionContent", "Qi Ling activation / dismissal")
                                    font.family: systemFontFamily
                                    font.pointSize: cardTitleFontSize
                                    font.bold: true
                                    color: cardTitleColor
                                    padding: 0
                                    verticalAlignment: Text.AlignTop
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    id: qiLingShortcutKeyRow
                                    spacing: 8

                                    TextMetrics {
                                        id: superKeyTextMetrics
                                        font.family: systemFontFamily
                                        font.pointSize: shortcutsRect.shortcutKeyFontSize
                                        text: "Super"
                                    }
                                    TextMetrics {
                                        id: plusKeyTextMetrics
                                        font.family: systemFontFamily
                                        font.pointSize: shortcutsRect.shortcutKeyFontSize
                                        text: "+"
                                    }
                                    TextMetrics {
                                        id: cKeyTextMetrics
                                        font.family: systemFontFamily
                                        font.pointSize: shortcutsRect.shortcutKeyFontSize
                                        text: "C"
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: {
                                            var _ = shortcutsRect.shortcutKeyLayoutEpoch
                                            return Math.max(
                                                        superKeyTextMetrics.width + shortcutsRect.shortcutKeyHorizontalPadding * 2,
                                                        shortcutsRect.shortcutKeyMinWidth)
                                        }
                                        Layout.preferredHeight: superKeyTextMetrics.height + shortcutsRect.shortcutKeyVerticalPadding * 2
                                        Layout.minimumWidth: Layout.preferredWidth
                                        Layout.maximumWidth: Layout.preferredWidth
                                        radius: 8
                                        color: "white"
                                        border.width: 1
                                        border.color: shortcutsRect.shortcutKeyBoxBorderColor
                                        Label {
                                            id: superKeyLabel
                                            anchors.centerIn: parent
                                            text: "Super"
                                            font.family: systemFontFamily
                                            font.pointSize: shortcutsRect.shortcutKeyFontSize
                                            color: shortcutsRect.shortcutKeyTextColor
                                        }
                                    }

                                    Label {
                                        text: "+"
                                        font.family: systemFontFamily
                                        font.pointSize: shortcutsRect.shortcutKeyFontSize
                                        color: cardTitleColor
                                        Layout.preferredWidth: plusKeyTextMetrics.width
                                        Layout.preferredHeight: plusKeyTextMetrics.height
                                        Layout.alignment: Qt.AlignVCenter
                                        padding: 0
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: {
                                            var _ = shortcutsRect.shortcutKeyLayoutEpoch
                                            return cKeyTextMetrics.width + shortcutsRect.shortcutKeyHorizontalPadding * 2
                                        }
                                        Layout.preferredHeight: cKeyTextMetrics.height + shortcutsRect.shortcutKeyVerticalPadding * 2
                                        Layout.minimumWidth: Layout.preferredWidth
                                        Layout.maximumWidth: Layout.preferredWidth
                                        radius: 8
                                        color: "white"
                                        border.width: 1
                                        border.color: shortcutsRect.shortcutKeyBoxBorderColor

                                        Label {
                                            id: cKeyLabel
                                            anchors.centerIn: parent
                                            text: "C"
                                            font.family: systemFontFamily
                                            font.pointSize: shortcutsRect.shortcutKeyFontSize
                                            color: shortcutsRect.shortcutKeyTextColor
                                        }
                                    }
                                }
                            }
                        }

                        // 键盘图示区（底对齐，下边距与 margin 对齐）
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Image {
                                width: Math.min(shortcutsRect.cardIllustrationWidth, parent.width)
                                height: shortcutsRect.cardIllustrationHeight
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                source: "qrc:/icon/icon/super-c.svg"
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }
                }

                // 右卡：划词悬浮条
                Rectangle {
                    id: rightCard
                    Layout.preferredWidth: shortcutsRect.cardDesignWidth
                    Layout.preferredHeight: shortcutsRect.cardDesignHeight
                    Layout.minimumWidth: shortcutsRect.cardDesignWidth
                    Layout.minimumHeight: shortcutsRect.cardDesignHeight
                    Layout.maximumWidth: shortcutsRect.cardDesignWidth
                    Layout.maximumHeight: shortcutsRect.cardDesignHeight
                    Layout.alignment: Qt.AlignTop
                    radius: 8
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(97 / 255, 53 / 255, 1, 13 / 255)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(53 / 255, 93 / 255, 1, 13 / 255)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: shortcutsRect.cardInnerGap

                        // 标题 2 + 两行唤起方式选项
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Item {
                                Layout.preferredWidth: 24
                                Layout.minimumWidth: 24
                                Layout.maximumWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignTop

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/icon/icon/2.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTranslate("IntroductionContent", "Floating toolbar for selected text")
                                    font.family: systemFontFamily
                                    font.pointSize: cardTitleFontSize
                                    font.bold: true
                                    color: cardTitleColor
                                    padding: 0
                                    verticalAlignment: Text.AlignTop
                                    wrapMode: Text.WordWrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    RadioButton {
                                        id: windowAutoWakeRadio
                                        checked: true
                                        hoverEnabled: true
                                        text: qsTranslate("IntroductionContent", "Automatic wake-up")
                                        Layout.alignment: Qt.AlignVCenter
                                        ButtonGroup.group: floatingBarWakeButtons
                                        property bool isUpdating: false

                                        indicator: Rectangle {
                                            x: root.leftPadding
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 16
                                            height: width
                                            antialiasing: true
                                            radius: width / 2
                                            border.width: windowAutoWakeRadio.checked ? width * 0.25 : 1
                                            border.color: windowAutoWakeRadio.checked ? "#3790FA" : "#CCCCCC"
                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: parent.width * 0.5
                                                height: width
                                                antialiasing: true
                                                radius: width / 2
                                                color: root.checkedColor
                                                visible: root.checked
                                            }
                                        }

                                        contentItem: Label {
                                            text: windowAutoWakeRadio.text
                                            font.family: systemFontFamily
                                            font.pointSize: systemFontSize
                                            color: "#333333"
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: windowAutoWakeRadio.indicator.width + windowAutoWakeRadio.spacing
                                            elide: Text.ElideRight
                                        }

                                        HoverHandler {
                                            id: windowGuideAutoWakeTipHoverHandler
                                        }
                                        ToolTip.delay: 500
                                        ToolTip.visible: windowGuideAutoWakeTipHoverHandler.hovered
                                        ToolTip.text: qsTranslate("SettingSelectPage", "After selecting text, the floating bar appears automatically.")

                                        onCheckedChanged: {
                                            if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !windowShortcutWakeRadio.checked)
                                                globals.setScribeAutoInvoke(true)
                                        }
                                    }

                                    RadioButton {
                                        id: windowShortcutWakeRadio
                                        checked: false
                                        hoverEnabled: true
                                        text: qsTranslate("IntroductionContent", "Shortcut wake-up (Super + Alt + C)")
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        ButtonGroup.group: floatingBarWakeButtons
                                        property bool isUpdating: false

                                        indicator: Rectangle {
                                            x: root.leftPadding
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 16
                                            height: width
                                            antialiasing: true
                                            radius: width / 2
                                            border.width: windowShortcutWakeRadio.checked ? width * 0.25 : 1
                                            border.color: windowShortcutWakeRadio.checked ? "#3790FA" : "#CCCCCC"
                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: parent.width * 0.5
                                                height: width
                                                antialiasing: true
                                                radius: width / 2
                                                color: root.checkedColor
                                                visible: root.checked
                                            }
                                        }

                                        contentItem: Label {
                                            id: windowShortcutWakeLabel
                                            text: windowShortcutWakeRadio.text
                                            font.family: systemFontFamily
                                            font.pointSize: systemFontSize
                                            color: "#333333"
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: windowShortcutWakeRadio.indicator.width + windowShortcutWakeRadio.spacing
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }

                                        onCheckedChanged: {
                                            if (!isUpdating && checked && typeof globals !== "undefined" && globals !== null && !windowAutoWakeRadio.checked)
                                                globals.setScribeAutoInvoke(false)
                                        }

                                        TextMetrics {
                                            id: windowShortcutWakeTextMetrics
                                            font: windowShortcutWakeRadio.font
                                            text: windowShortcutWakeRadio.text
                                        }

                                        ToolTip.delay: 500
                                        ToolTip.visible: windowShortcutWakeRadio.hovered
                                        ToolTip.text: (((windowShortcutWakeLabel.width - windowShortcutWakeLabel.leftPadding) > 1)
                                            && (windowShortcutWakeTextMetrics.width > (windowShortcutWakeLabel.width - windowShortcutWakeLabel.leftPadding)))
                                            ? windowShortcutWakeRadio.text
                                            : qsTranslate("SettingSelectPage", "After selecting text, press the shortcut keys (Super + Alt + C) to display the floating bar.")
                                    }
                                }
                            }
                        }

                        Connections {
                            target: (typeof globals !== "undefined" && globals !== null) ? globals : null
                            function onSigScribeAutoInvokeChanged(autoInvokeEnabled) {
                                shortcutsRect.applyScribeAutoInvokeRadios(autoInvokeEnabled)
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Image {
                                id: windowFloatingBarIllustration
                                width: Math.min(shortcutsRect.cardIllustrationWidth, parent.width)
                                height: shortcutsRect.cardIllustrationHeight
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                source: windowAutoWakeRadio.checked
                                        ? shortcutsRect.floatingBarAutoIllustrationSource
                                        : shortcutsRect.floatingBarShortcutIllustrationSource
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
