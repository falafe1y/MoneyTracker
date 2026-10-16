import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: panel

    readonly property var report: financeController.investmentPerformance || ({})
    readonly property var choices: [{ id: "", name: qsTr("Все инвестиционные счета") },
        { id: "group:brokerage", name: qsTr("Все брокерские счета") },
        { id: "group:deposit", name: qsTr("Все вклады") }]
        .concat(financeController.performanceAccounts || [])
    readonly property var warnings: report.warnings || []
    property bool detailsOpen: false

    function available(value) {
        return value !== undefined && value !== null && isFinite(Number(value));
    }
    function amount(value) {
        return available(value) ? MobileFormat.money(value, panel.report.currency || "RUB", false) : "—";
    }
    function percent(value) {
        return available(value) ? Number(value).toLocaleString(MobileFormat.locale(), "f", 2) + " %" : "—";
    }
    function date(value) {
        return value ? value.substr(8, 2) + "." + value.substr(5, 2) + "." + value.substr(0, 4) : "—";
    }
    function accountName() {
        const current = financeController.performanceAccountId || "";
        for (let i = 0; i < panel.choices.length; ++i)
            if (panel.choices[i].id === current)
                return panel.choices[i].name;
        return panel.choices[0].name;
    }

    spacing: 12

    Text {
        Layout.fillWidth: true
        text: qsTr("Доходность")
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeTitle
        font.weight: Font.Bold
        wrapMode: Text.Wrap
    }

    MobileButton {
        Layout.fillWidth: true
        text: panel.accountName()
        onClicked: accountSheet.open()
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Действующие брокерские счета и вклады · Валюта отчёта: %1").arg(panel.report.currency || "")
            + "\n" + qsTr("Оценки: %1 — %2 · Дней с оценкой: %3")
                .arg(panel.date(panel.report.from)).arg(panel.date(panel.report.to))
                .arg(panel.report.observationCount || 0)
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 10

        Repeater {
            model: [
                { title: qsTr("Результат за период"), value: panel.amount(panel.report.gainMinor), hint: qsTr("Изменение капитала без пополнений и выводов"), metric: "gain" },
                { title: qsTr("Доходность за период"), value: panel.percent(panel.report.periodPercent), hint: qsTr("По дневным оценкам, с учётом денежных потоков"), metric: "period" },
                { title: qsTr("Годовой эквивалент"), value: panel.percent(panel.report.annualPercent), hint: qsTr("Пересчёт темпа роста на год, без прогноза"), metric: "annual" },
                { title: qsTr("Прибыль после продаж"), value: panel.amount(panel.report.realizedMinor), hint: panel.available(panel.report.realizedMinor) ? qsTr("Продажи и расчёты по контрактам, после учтённых расходов; по фильтру дат") : qsTr("Известная часть: %1").arg(panel.amount(panel.report.knownRealizedMinor)), metric: "realized" },
                { title: qsTr("Прибыль открытых позиций"), value: panel.amount(panel.report.unrealizedMinor), hint: qsTr("На конец периода, относительно стоимости приобретения"), metric: "unrealized" },
                { title: qsTr("После заданной инфляции"), value: panel.percent(panel.report.realPercent), hint: qsTr("Доходность за период по указанной ниже годовой ставке"), metric: "inflation" }
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
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeCaption
                        wrapMode: Text.Wrap
                    }
                    MobileInfoButton {
                        metric: modelData.metric
                        displayValue: modelData.value
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.value
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeTitle
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.hint
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Заданная инфляция, % в год")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        font.weight: Font.Medium
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        MobileField {
            id: inflationField
            Layout.fillWidth: true
            placeholder: qsTr("Не задана")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            text: financeController.performanceInflation || ""
        }
        MobileButton {
            implicitHeight: 48
            text: qsTr("Применить")
            onClicked: inflationError.text = financeController.setPerformanceInflation(inflationField.text)
                ? "" : qsTr("Введите ставку больше −100 и не выше 1000 %; до 4 знаков после запятой.")
        }
    }
    Text {
        id: inflationError
        Layout.fillWidth: true
        visible: text.length > 0
        color: MobileTheme.red
        wrapMode: Text.Wrap
        font.pixelSize: MobileTheme.sizeCaption
    }
    Text {
        Layout.fillWidth: true
        text: qsTr("Ставка — ваше допущение, одинаковое на весь период. Это не исторический индекс потребительских цен.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    MobileInvestmentDataQuality {
        Layout.fillWidth: true
        quality: panel.report.dataQuality || ({})
    }

    MobileInvestmentBenchmark {
        Layout.fillWidth: true
    }

    MobileInvestmentDeposit {
        Layout.fillWidth: true
    }

    MobileInvestmentRisk {
        Layout.fillWidth: true
        risk: panel.report.risk || ({})
    }

    MobileInvestmentStructure {
        Layout.fillWidth: true
        allocation: panel.report.allocation || ({})
        contributions: panel.report.contributions || ({})
        currency: panel.report.currency || "RUB"
    }

    MobileInvestmentPayments {
        Layout.fillWidth: true
        payments: panel.report.payments || ({})
        currency: panel.report.currency || "RUB"
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: 6
        text: qsTr("Разложение результата за период")
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeHeading
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
    }
    Text {
        Layout.fillWidth: true
        text: qsTr("Чистые пополнения и выводы: %1 · Изменение прибыли открытых позиций: %2")
            .arg(panel.amount(panel.report.externalFlowMinor))
            .arg(panel.amount(panel.report.unrealizedChangeMinor))
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        MobileInfoButton {
            metric: "flow"
            displayValue: panel.amount(panel.report.externalFlowMinor)
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Пополнения и выводы")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
        }
        MobileInfoButton {
            metric: "unrealized_change"
            displayValue: panel.amount(panel.report.unrealizedChangeMinor)
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Изменение открытой прибыли")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
            wrapMode: Text.Wrap
        }
    }

    Repeater {
        model: panel.available(panel.report.gainMinor) ? (panel.report.breakdown || []) : []

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: modelData.label + (modelData.complete ? "" : qsTr(" · известная часть"))
                wrapMode: Text.Wrap
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeCaption
            }
            Text {
                text: panel.amount(modelData.amountMinor)
                color: modelData.amountMinor < 0 ? MobileTheme.red : MobileTheme.income
                font.pixelSize: MobileTheme.sizeCaption
                font.weight: Font.Bold
            }
            MobileInfoButton {
                metric: "part_" + modelData.code
                displayValue: panel.amount(modelData.amountMinor)
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !panel.available(panel.report.gainMinor)
        text: qsTr("Разложение появится, когда будут известны результат периода и денежные потоки.")
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        wrapMode: Text.Wrap
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Продажи и переоценка показаны до отдельно указанных комиссий и удержаний. Валютная строка отражает изменение курса валюты счёта к валюте отчёта; курс валюты самого инструмента включён в результат позиции.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    MobileButton {
        visible: panel.warnings.length > 0
        Layout.fillWidth: true
        text: panel.detailsOpen ? qsTr("Скрыть пояснения") : qsTr("Пояснения к расчёту (%1)").arg(panel.warnings.length)
        onClicked: panel.detailsOpen = !panel.detailsOpen
    }
    Text {
        visible: panel.detailsOpen && panel.warnings.length > 0
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
        text: panel.warnings.join("\n")
    }

    MobileSheet {
        id: accountSheet
        title: qsTr("Инвестиционный счёт")

        Repeater {
            model: panel.choices

            delegate: MobileListRow {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                first: index === 0
                title: modelData.name
                marked: panel.accountName() === modelData.name
                onClicked: {
                    financeController.setPerformanceAccount(modelData.id);
                    accountSheet.close();
                }
            }
        }
    }
}
