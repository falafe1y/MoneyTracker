import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    width: 920
    height: Math.min(780, parent ? parent.height - 40 : 780)
    modal: true
    anchors.centerIn: parent
    padding: 0
    closePolicy: Popup.CloseOnEscape

    required property var controller
    property color panelColor: "white"
    property color softColor: "#faf9ec"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#d8d7c7"
    property color accentColor: "#031528"
    property color errorColor: "#b94f48"
    property color successColor: "#3f735f"

    property url csvFile
    property var headers: []
    property var previewRows: []
    property string editingProfileId: ""
    property string detectedInfo: ""
    property string statusText: ""
    property bool statusOk: true

    signal importFinished(var result)

    function indexByValue(model, value) {
        for (let i = 0; i < model.length; ++i)
            if (model[i].value === value)
                return i;
        return model.length > 0 ? 0 : -1;
    }

    function profileItems() {
        const result = [{ label: qsTr("Новый профиль"), value: "" }];
        const profiles = controller.bankCsvProfiles;
        for (let i = 0; i < profiles.length; ++i)
            result.push({ label: profiles[i].name, value: profiles[i].id });
        return result;
    }

    function fiatAccounts() {
        const result = [];
        const accounts = controller.allAccounts;
        for (let i = 0; i < accounts.length; ++i) {
            if (accounts[i].asset === "fiat")
                result.push({
                    label: accounts[i].displayName,
                    value: accounts[i].id
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

    function columnItems(optional) {
        const result = [{
            label: optional ? qsTr("Не используется") : qsTr("Выберите столбец"),
            value: -1
        }];
        for (let i = 0; i < headers.length; ++i)
            result.push(headers[i]);
        return result;
    }

    function setValue(combo, value) {
        combo.currentIndex = indexByValue(combo.model, value);
    }

    function normalizedHeader(label) {
        return String(label).toLowerCase().replace(/[ _\-.]/g, "");
    }

    function findHeader(words) {
        for (let i = 0; i < headers.length; ++i) {
            const label = normalizedHeader(headers[i].label);
            for (let word = 0; word < words.length; ++word) {
                if (label.indexOf(normalizedHeader(words[word])) >= 0)
                    return headers[i].value;
            }
        }
        return -1;
    }

    function autoMapColumns() {
        setValue(dateColumnBox,
                 findHeader(["датаоперации", "дата", "date"]));
        const income = findHeader(["приход", "зачисление", "доход", "credit"]);
        const expense = findHeader(["расход", "списание", "debit"]);
        if (income >= 0 && expense >= 0) {
            setValue(amountModeBox, "separate");
            setValue(incomeColumnBox, income);
            setValue(expenseColumnBox, expense);
        } else {
            setValue(amountModeBox, "signed");
            setValue(amountColumnBox,
                     findHeader(["суммаоперации", "сумма", "amount"]));
        }
        setValue(descriptionColumnBox,
                 findHeader(["описание", "назначение", "merchant", "description"]));
        setValue(idColumnBox,
                 findHeader(["идентификатороперации", "номероперации", "operationid", "transactionid"]));
        setValue(categoryColumnBox,
                 findHeader(["категория", "category"]));
    }

    function inspectFile(profile) {
        const result = controller.inspectBankCsv(
            csvFile,
            Math.max(0, Number(headerRowField.text || "1") - 1),
            delimiterBox.currentValue,
            encodingBox.currentValue
        );
        statusOk = result.ok;
        statusText = result.ok ? "" : result.error;
        if (!result.ok)
            return;
        headers = result.headers;
        previewRows = result.preview;
        detectedInfo = qsTr("Определено: %1, разделитель: %2, строк: %3")
            .arg(result.encodingLabel)
            .arg(result.delimiterLabel)
            .arg(result.rowCount);
        if (profile) {
            setValue(dateColumnBox, profile.dateColumn);
            setValue(amountColumnBox, profile.amountColumn);
            setValue(incomeColumnBox, profile.incomeColumn);
            setValue(expenseColumnBox, profile.expenseColumn);
            setValue(descriptionColumnBox, profile.descriptionColumn);
            setValue(idColumnBox, profile.idColumn);
            setValue(categoryColumnBox, profile.categoryColumn);
        } else {
            autoMapColumns();
        }
    }

    function resetForNewProfile() {
        editingProfileId = "";
        profileNameField.text = "";
        headerRowField.text = "1";
        setValue(delimiterBox, "auto");
        setValue(encodingBox, "auto");
        setValue(dateFormatBox, "auto");
        setValue(amountModeBox, "signed");
        positiveIncomeCheck.checked = true;
        const accounts = fiatAccounts();
        accountBox.currentIndex = accounts.length > 0 ? 0 : -1;
        const incomes = categoryItems("income");
        incomeCategoryBox.currentIndex = incomes.length > 0 ? 0 : -1;
        const expenses = categoryItems("expense");
        expenseCategoryBox.currentIndex = expenses.length > 0 ? 0 : -1;
        inspectFile(null);
    }

    function loadProfile(id) {
        if (!id) {
            resetForNewProfile();
            return;
        }
        const profiles = controller.bankCsvProfiles;
        for (let i = 0; i < profiles.length; ++i) {
            const profile = profiles[i];
            if (profile.id !== id)
                continue;
            editingProfileId = profile.id;
            profileNameField.text = profile.name;
            headerRowField.text = String(profile.headerRow + 1);
            setValue(delimiterBox, profile.delimiter);
            setValue(encodingBox, profile.encoding);
            setValue(dateFormatBox, profile.dateFormat);
            setValue(amountModeBox, profile.amountMode);
            positiveIncomeCheck.checked = profile.positiveMeansIncome;
            setValue(accountBox, profile.accountId);
            setValue(incomeCategoryBox, profile.incomeCategoryId);
            setValue(expenseCategoryBox, profile.expenseCategoryId);
            inspectFile(profile);
            return;
        }
    }

    function currentProfileValues() {
        return {
            id: editingProfileId,
            name: profileNameField.text.trim(),
            accountId: accountBox.currentValue || "",
            incomeCategoryId: incomeCategoryBox.currentValue || "",
            expenseCategoryId: expenseCategoryBox.currentValue || "",
            encoding: encodingBox.currentValue,
            delimiter: delimiterBox.currentValue,
            dateFormat: dateFormatBox.currentValue,
            amountMode: amountModeBox.currentValue,
            headerRow: Math.max(0, Number(headerRowField.text || "1") - 1),
            dateColumn: dateColumnBox.currentValue,
            amountColumn: amountColumnBox.currentValue,
            incomeColumn: incomeColumnBox.currentValue,
            expenseColumn: expenseColumnBox.currentValue,
            descriptionColumn: descriptionColumnBox.currentValue,
            idColumn: idColumnBox.currentValue,
            categoryColumn: categoryColumnBox.currentValue,
            positiveMeansIncome: positiveIncomeCheck.checked
        };
    }

    function saveProfile() {
        const result = controller.saveBankCsvProfile(currentProfileValues());
        statusOk = result.ok;
        statusText = result.ok ? qsTr("Профиль сохранён") : result.error;
        if (result.ok) {
            editingProfileId = result.id;
            setValue(profileBox, result.id);
        }
        return result;
    }

    function openForFile(fileUrl) {
        csvFile = fileUrl;
        statusText = "";
        detectedInfo = "";
        profileBox.currentIndex = 0;
        resetForNewProfile();
        open();
    }

    component FormField: TextField {
        id: formField
        implicitHeight: 42
        color: dialog.textColor
        placeholderTextColor: dialog.mutedColor
        background: Rectangle {
            radius: 9
            color: dialog.softColor
            border.width: 1
            border.color: formField.activeFocus ? dialog.accentColor : dialog.lineColor
        }
    }

    component FormCombo: ComboBox {
        id: formCombo
        implicitHeight: 42
        textRole: "label"
        valueRole: "value"
        contentItem: Text {
            leftPadding: 10
            rightPadding: 28
            text: formCombo.displayText
            color: dialog.textColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 9
            color: dialog.softColor
            border.width: 1
            border.color: dialog.lineColor
        }
    }

    component FormButton: Button {
        id: formButton
        implicitHeight: 40
        property bool primary: false
        contentItem: Text {
            text: formButton.text
            color: formButton.primary ? "#fffff0" : dialog.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 9
            color: formButton.primary ? dialog.accentColor : dialog.softColor
            border.width: formButton.primary ? 0 : 1
            border.color: dialog.lineColor
        }
    }

    background: Rectangle {
        color: panelColor
        radius: 18
        border.width: 1
        border.color: lineColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 22
            Text {
                text: qsTr("Импорт банковского CSV")
                color: dialog.textColor
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            FormButton {
                text: "×"
                implicitWidth: 42
                onClicked: dialog.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.max(840, dialog.width - 44)
                spacing: 12
                anchors.margins: 20

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 8

                    Text { text: qsTr("Профиль"); color: dialog.mutedColor }
                    FormCombo {
                        id: profileBox
                        Layout.fillWidth: true
                        model: dialog.profileItems()
                        onActivated: dialog.loadProfile(currentValue)
                    }
                    Text { text: qsTr("Название профиля"); color: dialog.mutedColor }
                    FormField {
                        id: profileNameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Например, Альфа-Банк")
                    }

                    Text { text: qsTr("Кодировка"); color: dialog.mutedColor }
                    FormCombo {
                        id: encodingBox
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Определить автоматически"), value: "auto" },
                            { label: "UTF-8", value: "utf8" },
                            { label: "Windows-1251", value: "windows1251" },
                            { label: "UTF-16LE", value: "utf16le" },
                            { label: "UTF-16BE", value: "utf16be" }
                        ]
                    }
                    Text { text: qsTr("Разделитель"); color: dialog.mutedColor }
                    FormCombo {
                        id: delimiterBox
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Определить автоматически"), value: "auto" },
                            { label: qsTr("Точка с запятой"), value: "semicolon" },
                            { label: qsTr("Запятая"), value: "comma" },
                            { label: qsTr("Табуляция"), value: "tab" }
                        ]
                    }

                    Text { text: qsTr("Строка заголовков"); color: dialog.mutedColor }
                    FormField {
                        id: headerRowField
                        Layout.fillWidth: true
                        text: "1"
                        validator: IntValidator { bottom: 1; top: 1000 }
                    }
                    FormButton {
                        text: qsTr("Перечитать столбцы")
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        onClicked: dialog.inspectFile(null)
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: dialog.detectedInfo
                    color: dialog.mutedColor
                    font.pixelSize: 12
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 8

                    Text { text: qsTr("Счёт"); color: dialog.mutedColor }
                    FormCombo {
                        id: accountBox
                        Layout.fillWidth: true
                        model: dialog.fiatAccounts()
                    }
                    Text { text: qsTr("Формат даты"); color: dialog.mutedColor }
                    FormCombo {
                        id: dateFormatBox
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Определить автоматически"), value: "auto" },
                            { label: "ДД.ММ.ГГГГ", value: "dd.MM.yyyy" },
                            { label: "ДД.ММ.ГГ", value: "dd.MM.yy" },
                            { label: "ДД.ММ.ГГГГ ЧЧ:ММ", value: "dd.MM.yyyy HH:mm" },
                            { label: "ГГГГ-ММ-ДД", value: "yyyy-MM-dd" },
                            { label: "ДД/ММ/ГГГГ", value: "dd/MM/yyyy" },
                            { label: "ММ/ДД/ГГГГ", value: "MM/dd/yyyy" }
                        ]
                    }

                    Text { text: qsTr("Столбец даты"); color: dialog.mutedColor }
                    FormCombo {
                        id: dateColumnBox
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    Text { text: qsTr("Хранение суммы"); color: dialog.mutedColor }
                    FormCombo {
                        id: amountModeBox
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Один столбец со знаком"), value: "signed" },
                            { label: qsTr("Отдельно приход и расход"), value: "separate" }
                        ]
                    }

                    Text {
                        visible: amountModeBox.currentValue === "signed"
                        text: qsTr("Столбец суммы")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: amountColumnBox
                        visible: amountModeBox.currentValue === "signed"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    CheckBox {
                        id: positiveIncomeCheck
                        visible: amountModeBox.currentValue === "signed"
                        Layout.columnSpan: 2
                        checked: true
                        text: qsTr("Положительное значение — доход")
                    }

                    Text {
                        visible: amountModeBox.currentValue === "separate"
                        text: qsTr("Столбец прихода")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: incomeColumnBox
                        visible: amountModeBox.currentValue === "separate"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    Text {
                        visible: amountModeBox.currentValue === "separate"
                        text: qsTr("Столбец расхода")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: expenseColumnBox
                        visible: amountModeBox.currentValue === "separate"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }

                    Text { text: qsTr("Описание"); color: dialog.mutedColor }
                    FormCombo {
                        id: descriptionColumnBox
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    Text { text: qsTr("Идентификатор операции"); color: dialog.mutedColor }
                    FormCombo {
                        id: idColumnBox
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }

                    Text { text: qsTr("Категория из CSV"); color: dialog.mutedColor }
                    FormCombo {
                        id: categoryColumnBox
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    Text { text: qsTr("Категория дохода по умолчанию"); color: dialog.mutedColor }
                    FormCombo {
                        id: incomeCategoryBox
                        Layout.fillWidth: true
                        model: dialog.categoryItems("income")
                    }

                    Item { Layout.preferredHeight: 1 }
                    Item { Layout.preferredHeight: 1 }
                    Text { text: qsTr("Категория расхода по умолчанию"); color: dialog.mutedColor }
                    FormCombo {
                        id: expenseCategoryBox
                        Layout.fillWidth: true
                        model: dialog.categoryItems("expense")
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

                Text {
                    text: qsTr("Предварительный просмотр")
                    color: dialog.textColor
                    font.weight: Font.DemiBold
                }
                Repeater {
                    model: dialog.previewRows
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: dialog.mutedColor
                        font.family: "monospace"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.statusText.length > 0
                    text: dialog.statusText
                    color: dialog.statusOk ? dialog.successColor : dialog.errorColor
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 10
            FormButton {
                text: qsTr("Удалить профиль")
                enabled: dialog.editingProfileId.length > 0
                onClicked: {
                    if (controller.deleteBankCsvProfile(dialog.editingProfileId)) {
                        dialog.statusOk = true;
                        dialog.statusText = qsTr("Профиль удалён");
                        profileBox.currentIndex = 0;
                        dialog.resetForNewProfile();
                    }
                }
            }
            Item { Layout.fillWidth: true }
            FormButton {
                text: qsTr("Сохранить профиль")
                onClicked: dialog.saveProfile()
            }
            FormButton {
                text: qsTr("Импортировать")
                primary: true
                onClicked: {
                    const saved = dialog.saveProfile();
                    if (!saved.ok)
                        return;
                    const result = controller.importBankCsv(dialog.csvFile, saved.id);
                    dialog.statusOk = result.ok;
                    dialog.statusText = result.ok
                        ? qsTr("Импортировано: %1, дубликатов: %2, отклонено строк: %3")
                            .arg(result.imported).arg(result.skipped).arg(result.rejected)
                        : result.error;
                    if (result.ok)
                        dialog.importFinished(result);
                }
            }
        }
    }
}
