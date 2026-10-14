import QtQuick
import QtQuick.Layouts

Item {
    id: page

    readonly property var recurrenceOptions: [
        { label: qsTr("Ежедневно"), value: "daily" },
        { label: qsTr("Каждую неделю"), value: "weekly" },
        { label: qsTr("Каждый месяц"), value: "monthly_day" },
        { label: qsTr("День недели месяца"), value: "monthly_weekday" }
    ]
    readonly property var weekdayOptions: [
        { label: qsTr("Пн"), value: 1 },
        { label: qsTr("Вт"), value: 2 },
        { label: qsTr("Ср"), value: 3 },
        { label: qsTr("Чт"), value: 4 },
        { label: qsTr("Пт"), value: 5 },
        { label: qsTr("Сб"), value: 6 },
        { label: qsTr("Вс"), value: 7 }
    ]
    readonly property var weekOfMonthOptions: [
        { label: qsTr("Первая"), value: 1 },
        { label: qsTr("Вторая"), value: 2 },
        { label: qsTr("Третья"), value: 3 },
        { label: qsTr("Четвёртая"), value: 4 },
        { label: qsTr("Последняя"), value: 5 }
    ]

    function fiatAccounts() {
        const rows = financeController.allAccounts;
        const result = [];
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].asset === "fiat")
                result.push(rows[i]);
        return result;
    }

    function categoriesFor(kind) {
        return financeController.categories.filter(function(item) { return item.type === kind; });
    }

    function accountCurrency(accountId) {
        const rows = fiatAccounts();
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === accountId)
                return rows[i].currency;
        return financeController.appCurrency;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            boundsBehavior: Flickable.StopAtBounds
            model: financeController.scheduledTransactions
            footer: Item {
                width: list.width
                height: 72
            }

            delegate: MobileCard {
                id: scheduleCard

                required property var modelData
                readonly property var row: modelData

                width: list.width
                gap: 6

                TapHandler {
                    onTapped: sheet.openFor(scheduleCard.row)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: scheduleCard.row.name
                        elide: Text.ElideRight
                        color: MobileTheme.accent
                        font.pixelSize: MobileTheme.sizeHeading
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: MobileFormat.money(scheduleCard.row.type === "income" ? scheduleCard.row.amount : -scheduleCard.row.amount,
                                                 scheduleCard.row.currency, true)
                        color: scheduleCard.row.type === "income" ? MobileTheme.income : MobileTheme.red
                        font.pixelSize: MobileTheme.sizeBody
                        font.weight: Font.DemiBold
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: scheduleCard.row.scheduleText
                    wrapMode: Text.Wrap
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeCaption
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Следующая: %1").arg(MobileFormat.isoDate(scheduleCard.row.nextDate))
                        + " · " + scheduleCard.row.accountName
                    elide: Text.ElideRight
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }
            }
        }
    }

    MobileEmptyState {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: list.count === 0
        icon: "calendar"
        title: qsTr("Регулярных операций пока нет")
        text: qsTr("Зарплата, аренда или подписки будут попадать в операции автоматически.")
        actionText: qsTr("Добавить регулярную операцию")
        onAction: sheet.openFor(null)
    }

    MobileFab {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: sheet.openFor(null)
    }

    MobileSheet {
        id: sheet

        property string editingId: ""
        property string operationKind: "expense"
        property string accountId: ""
        property string categoryId: ""
        property string recurrence: "monthly_day"
        property int weekday: 1
        property int weekOfMonth: 1
        property string errorText: ""
        property bool confirmingDelete: false

        title: editingId.length > 0 ? qsTr("Регулярная операция") : qsTr("Новая регулярная операция")

        function openFor(row) {
            const accounts = page.fiatAccounts();
            editingId = row ? row.id : "";
            nameField.text = row ? row.name : "";
            operationKind = row ? row.type : "expense";
            amountField.text = row ? MobileFormat.amountText(row.amount) : "";
            accountId = row ? row.accountId : (accounts.length > 0 ? accounts[0].id : "");
            categoryId = row ? row.categoryId : "";
            recurrence = row ? row.recurrence : "monthly_day";
            weekday = row && row.weekday ? row.weekday : 1;
            weekOfMonth = row && row.weekOfMonth ? row.weekOfMonth : 1;
            dayField.text = row && row.dayOfMonth ? String(row.dayOfMonth) : "1";
            startField.text = row ? MobileFormat.isoDate(row.startsOn) : Qt.formatDate(new Date(), "dd.MM.yyyy");
            errorText = "";
            confirmingDelete = false;
            open();
        }

        function save() {
            const date = MobileFormat.dateFromInput(startField.text);
            if (!date) {
                errorText = qsTr("Укажите дату начала в формате дд.мм.гггг");
                return;
            }
            const result = financeController.saveScheduledTransaction({
                id: editingId,
                name: nameField.text,
                accountId: accountId,
                categoryId: categoryId,
                type: operationKind,
                amount: MobileFormat.minorFromText(amountField.text),
                currency: page.accountCurrency(accountId),
                recurrence: recurrence,
                weekday: weekday,
                dayOfMonth: Math.round(MobileFormat.numberFromText(dayField.text)),
                weekOfMonth: weekOfMonth,
                startsOn: MobileFormat.isoFromDate(date)
            });
            if (result.ok)
                sheet.close();
            else
                errorText = result.error || qsTr("Не удалось сохранить операцию");
        }

        component SheetLabel: Text {
            Layout.fillWidth: true
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
            font.weight: Font.Medium
        }

        MobileField {
            id: nameField
            Layout.fillWidth: true
            label: qsTr("Название")
            placeholder: qsTr("Например, аренда")
        }

        MobileSegmented {
            Layout.fillWidth: true
            options: [
                { label: qsTr("Расход"), value: "expense" },
                { label: qsTr("Доход"), value: "income" }
            ]
            value: sheet.operationKind
            onActivated: function(chosen) {
                if (sheet.operationKind !== chosen) {
                    sheet.operationKind = chosen;
                    sheet.categoryId = "";
                }
            }
        }

        MobileField {
            id: amountField
            Layout.fillWidth: true
            label: qsTr("Сумма")
            placeholder: "0"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }

        SheetLabel {
            text: qsTr("Счёт")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: page.fiatAccounts()

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.name + " · " + modelData.currency
                    selected: sheet.accountId === modelData.id
                    onClicked: sheet.accountId = modelData.id
                }
            }
        }

        SheetLabel {
            text: qsTr("Категория")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: page.categoriesFor(sheet.operationKind)

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.label
                    selected: sheet.categoryId === modelData.value
                    onClicked: sheet.categoryId = modelData.value
                }
            }
        }

        SheetLabel {
            text: qsTr("Повторять")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: page.recurrenceOptions

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.label
                    selected: sheet.recurrence === modelData.value
                    onClicked: sheet.recurrence = modelData.value
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            visible: sheet.recurrence === "weekly" || sheet.recurrence === "monthly_weekday"
            spacing: 8

            Repeater {
                model: page.weekdayOptions

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.label
                    selected: sheet.weekday === modelData.value
                    onClicked: sheet.weekday = modelData.value
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            visible: sheet.recurrence === "monthly_weekday"
            spacing: 8

            Repeater {
                model: page.weekOfMonthOptions

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.label
                    selected: sheet.weekOfMonth === modelData.value
                    onClicked: sheet.weekOfMonth = modelData.value
                }
            }
        }

        MobileField {
            id: dayField
            visible: sheet.recurrence === "monthly_day"
            Layout.fillWidth: true
            label: qsTr("День месяца, 1–31")
            inputMethodHints: Qt.ImhDigitsOnly
        }

        MobileField {
            id: startField
            Layout.fillWidth: true
            label: qsTr("Дата начала")
            placeholder: qsTr("дд.мм.гггг")
        }

        Text {
            Layout.fillWidth: true
            visible: sheet.errorText.length > 0
            text: sheet.errorText
            wrapMode: Text.Wrap
            color: MobileTheme.red
            font.pixelSize: MobileTheme.sizeCaption
        }

        MobileButton {
            Layout.fillWidth: true
            variant: "primary"
            text: qsTr("Сохранить")
            onClicked: sheet.save()
        }

        MobileButton {
            Layout.fillWidth: true
            visible: sheet.editingId.length > 0
            variant: "danger"
            text: sheet.confirmingDelete ? qsTr("Нажмите ещё раз, чтобы удалить") : qsTr("Удалить расписание")
            onClicked: {
                if (!sheet.confirmingDelete) {
                    sheet.confirmingDelete = true;
                    return;
                }
                if (financeController.deleteScheduledTransaction(sheet.editingId))
                    sheet.close();
                else
                    sheet.errorText = qsTr("Не удалось удалить расписание");
            }
        }
    }
}
