import QtQuick

Canvas {
    id: icon

    property string name: ""
    property color color: MobileTheme.accent
    property real strokeWidth: 2

    implicitWidth: 22
    implicitHeight: 22

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function trace(ctx, s, points, closed) {
        ctx.beginPath();
        ctx.moveTo(points[0] * s, points[1] * s);
        for (let i = 2; i < points.length; i += 2)
            ctx.lineTo(points[i] * s, points[i + 1] * s);
        if (closed)
            ctx.closePath();
        ctx.stroke();
    }

    function dot(ctx, s, x, y, r) {
        ctx.beginPath();
        ctx.arc(x * s, y * s, r * s, 0, 2 * Math.PI);
        ctx.fill();
    }

    function ring(ctx, s, x, y, r) {
        ctx.beginPath();
        ctx.arc(x * s, y * s, r * s, 0, 2 * Math.PI);
        ctx.stroke();
    }

    onPaint: {
        const ctx = getContext("2d");
        const s = width / 24;
        ctx.clearRect(0, 0, width, height);
        ctx.strokeStyle = icon.color;
        ctx.fillStyle = icon.color;
        ctx.lineWidth = icon.strokeWidth * s;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";

        switch (icon.name) {
        case "home":
            trace(ctx, s, [3, 10.5, 12, 3, 21, 10.5, 21, 20, 3, 20], true);
            trace(ctx, s, [9, 20, 9, 14, 15, 14, 15, 20], false);
            break;
        case "wallet":
            trace(ctx, s, [3, 6, 21, 6, 21, 19, 3, 19], true);
            trace(ctx, s, [3, 10, 21, 10], false);
            dot(ctx, s, 16.5, 14.5, 1.1);
            break;
        case "list":
            trace(ctx, s, [9, 6, 20, 6], false);
            trace(ctx, s, [9, 12, 20, 12], false);
            trace(ctx, s, [9, 18, 20, 18], false);
            dot(ctx, s, 4.5, 6, 1);
            dot(ctx, s, 4.5, 12, 1);
            dot(ctx, s, 4.5, 18, 1);
            break;
        case "chart":
            trace(ctx, s, [4, 20, 20, 20], false);
            trace(ctx, s, [7, 16, 7, 12], false);
            trace(ctx, s, [12, 16, 12, 8], false);
            trace(ctx, s, [17, 16, 17, 5], false);
            break;
        case "more":
            dot(ctx, s, 5, 12, 1.5);
            dot(ctx, s, 12, 12, 1.5);
            dot(ctx, s, 19, 12, 1.5);
            break;
        case "search":
            ring(ctx, s, 10.5, 10.5, 6.5);
            trace(ctx, s, [15.5, 15.5, 20, 20], false);
            break;
        case "plus":
            trace(ctx, s, [12, 5, 12, 19], false);
            trace(ctx, s, [5, 12, 19, 12], false);
            break;
        case "close":
            trace(ctx, s, [6, 6, 18, 18], false);
            trace(ctx, s, [18, 6, 6, 18], false);
            break;
        case "check":
            trace(ctx, s, [5, 12.5, 9.5, 17, 19, 7.5], false);
            break;
        case "chevronRight":
            trace(ctx, s, [9, 6, 15, 12, 9, 18], false);
            break;
        case "chevronLeft":
            trace(ctx, s, [15, 6, 9, 12, 15, 18], false);
            break;
        case "chevronDown":
            trace(ctx, s, [6, 9, 12, 15, 18, 9], false);
            break;
        case "budget":
            ring(ctx, s, 12, 12, 8);
            trace(ctx, s, [12, 12, 12, 4], false);
            trace(ctx, s, [12, 12, 19.5, 12], false);
            break;
        case "goal":
            ring(ctx, s, 12, 12, 8);
            ring(ctx, s, 12, 12, 4);
            dot(ctx, s, 12, 12, 1);
            break;
        case "trend":
            trace(ctx, s, [3, 17, 9, 11, 13, 14, 21, 6], false);
            trace(ctx, s, [15, 6, 21, 6, 21, 12], false);
            break;
        case "folder":
            trace(ctx, s, [3, 7, 9, 7, 11, 9, 21, 9, 21, 19, 3, 19], true);
            break;
        case "note":
            trace(ctx, s, [6, 3, 14, 3, 18, 7, 18, 21, 6, 21], true);
            trace(ctx, s, [9, 11, 15, 11], false);
            trace(ctx, s, [9, 15, 15, 15], false);
            break;
        case "tag":
            trace(ctx, s, [4, 4, 10, 4, 10, 10, 4, 10], true);
            trace(ctx, s, [14, 4, 20, 4, 20, 10, 14, 10], true);
            trace(ctx, s, [4, 14, 10, 14, 10, 20, 4, 20], true);
            trace(ctx, s, [14, 14, 20, 14, 20, 20, 14, 20], true);
            break;
        case "sliders":
            trace(ctx, s, [4, 7, 20, 7], false);
            trace(ctx, s, [4, 12, 20, 12], false);
            trace(ctx, s, [4, 17, 20, 17], false);
            dot(ctx, s, 9, 7, 2.2);
            dot(ctx, s, 15, 12, 2.2);
            dot(ctx, s, 8, 17, 2.2);
            break;
        case "trash":
            trace(ctx, s, [4, 7, 20, 7], false);
            trace(ctx, s, [9, 7, 9, 4, 15, 4, 15, 7], false);
            trace(ctx, s, [6, 7, 7, 20, 17, 20, 18, 7], false);
            break;
        case "calendar":
            trace(ctx, s, [4, 6, 20, 6, 20, 20, 4, 20], true);
            trace(ctx, s, [4, 10, 20, 10], false);
            trace(ctx, s, [8, 4, 8, 8], false);
            trace(ctx, s, [16, 4, 16, 8], false);
            break;
        case "filter":
            trace(ctx, s, [3, 5, 21, 5, 14, 13, 14, 19, 10, 21, 10, 13], true);
            break;
        case "eye":
            trace(ctx, s, [2, 12, 12, 6, 22, 12, 12, 18], true);
            ring(ctx, s, 12, 12, 3);
            break;
        case "eyeOff":
            trace(ctx, s, [2, 12, 12, 6, 22, 12, 12, 18], true);
            ring(ctx, s, 12, 12, 3);
            trace(ctx, s, [3, 3, 21, 21], false);
            break;
        case "arrowIn":
            trace(ctx, s, [12, 4, 12, 16], false);
            trace(ctx, s, [7, 11, 12, 16, 17, 11], false);
            break;
        case "arrowOut":
            trace(ctx, s, [12, 20, 12, 8], false);
            trace(ctx, s, [7, 13, 12, 8, 17, 13], false);
            break;
        case "swap":
            trace(ctx, s, [4, 8, 18, 8], false);
            trace(ctx, s, [15, 5, 18, 8, 15, 11], false);
            trace(ctx, s, [20, 16, 6, 16], false);
            trace(ctx, s, [9, 13, 6, 16, 9, 19], false);
            break;
        }
    }
}
