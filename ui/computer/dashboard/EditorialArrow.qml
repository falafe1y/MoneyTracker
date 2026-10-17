import QtQuick

Canvas {
    width: 24; height: 24
    property color ink: "#031528"
    onInkChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(width / 24, height / 24);
        ctx.strokeStyle = ink;
        ctx.lineWidth = 1.5;
        ctx.beginPath(); ctx.moveTo(6, 18); ctx.lineTo(18, 6);
        ctx.moveTo(6, 6); ctx.lineTo(18, 6); ctx.lineTo(18, 18); ctx.stroke();
    }
}
