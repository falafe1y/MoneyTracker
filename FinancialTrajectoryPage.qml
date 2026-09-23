import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property var controller
    required property var theme
    property var trajectory: controller.financialTrajectory
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function money(value) { return theme.money(value, trajectory.currency, false); }
    function number(text) {
        var value = Number(String(text).replace(",", "."));
        return isFinite(value) ? value : 0;
    }
    function applySettings() {
        var result = controller.saveFinancialTrajectorySettings({
            analysisMonths: Number(analysis.currentValue),
            horizonMonths: Number(horizon.currentValue),
            annualReturnPercent: number(returnField.text),
            annualInflationPercent: number(inflationField.text),
            incomeChangePercent: number(incomeField.text),
            expenseChangePercent: number(expenseField.text),
            purchaseMinor: Math.max(0, Math.round(number(purchaseField.text) * 100)),
            purchaseMonth: purchaseEnabled.checked
                ? Math.min(Number(purchaseMonth.currentValue), Number(horizon.currentValue)) : 0
        });
        errorText.text = result.ok ? "" : result.error;
    }
    function scenario(kind) {
        if (kind === "safe") { returnField.text = "2"; incomeField.text = "-10"; expenseField.text = "10"; }
        else if (kind === "growth") { returnField.text = "8"; incomeField.text = "5"; expenseField.text = "-5"; }
        else { returnField.text = "5"; incomeField.text = "0"; expenseField.text = "0"; }
        applySettings();
    }
    component Card: Rectangle {
        radius: 16; color: theme.panel; border.color: theme.line
    }
    component Caption: Text { color: theme.muted; font.pixelSize: 13 }
    component Field: TextField {
        color: theme.accent; selectByMouse: true; horizontalAlignment: Text.AlignRight
        validator: DoubleValidator { bottom: -99; top: 1000; decimals: 2 }
        background: Rectangle { radius: 8; color: theme.soft; border.color: theme.line }
    }
    component Choice: ComboBox {
        textRole: "text"; valueRole: "value"
        contentItem: Text { leftPadding: 10; text: parent.displayText; color: theme.accent
            verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 8; color: theme.soft; border.color: theme.line }
    }
    component ScenarioButton: Button {
        contentItem: Text { text: parent.text; color: theme.accent; horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 9; color: theme.soft; border.color: theme.line }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: qsTr("Прогноз капитала на основе ваших фактических доходов и расходов")
                color: theme.muted; wrapMode: Text.WordWrap }
            ScenarioButton { text: qsTr("Осторожный"); onClicked: page.scenario("safe") }
            ScenarioButton { text: qsTr("Базовый"); onClicked: page.scenario("base") }
            ScenarioButton { text: qsTr("Рост"); onClicked: page.scenario("growth") }
        }
        GridLayout {
            Layout.fillWidth: true; columns: 4; columnSpacing: 14; rowSpacing: 14
            Repeater {
                model: [
                    {label: qsTr("Текущий капитал"), value: page.money(trajectory.currentMinor)},
                    {label: qsTr("Среднее накопление в месяц"), value: page.money(trajectory.averageSavingMinor)},
                    {label: qsTr("К концу прогноза"), value: trajectory.forecast.length ? page.money(trajectory.forecast[trajectory.forecast.length - 1].valueMinor) : "—"},
                    {label: qsTr("Доля накоплений"), value: Number(trajectory.savingRate).toFixed(1) + "%"}
                ]
                Card {
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: 92
                    Column { anchors.fill: parent; anchors.margins: 16; spacing: 9
                        Caption { text: parent.parent.modelData.label }
                        Text { text: parent.parent.modelData.value; color: theme.accent; font.pixelSize: 19; font.bold: true }
                    }
                }
            }
        }
        Card {
            Layout.fillWidth: true; Layout.preferredHeight: 340
            ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 10
                RowLayout { Layout.fillWidth: true
                    Text { text: qsTr("Финансовая траектория"); color: theme.accent; font.pixelSize: 20; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Caption { text: qsTr("— история   - - прогноз   ··· с учётом инфляции") }
                }
                Canvas {
                    id: chart
                    Layout.fillWidth: true; Layout.fillHeight: true
                    property var history: trajectory.history || []
                    property var forecast: trajectory.forecast || []
                    onHistoryChanged: requestPaint()
                    onForecastChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    function drawLine(ctx, rows, key, color, dash) {
                        if (!rows || rows.length < 1) return;
                        ctx.beginPath(); ctx.strokeStyle = color; ctx.lineWidth = 2.5; ctx.setLineDash(dash);
                        for (var i = 0; i < rows.length; ++i) {
                            var x = 18 + (width - 36) * ((offset + i) / Math.max(1, total - 1));
                            var y = 18 + (height - 36) * (1 - (Number(rows[i][key]) - low) / Math.max(1, high - low));
                            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                        }
                        ctx.stroke(); ctx.setLineDash([]);
                    }
                    property real low: 0
                    property real high: 1
                    property int offset: 0
                    property int total: 1
                    onPaint: {
                        var ctx = getContext("2d"); ctx.reset();
                        var values = [];
                        for (var i = 0; i < history.length; ++i) values.push(Number(history[i].valueMinor));
                        for (var j = 0; j < forecast.length; ++j) { values.push(Number(forecast[j].valueMinor)); values.push(Number(forecast[j].realMinor)); }
                        if (!values.length) return;
                        low = Math.min.apply(Math, values); high = Math.max.apply(Math, values);
                        var pad = Math.max(1, (high - low) * 0.12); low -= pad; high += pad;
                        total = Math.max(1, history.length + forecast.length - 1); offset = Math.max(0, history.length - 1);
                        ctx.strokeStyle = theme.line; ctx.lineWidth = 1;
                        for (var g = 0; g < 4; ++g) { var gy = 18 + (height - 36) * g / 3;
                            ctx.beginPath(); ctx.moveTo(18, gy); ctx.lineTo(width - 18, gy); ctx.stroke(); }
                        offset = 0; drawLine(ctx, history, "valueMinor", theme.accentSoft, []);
                        offset = Math.max(0, history.length - 1); drawLine(ctx, forecast, "valueMinor", theme.income, [9, 6]);
                        drawLine(ctx, forecast, "realMinor", theme.muted, [2, 5]);
                    }
                }
                Caption { Layout.fillWidth: true; visible: trajectory.history.length <= 1
                    text: qsTr("История начнёт расти со следующего дня: снимок капитала сохраняется один раз в день.") }
            }
        }
        Card {
            Layout.fillWidth: true; Layout.preferredHeight: settingsGrid.implicitHeight + 40
            GridLayout { id: settingsGrid; anchors.fill: parent; anchors.margins: 18; columns: 4; columnSpacing: 18; rowSpacing: 10
                Text { text: qsTr("Параметры прогноза"); color: theme.accent; font.pixelSize: 20; font.bold: true; Layout.columnSpan: 4 }
                Caption { text: qsTr("Среднее за") }
                Caption { text: qsTr("Горизонт") }
                Caption { text: qsTr("Доходность в год, %") }
                Caption { text: qsTr("Инфляция в год, %") }
                Choice { id: analysis; Layout.fillWidth: true; model: [{text: qsTr("3 месяца"),value:3},{text:qsTr("6 месяцев"),value:6},{text:qsTr("12 месяцев"),value:12}]
                    Component.onCompleted: currentIndex = Math.max(0, [3,6,12].indexOf(trajectory.analysisMonths)) }
                Choice { id: horizon; Layout.fillWidth: true; model: [{text:qsTr("6 месяцев"),value:6},{text:qsTr("1 год"),value:12},{text:qsTr("3 года"),value:36},{text:qsTr("5 лет"),value:60}]
                    Component.onCompleted: currentIndex = Math.max(0, [6,12,36,60].indexOf(trajectory.horizonMonths)) }
                Field { id: returnField; Layout.fillWidth: true; text: trajectory.annualReturnPercent }
                Field { id: inflationField; Layout.fillWidth: true; text: trajectory.annualInflationPercent }
                Caption { text: qsTr("Изменение доходов, %") }
                Caption { text: qsTr("Изменение расходов, %") }
                Caption { text: qsTr("Разовая покупка") }
                Caption { text: qsTr("Через") }
                Field { id: incomeField; Layout.fillWidth: true; text: trajectory.incomeChangePercent }
                Field { id: expenseField; Layout.fillWidth: true; text: trajectory.expenseChangePercent }
                RowLayout { CheckBox { id: purchaseEnabled; checked: trajectory.purchaseMonth > 0 }
                    Field { id: purchaseField; Layout.fillWidth: true; enabled: purchaseEnabled.checked
                        text: (trajectory.purchaseMinor / 100).toFixed(2) } }
                Choice { id: purchaseMonth; Layout.fillWidth: true; enabled: purchaseEnabled.checked
                    model: [{text:qsTr("1 месяц"),value:1},{text:qsTr("3 месяца"),value:3},{text:qsTr("6 месяцев"),value:6},{text:qsTr("12 месяцев"),value:12},{text:qsTr("3 года"),value:36}]
                    Component.onCompleted: { var vals=[1,3,6,12,36]; currentIndex=Math.max(0, vals.indexOf(trajectory.purchaseMonth)); } }
                Text { id: errorText; Layout.columnSpan: 3; Layout.fillWidth: true; color: theme.red; wrapMode: Text.WordWrap
                    text: !trajectory.complete ? qsTr("Часть котировок недоступна: прогноз построен по известным активам, снимок за сегодня не сохранён.") : "" }
                Button { text: qsTr("Пересчитать и сохранить"); onClicked: page.applySettings()
                    contentItem: Text { text: parent.text; color: theme.white; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 9; color: theme.accent } }
            }
        }
        Caption { Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Прогноз не является инвестиционной рекомендацией. Доходность применяется только к текущей стоимости инвестиций; будущие накопления автоматически не считаются вложенными.") }
    }
}
