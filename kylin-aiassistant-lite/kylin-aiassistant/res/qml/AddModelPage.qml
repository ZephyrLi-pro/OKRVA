import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: addModelPageRect
    color: "transparent"

    property int welcomePanelBottomMargin: 138

    signal skipClicked()
    signal previousClicked()
    signal configureClicked()

    readonly property bool isSidebarMode: windowCtrl.winMode === 2

    readonly property int welcomeTextColumnWidth: 424
    readonly property int welcomeIllustrationWidth: 496
    readonly property int welcomeIllustrationHeight: 256
    readonly property int welcomePanelGap: 32
    readonly property int welcomePanelWidth: welcomeTextColumnWidth + welcomePanelGap + welcomeIllustrationWidth
    readonly property int welcomePanelMinHeight: welcomeIllustrationHeight
    readonly property int welcomeTitleBodySpacing: 24
    readonly property int welcomeBodyNoteSpacing: 16
    readonly property color guidePageTitleColor: Qt.rgba(0, 0, 0, 0.85)
    readonly property string systemFontFamily: Qt.application.font.family
    readonly property int systemFontSize: (typeof globals !== "undefined") ? globals.fontsize : 11
    readonly property int welcomeBodyFontSize: systemFontSize + 2

    readonly property string welcomeFullText: qsTranslate("IntroductionContent",
        "Welcome to Qi Ling!<br><br>For first-time use, please go to the 「Models」 page to complete AI model configuration.<br>You can also tap 「Configure Now」 below for quick access.<br>After configuration, you can experience Qi Ling's intelligent agent capabilities and automated task features.<br><br>")

    readonly property string welcomeNoteText: qsTranslate("IntroductionContent",
        "Note:<br/>The current configuration applies only to 「Agent」 mode. For 「Quick Q&A」 mode model configuration and switching, please go to 「System Control > AI Module Management」 to configure.")

    function welcomeStyle(weight, color) {
        return "font-family:'" + systemFontFamily
                + "';font-weight:" + weight
                + ";line-height:normal;letter-spacing:0;color:" + color + ";"
    }

    function boldBookTitleTerms(text) {
        var boldStyle = welcomeStyle(700, "rgba(0,0,0,0.85)")
        return text.replace(/「([^」]+)」/g, function(match, inner) {
            return "「<span style=\"" + boldStyle + "\">" + inner + "</span>」"
        })
    }

    function welcomeGreetingText() {
        var sep = "<br><br>"
        var idx = welcomeFullText.indexOf(sep)
        return idx >= 0 ? welcomeFullText.substring(0, idx) : welcomeFullText
    }

    function welcomeBodyPlainText() {
        var sep = "<br><br>"
        var idx = welcomeFullText.indexOf(sep)
        if (idx < 0)
            return ""
        return welcomeFullText.substring(idx + sep.length).replace(/<br><br>\s*$/, "")
    }

    function buildWelcomeBodyRichText() {
        var body = welcomeBodyPlainText()
        if (body.length === 0)
            return ""
        var bodyStyle = welcomeStyle(400, "rgba(0,0,0,0.85)")
        return "<span style=\"" + bodyStyle + "\">" + boldBookTitleTerms(body) + "</span>"
    }

    function buildWelcomeNoteRichText() {
        return "<span style=\"color: rgba(0, 0, 0, 0.6)\">" + welcomeNoteText + "</span>"
    }

    readonly property int sidebarCardWidth: 354
    readonly property int sidebarIconSize: 48
    readonly property int sidebarTitleFontSize: (typeof globals !== "undefined") ? globals.fontsize + 8 : 22
    readonly property int sidebarButtonToContentGap: 64
    readonly property int sidebarIconToTitleGap: 16
    readonly property int sidebarTitleToBodyGap: 16
    readonly property int sidebarBodyToNoteGap: 12
    readonly property int sidebarNoteToImageGap: 64
    readonly property int sidebarImageWidth: 322
    readonly property int sidebarImageHeight: 166
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

                Label {
                    Layout.fillWidth: true
                    text: welcomeGreetingText()
                    font.family: systemFontFamily
                    font.pointSize: globals.fontsize + 18
                    font.weight: Font.Medium
                    color: guidePageTitleColor
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: sidebarTitleToBodyGap
                    text: buildWelcomeBodyRichText()
                    textFormat: Text.RichText
                    font.family: systemFontFamily
                    font.pointSize: welcomeBodyFontSize
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: sidebarBodyToNoteGap
                    text: buildWelcomeNoteRichText()
                    textFormat: Text.RichText
                    font.family: systemFontFamily
                    font.pointSize: systemFontSize - 1
                    horizontalAlignment: Text.AlignLeft
                    wrapMode: Text.WordWrap
                }

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: sidebarNoteToImageGap
                    source: "qrc:/icon/icon/model-add.svg"
                    sourceSize.width: sidebarImageWidth
                    sourceSize.height: sidebarImageHeight
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: sidebarButtonToContentGap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: sidebarAcceptBtn
                        text: qsTr("Skip")
                        focusPolicy: Qt.NoFocus
                        highlighted: false
                        width: guidePrimaryButtonWidth
                        height: guidePrimaryButtonHeight
                        background: Rectangle {
                            color: {
                                if (sidebarAcceptBtn.pressed)
                                    return "#B9B9B9"
                                if (sidebarAcceptBtn.hovered)
                                    return "#DCDCDC"
                                return "#E6E6E6"
                            }
                            radius: 6
                            border.width: 1
                            border.color: "transparent"
                        }
                        contentItem: Label {
                            anchors.centerIn: parent
                            text: sidebarAcceptBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#333333"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            skipClicked()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        id: sidebarPreviousBtn
                        text: qsTr("Previous")
                        focusPolicy: Qt.NoFocus
                        highlighted: false
                        width: guidePrimaryButtonWidth
                        height: guidePrimaryButtonHeight
                        background: Rectangle {
                            color: {
                                if (sidebarPreviousBtn.pressed)
                                    return "#B9B9B9"
                                if (sidebarPreviousBtn.hovered)
                                    return "#DCDCDC"
                                return "#E6E6E6"
                            }
                            radius: 6
                            border.width: 1
                            border.color: "transparent"
                        }
                        contentItem: Label {
                            anchors.centerIn: parent
                            text: sidebarPreviousBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#333333"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            previousClicked()
                        }
                    }

                    Button {
                        id: sidebarConfigureBtn
                        text: qsTr("Configure now")
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
                            text: sidebarConfigureBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            configureClicked()
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

        RowLayout {
            id: welcomePanel
            anchors.bottom: parent.bottom
            anchors.bottomMargin: welcomePanelBottomMargin
            anchors.left: parent.left
            width: Math.min(welcomePanelWidth, parent.width)
            spacing: welcomePanelGap

            ColumnLayout {
                id: textColumn
                Layout.preferredWidth: welcomeTextColumnWidth
                Layout.minimumWidth: welcomeTextColumnWidth
                Layout.maximumWidth: welcomeTextColumnWidth
                Layout.alignment: Qt.AlignTop
                spacing: 0

                Label {
                    id: welcomeTitleLabel
                    Layout.fillWidth: true
                    text: welcomeGreetingText()
                    font.family: systemFontFamily
                    font.pointSize: systemFontSize + 30
                    font.weight: Font.Medium
                    color: guidePageTitleColor
                    lineHeight: 1
                    lineHeightMode: Text.ProportionalHeight
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    id: welcomeBodyLabel
                    Layout.fillWidth: true
                    Layout.topMargin: welcomeTitleBodySpacing
                    text: buildWelcomeBodyRichText()
                    textFormat: Text.RichText
                    font.family: systemFontFamily
                    font.pointSize: welcomeBodyFontSize
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignTop
                }

                Label {
                    id: welcomeNoteLabel
                    Layout.fillWidth: true
                    Layout.topMargin: welcomeBodyNoteSpacing
                    text: buildWelcomeNoteRichText()
                    textFormat: Text.RichText
                    font.family: systemFontFamily
                    font.pointSize: systemFontSize - 1
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignTop
                }
            }

            Item {
                Layout.preferredWidth: welcomeIllustrationWidth
                Layout.minimumWidth: welcomeIllustrationWidth
                Layout.maximumWidth: welcomeIllustrationWidth
                Layout.preferredHeight: welcomeIllustrationHeight
                Layout.minimumHeight: welcomeIllustrationHeight
                Layout.maximumHeight: welcomeIllustrationHeight
                Layout.alignment: Qt.AlignTop

                Image {
                    anchors.fill: parent
                    source: "qrc:/icon/icon/model-add.svg"
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }
}
