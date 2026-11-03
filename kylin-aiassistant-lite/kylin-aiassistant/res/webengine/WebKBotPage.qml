import QtQuick 2.12
import QtWebEngine 1.1
import QtWebChannel 1.0

Item {
    id: root

    property url pageUrl: ""
    signal pageLoaded()

    function reload() {
        taskWebView.reload()
    }

    // Do not use registeredObjects here: registerObject() avoids a Qt 5
    // WebChannel nameForObject crash observed by the embedded dashboard.
    WebChannel {
        id: taskWebChannel
        Component.onCompleted: {
            taskWebChannel.registerObject("webApiBridge", webApiBridge)
        }
    }

    WebEngineView {
        id: taskWebView
        anchors.fill: parent
        url: root.pageUrl
        settings.localContentCanAccessRemoteUrls: true
        webChannel: taskWebChannel

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                root.pageLoaded()
            }
        }

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            console.log("[WebEngine] " + sourceID + ":" + lineNumber + " " + message)
        }
    }
}
