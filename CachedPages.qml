import QtQuick

Item {
    id: host
    required property string currentKey
    required property var pages

    // Create each page on its first visit and keep its local state until this host closes.
    Repeater {
        model: host.pages
        delegate: Loader {
            required property var modelData
            readonly property bool current: host.currentKey === modelData.key
            property bool visited: false
            anchors.fill: parent
            active: visited || current
            visible: current
            enabled: current
            sourceComponent: modelData.component
            onCurrentChanged: { if (current) visited = true }
            Component.onCompleted: { if (current) visited = true }
        }
    }
}
