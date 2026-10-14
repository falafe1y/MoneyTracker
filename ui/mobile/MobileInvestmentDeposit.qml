import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: panel

    readonly property var settings: financeController.performanceDeposit || ({})
    readonly property var comparison: (financeController.investmentPerformance || ({})).depositComparison || ({})
    readonly property var points: comparison.available ? (comparison.series || []) : []
    readonly property var modes: ["none", "monthly", "daily"]
    readonly property var xFractions: points.map(function(row, index) {
        return points.length > 1 ? index / (points.length - 1) : 0.5;
    })
    readonly property var chartSeries: [
        { values: points.map(function(row) { return Number(row.portfolioMinor); }), color: MobileTheme.accent, width: 2 },
        { values: points.map(function(row) { return Number(row.depositMinor); }), color: MobileTheme.income, width: 2 }
    ]

    function amount(value) {
        return value === null || value === undefined ? "—" : MobileFormat.money(value, comparison.currency || "RUB", false);
    }
    function date(value) {
        return value ? Qt.formatDate(new Date(value + "T12:00:00"), "dd.MM.yyyy") : "—";
    }

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: qsTr("А если бы деньги лежали на вкладе?")
            wrapMode: Text.Wrap
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeHeading
            font.weight: Font.DemiBold
        }
        MobileInfoButton { metric: "deposit" }
    }

    MobileField {
        id: rate
        Layout.fillWidth: true
        label: qsTr("Годовая ставка, %")
        placeholder: qsTr("Укажите ставку")
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        text: panel.settings.rate || ""
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Капитализация процентов")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        font.weight: Font.Medium
    }
    MobileSegmented {
        id: capitalization
        Layout.fillWidth: true
        options: [
            { label: qsTr("Без капитализации"), value: "none" },
            { label: qsTr("Ежемесячная"), value: "monthly" },
            { label: qsTr("Ежедневная"), value: "daily" }
        ]
        value: panel.settings.capitalization || "monthly"
        onActivated: function(chosen) { capitalization.value = chosen; }
    }

    MobileButton {
        Layout.fillWidth: true
        variant: "primary"
        text: qsTr("Рассчитать")
        onClicked: {
            const ok = financeController.setPerformanceDeposit(rate.text, capitalization.value);
            depositError.text = ok ? "" : qsTr("Введите ставку от 0 до 1000 %, до четырёх знаков после запятой.");
        }
    }

    Text {
        id: depositError
        Layout.fillWidth: true
        visible: text.length > 0
        color: MobileTheme.red
        wrapMode: Text.Wrap
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Валюта отчёта: %1").arg(panel.comparison.currency || "RUB")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        wrapMode: Text.Wrap
    }

    Text {
        Layout.fillWidth: true
        visible: !panel.comparison.available
        text: panel.comparison.reason || qsTr("Укажите ставку и накопите дневные оценки выбранных счетов.")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        wrapMode: Text.Wrap
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.comparison.available
        text: qsTr("%1 — %2 · Начальный капитал: %3")
            .arg(panel.date(panel.comparison.from)).arg(panel.date(panel.comparison.to))
            .arg(panel.amount(panel.comparison.openingMinor))
            + (panel.comparison.liveDay ? "\n" + qsTr("Оценка текущего дня ещё может измениться.") : "")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        wrapMode: Text.Wrap
    }

    GridLayout {
        visible: !!panel.comparison.available
        Layout.fillWidth: true
        columns: 1
        rowSpacing: 10

        Repeater {
            model: [
                { title: qsTr("Результат вложений"), value: panel.comparison.portfolioGainMinor },
                { title: qsTr("Доход условного вклада"), value: panel.comparison.depositGainMinor },
                { title: qsTr("Разница результатов"), value: panel.comparison.differenceMinor }
            ]

            delegate: MobileCard {
                required property var modelData

                Layout.fillWidth: true
                cardPadding: 12
                gap: 4

                Text {
                    Layout.fillWidth: true
                    text: modelData.title
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    text: panel.amount(modelData.value)
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeTitle
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
            }
        }
    }

    MobileCard {
        visible: !!panel.comparison.available && panel.points.length >= 2
        Layout.fillWidth: true
        gap: 6

        MobileSeriesChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 230
            series: panel.chartSeries
            xFractions: panel.xFractions
            xLabels: [
                { fraction: 0, text: panel.date(panel.comparison.from), align: "left" },
                { fraction: 1, text: panel.date(panel.comparison.to), align: "right" }
            ]
            tooltips: panel.points.map(function(row) {
                return panel.date(row.date) + "\n" + qsTr("Вложения: %1").arg(panel.amount(row.portfolioMinor))
                    + "\n" + qsTr("Вклад: %1").arg(panel.amount(row.depositMinor));
            })
            formatValue: function(value) { return MobileAnalytics.capitalAxisMoney(value, panel.comparison.currency || "RUB"); }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { implicitWidth: 20; implicitHeight: 3; color: MobileTheme.accent }
            Text { text: qsTr("Вложения"); color: MobileTheme.accent; font.pixelSize: MobileTheme.sizeCaption }
            Rectangle { implicitWidth: 20; implicitHeight: 3; color: MobileTheme.income }
            Text { Layout.fillWidth: true; text: qsTr("Условный вклад"); color: MobileTheme.accent; font.pixelSize: MobileTheme.sizeCaption }
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Одинаковый начальный капитал и внешние пополнения/выводы. Ставка постоянная, вклад в валюте отчёта. Между дневными оценками пополнения считаются внесёнными до начисления процентов, выводы — после. Проценты: ставка / 365 или 366 за календарный день. Ежемесячная капитализация — в дату открытия, в коротком месяце в его последний день. Без капитализации проценты хранятся отдельно и не приносят доход; выводы сначала расходуют их. Начисленные, но ещё не выплаченные проценты включены в итог, но не доступны для вывода. Налоги, ограничения банка и потеря процентов при досрочном закрытии не учитываются.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }
}
