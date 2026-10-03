import QtQuick

Item {
    id: controller
    property var scrollTarget: null
    property real originX: 0
    property real originY: 0
    property real pointerY: 0
    property bool active: false
    anchors.fill: parent
    z: 10000
    visible: active

    // Nested lists take priority over the surrounding page's scroll surface.
    function targetAt(x, y, fallback) {
        let chosen = fallback;
        let chosenDepth = -1;
        function visit(item, depth) {
            if (!item.visible) return;
            const point = controller.mapToItem(item, x, y);
            const inside = point.x >= 0 && point.y >= 0
                        && point.x < item.width && point.y < item.height;
            if (item.clip && !inside) return;
            if (inside && item.middleScrollViewport === true && item.enabled
                && item.scroller === controller && depth > chosenDepth) {
                chosen = item.scrollTarget;
                chosenDepth = depth;
            }
            for (let i = 0; i < item.children.length; ++i) visit(item.children[i], depth + 1);
        }
        visit(controller.parent, 0);
        return chosen;
    }
    function start(target, source, x, y) {
        if (!target || target.contentHeight <= target.height) return;
        const point = source.mapToItem(controller, x, y);
        target = targetAt(point.x, point.y, target);
        scrollTarget = target;
        originX = point.x; originY = point.y; pointerY = point.y;
        target.cancelFlick();
        active = true;
    }
    function stop() { active = false; scrollTarget = null; }
    function track(source, x, y) {
        pointerY = source.mapToItem(controller, x, y).y;
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.SizeVerCursor
        onPositionChanged: function(mouse) { controller.pointerY = mouse.y; }
        onPressed: controller.stop()
        onWheel: function(wheel) { controller.stop(); wheel.accepted = false; }
    }
    Rectangle {
        x: controller.originX - width / 2
        y: controller.originY - height / 2
        width: 32; height: 32; radius: 16
        color: "#FFFFF8"
        border.color: "#687483"
        Text { anchors.centerIn: parent; text: "↕"; color: "#031528"; font.pixelSize: 20 }
    }
    Timer {
        interval: 16
        repeat: true
        running: controller.active
        onTriggered: {
            const target = controller.scrollTarget;
            if (!target || !target.visible) { controller.stop(); return; }
            const delta = controller.pointerY - controller.originY;
            const distance = Math.max(0, Math.abs(delta) - 12);
            if (!distance) return;
            const step = Math.min(96, distance * 0.10 + distance * distance * 0.002);
            const top = target.originY;
            const bottom = Math.max(top, top + target.contentHeight - target.height);
            target.contentY = Math.max(top, Math.min(bottom,
                target.contentY + (delta < 0 ? -step : step)));
        }
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: controller.active
        onActivated: controller.stop()
    }
}
