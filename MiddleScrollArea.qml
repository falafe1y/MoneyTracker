import QtQuick

// Compatibility marker for existing page declarations. Input is handled once,
// at the window root, so it also reaches controls and modal dialogs.
Item {
    readonly property bool middleScrollViewport: true
    required property var scroller
    required property var scrollTarget
    anchors.fill: parent
}
