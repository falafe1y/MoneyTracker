import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    property var allocation: ({})
    property var contributions: ({})
    property string currency: "RUB"
    property color ink: "#031528"
    property color muted: "#687483"
    property color line: "#D8D7C7"
    property color positive: "#3F735F"
    property color negative: "#B94F48"
    readonly property var colors: ["#3F735F", "#6B92AD", "#B68B4C", "#8B749F", "#597C8A", "#C08A7B", "#9AA766", "#66728B"]
    readonly property var holdings: (group.currentIndex === 1 ? allocation.types : group.currentIndex === 2 ? allocation.accounts : group.currentIndex === 3 ? allocation.currencies : allocation.items) || []
    readonly property var resultRows: contributions.items || []
    readonly property real maximumResult: {
        var n = 0
        for (var i = 0; i < resultRows.length; ++i) n = Math.max(n, Math.abs(Number(resultRows[i].amountMinor)))
        return Math.max(n, 1)
    }
    function available(n) { return n !== undefined && n !== null && isFinite(Number(n)) }
    function amount(n) { return available(n) ? Number(n / 100).toLocaleString(Qt.locale(), 'f', 2) + " " + currency : "—" }
    function percent(n) { return available(n) ? Number(n).toLocaleString(Qt.locale(), 'f', 2) + " %" : "—" }
    onHoldingsChanged: donut.requestPaint()
    onAllocationChanged: donut.requestPaint()
    spacing: 12

    GridLayout {
        Layout.fillWidth: true
        columns: panel.width < 850 ? 1 : 2
        columnSpacing: 24
        rowSpacing: 20
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 10
            Label { text: qsTr("Структура вложений"); color: panel.ink; font.pixelSize: 17; font.bold: true }
            ComboBox { id: group; Layout.fillWidth: true; model: [qsTr("По активам"), qsTr("По типам активов"), qsTr("По счетам"), qsTr("По валютам счетов")] }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: panel.muted; text: qsTr("На конец выбранного периода · Стоимость: %1").arg(panel.amount(panel.allocation.capitalMinor)) }
            Canvas {
                id: donut
                visible: !!panel.allocation.pieAvailable
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 190
                Layout.preferredHeight: 190
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    if (!panel.allocation.pieAvailable) return
                    var start = -Math.PI / 2
                    for (var i = 0; i < panel.holdings.length; ++i) {
                        var percent = Number(panel.holdings[i].percent || 0)
                        if (percent <= 0) continue
                        var end = start + percent / 100 * Math.PI * 2
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, 76, start, end)
                        ctx.strokeStyle = panel.colors[i % panel.colors.length]
                        ctx.lineWidth = 28
                        ctx.stroke()
                        start = end
                    }
                }
                Label { anchors.centerIn: parent; text: "100 %"; color: panel.ink; font.pixelSize: 22; font.bold: true }
            }
            Label {
                visible: !panel.allocation.pieAvailable
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: panel.muted
                text: !panel.allocation.available ? qsTr("Для долей нужна полная оценка всех активов на конец периода.") : qsTr("При нулевой или отрицательной стоимости круговые доли не рассчитываются. Суммы приведены ниже.")
            }
            Repeater {
                model: panel.holdings
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Rectangle { width: 9; height: 9; radius: 3; color: panel.colors[index % panel.colors.length] }
                    Label { Layout.fillWidth: true; text: modelData.label; color: panel.ink; wrapMode: Text.WordWrap }
                    Label { text: panel.amount(modelData.amountMinor); color: panel.ink }
                    Label { Layout.preferredWidth: 66; horizontalAlignment: Text.AlignRight; text: panel.percent(panel.allocation.pieAvailable ? modelData.percent : null); color: panel.muted }
                }
            }
            Label { visible: (panel.allocation.adjustments || []).length > 0; color: panel.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Неучтённый в деньгах результат контрактов: %1. Это переоценка, а не стоимость вложенного капитала; в круговые доли она не входит.").arg(panel.amount(panel.allocation.adjustmentMinor)) }
            Repeater {
                model: panel.allocation.adjustments || []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: modelData.label; color: panel.muted }
                    Label { text: panel.amount(modelData.amountMinor); color: panel.ink }
                }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 12; color: panel.muted; text: qsTr("Валюты — валюты оценки на счетах, а не валютная экспозиция компаний и фондов. Заблокированное обеспечение контрактов не добавляется повторно к деньгам.") }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 10
            Label { text: qsTr("Вклад активов в результат"); color: panel.ink; font.pixelSize: 17; font.bold: true }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: panel.muted; text: qsTr("За выбранный период, включая закрытые позиции, выплаты и учтённые расходы. Пополнения и выводы исключены.") }
            Label { visible: !panel.contributions.available; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: panel.muted; text: qsTr("Нужны достоверные начальная и конечная оценки, денежные потоки и курсы валют.") }
            Repeater {
                model: panel.contributions.available ? panel.resultRows : []
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: modelData.label; color: panel.ink }
                        Label { text: panel.amount(modelData.amountMinor); color: modelData.amountMinor < 0 ? panel.negative : panel.positive; font.bold: true }
                    }
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 12
                        Rectangle { x: parent.width / 2; width: 1; height: parent.height; color: panel.line }
                        Rectangle {
                            width: Math.abs(Number(modelData.amountMinor)) / panel.maximumResult * (parent.width / 2 - 2)
                            height: 8; y: 2; radius: 3
                            x: modelData.amountMinor < 0 ? parent.width / 2 - width : parent.width / 2 + 1
                            color: modelData.amountMinor < 0 ? panel.negative : panel.positive
                        }
                    }
                }
            }
            Label { visible: !!panel.contributions.available; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: panel.muted; text: qsTr("Валютная переоценка счетов: %1\nНеобъяснённая разница: %2\nИтого: %3").arg(panel.amount(panel.contributions.fxMinor)).arg(panel.amount(panel.contributions.unexplainedMinor)).arg(panel.amount(panel.contributions.totalMinor)) }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: panel.muted; font.pixelSize: 12; text: qsTr("Результат активов рассчитан при курсе валют счетов на начало периода. Валютная переоценка показана отдельно. Длина полос отражает сумму прибыли или убытка, а не долю вложений.") }
        }
    }
}
