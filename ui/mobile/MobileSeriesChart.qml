import QtQuick

Item {
    id: chart

    property var series: []
    property var xFractions: []
    property var xLabels: []
    property var tooltips: []
    property var formatValue: function(value) { return String(Math.round(value)); }
    property int selectedIndex: -1

    readonly property real leftMargin: 64
    readonly property real rightMargin: 10
    readonly property real topMargin: 10
    readonly property real bottomMargin: 26
    readonly property var axis: computeAxis()

    function computeAxis() {
        let minimum = Infinity;
        let maximum = -Infinity;
        for (let s = 0; s < series.length; ++s) {
            const values = series[s].values || [];
            for (let i = 0; i < values.length; ++i) {
                const value = Number(values[i]);
                if (Number.isFinite(value)) {
                    minimum = Math.min(minimum, value);
                    maximum = Math.max(maximum, value);
                }
            }
        }
        if (!Number.isFinite(minimum))
            return { minimum: 0, maximum: 1, step: 0.2, intervals: 5 };
        return MobileAnalytics.niceChartAxis(minimum, maximum);
    }

    function selectFromX(x) {
        const fractions = chart.xFractions || [];
        if (fractions.length === 0) {
            chart.selectedIndex = -1;
            return;
        }
        const plotWidth = Math.max(1, chart.width - chart.leftMargin - chart.rightMargin);
        const fraction = Math.max(0, Math.min(1, (x - chart.leftMargin) / plotWidth));
        let best = 0;
        let distance = Infinity;
        for (let i = 0; i < fractions.length; ++i) {
            const delta = Math.abs(fractions[i] - fraction);
            if (delta < distance) {
                distance = delta;
                best = i;
            }
        }
        chart.selectedIndex = best;
    }

    onSeriesChanged: canvas.requestPaint()
    onXFractionsChanged: canvas.requestPaint()
    onXLabelsChanged: canvas.requestPaint()
    onSelectedIndexChanged: canvas.requestPaint()

    Canvas {
        id: canvas

        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            const plotWidth = Math.max(1, width - chart.leftMargin - chart.rightMargin);
            const plotHeight = Math.max(1, height - chart.topMargin - chart.bottomMargin);
            const axis = chart.axis;
            const range = Math.max(1e-9, axis.maximum - axis.minimum);
            const yFor = function(value) {
                return chart.topMargin + (axis.maximum - value) / range * plotHeight;
            };
            const xFor = function(index) {
                return chart.leftMargin + (chart.xFractions[index] || 0) * plotWidth;
            };

            ctx.font = "12px Commissioner";
            ctx.lineWidth = 1;
            for (let tick = 0; tick <= axis.intervals; ++tick) {
                const y = chart.topMargin + tick / axis.intervals * plotHeight;
                ctx.strokeStyle = MobileTheme.line;
                ctx.beginPath();
                ctx.moveTo(chart.leftMargin, y);
                ctx.lineTo(chart.leftMargin + plotWidth, y);
                ctx.stroke();
                ctx.fillStyle = MobileTheme.muted;
                ctx.textAlign = "right";
                ctx.textBaseline = "middle";
                ctx.fillText(chart.formatValue(axis.maximum - tick * axis.step), chart.leftMargin - 6, y);
            }

            for (let label = 0; label < chart.xLabels.length; ++label) {
                const item = chart.xLabels[label];
                ctx.fillStyle = MobileTheme.muted;
                ctx.textAlign = item.align || "center";
                ctx.textBaseline = "top";
                ctx.fillText(item.text, chart.leftMargin + item.fraction * plotWidth, chart.topMargin + plotHeight + 6);
            }

            for (let s = 0; s < chart.series.length; ++s) {
                const line = chart.series[s];
                const values = line.values || [];
                if (values.length === 0)
                    continue;
                ctx.beginPath();
                ctx.strokeStyle = line.color;
                ctx.lineWidth = line.width || 2;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";
                ctx.setLineDash(line.dashed ? [6, 4] : []);
                for (let i = 0; i < values.length; ++i) {
                    const x = xFor(i);
                    const y = yFor(Number(values[i]));
                    if (i === 0)
                        ctx.moveTo(x, y);
                    else
                        ctx.lineTo(x, y);
                }
                ctx.stroke();
                ctx.setLineDash([]);
            }

            if (chart.selectedIndex >= 0 && chart.selectedIndex < chart.xFractions.length) {
                const selectedX = xFor(chart.selectedIndex);
                ctx.strokeStyle = MobileTheme.muted;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(selectedX, chart.topMargin);
                ctx.lineTo(selectedX, chart.topMargin + plotHeight);
                ctx.stroke();
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) { chart.selectFromX(mouse.x); }
        onPositionChanged: function(mouse) { chart.selectFromX(mouse.x); }
        onReleased: chart.selectedIndex = -1
        onCanceled: chart.selectedIndex = -1
    }

    Rectangle {
        visible: chart.selectedIndex >= 0 && chart.selectedIndex < chart.tooltips.length
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: tooltipText.implicitWidth + 16
        height: tooltipText.implicitHeight + 10
        radius: 8
        color: MobileTheme.panel
        border.width: 1
        border.color: MobileTheme.accent

        Text {
            id: tooltipText
            anchors.centerIn: parent
            text: chart.selectedIndex >= 0 && chart.selectedIndex < chart.tooltips.length ? chart.tooltips[chart.selectedIndex] : ""
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeCaption
        }
    }
}
