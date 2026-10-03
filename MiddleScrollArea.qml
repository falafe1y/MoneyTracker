import QtQuick

MouseArea {
    id: area
    readonly property bool middleScrollViewport: true
    required property var scroller
    required property var scrollTarget
    anchors.fill: parent
    z: 100
    acceptedButtons: Qt.MiddleButton
    enabled: scrollTarget !== null && scrollTarget.contentHeight > scrollTarget.height
    preventStealing: true
    onPressed: function(mouse) { scroller.start(scrollTarget, area, mouse.x, mouse.y); }
    onPositionChanged: function(mouse) {
        if (scroller.active && (mouse.buttons & Qt.MiddleButton)) scroller.track(area, mouse.x, mouse.y);
    }
}
