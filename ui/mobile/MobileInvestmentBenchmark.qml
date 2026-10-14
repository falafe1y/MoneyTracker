import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: panel

    readonly property var selection: financeController.benchmarkSelection || ({})
    readonly property var comparison: financeController.benchmarkComparison || ({})
    readonly property var points: comparison.points || []
    readonly property var indices: financeController.benchmarkIndices || []
    readonly property var choices: [{ key: "", name: qsTr("Без сравнения") }]
        .concat(indices).concat([{ key: "fund", name: qsTr("Биржевой фонд Мосбиржи…") }])
    readonly property var xFractions: points.map(function(row, index) {
        return points.length > 1 ? index / (points.length - 1) : 0.5;
    })
    readonly property var chartSeries: [
        { values: points.map(function(row) { return Number(row.portfolio); }), color: MobileTheme.accent, width: 2 },
        { values: points.map(function(row) { return Number(row.benchmark); }), color: MobileTheme.income, width: 2 }
    ]

    function available(value) {
        return value !== null && value !== undefined && isFinite(Number(value));
    }
    function percent(value) {
        return available(value) ? Number(value).toLocaleString(MobileFormat.locale(), "f", 2) + " %" : "—";
    }
    function date(value) {
        return value ? value.substr(8, 2) + "." + value.substr(5, 2) + "." + value.substr(0, 4) : "—";
    }
    function choiceName() {
        if (selection.kind === "fund")
            return panel.choices[panel.choices.length - 1].name;
        for (let i = 0; i < panel.choices.length; ++i)
            if (panel.choices[i].key === (selection.key || ""))
                return panel.choices[i].name;
        return panel.choices[0].name;
    }
    function chooseChoice(choice) {
        choiceSheet.close();
        if (choice.key === "fund")
            fundSheet.open();
        else
            financeController.selectBenchmarkIndex(choice.key);
    }

    spacing: 12

    Text {
        Layout.fillWidth: true
        text: qsTr("Сравнение с индексом или фондом")
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeHeading
        font.weight: Font.DemiBold
    }

    MobileButton {
        Layout.fillWidth: true
        text: panel.choiceName()
        onClicked: choiceSheet.open()
    }

    ColumnLayout {
        visible: !!panel.selection.key
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: panel.selection.name || panel.selection.symbol || ""
            wrapMode: Text.Wrap
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeBody
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MobileButton {
                Layout.fillWidth: true
                visible: panel.selection.kind === "fund"
                text: qsTr("Выбрать другой фонд")
                onClicked: fundSheet.open()
            }
            MobileButton {
                Layout.fillWidth: true
                text: financeController.benchmarkBusy ? qsTr("Загрузка…") : qsTr("Обновить историю")
                enabled: !financeController.benchmarkBusy && !!panel.comparison.canRefresh
                onClicked: financeController.refreshBenchmark()
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.selection.key
        text: panel.selection.kind === "fund"
            ? qsTr("Сравнение по биржевой цене пая. Выплаты владельцу пая и дробления отдельно не учитываются. Расходы управления, уже отражённые в цене, остаются в ней.")
            : panel.selection.totalReturn
                ? qsTr("Полная доходность брутто: индекс включает реинвестирование выплат до налогов. Ваши личные комиссии и налоги в ориентире не учитываются.")
                : qsTr("Ценовой индекс: дивиденды не включены. Доходность вашего счёта учитывает записанные выплаты и расходы.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: financeController.benchmarkError.length > 0
        text: financeController.benchmarkError
        wrapMode: Text.Wrap
        color: MobileTheme.red
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.comparison.available && panel.comparison.maximumQuoteAgeDays > 7
        text: qsTr("В части периода последняя биржевая цена старше семи дней. Торги могли не проводиться или быть приостановлены. Фактическая дата цены видна в подсказке на графике.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: !panel.comparison.available
        text: panel.comparison.reason || qsTr("Выберите ориентир для сравнения.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.selection.key && !!panel.comparison.canRefresh
        text: qsTr("История ориентира: %1 — %2 · %3").arg(panel.date(panel.comparison.historyFrom))
            .arg(panel.date(panel.comparison.historyTo))
            .arg(panel.comparison.historyLoaded ? qsTr("сохранена в базе") : qsTr("можно загрузить заранее"))
            + (panel.comparison.historyFallback ? "\n" + qsTr("В выбранном фильтре ещё нет завершённых дней. Для предварительной загрузки используется последний год до вчерашнего дня.") : "")
            + (!panel.comparison.available ? "\n" + qsTr("Котировки можно загрузить без оценок портфеля. Сравнение появится, когда накопится достаточная непрерывная история. Без фильтра и прошлых оценок загружается последний год.") : "")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.comparison.available
        text: qsTr("%1 — %2 · Валюта: %3 · Текущий день исключён")
            .arg(panel.date(panel.comparison.from)).arg(panel.date(panel.comparison.to))
            .arg(financeController.investmentPerformance.currency || "")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    GridLayout {
        visible: !!panel.comparison.available
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 10

        Repeater {
            model: [
                { title: qsTr("Ваш результат"), value: panel.percent(panel.comparison.portfolioPercent), metric: "benchmark_portfolio" },
                { title: qsTr("Результат ориентира"), value: panel.percent(panel.comparison.benchmarkPercent), metric: "benchmark" },
                { title: qsTr("Разница результатов"), value: panel.available(panel.comparison.differencePoints)
                    ? Number(panel.comparison.differencePoints).toLocaleString(MobileFormat.locale(), "f", 2) + qsTr(" п.п.") : "—", metric: "benchmark_difference" }
            ]

            delegate: MobileCard {
                required property var modelData

                Layout.fillWidth: true
                Layout.columnSpan: modelData.metric === "benchmark_difference" ? 2 : 1
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
            }
        }
    }

    MobileCard {
        visible: !!panel.comparison.available && panel.points.length >= 2
        Layout.fillWidth: true
        gap: 6

        MobileSeriesChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            series: panel.chartSeries
            xFractions: panel.xFractions
            xLabels: [
                { fraction: 0, text: panel.date(panel.comparison.from), align: "left" },
                { fraction: 1, text: panel.date(panel.comparison.to), align: "right" }
            ]
            tooltips: panel.points.map(function(row) {
                return panel.date(row.date) + "\n" + qsTr("Ваш результат: %1").arg(panel.percent(row.portfolio))
                    + "\n" + qsTr("Ориентир: %1").arg(panel.percent(row.benchmark))
                    + "\n" + qsTr("Дата цены ориентира: %1").arg(panel.date(row.quoteDate));
            })
            formatValue: function(value) { return Number(value).toLocaleString(MobileFormat.locale(), "f", 1) + " %"; }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle { implicitWidth: 20; implicitHeight: 3; color: MobileTheme.accent }
            Text { text: qsTr("Ваш результат"); color: MobileTheme.accent; font.pixelSize: MobileTheme.sizeCaption }
            Rectangle { implicitWidth: 20; implicitHeight: 3; color: MobileTheme.income }
            Text { Layout.fillWidth: true; text: panel.selection.symbol || qsTr("Ориентир"); color: MobileTheme.accent; font.pixelSize: MobileTheme.sizeCaption; elide: Text.ElideRight }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.selection.key
        text: qsTr("Обе линии начинаются с нуля и показывают номинальную доходность без поправки на инфляцию. Разница указана в процентных пунктах. Доходность счёта очищена от пополнений и выводов. Сравнение идёт по календарным датам и ценам закрытия, без внутридневной синхронизации. В дни без новой биржевой цены сохраняется последняя предыдущая цена; её дата показана в подсказке.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    MobileSheet {
        id: choiceSheet
        title: qsTr("Сравнение")

        Repeater {
            model: panel.choices

            delegate: MobileListRow {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                first: index === 0
                title: modelData.name
                marked: panel.choiceName() === modelData.name
                onClicked: panel.chooseChoice(modelData)
            }
        }
    }

    MobileSheet {
        id: fundSheet
        title: qsTr("Биржевой фонд для сравнения")

        function search() {
            financeController.searchBenchmarkFunds(fundQuery.text);
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MobileField {
                id: fundQuery
                Layout.fillWidth: true
                placeholder: qsTr("Название или обозначение фонда")
            }
            MobileButton {
                implicitHeight: 48
                text: qsTr("Найти")
                enabled: !financeController.benchmarkSearchBusy
                onClicked: fundSheet.search()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: financeController.benchmarkSearchBusy
            text: qsTr("Ищем биржевые фонды…")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
        }
        Text {
            Layout.fillWidth: true
            visible: financeController.benchmarkSearchError.length > 0
            text: financeController.benchmarkSearchError
            wrapMode: Text.Wrap
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
        }

        Repeater {
            model: financeController.benchmarkSearchResults

            delegate: MobileListRow {
                required property var modelData

                Layout.fillWidth: true
                title: modelData.symbol
                subtitle: modelData.name
                onClicked: {
                    financeController.selectBenchmarkFund(modelData.index);
                    fundSheet.close();
                }
            }
        }
    }
}
