import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: panel

    property var risk: ({})

    readonly property var points: risk.drawdowns || []
    readonly property var xFractions: points.map(function(row, index) {
        return points.length > 1 ? index / (points.length - 1) : 0.5;
    })
    readonly property var chartSeries: [
        { values: points.map(function(row) { return -Number(row.percent); }), color: MobileTheme.red, width: 2 }
    ]

    function available(value) {
        return value !== undefined && value !== null && isFinite(Number(value));
    }
    function percent(value) {
        return available(value) ? Number(value).toLocaleString(MobileFormat.locale(), "f", 2) + " %" : "—";
    }
    function date(value) {
        return value ? value.substr(8, 2) + "." + value.substr(5, 2) + "." + value.substr(0, 4) : "—";
    }

    spacing: 12

    Text {
        Layout.fillWidth: true
        text: qsTr("Колебания и просадки")
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeHeading
        font.weight: Font.DemiBold
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 10

        Repeater {
            model: [
                { title: qsTr("Годовая волатильность"), value: panel.percent(panel.risk.annualVolatilityPercent), hint: qsTr("Разброс дневной доходности, приведённый к году"), metric: "volatility" },
                { title: qsTr("Максимальная просадка"), value: panel.percent(panel.risk.maximumDrawdownPercent), hint: panel.risk.peakDate ? panel.date(panel.risk.peakDate) + " → " + panel.date(panel.risk.troughDate) : qsTr("Падение от предыдущего максимума за выбранный период"), metric: "maximum_drawdown" },
                { title: qsTr("Просадка на конец периода"), value: panel.percent(panel.risk.currentDrawdownPercent), hint: qsTr("Ниже максимума, достигнутого в этом периоде"), metric: "current_drawdown" },
                { title: qsTr("Лучший день"), value: panel.percent(panel.risk.bestDayPercent), hint: panel.date(panel.risk.bestDay), metric: "best_day" },
                { title: qsTr("Худший день"), value: panel.percent(panel.risk.worstDayPercent), hint: panel.date(panel.risk.worstDay), metric: "worst_day" },
                { title: qsTr("Дней доходности в расчёте"), value: panel.risk.available ? String(panel.risk.returnDays || 0) : "—", hint: qsTr("Дни, когда была положительная база для расчёта"), metric: "risk_days" }
            ]

            delegate: MobileCard {
                required property var modelData

                Layout.fillWidth: true
                cardPadding: 12
                gap: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: modelData.title
                        wrapMode: Text.Wrap
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeCaption
                    }
                    MobileInfoButton {
                        metric: modelData.metric
                        displayValue: modelData.value
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.value
                    elide: Text.ElideRight
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeTitle
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.hint
                    wrapMode: Text.Wrap
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        text: !panel.risk.available
            ? qsTr("Для расчёта нужна непрерывная история оценок и известные денежные потоки. Причины пропусков указаны в пояснениях к расчёту.")
            : !panel.available(panel.risk.annualVolatilityPercent)
                ? qsTr("Для волатильности нужны минимум два дня доходности — три дневные оценки. Просадки и лучший/худший день уже доступны.")
                : qsTr("Выборочное отклонение дневной доходности × √365,25. Дни с нулевым капиталом и без движений исключены из волатильности. Короткая история даёт менее устойчивую оценку.")
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.risk.available && panel.risk.maximumDrawdownPercent > 0
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        text: panel.risk.recoveryDate
            ? qsTr("Максимальная просадка восстановилась: %1").arg(panel.date(panel.risk.recoveryDate))
            : qsTr("Максимальная просадка не восстановилась к концу выбранного периода.")
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Просадка относительно предыдущего максимума")
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeBody
        font.weight: Font.DemiBold
    }

    MobileCard {
        visible: panel.points.length >= 2
        Layout.fillWidth: true
        gap: 6

        MobileSeriesChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            series: panel.chartSeries
            xFractions: panel.xFractions
            xLabels: [
                { fraction: 0, text: panel.date(panel.points.length ? panel.points[0].date : ""), align: "left" },
                { fraction: 1, text: panel.date(panel.points.length ? panel.points[panel.points.length - 1].date : ""), align: "right" }
            ]
            tooltips: panel.points.map(function(row) {
                return panel.date(row.date) + " · " + panel.percent(row.percent);
            })
            formatValue: function(value) { return Number(value).toLocaleString(MobileFormat.locale(), "f", 1) + " %"; }
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        text: qsTr("Просадки считаются по связанной доходности с учётом пополнений и выводов. Это падение инвестиционного результата, а не денежного остатка. Начальный максимум задан на начало выбранного периода; оценка текущего дня может быть неполной.")
    }
}
