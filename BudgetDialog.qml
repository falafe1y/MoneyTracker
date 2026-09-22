import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    width: Math.min(760, parent ? parent.width - 40 : 760)
    height: Math.min(780, parent ? parent.height - 40 : 780)
    modal: true
    anchors.centerIn: parent
    padding: 0
    closePolicy: Popup.CloseOnEscape

    required property var controller
    property color panelColor: "#FFFFF8"
    property color softColor: "#FFFFF0"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color accentColor: "#031528"
    property color errorColor: "#B94F48"
    property string editingId: ""

    ListModel { id: accountSelection }
    ListModel { id: categorySelection }

    function contains(values, value) {
        if (!values)
            return false;
        for (let i = 0; i < values.length; ++i)
            if (values[i] === value)
                return true;
        return false;
    }

    function amountForInput(minor) {
        const value = Math.max(0, Math.round(Number(minor)));
        const whole = Math.floor(value / 100);
        const cents = value % 100;
        return cents === 0 ? String(whole)
                           : String(whole) + ","
                             + (cents < 10 ? "0" : "") + cents;
    }

    function limitFor(categoryRows, categoryId) {
        if (!categoryRows)
            return null;
        for (let i = 0; i < categoryRows.length; ++i) {
            if (categoryRows[i].categoryId === categoryId
                    && categoryRows[i].configured)
                return categoryRows[i].limitMinor;
        }
        return null;
    }

    function rebuildSelections(row) {
        accountSelection.clear();
        const accounts = controller.allAccounts;
        for (let i = 0; i < accounts.length; ++i) {
            accountSelection.append({
                accountId: accounts[i].id,
                label: accounts[i].name + " · " + accounts[i].currency,
                selected: row ? contains(row.accountIds, accounts[i].id) : false
            });
        }

        categorySelection.clear();
        const categories = controller.categories;
        for (let j = 0; j < categories.length; ++j) {
            if (categories[j].type !== "expense"
                    || categories[j].value === "transfer-out"
                    || categories[j].value === "transfer-in")
                continue;
            const savedLimit = row
                ? limitFor(row.categoryLimits, categories[j].value) : null;
            categorySelection.append({
                categoryId: categories[j].value,
                label: categories[j].label,
                selected: savedLimit !== null,
                limitText: savedLimit !== null && Number(savedLimit) > 0
                         ? amountForInput(savedLimit) : ""
            });
        }
    }

    function openForNew() {
        editingId = "";
        nameField.clear();
        amountField.clear();
        currencyBox.currentIndex = Math.max(
            0, currencyBox.model.indexOf(controller.appCurrency));
        allAccountsBox.checked = true;
        allCategoriesBox.checked = true;
        rebuildSelections(null);
        formError.text = "";
        open();
        Qt.callLater(function() { nameField.forceActiveFocus(); });
    }

    function openForEdit(row) {
        if (!row)
            return;
        editingId = row.id;
        nameField.text = row.name;
        amountField.text = amountForInput(row.limitMinor);
        currencyBox.currentIndex = Math.max(
            0, currencyBox.model.indexOf(row.currency));
        allAccountsBox.checked = row.allAccounts;
        allCategoriesBox.checked = row.allCategories;
        rebuildSelections(row);
        formError.text = "";
        open();
        Qt.callLater(function() {
            nameField.forceActiveFocus();
            nameField.selectAll();
        });
    }

    function submit() {
        const accountIds = [];
        for (let i = 0; i < accountSelection.count; ++i) {
            const row = accountSelection.get(i);
            if (row.selected)
                accountIds.push(row.accountId);
        }
        const categoryLimits = [];
        for (let j = 0; j < categorySelection.count; ++j) {
            const category = categorySelection.get(j);
            if (!category.selected)
                continue;
            const value = Number(String(category.limitText).replace(",", "."));
            categoryLimits.push({
                categoryId: category.categoryId,
                limitMinor: Number.isFinite(value) && value > 0
                          ? Math.round(value * 100) : 0
            });
        }
        const amount = Number(amountField.text.replace(",", "."));
        const result = controller.saveBudget({
            id: editingId,
            name: nameField.text,
            currency: currencyBox.currentText,
            limitMinor: Number.isFinite(amount) ? Math.round(amount * 100) : 0,
            allAccounts: allAccountsBox.checked,
            accountIds: accountIds,
            allCategories: allCategoriesBox.checked,
            categoryLimits: categoryLimits
        });
        if (!result.ok) {
            formError.text = result.error || qsTr("Не удалось сохранить бюджет");
            return;
        }
        close();
    }

    onClosed: editingId = ""

    background: Rectangle {
        color: dialog.panelColor
        radius: 18
        border.width: 1
        border.color: dialog.lineColor
    }

    component Field: TextField {
        id: field
        implicitHeight: 44
        leftPadding: 13
        rightPadding: 13
        color: dialog.textColor
        placeholderTextColor: dialog.mutedColor
        background: Rectangle {
            radius: 10
            color: field.activeFocus ? dialog.panelColor : dialog.softColor
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus
                        ? dialog.accentColor : dialog.lineColor
        }
    }

    component ActionButton: Button {
        id: button
        property bool primary: false
        implicitHeight: 40
        leftPadding: 16
        rightPadding: 16
        contentItem: Text {
            text: button.text
            color: button.primary ? dialog.panelColor : dialog.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 9
            color: button.primary ? dialog.accentColor : dialog.panelColor
            border.width: button.primary ? 0 : 1
            border.color: dialog.lineColor
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            Text {
                Layout.fillWidth: true
                text: dialog.editingId.length > 0
                    ? qsTr("Редактирование бюджета") : qsTr("Новый бюджет")
                color: dialog.textColor
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            ActionButton { text: qsTr("Закрыть"); onClicked: dialog.close() }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: Math.max(0, parent.width - 40)
                x: 20
                spacing: 12

                Text { text: qsTr("Название"); color: dialog.mutedColor }
                Field {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Например, общий бюджет")
                    maximumLength: 80
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("Лимит на месяц"); color: dialog.mutedColor }
                        Field {
                            id: amountField
                            Layout.fillWidth: true
                            placeholderText: qsTr("0,00")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator { bottom: 0.01; decimals: 2 }
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 130
                        Text { text: qsTr("Валюта"); color: dialog.mutedColor }
                        ComboBox {
                            id: currencyBox
                            Layout.fillWidth: true
                            implicitHeight: 44
                            model: ["RUB", "USD", "EUR"]
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }
                CheckBox {
                    id: allAccountsBox
                    text: qsTr("Учитывать все счета")
                    palette.text: dialog.textColor
                }
                Text {
                    visible: !allAccountsBox.checked
                    text: qsTr("Выбранные счета")
                    color: dialog.mutedColor
                }
                Repeater {
                    model: accountSelection
                    delegate: CheckBox {
                        required property int index
                        required property string label
                        required property bool selected
                        visible: !allAccountsBox.checked
                        text: label
                        checked: selected
                        palette.text: dialog.textColor
                        onToggled: accountSelection.setProperty(index, "selected", checked)
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }
                CheckBox {
                    id: allCategoriesBox
                    text: qsTr("Учитывать все категории расходов")
                    palette.text: dialog.textColor
                }
                Text {
                    Layout.fillWidth: true
                    text: allCategoriesBox.checked
                        ? qsTr("Отметьте категории, если хотите задать им отдельные лимиты")
                        : qsTr("Отметьте категории, которые входят в бюджет")
                    color: dialog.mutedColor
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: categorySelection
                    delegate: RowLayout {
                        required property int index
                        required property string label
                        required property bool selected
                        required property string limitText
                        Layout.fillWidth: true
                        CheckBox {
                            checked: parent.selected
                            text: parent.label
                            palette.text: dialog.textColor
                            Layout.fillWidth: true
                            onToggled: categorySelection.setProperty(
                                parent.index, "selected", checked)
                        }
                        Field {
                            visible: parent.selected
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("Лимит, необязательно")
                            text: parent.limitText
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onTextEdited: categorySelection.setProperty(
                                parent.index, "limitText", text)
                        }
                    }
                }

                Text {
                    id: formError
                    Layout.fillWidth: true
                    color: dialog.errorColor
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                Item { Layout.preferredHeight: 8 }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Item { Layout.fillWidth: true }
            ActionButton { text: qsTr("Отмена"); onClicked: dialog.close() }
            ActionButton {
                text: qsTr("Сохранить")
                primary: true
                onClicked: dialog.submit()
            }
        }
    }
}
