import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: agreementPageRect
    color: "transparent"

    signal agreeClicked()
    signal disagreeClicked()

    readonly property bool isSidebarMode: windowCtrl.winMode === 2

    readonly property int agreementPanelWidth: 952
    readonly property int agreementPanelHeight: 400
    readonly property int agreementPanelPaddingTop: 24
    readonly property int agreementPanelPaddingRight: 0
    readonly property int agreementPanelPaddingBottom: 24
    readonly property int agreementPanelPaddingLeft: 24
    readonly property int agreementPanelContentGap: 4

    readonly property color guidePageTitleColor: Qt.rgba(0, 0, 0, 0.85)
    readonly property string systemFontFamily: Qt.application.font.family

    readonly property int titleToContentGap: 36

    readonly property int sidebarPanelWidth: 354
    readonly property int sidebarPanelHeight: 466
    readonly property int sidebarIconSize: 48
    readonly property int sidebarTitleFontSize: (typeof globals !== "undefined") ? globals.fontsize + 8 : 22
    readonly property int sidebarButtonToPanelGap: 36
    readonly property int sidebarIconToTitleGap: 16
    readonly property int sidebarTitleToPanelGap: 24

    readonly property real systemFontSize: (typeof globals !== "undefined") ? globals.fontsize : 11
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
            width: sidebarPanelWidth
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
                    text: qsTranslate("IntroductionContent", "Beginner's Guide / User Agreement")
                    font.family: systemFontFamily
                    font.weight: Font.Medium
                    font.pointSize: sidebarTitleFontSize
                    color: guidePageTitleColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: sidebarTitleToPanelGap
                    width: sidebarPanelWidth
                    height: sidebarPanelHeight
                    color: "#F6F6F6"
                    radius: 6
                    clip: true

                    function loadHtmlFile(fileUrl) {
                        var htmlFile = new XMLHttpRequest();
                        htmlFile.open("GET", fileUrl);
                        htmlFile.onreadystatechange = function() {
                            if (htmlFile.readyState === XMLHttpRequest.DONE) {
                                if (htmlFile.status === 200 || htmlFile.status === 0) {
                                    sidebarAgreementContentText.text = htmlFile.responseText;
                                } else {
                                    console.error("Failed to load HTML file:", htmlFile.statusText);
                                }
                            }
                        };
                        htmlFile.send();
                    }

                    Component.onCompleted: {
                        loadHtmlFile("qrc:/html/html/user-agreement.html");
                    }

                    Flickable {
                        id: sidebarAgreementContentScroll
                        anchors.fill: parent
                        anchors.margins: 16
                        clip: true
                        contentWidth: width
                        contentHeight: sidebarAgreementContentText.height
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            id: sidebarAgreementScrollBar
                            policy: ScrollBar.AlwaysOn
                        }

                        Text {
                            id: sidebarAgreementContentText
                            width: sidebarAgreementContentScroll.width - 12
                            wrapMode: Text.WordWrap
                            textFormat: Text.RichText
                            font.pointSize: globals.fontsize - 2
                            color: "#262626"
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: sidebarButtonToPanelGap
                }

                Row {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    Button {
                        id: sidebarDisagreeBtn
                        text: qsTr("disagree")
                        focusPolicy: Qt.NoFocus
                        highlighted: false
                        width: guidePrimaryButtonWidth
                        height: guidePrimaryButtonHeight
                        background: Rectangle {
                            color: {
                                if (sidebarDisagreeBtn.pressed)
                                    return "#B9B9B9"
                                if (sidebarDisagreeBtn.hovered)
                                    return "#DCDCDC"
                                return "#E6E6E6"
                            }
                            radius: 6
                            border.width: 1
                            border.color: "transparent"
                        }
                        contentItem: Label {
                            anchors.centerIn: parent
                            text: sidebarDisagreeBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#333333"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            disagreeClicked()
                        }
                    }

                    Button {
                        id: sidebarAgreeBtn
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
                            anchors.centerIn: parent
                            text: sidebarAgreeBtn.text
                            font.pointSize: systemFontSize
                            font.family: systemFontFamily
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        onClicked: {
                            agreeClicked()
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

            // 标题：灯泡 48×48 + 文案（与新手引导/使用技巧页一致）
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: agreementRow.height
                Layout.maximumHeight: agreementRow.height

                Row {
                    id: agreementRow
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 0
                    bottomPadding: titleToContentGap

                    Row {
                        id: agreementHeaderRow
                        spacing: 12
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0

                        Image {
                            id: agreementHeaderRowImage
                            verticalAlignment: Text.AlignVCenter
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
                            id: agreementTitle
                            text: qsTranslate("IntroductionContent", "Beginner's Guide / User Agreement")
                            font.family: systemFontFamily
                            font.weight: Font.Medium
                            font.pointSize: (typeof globals !== "undefined") ? globals.fontsize + 16 : 30
                            color: guidePageTitleColor
                            lineHeight: font.pixelSize
                            lineHeightMode: Text.FixedHeight
                            padding: 0
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                            anchors.verticalCenter: parent.verticalCenter
                            Layout.maximumWidth: Math.max(agreementPageRect.width - 48 - 12 - 32, 100)
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: agreementPanelHeight
                Layout.maximumHeight: agreementPanelHeight

                Item {
                    id: agreementPanelHost
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(agreementPanelWidth, parent.width)
                    height: agreementPanelHeight

                    Rectangle {
                        id: agreementPanel
                        anchors.fill: parent
                        z: 2
                        color: "#F6F6F6"
                        radius: 6
                        clip: true

                        function loadHtmlFile(fileUrl) {
                            var htmlFile = new XMLHttpRequest();
                            htmlFile.open("GET", fileUrl);
                            htmlFile.onreadystatechange = function() {
                                if (htmlFile.readyState === XMLHttpRequest.DONE) {
                                    if (htmlFile.status === 200 || htmlFile.status === 0) {
                                        agreementContentText.text = htmlFile.responseText;
                                    } else {
                                        console.error("Failed to load HTML file:", htmlFile.statusText);

                                    }
                                }
                            };
                            htmlFile.send();
                        }

                        Component.onCompleted: {
                            loadHtmlFile("qrc:/html/html/user-agreement.html");
                            console.log(
                                "row:", agreementHeaderRow.height,
                                "image:", agreementHeaderRowImage.width, agreementHeaderRowImage.height,
                                "title:", agreementTitle.width, agreementTitle.height,
                                "title padding:", agreementTitle.topPadding, agreementTitle.bottomPadding,
                                "fontSize:", globals.fontsize, "pointSize:", agreementTitle.font.pointSize
                            )
                        }

                        Flickable {
                            id: agreementContentScroll
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.topMargin: agreementPanelPaddingTop
                            anchors.bottomMargin: agreementPanelPaddingBottom
                            anchors.leftMargin: agreementPanelPaddingLeft
                            anchors.rightMargin: agreementPanelPaddingRight
                            clip: true
                            contentWidth: width
                            contentHeight: agreementContentText.height
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {
                                id: agreementScrollBar
                                policy: ScrollBar.AlwaysOn
                            }

                            Text {
                                id: agreementContentText
                                width: Math.max(0, agreementContentScroll.width - 12)
                                x: 0
                                y: 0
                                height: implicitHeight
                                wrapMode: Text.WordWrap
                                textFormat: Text.RichText
                                font.pointSize: globals.fontsize - 2
                                color: "#262626"
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
