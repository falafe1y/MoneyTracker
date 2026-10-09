import QtQuick
import QtQuick.Controls

Item {
    id: controller
    property var input: windowScrollInput
    property var scrollTarget: null
    property real originX: 0
    property real originY: 0
    property real pointerX: 0
    property real pointerY: 0
    property bool active: false
    readonly property var windowOverlay: Overlay.overlay
    anchors.fill: parent
    visible: false

    function canScroll(item) {
        return item && typeof item.cancelFlick === "function"
            && (item.contentHeight > item.height || item.contentWidth > item.width);
    }

    // Follow the topmost visual branch, then use its nearest scrollable ancestor.
    // Popup contents and nested lists take priority over the page underneath.
    function targetAt(x, y, fallback) {
        function visit(item, inherited) {
            if (!item || !item.visible || !item.enabled || item.opacity <= 0) return null;
            const point = controller.mapToItem(item, x, y);
            const inside = point.x >= 0 && point.y >= 0 && point.x < item.width && point.y < item.height;
            if (item.clip && !inside) return null;
            const target = inside && controller.canScroll(item) ? item : inherited;
            const children = Array.from(item.children).filter(function(child) {
                return child && child !== controller && child !== scrollOverlay && child.middleScrollViewport !== true && child.visible !== undefined;
            }).map(function(child, index) { return {item: child, index: index}; });
            children.sort(function(a, b) { return b.item.z - a.item.z || b.index - a.index; });
            for (let i = 0; i < children.length; ++i) {
                const hit = visit(children[i].item, target);
                if (hit) return hit;
            }
            return inside ? {target: target} : null;
        }
        if (controller.windowOverlay && controller.windowOverlay.visible) {
            const popupHit = visit(controller.windowOverlay, null);
            if (popupHit) return popupHit.target;
        }
        const hit = visit(controller.parent, null);
        return hit ? hit.target : fallback;
    }

    function start(target, source, x, y) {
        const point = source.mapToItem(controller, x, y);
        target = targetAt(point.x, point.y, target);
        if (!canScroll(target)) return;
        scrollTarget = target;
        originX = point.x; originY = point.y;
        pointerX = point.x; pointerY = point.y;
        target.cancelFlick();
        active = true;
    }
    function stop() { active = false; scrollTarget = null; }
    function track(source, x, y) {
        const point = source.mapToItem(controller, x, y);
        pointerX = point.x; pointerY = point.y;
    }

    Binding { target: controller.input; property: "window"; value: controller.Window.window }
    Binding { target: controller.input; property: "active"; value: controller.active }
    Connections {
        target: controller.input
        function onMiddlePressed(position) {
            controller.start(null, controller.Window.window.contentItem, position.x, position.y);
        }
        function onPointerMoved(position) {
            controller.track(controller.Window.window.contentItem, position.x, position.y);
        }
        function onCancelled() { controller.stop(); }
    }

    Item {
        id: scrollOverlay
        parent: controller.windowOverlay && controller.windowOverlay.visible
            ? controller.windowOverlay : controller.parent
        anchors.fill: parent
        z: 1000000
        visible: controller.active
        Rectangle {
            readonly property point origin: controller.mapToItem(scrollOverlay, controller.originX, controller.originY)
            x: origin.x - width / 2; y: origin.y - height / 2
            width: 32; height: 32; radius: 16
            color: "#FFFFF8"
            border.color: "#687483"
            Text {
                anchors.centerIn: parent
                text: controller.scrollTarget && controller.scrollTarget.contentWidth > controller.scrollTarget.width
                    ? (controller.scrollTarget.contentHeight > controller.scrollTarget.height ? "+" : "↔") : "↕"
                color: "#031528"; font.pixelSize: 20
            }
        }
    }
    Timer {
        interval: 16
        repeat: true
        running: controller.active
        function advance(value, delta, top, bottom) {
            const distance = Math.max(0, Math.abs(delta) - 12);
            if (!distance) return value;
            const step = Math.min(96, distance * 0.10 + distance * distance * 0.002);
            return Math.max(top, Math.min(bottom, value + (delta < 0 ? -step : step)));
        }
        onTriggered: {
            const target = controller.scrollTarget;
            if (!target || !target.visible || !target.enabled) { controller.stop(); return; }
            if (target.contentHeight > target.height)
                target.contentY = advance(target.contentY, controller.pointerY - controller.originY,
                    target.originY, target.originY + target.contentHeight - target.height);
            if (target.contentWidth > target.width)
                target.contentX = advance(target.contentX, controller.pointerX - controller.originX,
                    target.originX, target.originX + target.contentWidth - target.width);
        }
    }
}
