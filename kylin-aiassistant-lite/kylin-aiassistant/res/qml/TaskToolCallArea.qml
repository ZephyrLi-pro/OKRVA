import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

Column {
    id: root
    width: parent ? parent.width : 0
    visible: toolModel.count > 0
    spacing: 8
    topPadding: 16
    bottomPadding: 8

    property string toolCallsJson: ""
    property bool expanded: true
    property bool replying: false

    signal contentChanged()

    Component.onCompleted: {
        if (!replying)
            expanded = false
        rebuild()
    }

    onReplyingChanged: {
        var target = replying
        expanded = target
        for (var i = 0; i < toolModel.count; ++i) {
            toolModel.setProperty(i, "toolExpanded", target)
            toolModel.setProperty(i, "argsExpanded", target)
            toolModel.setProperty(i, "resultExpanded", target)
        }
        contentChanged()
    }

    ListModel {
        id: toolModel
    }

    onToolCallsJsonChanged: rebuild()
    onHeightChanged: contentChanged()

    function parsePayload(value) {
        if (value === undefined || value === null || value === "")
            return {}
        if (typeof value === "object")
            return value
        try {
            return JSON.parse(value)
        } catch (e) {
            return {"raw": String(value)}
        }
    }

    function pickFirst(obj, keys, fallback) {
        for (var i = 0; i < keys.length; ++i) {
            if (obj[keys[i]] !== undefined && obj[keys[i]] !== null && obj[keys[i]] !== "")
                return obj[keys[i]]
        }
        return fallback
    }

    function stringifyBlock(value) {
        if (value === undefined || value === null)
            return ""
        if (typeof value === "string") {
            var parsed = parsePayload(value)
            if (typeof parsed === "object" && parsed.raw === undefined)
                return JSON.stringify(parsed, null, 4)
            return value
        }
        return JSON.stringify(value, null, 4)
    }

    function normalizeCall(raw) {
        var args = pickFirst(raw, ["args", "arguments", "input", "params", "parameters"], {})
        var output = pickFirst(raw, ["output", "result", "content", "message", "observation"], "")
        var name = pickFirst(raw, ["name", "tool_name", "tool", "function", "server", "id"], "")
        if (typeof name === "object")
            name = pickFirst(name, ["name"], "")
        return {
            "id": String(pickFirst(raw, ["call_id", "tool_call_id", "id", "name", "tool_name", "tool"], "")),
            "name": String(name || ""),
            "args": stringifyBlock(args),
            "output": stringifyBlock(output)
        }
    }

    function firstLine(text) {
        if (!text)
            return ""
        var s = String(text).replace(/^\s+/, "")
        var idx = s.indexOf("\n")
        return idx >= 0 ? s.substring(0, idx) : s
    }

    function arrowSource(hovered, expanded) {
        if (hovered) {
            return expanded ? "qrc:/icon/icon/arrow-up-hover.svg"
                            : "qrc:/icon/icon/arrow-down-hover.svg"
        }
        return expanded ? "qrc:/icon/icon/arrow-up.svg"
                        : "qrc:/icon/icon/arrow-down.svg"
    }

    function toolCallsTitle(count) {
        var title = qsTr("Tool calls")
        if (Qt.locale().name.indexOf("zh") === 0)
            return title + "（" + count + "）"
        return title + " (" + count + ")"
    }

    function rebuild() {
        toolModel.clear()
        var data = parsePayload(toolCallsJson)
        var calls = []
        if (Array.isArray(data)) {
            calls = data
        } else if (data.toolCalls !== undefined) {
            calls = data.toolCalls
        } else if (data.tool_calls !== undefined) {
            calls = data.tool_calls
        }

        if (!Array.isArray(calls))
            calls = []

        var merged = []
        for (var i = 0; i < calls.length; ++i) {
            var call = normalizeCall(calls[i])
            var targetIndex = -1
            for (var j = 0; j < merged.length; ++j) {
                if (call.id !== "" && merged[j].id === call.id) {
                    targetIndex = j
                    break
                }
            }

            if (targetIndex < 0) {
                merged.push(call)
            } else {
                if (call.name !== "")
                    merged[targetIndex].name = call.name
                if (call.args !== "" && call.args !== "{}")
                    merged[targetIndex].args = call.args
                if (call.output !== "")
                    merged[targetIndex].output = call.output
            }
        }

        for (var k = 0; k < merged.length; ++k) {
            toolModel.append({
                "name": merged[k].name,
                "args": merged[k].args,
                "output": merged[k].output,
                "toolExpanded": root.replying,
                "argsExpanded": root.replying,
                "resultExpanded": root.replying
            })
        }
        contentChanged()
    }

    Row {
        id: headerRow
        width: parent.width
        height: 20
        leftPadding: 16
        rightPadding: 16
        spacing: 4

        property bool hovered: false
        property bool pressed: false
        property color activeColor: pressed ? "#2C73C8" : "#3489ED"

        Rectangle {
            width: 20
            height: 20
            color: "transparent"

            Image {
                id: funcIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "qrc:/icon/icon/func.svg"
            }

            ColorOverlay {
                anchors.fill: funcIcon
                source: funcIcon
                visible: headerRow.hovered || headerRow.pressed
                color: headerRow.activeColor
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: headerRow.hovered = true
                onExited: headerRow.hovered = false
                onPressed: headerRow.pressed = true
                onReleased: headerRow.pressed = false
                onCanceled: headerRow.pressed = false
                onClicked: {
                    root.expanded = !root.expanded
                    root.contentChanged()
                }
            }
        }

        Label {
            id: headerLabel
            height: headerRow.height
            text: root.toolCallsTitle(toolModel.count)
            color: headerRow.pressed ? "#2C73C8" : (headerRow.hovered ? "#3489ED" : "#262626")
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: headerRow.hovered = true
                onExited: headerRow.hovered = false
                onPressed: headerRow.pressed = true
                onReleased: headerRow.pressed = false
                onCanceled: headerRow.pressed = false
                onClicked: {
                    root.expanded = !root.expanded
                    root.contentChanged()
                }
            }
        }

        Rectangle {
            width: 20
            height: 20
            color: "transparent"

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: root.arrowSource(headerRow.hovered, root.expanded)
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: headerRow.hovered = true
                onExited: headerRow.hovered = false
                onPressed: headerRow.pressed = true
                onReleased: headerRow.pressed = false
                onCanceled: headerRow.pressed = false
                onClicked: {
                    root.expanded = !root.expanded
                    root.contentChanged()
                }
            }
        }
    }

    Repeater {
        model: toolModel
        delegate: Column {
            id: toolDelegate
            width: root.width
            visible: root.expanded
            spacing: 6

            Row {
                id: nameRow
                width: parent.width
                height: 20
                leftPadding: 16
                rightPadding: 16
                spacing: 4

                property bool hovered: false

                Label {
                    id: nameLabel
                    height: nameRow.height
                    text: name
                    color: nameRow.hovered ? "#3489ED" : "#262626"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth,
                                    nameRow.width - nameRow.leftPadding - nameRow.rightPadding - 20 - nameRow.spacing)

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: nameRow.hovered = true
                        onExited: nameRow.hovered = false
                        onClicked: {
                            toolModel.setProperty(index, "toolExpanded", !toolExpanded)
                            root.contentChanged()
                        }
                    }
                }

                Rectangle {
                    width: 20
                    height: 20
                    color: "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: root.arrowSource(nameRow.hovered, toolExpanded)
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: nameRow.hovered = true
                        onExited: nameRow.hovered = false
                        onClicked: {
                            toolModel.setProperty(index, "toolExpanded", !toolExpanded)
                            root.contentChanged()
                        }
                    }
                }
            }

            Row {
                width: parent.width
                visible: toolExpanded
                height: visible ? innerColumn.height : 0
                leftPadding: 16
                rightPadding: 16

                Rectangle {
                    width: parent.width - 32
                    height: innerColumn.height
                    color: "transparent"

                    Rectangle {
                        id: blockquoteBar
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: "#0d262626"
                    }

                    Column {
                        id: innerColumn
                        width: parent.width
                        spacing: 6

                        TaskToolCodeSection {
                            width: parent.width
                            title: "args"
                            codeText: args
                            expanded: argsExpanded
                            onToggleRequested: {
                                toolModel.setProperty(index, "argsExpanded", !argsExpanded)
                                root.contentChanged()
                            }
                        }

                        TaskToolCodeSection {
                            width: parent.width
                            visible: output !== ""
                            title: root.firstLine(output)
                            codeText: output
                            expanded: resultExpanded
                            onToggleRequested: {
                                toolModel.setProperty(index, "resultExpanded", !resultExpanded)
                                root.contentChanged()
                            }
                        }
                    }
                }
            }
        }
    }
}
