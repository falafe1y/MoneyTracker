import QtQuick

Item {
    id: chart

    property var points: []
    property string currency: "RUB"
    property real averageMinor: 0

    onPointsChanged: canvas.requestPaint()
    onAverageMinorChanged: canvas.requestPaint()

    Canvas {
        id: canvas

        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            const data = chart.points || [];
            if (data.length === 0)
                return;
            const left = 64, right = 10, top = 10, bottom = 26;
            const plotWidth = Math.max(1, width - left - right);
            const plotHeight = Math.max(1, height - top - bottom);
            let maximum = 0;
            for (let i = 0; i < data.length; ++i)
                maximum = Math.max(maximum, Number(data[i].totalMinor));
            const paddedMaximum = Math.max(100, maximum * 1.08);
            const tickIntervals = MobileAnalytics.chartPriceLevels - 1;
            const tickStep = MobileAnalytics.niceChartStep(paddedMaximum, tickIntervals);
            maximum = tickIntervals * tickStep;

            ctx.font = "12px Commissioner";
            for (let tick = 0; tick <= tickIntervals; ++tick) {
                const y = top + tick / tickIntervals * plotHeight;
                ctx.strokeStyle = MobileTheme.line;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(left, y);
                ctx.lineTo(left + plotWidth, y);
                ctx.stroke();
                ctx.fillStyle = MobileTheme.muted;
                ctx.textAlign = "right";
                ctx.textBaseline = "middle";
                ctx.fillText(MobileAnalytics.capitalAxisMoney(maximum - tick * tickStep, chart.currency), left - 6, y);
            }

            const slot = plotWidth / data.length;
            const barWidth = Math.max(2, slot * 0.68);
            ctx.fillStyle = MobileTheme.red;
            for (let bar = 0; bar < data.length; ++bar) {
                const value = Number(data[bar].totalMinor);
                const barHeight = value / maximum * plotHeight;
                ctx.fillRect(left + bar * slot + (slot - barWidth) / 2, top + plotHeight - barHeight, barWidth, barHeight);
            }

            if (chart.averageMinor > 0) {
                const averageY = top + plotHeight - Math.min(chart.averageMinor, maximum) / maximum * plotHeight;
                ctx.save();
                ctx.strokeStyle = MobileTheme.accent;
                ctx.lineWidth = 1.5;
                ctx.setLineDash([6, 4]);
                ctx.beginPath();
                ctx.moveTo(left, averageY);
                ctx.lineTo(left + plotWidth, averageY);
                ctx.stroke();
                ctx.restore();
            }

            ctx.fillStyle = MobileTheme.muted;
            ctx.textBaseline = "top";
            ctx.textAlign = "center";
            for (let index = 0; index < data.length; ++index)
                ctx.fillText(MobileAnalytics.expenseMonthLabel(data[index].monthIndex),
                             left + (index + 0.5) * slot, top + plotHeight + 6);
        }
    }
}
