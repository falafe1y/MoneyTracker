import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var controller
    property color surface: "#FFFFF8"
    property color ink: "#031528"
    property color muted: "#687483"
    property color line: "#D8D7C7"
    property color positive: "#3F735F"
    property color negative: "#B94F48"
    readonly property var report: controller ? (controller.investmentPerformance || ({})) : ({})
    readonly property var choices: [{id: "", name: qsTr("Все инвестиционные счета")}].concat(controller ? (controller.performanceAccounts || []) : [])
    readonly property var warnings: report.warnings || []
    property bool detailsOpen: false
    color: surface
    border.color: ink
    radius: 12
    implicitHeight: content.implicitHeight + 40

    function available(n) { return n !== undefined && n !== null && isFinite(Number(n)) }
    function amount(n) { return available(n) ? Number(n / 100).toLocaleString(Qt.locale(), 'f', 2) + " " + (report.currency || "") : "—" }
    function percent(n) { return available(n) ? Number(n).toLocaleString(Qt.locale(), 'f', 2) + " %" : "—" }
    function date(s) { return s ? s.substr(8, 2) + "." + s.substr(5, 2) + "." + s.substr(0, 4) : "—" }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Результат инвестиций"); color: panel.ink; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
            ComboBox {
                id: account
                Layout.preferredWidth: 290
                model: panel.choices
                textRole: "name"
                currentIndex: {
                    for (var i = 0; i < panel.choices.length; ++i)
                        if (panel.choices[i].id === (panel.controller ? panel.controller.performanceAccountId : "")) return i
                    return 0
                }
                onActivated: panel.controller.setPerformanceAccount(panel.choices[currentIndex].id)
            }
        }
        Label {
            Layout.fillWidth: true
            color: panel.muted
            wrapMode: Text.WordWrap
            text: qsTr("Действующие брокерские счета и вклады · Валюта отчёта: %1").arg(panel.report.currency || "")
                + "\n" + qsTr("Оценки: %1 — %2 · Дней с оценкой: %3").arg(panel.date(panel.report.from)).arg(panel.date(panel.report.to)).arg(panel.report.observationCount || 0)
        }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.width < 700 ? 2 : 3
            columnSpacing: 12
            rowSpacing: 12
            Repeater {
                model: [
                    {title: qsTr("Результат за период"), value: panel.amount(panel.report.gainMinor), hint: qsTr("Изменение капитала без пополнений и выводов")},
                    {title: qsTr("Доходность за период"), value: panel.percent(panel.report.periodPercent), hint: qsTr("По дневным оценкам, с учётом денежных потоков")},
                    {title: qsTr("Годовой эквивалент"), value: panel.percent(panel.report.annualPercent), hint: qsTr("Пересчёт темпа роста на год, без прогноза")},
                    {title: qsTr("Прибыль после продаж"), value: panel.amount(panel.report.realizedMinor), hint: panel.available(panel.report.realizedMinor) ? qsTr("Продажи и расчёты по контрактам, после учтённых расходов; по фильтру дат") : qsTr("Известная часть: %1").arg(panel.amount(panel.report.knownRealizedMinor))},
                    {title: qsTr("Прибыль открытых позиций"), value: panel.amount(panel.report.unrealizedMinor), hint: qsTr("На конец периода, относительно стоимости приобретения")},
                    {title: qsTr("После заданной инфляции"), value: panel.percent(panel.report.realPercent), hint: qsTr("Доходность за период по указанной ниже годовой ставке")}
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 140
                    color: "transparent"
                    border.color: panel.line
                    radius: 8
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        Label { Layout.fillWidth: true; text: modelData.title; color: panel.muted; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: modelData.value; color: panel.ink; font.pixelSize: 24; font.bold: true; elide: Text.ElideRight }
                        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: modelData.hint; color: panel.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Заданная инфляция, % в год:"); color: panel.ink }
            TextField {
                id: inflation
                Layout.preferredWidth: 110
                text: panel.controller ? panel.controller.performanceInflation : ""
                placeholderText: qsTr("Не задана")
                selectByMouse: true
                onAccepted: saveInflation.clicked()
            }
            Button {
                id: saveInflation
                text: qsTr("Применить")
                onClicked: inflationError.text = panel.controller.setPerformanceInflation(inflation.text) ? "" : qsTr("Введите ставку больше −100 и не выше 1000 %; до 4 знаков после запятой.")
            }
            Item { Layout.fillWidth: true }
        }
        Label { id: inflationError; visible: text.length > 0; color: panel.negative; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { Layout.fillWidth: true; text: qsTr("Ставка — ваше допущение, одинаковое на весь период. Это не исторический индекс потребительских цен."); color: panel.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: panel.line }
        InvestmentBenchmarkPanel {
            Layout.fillWidth: true
            controller: panel.controller
            ink: panel.ink
            muted: panel.muted
            line: panel.line
            accent: panel.positive
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: panel.line }
        InvestmentRiskPanel {
            Layout.fillWidth: true
            risk: panel.report.risk || ({})
            ink: panel.ink
            muted: panel.muted
            line: panel.line
            negative: panel.negative
            positive: panel.positive
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: panel.line }
        Label { text: qsTr("Разложение результата за период"); font.pixelSize: 17; font.bold: true; color: panel.ink }
        Label {
            Layout.fillWidth: true
            text: qsTr("Чистые пополнения и выводы: %1 · Изменение прибыли открытых позиций: %2").arg(panel.amount(panel.report.externalFlowMinor)).arg(panel.amount(panel.report.unrealizedChangeMinor))
            wrapMode: Text.WordWrap
            color: panel.muted
        }
        Repeater {
            model: panel.available(panel.report.gainMinor) ? (panel.report.breakdown || []) : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData.label + (modelData.complete ? "" : qsTr(" · известная часть")); color: panel.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: panel.amount(modelData.amountMinor); color: modelData.amountMinor < 0 ? panel.negative : panel.positive; font.bold: true }
            }
        }
        Label {
            visible: !panel.available(panel.report.gainMinor)
            text: qsTr("Разложение появится, когда будут известны результат периода и денежные потоки.")
            color: panel.muted
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            text: qsTr("Продажи и переоценка показаны до отдельно указанных комиссий и удержаний. Валютная строка отражает изменение курса валюты счёта к валюте отчёта; курс валюты самого инструмента включён в результат позиции.")
            color: panel.muted
        }
        Button { visible: panel.warnings.length > 0; text: panel.detailsOpen ? qsTr("Скрыть пояснения") : qsTr("Пояснения к расчёту (%1)").arg(panel.warnings.length); onClicked: panel.detailsOpen = !panel.detailsOpen }
        Label {
            visible: panel.detailsOpen && panel.warnings.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: panel.muted
            text: panel.warnings.join("\n")
        }
    }
}
