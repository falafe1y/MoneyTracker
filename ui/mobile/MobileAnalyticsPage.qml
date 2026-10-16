import QtQuick
import QtQuick.Layouts

Flickable {
    id: page

    property int expenseYear: new Date().getFullYear()

    readonly property var expenseRows: MobileAnalytics.expenseRowsForYear(page.expenseYear)
    readonly property var capitalRub: financeController.capitalHistoryRub || []
    readonly property var capitalCurrency: financeController.capitalHistoryAnalyticsCurrency || []

    function timeFractions(rows) {
        if (rows.length === 0)
            return [];
        const first = MobileAnalytics.dateFromIso(rows[0].date).getTime();
        const last = MobileAnalytics.dateFromIso(rows[rows.length - 1].date).getTime();
        const span = Math.max(1, last - first);
        return rows.map(function(row) {
            if (rows.length === 1 || last === first)
                return 0.5;
            return (MobileAnalytics.dateFromIso(row.date).getTime() - first) / span;
        });
    }

    function timeLabels(rows) {
        if (rows.length === 0)
            return [];
        const first = MobileAnalytics.dateFromIso(rows[0].date);
        const last = MobileAnalytics.dateFromIso(rows[rows.length - 1].date);
        if (rows.length === 1 || first.getTime() === last.getTime())
            return [{ fraction: 0.5, text: Qt.formatDate(first, "dd.MM.yyyy"), align: "center" }];
        const middle = new Date((first.getTime() + last.getTime()) / 2);
        return [
            { fraction: 0, text: Qt.formatDate(first, "dd.MM.yyyy"), align: "left" },
            { fraction: 0.5, text: Qt.formatDate(middle, "dd.MM.yyyy"), align: "center" },
            { fraction: 1, text: Qt.formatDate(last, "dd.MM.yyyy"), align: "right" }
        ];
    }

    function capitalTooltips(rows, currency) {
        return rows.map(function(row) {
            return MobileAnalytics.capitalDateLabel(row) + " · " + MobileFormat.money(row.totalMinor, currency, false);
        });
    }

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: layout.implicitHeight + 16

    ColumnLayout {
        id: layout

        width: page.width
        spacing: 14

        MobileInvestmentPerformance {
            Layout.fillWidth: true
        }

        AverageReturnPanel {
            Layout.fillWidth: true
            controller: financeController
            surface: MobileTheme.panel
            ink: MobileTheme.accent
            muted: MobileTheme.muted
            line: MobileTheme.line
            positive: MobileTheme.income
            negative: MobileTheme.red
        }

        MobileCard {
            Layout.fillWidth: true
            gap: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("История расходов по месяцам")
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeHeading
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MobileIconButton {
                    icon: "chevronLeft"
                    enabled: page.expenseYear > MobileAnalytics.expenseHistoryFirstYear()
                    onClicked: page.expenseYear -= 1
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.expenseYear
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeHeading
                    font.weight: Font.DemiBold
                }
                MobileIconButton {
                    icon: "chevronRight"
                    enabled: page.expenseYear < MobileAnalytics.expenseHistoryLastYear()
                    onClicked: page.expenseYear += 1
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("В среднем %1").arg(MobileFormat.money(MobileAnalytics.averageCompletedMonthExpense(page.expenseRows), "RUB", false))
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            MobileBarChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                points: page.expenseRows
                currency: "RUB"
                averageMinor: MobileAnalytics.averageCompletedMonthExpense(page.expenseRows)
            }
        }

        MobileCard {
            Layout.fillWidth: true
            gap: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("История капитала в рублях")
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeHeading
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
                Text {
                    text: MobileAnalytics.historyResolutionLabel(page.capitalRub)
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }
            }

            MobileSeriesChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                visible: page.capitalRub.length > 0
                series: [{ values: page.capitalRub.map(function(row) { return Number(row.totalMinor); }), color: MobileTheme.chartColors[2], width: 2.5 }]
                xFractions: page.timeFractions(page.capitalRub)
                xLabels: page.timeLabels(page.capitalRub)
                tooltips: page.capitalTooltips(page.capitalRub, "RUB")
                formatValue: function(value) { return MobileAnalytics.capitalAxisMoney(value, "RUB"); }
            }
            Text {
                Layout.fillWidth: true
                visible: page.capitalRub.length === 0
                text: qsTr("Добавьте операцию — здесь появится история капитала")
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
                wrapMode: Text.Wrap
            }
        }

        MobileCard {
            Layout.fillWidth: true
            gap: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Капитал в выбранной валюте")
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeHeading
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: ["USD", "EUR", "CNY"]

                    delegate: MobileChip {
                        required property string modelData
                        text: modelData
                        selected: financeController.analyticsCurrency === modelData
                        onClicked: financeController.analyticsCurrency = modelData
                    }
                }
            }

            MobileSeriesChart {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                visible: page.capitalCurrency.length > 0
                series: [{ values: page.capitalCurrency.map(function(row) { return Number(row.totalMinor); }), color: MobileTheme.chartColors[2], width: 2.5 }]
                xFractions: page.timeFractions(page.capitalCurrency)
                xLabels: page.timeLabels(page.capitalCurrency)
                tooltips: page.capitalTooltips(page.capitalCurrency, financeController.analyticsCurrency)
                formatValue: function(value) { return MobileAnalytics.capitalAxisMoney(value, financeController.analyticsCurrency); }
            }
            Text {
                Layout.fillWidth: true
                visible: page.capitalCurrency.length === 0
                text: qsTr("Добавьте операцию — здесь появится история капитала")
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
                wrapMode: Text.Wrap
            }
        }
    }
}
