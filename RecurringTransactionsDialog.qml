import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    width: Math.min(860, parent ? parent.width - 40 : 860)
    height: Math.min(720, parent ? parent.height - 40 : 720)
    modal: true
    anchors.centerIn: parent
    padding: 0
    closePolicy: Popup.CloseOnEscape

    required property var controller
    property var moneyFormatter
    property color panelColor: "#FFFFF8"
    property color softColor: "#FFFFF0"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color accentColor: "#031528"
    property color errorColor: "#B94F48"
    property color successColor: "#3F735F"
    property int pageIndex: 0
    property string editingId: ""
    property date startsOn: new Date()
    property string statusText: ""

    function openManager() {
        pageIndex = 0;
        editingId = "";
        statusText = "";
        open();
    }

    function fiatAccounts() {
        const result = [];
        const accounts = controller.allAccounts;
        for (let i = 0; i < accounts.length; ++i) {
            if (accounts[i].asset === "fiat")
                result.push({
                    label: accounts[i].name + " · " + accounts[i].currency,
                    value: accounts[i].id,
                    currency: accounts[i].currency
                });
        }
        return result;
    }

    function categoryItems(type) {
        const result = [];
        const categories = controller.categories;
        for (let i = 0; i < categories.length; ++i) {
            if (categories[i].type === type)
                result.push(categories[i]);
        }
        return result;
    }

    function indexByValue(model, value) {
        for (let i = 0; i < model.length; ++i)
            if (model[i].value === value)
                return i;
        return model.length > 0 ? 0 : -1;
    }

    function selectAccountCurrency() {
        const items = accountBox.model;
        const index = accountBox.currentIndex;
        if (index >= 0 && index < items.length)
            currencyBox.currentIndex = indexByValue(
                currencyBox.model, items[index].currency);
    }

    function dateFromIso(value) {
        const parts = String(value || "").split("-");
        if (parts.length !== 3)
            return new Date();
        return new Date(Number(parts[0]), Number(parts[1]) - 1,
                        Number(parts[2]), 12, 0, 0, 0);
    }

    function dateToIso(value) {
        const month = value.getMonth() + 1;
        const day = value.getDate();
        return value.getFullYear() + "-"
             + (month < 10 ? "0" : "") + month + "-"
             + (day < 10 ? "0" : "") + day;
    }

    function amountForInput(minor) {
        const value = Math.max(0, Math.round(Number(minor)));
        const whole = Math.floor(value / 100);
        const cents = value % 100;
        return cents === 0 ? String(whole)
                           : String(whole) + ","
                             + (cents < 10 ? "0" : "") + cents;
    }

    function formatAmount(row) {
        return moneyFormatter
            ? moneyFormatter(row.amount, row.currency, false)
            : String(Number(row.amount) / 100) + " " + row.currency;
    }

    function dayItems() {
        const result = [];
        for (let day = 1; day <= 31; ++day)
            result.push({ label: String(day), value: day });
        return result;
    }

    function openForNew() {
        editingId = "";
        nameField.clear();
        amountField.clear();
        typeBox.currentIndex = 1;
        accountBox.model = fiatAccounts();
        accountBox.currentIndex = accountBox.model.length > 0 ? 0 : -1;
        selectAccountCurrency();
        categoryBox.model = categoryItems("expense");
        categoryBox.currentIndex = categoryBox.model.length > 0 ? 0 : -1;
        recurrenceBox.currentIndex = 0;
        weekdayBox.currentIndex = (new Date().getDay() + 6) % 7;
        dayBox.currentIndex = Math.max(0, new Date().getDate() - 1);
        weekBox.currentIndex = 0;
        startsOn = new Date();
        formError.text = "";
        pageIndex = 1;
        Qt.callLater(function() { nameField.forceActiveFocus(); });
    }

    function openForEdit(row) {
        editingId = row.id;
        nameField.text = row.name;
        amountField.text = amountForInput(row.amount);
        typeBox.currentIndex = row.type === "income" ? 0 : 1;
        accountBox.model = fiatAccounts();
        accountBox.currentIndex = indexByValue(accountBox.model, row.accountId);
        currencyBox.currentIndex = indexByValue(currencyBox.model, row.currency);
        categoryBox.model = categoryItems(row.type);
        categoryBox.currentIndex = indexByValue(categoryBox.model, row.categoryId);
        recurrenceBox.currentIndex = row.recurrence === "daily" ? 0
                                   : row.recurrence === "weekly" ? 1
                                   : row.recurrence === "monthly_day" ? 2 : 3;
        weekdayBox.currentIndex = indexByValue(weekdayBox.model, row.weekday);
        dayBox.currentIndex = indexByValue(dayBox.model, row.dayOfMonth);
        weekBox.currentIndex = indexByValue(weekBox.model, row.weekOfMonth);
        startsOn = dateFromIso(row.startsOn);
        formError.text = "";
        pageIndex = 1;
        Qt.callLater(function() { nameField.forceActiveFocus(); });
    }

    function submit() {
        const minor = Math.round(
            (Number(amountField.text.replace(",", ".")) || 0) * 100
        );
        const result = controller.saveScheduledTransaction({
            id: editingId,
            name: nameField.text,
            accountId: accountBox.currentValue || "",
            categoryId: categoryBox.currentValue || "",
            type: typeBox.currentValue,
            amount: minor,
            currency: currencyBox.currentValue,
            recurrence: recurrenceBox.currentValue,
            weekday: weekdayBox.currentValue,
            dayOfMonth: dayBox.currentValue,
            weekOfMonth: weekBox.currentValue,
            startsOn: dateToIso(startsOn)
        });
        if (!result.ok) {
            formError.text = result.error || qsTr("Не удалось сохранить операцию");
            return;
        }
        statusText = editingId.length > 0
                   ? qsTr("Расписание обновлено")
                   : qsTr("Операция запланирована");
        editingId = "";
        pageIndex = 0;
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        context: Qt.ApplicationShortcut
        enabled: dialog.visible && dialog.pageIndex === 1
              && !typeBox.activeFocus && !typeBox.popup.visible
              && !accountBox.activeFocus && !accountBox.popup.visible
              && !categoryBox.activeFocus && !categoryBox.popup.visible
              && !recurrenceBox.activeFocus && !recurrenceBox.popup.visible
              && !weekdayBox.activeFocus && !weekdayBox.popup.visible
              && !dayBox.activeFocus && !dayBox.popup.visible
              && !weekBox.activeFocus && !weekBox.popup.visible
              && !startDateButton.activeFocus
              && !formCancelButton.activeFocus
              && !formSubmitButton.activeFocus
              && !dateDialog.visible
        onActivated: dialog.submit()
    }

    component FormField: StyledTextField {
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appOnAccentColor: dialog.panelColor
    }

    component FormCombo: StyledComboBox {
        textRole: "label"
        valueRole: "value"
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: "#E4E8F1"
    }

    component FormButton: StyledButton {
        leftPadding: 14
        rightPadding: 14
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: "#E4E8F1"
        appOnAccentColor: dialog.panelColor
        appErrorColor: dialog.errorColor
    }

    background: Rectangle {
        color: dialog.panelColor
        radius: 18
        border.width: 1
        border.color: dialog.lineColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            spacing: 12
            FormButton {
                visible: dialog.pageIndex === 1
                text: "‹"
                onClicked: dialog.pageIndex = 0
            }
            Text {
                Layout.fillWidth: true
                text: dialog.pageIndex === 0
                    ? qsTr("Запланированные операции")
                    : dialog.editingId.length > 0
                      ? qsTr("Редактирование расписания")
                      : qsTr("Новая запланированная операция")
                color: dialog.textColor
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            FormButton {
                visible: dialog.pageIndex === 0
                text: qsTr("+ Добавить")
                primary: true
                onClicked: dialog.openForNew()
            }
            FormButton {
                text: qsTr("Закрыть")
                onClicked: dialog.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: dialog.pageIndex

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        visible: dialog.statusText.length > 0
                        text: dialog.statusText
                        color: dialog.successColor
                    }

                    ListView {
                        id: scheduleList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        clip: true
                        model: dialog.controller.scheduledTransactions

                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 98
                            radius: 12
                            color: dialog.softColor
                            border.width: 1
                            border.color: dialog.lineColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 13
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: dialog.textColor
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.scheduleText
                                            + " · " + qsTr("следующая: %1")
                                                .arg(Qt.formatDate(
                                                    dialog.dateFromIso(modelData.nextDate),
                                                    "dd.MM.yyyy"))
                                        color: dialog.mutedColor
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.accountName
                                            + " · " + modelData.categoryName
                                        color: dialog.mutedColor
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    text: (modelData.type === "income" ? "+" : "−")
                                        + dialog.formatAmount(modelData)
                                    color: modelData.type === "income"
                                         ? dialog.successColor : dialog.errorColor
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                FormButton {
                                    text: qsTr("Изменить")
                                    onClicked: dialog.openForEdit(modelData)
                                }
                                FormButton {
                                    text: qsTr("Удалить")
                                    destructive: true
                                    onClicked: deleteDialog.openFor(modelData)
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: scheduleList.count === 0
                            text: qsTr("Запланированных операций пока нет")
                            color: dialog.mutedColor
                        }
                    }
                }
            }

            ScrollView {
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: Math.max(0, parent.width - 40)
                    x: 20
                    spacing: 11

                    Text { text: qsTr("Название"); color: dialog.mutedColor }
                    FormField {
                        id: nameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Например, аренда квартиры")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Тип"); color: dialog.mutedColor }
                            FormCombo {
                                id: typeBox
                                Layout.fillWidth: true
                                model: [
                                    { label: qsTr("Доход"), value: "income" },
                                    { label: qsTr("Расход"), value: "expense" }
                                ]
                                onActivated: {
                                    categoryBox.model = dialog.categoryItems(currentValue);
                                    categoryBox.currentIndex = categoryBox.model.length > 0
                                                             ? 0 : -1;
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: qsTr("Сумма"); color: dialog.mutedColor }
                            FormField {
                                id: amountField
                                Layout.fillWidth: true
                                placeholderText: qsTr("0,00")
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                validator: DoubleValidator {
                                    bottom: 0.01
                                    top: 999999999
                                    decimals: 2
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.preferredWidth: 120
                            Text { text: qsTr("Валюта"); color: dialog.mutedColor }
                            FormCombo {
                                id: currencyBox
                                Layout.fillWidth: true
                                model: [
                                    { label: "RUB", value: "RUB" },
                                    { label: "USD", value: "USD" },
                                    { label: "EUR", value: "EUR" }
                                ]
                            }
                        }
                    }

                    Text { text: qsTr("Счёт"); color: dialog.mutedColor }
                    FormCombo {
                        id: accountBox
                        Layout.fillWidth: true
                        onActivated: dialog.selectAccountCurrency()
                    }
                    Text { text: qsTr("Категория"); color: dialog.mutedColor }
                    FormCombo { id: categoryBox; Layout.fillWidth: true }

                    Text { text: qsTr("Повторение"); color: dialog.mutedColor }
                    FormCombo {
                        id: recurrenceBox
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Ежедневно"), value: "daily" },
                            { label: qsTr("Каждую неделю"), value: "weekly" },
                            { label: qsTr("Каждый месяц в выбранное число"), value: "monthly_day" },
                            { label: qsTr("Выбранный день недели каждого месяца"), value: "monthly_weekday" }
                        ]
                    }

                    RowLayout {
                        visible: recurrenceBox.currentValue !== "daily"
                        Layout.fillWidth: true
                        spacing: 10
                        ColumnLayout {
                            visible: recurrenceBox.currentValue === "monthly_weekday"
                            Layout.fillWidth: true
                            Text { text: qsTr("Неделя месяца"); color: dialog.mutedColor }
                            FormCombo {
                                id: weekBox
                                Layout.fillWidth: true
                                model: [
                                    { label: qsTr("Первая"), value: 1 },
                                    { label: qsTr("Вторая"), value: 2 },
                                    { label: qsTr("Третья"), value: 3 },
                                    { label: qsTr("Четвёртая"), value: 4 },
                                    { label: qsTr("Последняя"), value: 5 }
                                ]
                            }
                        }
                        ColumnLayout {
                            visible: recurrenceBox.currentValue === "weekly"
                                  || recurrenceBox.currentValue === "monthly_weekday"
                            Layout.fillWidth: true
                            Text { text: qsTr("День недели"); color: dialog.mutedColor }
                            FormCombo {
                                id: weekdayBox
                                Layout.fillWidth: true
                                model: [
                                    { label: qsTr("Понедельник"), value: 1 },
                                    { label: qsTr("Вторник"), value: 2 },
                                    { label: qsTr("Среда"), value: 3 },
                                    { label: qsTr("Четверг"), value: 4 },
                                    { label: qsTr("Пятница"), value: 5 },
                                    { label: qsTr("Суббота"), value: 6 },
                                    { label: qsTr("Воскресенье"), value: 7 }
                                ]
                            }
                        }
                        ColumnLayout {
                            visible: recurrenceBox.currentValue === "monthly_day"
                            Layout.fillWidth: true
                            Text { text: qsTr("Число месяца"); color: dialog.mutedColor }
                            FormCombo {
                                id: dayBox
                                Layout.fillWidth: true
                                model: dialog.dayItems()
                            }
                        }
                    }

                    Text { text: qsTr("Начать с"); color: dialog.mutedColor }
                    FormButton {
                        id: startDateButton
                        Layout.fillWidth: true
                        text: Qt.formatDate(dialog.startsOn, "dd.MM.yyyy")
                        onClicked: dateDialog.openFor(dialog.startsOn)
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: recurrenceBox.currentValue === "monthly_day"
                        text: qsTr("Если выбранного числа нет в месяце, операция будет создана в последний день месяца.")
                        color: dialog.mutedColor
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        id: formError
                        Layout.fillWidth: true
                        color: dialog.errorColor
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        FormButton {
                            id: formCancelButton
                            text: qsTr("Отмена")
                            onClicked: dialog.pageIndex = 0
                        }
                        FormButton {
                            id: formSubmitButton
                            text: dialog.editingId.length > 0
                                ? qsTr("Сохранить") : qsTr("Добавить")
                            primary: true
                            onClicked: dialog.submit()
                        }
                    }
                    Item { Layout.preferredHeight: 12 }
                }
            }
        }
    }

    Dialog {
        id: deleteDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        modal: true
        padding: 22
        property var row: null
        function openFor(value) {
            row = value;
            deleteError.text = "";
            open();
        }
        function confirmDelete() {
            if (row && dialog.controller.deleteScheduledTransaction(row.id)) {
                dialog.statusText = qsTr("Расписание удалено");
                close();
            } else {
                deleteError.text = qsTr("Не удалось удалить расписание");
            }
        }
        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: deleteDialog.visible
                  && !deleteCancelButton.activeFocus
                  && !deleteSubmitButton.activeFocus
            onActivated: deleteDialog.confirmDelete()
        }
        background: Rectangle {
            color: dialog.panelColor
            radius: 16
            border.width: 1
            border.color: dialog.lineColor
        }
        contentItem: ColumnLayout {
            spacing: 13
            Text {
                text: qsTr("Удалить расписание?")
                color: dialog.textColor
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Будущие операции больше не будут создаваться. Уже добавленные в историю операции сохранятся.")
                color: dialog.mutedColor
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                text: deleteDialog.row ? deleteDialog.row.name : ""
                color: dialog.textColor
                font.weight: Font.DemiBold
            }
            Text { id: deleteError; color: dialog.errorColor }
            RowLayout {
                Item { Layout.fillWidth: true }
                FormButton {
                    id: deleteCancelButton
                    text: qsTr("Отмена")
                    onClicked: deleteDialog.close()
                }
                FormButton {
                    id: deleteSubmitButton
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteDialog.confirmDelete()
                }
            }
        }
    }

    Dialog {
        id: dateDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 400
        modal: true
        padding: 20
        property int displayedMonth: new Date().getMonth()
        property int displayedYear: new Date().getFullYear()
        function openFor(date) {
            displayedMonth = date.getMonth();
            displayedYear = date.getFullYear();
            open();
        }
        function shiftMonth(offset) {
            const shifted = new Date(displayedYear, displayedMonth + offset, 1);
            displayedMonth = shifted.getMonth();
            displayedYear = shifted.getFullYear();
        }
        background: Rectangle {
            color: dialog.panelColor
            radius: 16
            border.width: 1
            border.color: dialog.lineColor
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: qsTr("Дата начала")
                color: dialog.textColor
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            RowLayout {
                Layout.fillWidth: true
                FormButton { text: "‹"; onClicked: dateDialog.shiftMonth(-1) }
                Text {
                    Layout.fillWidth: true
                    text: startCalendar.title
                    color: dialog.textColor
                    horizontalAlignment: Text.AlignHCenter
                    font.weight: Font.DemiBold
                }
                FormButton { text: "›"; onClicked: dateDialog.shiftMonth(1) }
            }
            DayOfWeekRow {
                Layout.fillWidth: true
                locale: Qt.locale(dialog.controller.uiLanguage === "en"
                                  ? "en_US" : "ru_RU")
                delegate: Text {
                    required property string shortName
                    text: shortName
                    color: dialog.mutedColor
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            MonthGrid {
                id: startCalendar
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                month: dateDialog.displayedMonth
                year: dateDialog.displayedYear
                locale: Qt.locale(dialog.controller.uiLanguage === "en"
                                  ? "en_US" : "ru_RU")
                delegate: Button {
                    id: calendarDay
                    required property var model
                    flat: true
                    opacity: model.month === startCalendar.month ? 1 : 0.42
                    onClicked: {
                        dialog.startsOn = new Date(
                            model.date.getFullYear(), model.date.getMonth(),
                            model.date.getDate(), 12, 0, 0, 0);
                        dateDialog.close();
                    }
                    contentItem: Text {
                        text: calendarDay.model.day
                        color: dialog.textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: calendarDay.hovered
                             ? dialog.softColor : "transparent"
                    }
                }
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                FormButton {
                    text: qsTr("Сегодня")
                    onClicked: {
                        dialog.startsOn = new Date();
                        dateDialog.close();
                    }
                }
                FormButton {
                    text: qsTr("Отмена")
                    onClicked: dateDialog.close()
                }
            }
        }
    }
}
