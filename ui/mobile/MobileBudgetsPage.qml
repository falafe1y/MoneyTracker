import QtQuick
import QtQuick.Layouts

Item {
    id: page

    function shiftMonth(offset) {
        const parts = String(financeController.selectedBudgetMonth).split("-");
        const base = parts.length >= 2 ? new Date(Number(parts[0]), Number(parts[1]) - 1, 1) : new Date();
        const date = new Date(base.getFullYear(), base.getMonth() + offset, 1);
        const month = date.getMonth() + 1;
        financeController.selectedBudgetMonth = date.getFullYear() + "-" + (month < 10 ? "0" : "") + month + "-01";
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MobileIconButton {
                icon: "chevronLeft"
                onClicked: page.shiftMonth(-1)
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: MobileFormat.monthLabel(financeController.selectedBudgetMonth)
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeHeading
                font.weight: Font.DemiBold
            }
            MobileIconButton {
                icon: "chevronRight"
                onClicked: page.shiftMonth(1)
            }
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            boundsBehavior: Flickable.StopAtBounds
            model: financeController.budgets
            footer: Item {
                width: list.width
                height: 72
            }

            delegate: MobileCard {
                id: budgetCard

                required property var modelData
                readonly property var row: modelData
                readonly property int remaining: row.displayLimitMinor - row.displaySpentMinor

                width: list.width
                gap: 10

                TapHandler {
                    onTapped: budgetSheet.openFor(budgetCard.row)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: budgetCard.row.name
                        elide: Text.ElideRight
                        color: MobileTheme.accent
                        font.pixelSize: MobileTheme.sizeHeading
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: budgetCard.row.allAccounts ? qsTr("Все счета") : qsTr("Часть счетов")
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeCaption
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: MobileFormat.money(budgetCard.row.displaySpentMinor, budgetCard.row.displayCurrency, false)
                        color: budgetCard.row.progress > 1 ? MobileTheme.red : MobileTheme.accent
                        font.pixelSize: MobileTheme.sizeTitle
                        font.weight: Font.Bold
                    }
                    Text {
                        text: qsTr("из %1").arg(MobileFormat.money(budgetCard.row.displayLimitMinor, budgetCard.row.displayCurrency, false))
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeBody
                    }
                }

                MobileProgress {
                    Layout.fillWidth: true
                    value: budgetCard.row.progress
                }

                Text {
                    Layout.fillWidth: true
                    text: budgetCard.remaining < 0
                        ? qsTr("Превышено на %1").arg(MobileFormat.money(-budgetCard.remaining, budgetCard.row.displayCurrency, false))
                        : qsTr("Осталось %1").arg(MobileFormat.money(budgetCard.remaining, budgetCard.row.displayCurrency, false))
                    color: budgetCard.remaining < 0 ? MobileTheme.red : MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }

                Text {
                    Layout.fillWidth: true
                    visible: budgetCard.row.rateMissing
                    text: qsTr("Нет курса для пересчёта в валюту бюджета")
                    wrapMode: Text.Wrap
                    color: MobileTheme.red
                    font.pixelSize: MobileTheme.sizeCaption
                }

                Repeater {
                    model: budgetCard.row.categoryLimits.filter(function(item) { return item.configured; })

                    delegate: ColumnLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                color: MobileTheme.accent
                                font.pixelSize: MobileTheme.sizeCaption
                                font.weight: Font.Medium
                            }
                            Text {
                                text: MobileFormat.money(modelData.spentMinor, budgetCard.row.currency, false)
                                    + " / " + MobileFormat.money(modelData.limitMinor, budgetCard.row.currency, false)
                                color: MobileTheme.muted
                                font.pixelSize: MobileTheme.sizeCaption
                            }
                        }

                        MobileProgress {
                            Layout.fillWidth: true
                            value: modelData.progress
                        }
                    }
                }
            }
        }
    }

    MobileEmptyState {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: list.count === 0
        svg: "qrc:/icons/budgets.svg"
        title: qsTr("Бюджетов пока нет")
        text: qsTr("Создайте бюджет на месяц по счетам или категориям и следите за остатком.")
        actionText: qsTr("Создать бюджет")
        onAction: budgetSheet.openFor(null)
    }

    MobileFab {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: budgetSheet.openFor(null)
    }

    MobileBudgetSheet {
        id: budgetSheet
    }
}
