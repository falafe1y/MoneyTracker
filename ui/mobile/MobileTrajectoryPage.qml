import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    readonly property var trajectory: financeController.financialTrajectory
    readonly property var history: trajectory.history || []
    readonly property var forecast: trajectory.forecast || []
    property string saveError: ""

    function applySettings(values) {
        const result = financeController.saveFinancialTrajectorySettings(values);
        saveError = result.ok ? "" : result.error;
        return result.ok;
    }

    function scenario(kind) {
        const presets = {
            safe: { annualReturnPercent: 2, incomeChangePercent: -10, expenseChangePercent: 10 },
            base: { annualReturnPercent: 5, incomeChangePercent: 0, expenseChangePercent: 0 },
            growth: { annualReturnPercent: 8, incomeChangePercent: 5, expenseChangePercent: -5 }
        };
        const preset = presets[kind];
        applySettings({
            analysisMonths: trajectory.analysisMonths,
            horizonMonths: trajectory.horizonMonths,
            annualReturnPercent: preset.annualReturnPercent,
            annualInflationPercent: trajectory.annualInflationPercent,
            incomeChangePercent: preset.incomeChangePercent,
            expenseChangePercent: preset.expenseChangePercent,
            purchaseMinor: trajectory.purchaseMinor,
            purchaseMonth: trajectory.purchaseMonth
        });
    }

    function niceAxis(minimum, maximum) {
        let low = minimum;
        let high = maximum;
        if (high === low) {
            low -= 1;
            high += 1;
        }
        const rawStep = (high - low) / 4;
        const magnitude = Math.pow(10, Math.floor(Math.log10(rawStep)));
        const fraction = rawStep / magnitude;
        const step = (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10) * magnitude;
        const first = Math.floor(low / step) * step;
        const last = Math.ceil(high / step) * step;
        return {
            minimum: first,
            maximum: last,
            step: step,
            intervals: Math.max(1, Math.round((last - first) / step))
        };
    }

    Flickable {
        id: flick

        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: layout.implicitHeight + 16

        ColumnLayout {
            id: layout

            width: flick.width
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MobileChip {
                    text: qsTr("Осторожный")
                    onClicked: page.scenario("safe")
                }
                MobileChip {
                    text: qsTr("Базовый")
                    onClicked: page.scenario("base")
                }
                MobileChip {
                    text: qsTr("Рост")
                    onClicked: page.scenario("growth")
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: [
                        { label: qsTr("Текущий капитал"), value: MobileFormat.money(page.trajectory.currentMinor, page.trajectory.currency, false) },
                        { label: qsTr("Накопления в месяц"), value: MobileFormat.money(page.trajectory.averageSavingMinor, page.trajectory.currency, false) },
                        { label: qsTr("К концу прогноза"), value: page.forecast.length > 0 ? MobileFormat.money(page.forecast[page.forecast.length - 1].valueMinor, page.trajectory.currency, false) : "—" },
                        { label: qsTr("Доля накоплений"), value: Number(page.trajectory.savingRate).toFixed(1) + "%" }
                    ]

                    delegate: MobileCard {
                        required property var modelData

                        Layout.fillWidth: true
                        cardPadding: 14
                        gap: 6

                        Text {
                            Layout.fillWidth: true
                            text: modelData.label
                            wrapMode: Text.Wrap
                            color: MobileTheme.muted
                            font.pixelSize: MobileTheme.sizeCaption
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.value
                            elide: Text.ElideRight
                            color: MobileTheme.accent
                            font.pixelSize: MobileTheme.sizeHeading
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            MobileCard {
                Layout.fillWidth: true
                hard: true
                gap: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Финансовая траектория")
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeHeading
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Сплошная линия — история, пунктир — прогноз, точки — прогноз с учётом инфляции")
                    wrapMode: Text.Wrap
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }

                Canvas {
                    id: chart

                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        const values = [];
                        for (let i = 0; i < page.history.length; ++i)
                            values.push(Number(page.history[i].valueMinor));
                        for (let j = 0; j < page.forecast.length; ++j) {
                            values.push(Number(page.forecast[j].valueMinor));
                            values.push(Number(page.forecast[j].realMinor));
                        }
                        if (values.length === 0)
                            return;

                        const axis = page.niceAxis(Math.min(...values), Math.max(...values));
                        const left = 56;
                        const right = width - 6;
                        const top = 8;
                        const bottom = height - 8;
                        const total = Math.max(1, page.history.length + page.forecast.length - 1);
                        const xOf = function(index) { return left + (right - left) * index / total; };
                        const yOf = function(value) {
                            return top + (bottom - top) * (1 - (value - axis.minimum) / (axis.maximum - axis.minimum));
                        };

                        ctx.font = "11px Commissioner";
                        ctx.textAlign = "right";
                        ctx.textBaseline = "middle";
                        ctx.lineWidth = 1;
                        ctx.strokeStyle = MobileTheme.line;
                        ctx.fillStyle = MobileTheme.muted;
                        for (let g = 0; g <= axis.intervals; ++g) {
                            const value = axis.minimum + g * axis.step;
                            const y = yOf(value);
                            ctx.beginPath();
                            ctx.moveTo(left, y);
                            ctx.lineTo(right, y);
                            ctx.stroke();
                            ctx.fillText(MobileFormat.compactValue(value), left - 6, y);
                        }

                        const drawSeries = function(rows, key, color, dash, offset) {
                            if (!rows || rows.length === 0)
                                return;
                            ctx.beginPath();
                            ctx.strokeStyle = color;
                            ctx.lineWidth = 2;
                            ctx.setLineDash(dash);
                            for (let i = 0; i < rows.length; ++i) {
                                const x = xOf(offset + i);
                                const y = yOf(Number(rows[i][key]));
                                if (i === 0)
                                    ctx.moveTo(x, y);
                                else
                                    ctx.lineTo(x, y);
                            }
                            ctx.stroke();
                            ctx.setLineDash([]);
                        };

                        const forecastOffset = Math.max(0, page.history.length - 1);
                        drawSeries(page.history, "valueMinor", MobileTheme.accentSoft, [], 0);
                        drawSeries(page.forecast, "valueMinor", MobileTheme.income, [8, 5], forecastOffset);
                        drawSeries(page.forecast, "realMinor", MobileTheme.muted, [2, 4], forecastOffset);
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: page.history.length <= 1
                    text: qsTr("История начнёт расти со следующего дня: снимок капитала сохраняется один раз в день.")
                    wrapMode: Text.Wrap
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }
            }

            Text {
                Layout.fillWidth: true
                visible: !page.trajectory.complete
                text: qsTr("Часть котировок недоступна: прогноз построен по известным активам, снимок за сегодня не сохранён.")
                wrapMode: Text.Wrap
                color: MobileTheme.red
                font.pixelSize: MobileTheme.sizeCaption
            }

            Text {
                Layout.fillWidth: true
                visible: page.saveError.length > 0
                text: page.saveError
                wrapMode: Text.Wrap
                color: MobileTheme.red
                font.pixelSize: MobileTheme.sizeCaption
            }

            MobileButton {
                Layout.fillWidth: true
                text: qsTr("Параметры прогноза")
                onClicked: settingsSheet.openFor()
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Прогноз не является инвестиционной рекомендацией. Доходность применяется только к текущей стоимости инвестиций; будущие накопления автоматически не считаются вложенными.")
                wrapMode: Text.Wrap
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }
        }
    }

    onTrajectoryChanged: chart.requestPaint()

    MobileSheet {
        id: settingsSheet

        property int analysisMonths: 6
        property int horizonMonths: 12
        property bool purchaseEnabled: false
        property int purchaseMonth: 12
        property string errorText: ""

        title: qsTr("Параметры прогноза")

        function openFor() {
            const current = page.trajectory;
            analysisMonths = current.analysisMonths;
            horizonMonths = current.horizonMonths;
            returnField.text = String(current.annualReturnPercent);
            inflationField.text = String(current.annualInflationPercent);
            incomeField.text = String(current.incomeChangePercent);
            expenseField.text = String(current.expenseChangePercent);
            purchaseEnabled = current.purchaseMonth > 0;
            purchaseField.text = MobileFormat.amountText(current.purchaseMinor);
            purchaseMonth = current.purchaseMonth > 0 ? current.purchaseMonth : 12;
            errorText = "";
            open();
        }

        function save() {
            const ok = page.applySettings({
                analysisMonths: analysisMonths,
                horizonMonths: horizonMonths,
                annualReturnPercent: MobileFormat.numberFromText(returnField.text),
                annualInflationPercent: MobileFormat.numberFromText(inflationField.text),
                incomeChangePercent: MobileFormat.numberFromText(incomeField.text),
                expenseChangePercent: MobileFormat.numberFromText(expenseField.text),
                purchaseMinor: purchaseEnabled ? MobileFormat.minorFromText(purchaseField.text) : 0,
                purchaseMonth: purchaseEnabled ? Math.min(purchaseMonth, horizonMonths) : 0
            });
            if (ok)
                settingsSheet.close();
            else
                errorText = page.saveError;
        }

        component SheetLabel: Text {
            Layout.fillWidth: true
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
            font.weight: Font.Medium
        }

        SheetLabel {
            text: qsTr("Среднее накопление за")
        }
        MobileSegmented {
            Layout.fillWidth: true
            options: [
                { label: qsTr("3 мес"), value: 3 },
                { label: qsTr("6 мес"), value: 6 },
                { label: qsTr("12 мес"), value: 12 }
            ]
            value: settingsSheet.analysisMonths
            onActivated: function(chosen) { settingsSheet.analysisMonths = chosen; }
        }

        SheetLabel {
            text: qsTr("Горизонт прогноза")
        }
        MobileSegmented {
            Layout.fillWidth: true
            options: [
                { label: qsTr("6 мес"), value: 6 },
                { label: qsTr("1 год"), value: 12 },
                { label: qsTr("3 года"), value: 36 },
                { label: qsTr("5 лет"), value: 60 }
            ]
            value: settingsSheet.horizonMonths
            onActivated: function(chosen) { settingsSheet.horizonMonths = chosen; }
        }

        MobileField {
            id: returnField
            Layout.fillWidth: true
            label: qsTr("Доходность в год, %")
            inputMethodHints: Qt.ImhPreferNumbers
        }
        MobileField {
            id: inflationField
            Layout.fillWidth: true
            label: qsTr("Инфляция в год, %")
            inputMethodHints: Qt.ImhPreferNumbers
        }
        MobileField {
            id: incomeField
            Layout.fillWidth: true
            label: qsTr("Изменение доходов, %")
            inputMethodHints: Qt.ImhPreferNumbers
        }
        MobileField {
            id: expenseField
            Layout.fillWidth: true
            label: qsTr("Изменение расходов, %")
            inputMethodHints: Qt.ImhPreferNumbers
        }

        MobileCheckRow {
            Layout.fillWidth: true
            label: qsTr("Разовая покупка")
            checked: settingsSheet.purchaseEnabled
            onToggled: function(nowChecked) { settingsSheet.purchaseEnabled = nowChecked; }
        }
        MobileField {
            id: purchaseField
            visible: settingsSheet.purchaseEnabled
            Layout.fillWidth: true
            label: qsTr("Сумма покупки")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
        SheetLabel {
            visible: settingsSheet.purchaseEnabled
            text: qsTr("Через")
        }
        MobileSegmented {
            visible: settingsSheet.purchaseEnabled
            Layout.fillWidth: true
            options: [
                { label: qsTr("1 мес"), value: 1 },
                { label: qsTr("3 мес"), value: 3 },
                { label: qsTr("6 мес"), value: 6 },
                { label: qsTr("12 мес"), value: 12 },
                { label: qsTr("3 года"), value: 36 }
            ]
            value: settingsSheet.purchaseMonth
            onActivated: function(chosen) { settingsSheet.purchaseMonth = chosen; }
        }

        Text {
            Layout.fillWidth: true
            visible: settingsSheet.errorText.length > 0
            text: settingsSheet.errorText
            wrapMode: Text.Wrap
            color: MobileTheme.red
            font.pixelSize: MobileTheme.sizeCaption
        }

        MobileButton {
            Layout.fillWidth: true
            variant: "primary"
            text: qsTr("Пересчитать и сохранить")
            onClicked: settingsSheet.save()
        }
    }
}
