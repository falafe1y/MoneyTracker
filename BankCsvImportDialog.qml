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

    readonly property bool pdfFile: String(csvFile).toLowerCase().endsWith(".pdf")
    property url csvFile
    property var headers: []
    property var previewRows: []
    property string editingProfileId: ""
    property string detectedInfo: ""
    property string statusText: ""
    property bool statusOk: true
    property string previewSummary: ""
    property var operationRows: []
    property var categoryChoices: ({})
    property var savedCategoryRules: []
    property var savedRecipientRules: []
    property bool showReviewOnly: true
    property bool showSavedRules: false
    property string previewConfiguration: ""
    property bool inspecting: false
    property int inspectionRequestId: 0
    property var inspectionProfile: null

    Connections {
        target: dialog.controller
        function onBankCsvInspectionFinished(requestId, result) {
            if (requestId !== dialog.inspectionRequestId) return;
            dialog.inspectionRequestId = 0;
            dialog.inspecting = false;
            dialog.applyInspection(result, dialog.inspectionProfile);
        }
    }
    onClosed: {
        inspectionRequestId = 0;
        inspecting = false;
    }

    function choiceFor(row) {
        return categoryChoices[row.rowKey] || {
            categoryId: row.categoryId, confirmed: false, remember: false, fingerprint: row.fingerprint,
            pattern: row.merchant, matchMode: "exact", categoryField: "recipient",
            recipientName: row.merchant || "", recipientConfirmed: false, rememberRecipient: false,
            recipientPattern: row.recipientPattern || "", recipientField: row.recipientField || "description",
            recipientMode: row.recipientMode || "contains"
        };
    }

    function applyPendingRules(choices) {
        const result = controller.resolveBankImportRows(operationRows, choices);
        if (!result.ok) {
            statusOk = false; statusText = result.error;
            return categoryChoices;
        }
        operationRows = result.operationRows;
        statusOk = true; statusText = "";
        return choices;
    }

    function setChoice(row, changes) {
        const next = Object.assign({}, categoryChoices);
        const original = choiceFor(row);
        const edited = Object.assign({}, original, changes);
        if (original.remember && changes.categoryId !== undefined)
            edited.rememberedCategoryId = original.rememberedCategoryId || original.categoryId;
        next[row.rowKey] = edited;
        categoryChoices = applyPendingRules(next);
    }

    function categoryIndex(items, id) {
        for (let i = 0; i < items.length; ++i)
            if (items[i].value === id) return i;
        return -1;
    }

    function unresolved(row) { return !!row.needsReview; }
    function remainingReviews() {
        return operationRows.filter(function(row) { return unresolved(row); }).length;
    }

    function reviewGroups() {
        const groups = [], byKey = ({});
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            let group = byKey[row.groupKey];
            if (!group) {
                group = { key: row.groupKey, rows: [], merchant: row.merchant || "", total: 0,
                    currency: row.currency, type: row.type, needsReview: false, categoryId: row.categoryId,
                    mixedCategory: false, reason: row.reason, recipientReason: row.recipientReason,
                    recipientPattern: row.recipientPattern || "", recipientField: row.recipientField || "description",
                    recipientMode: row.recipientMode || "contains", recipientStatus: row.recipientStatus,
                    special: row.special, firstDate: row.date, lastDate: row.date };
                byKey[row.groupKey] = group; groups.push(group);
            }
            group.rows.push(row); group.total += Number(row.signedMinor);
            group.needsReview = group.needsReview || unresolved(row);
            if (group.categoryId !== row.categoryId) group.mixedCategory = true;
            group.lastDate = row.date;
        }
        return groups.filter(function(group) { return !showReviewOnly || group.needsReview; });
    }

    function recipientRuleSource(group, field) {
        const row = group.rows.length ? group.rows[0] : ({});
        if (field === "description") return row.description || "";
        if (field === "bank_recipient") return row.bankRecipient || "";
        if (field === "recipient_id") return row.recipientId || "";
        if (field === "recipient") return row.recognizedRecipient || "";
        return "";
    }

    function initialRecipientRulePattern(group, field) {
        const row = group.rows.length ? group.rows[0] : ({});
        const choice = categoryChoices[row.rowKey];
        // Keep a rule which the user has already edited and remembered.
        if (choice && choice.rememberRecipient && choice.recipientField === field)
            return choice.recipientPattern || "";
        if (row.recipientSource === "rule" && group.recipientField === field)
            return group.recipientPattern || "";
        return recipientRuleSource(group, field);
    }

    function applyGroup(group, name, categoryId, rememberCategory, rememberRecipient, pattern, field, mode) {
        if (!categoryId) return;
        const next = Object.assign({}, categoryChoices);
        const recipient = name.trim();
        for (let i = 0; i < group.rows.length; ++i) {
            const row = group.rows[i];
            next[row.rowKey] = Object.assign({}, choiceFor(row), {
                categoryId: categoryId, confirmed: true, recipientName: recipient, recipientConfirmed: true,
                remember: i === 0 && rememberCategory && !!recipient,
                pattern: recipient, matchMode: "exact", categoryField: "recipient", rememberedCategoryId: categoryId,
                rememberRecipient: i === 0 && rememberRecipient,
                recipientPattern: pattern.trim(), recipientField: field, recipientMode: mode
            });
        }
        categoryChoices = applyPendingRules(next);
    }

    function acceptSuggestions() {
        const next = Object.assign({}, categoryChoices);
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            if (!row.special && row.recipientStatus !== "candidate" && row.recipientStatus !== "ambiguous"
                    && categoryIndex(categoryItems(row.type), row.categoryId) >= 0)
                next[row.rowKey] = Object.assign({}, choiceFor(row), { confirmed: true });
        }
        categoryChoices = applyPendingRules(next);
    }

    function reloadRules() {
        const categories = controller.bankCategoryRules();
        const recipients = controller.bankRecipientRules();
        if (categories.ok && recipients.ok) {
            savedCategoryRules = categories.items; savedRecipientRules = recipients.items;
        } else { statusOk = false; statusText = categories.error || recipients.error; }
    }

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

    function findHeader(words, excluded) {
        for (let word = 0; word < words.length; ++word) {
            for (let i = 0; i < headers.length; ++i) {
                if (excluded && excluded.indexOf(headers[i].value) >= 0) continue;
                const label = normalizedHeader(headers[i].label);
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
                     findHeader(["суммаплатежа", "суммаввалютесчета", "суммаоперации", "сумма", "amount"]));
        }
        setValue(descriptionColumnBox,
                 findHeader(["описание", "назначение", "description"]));
        setValue(recipientIdColumnBox,
                 findHeader(["иннполучателя", "идентификаторполучателя", "merchantid", "recipientid"]));
        setValue(recipientColumnBox,
                 findHeader(["наименованиеполучателя", "контрагент", "торговаяточка", "merchant", "recipient", "получатель"],
                            [recipientIdColumnBox.currentValue]));
        setValue(idColumnBox,
                 findHeader(["идентификатороперации", "номероперации", "operationid", "transactionid"]));
        setValue(categoryColumnBox,
                 findHeader(["категория", "category"]));
        setValue(directionColumnBox,
                 findHeader(["приходрасход", "дебеткредит", "направление", "direction"]));
        setValue(currencyColumnBox,
                 findHeader(["валютаплатежа", "валютасчета", "currency"]));
    }

    function inspectFile(profile) {
        previewSummary = "";
        operationRows = []; categoryChoices = ({});
        headers = []; previewRows = [];
        inspecting = true;
        statusOk = true;
        statusText = qsTr("Читаем выписку…");
        inspectionProfile = profile;
        inspectionRequestId = controller.inspectBankCsvAsync(
            csvFile,
            Math.max(0, Number(headerRowField.text || "1") - 1),
            delimiterBox.currentValue,
            encodingBox.currentValue
        );
    }

    function applyInspection(result, profile) {
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
        if (pdfFile && result.previewTruncated)
            detectedInfo += qsTr(" · Показаны первые 1000 строк");
        if (pdfFile) {
            setValue(dateColumnBox, 0); setValue(amountColumnBox, 1);
            setValue(descriptionColumnBox, 2); setValue(idColumnBox, 3);
            setValue(recipientColumnBox, -1); setValue(recipientIdColumnBox, -1);
            setValue(currencyColumnBox, 4); setValue(categoryColumnBox, -1);
            setValue(directionColumnBox, -1); setValue(amountModeBox, "signed");
            setValue(dateFormatBox, "auto"); positiveIncomeCheck.checked = true;
            headerRowField.text = "1";
            if (!profileNameField.text.trim()) profileNameField.text = result.encodingLabel;
        } else if (profile) {
            setValue(dateColumnBox, profile.dateColumn);
            setValue(amountColumnBox, profile.amountColumn);
            setValue(incomeColumnBox, profile.incomeColumn);
            setValue(expenseColumnBox, profile.expenseColumn);
            setValue(descriptionColumnBox, profile.descriptionColumn);
            setValue(recipientColumnBox, profile.recipientColumn === undefined ? -1 : profile.recipientColumn);
            setValue(recipientIdColumnBox, profile.recipientIdColumn === undefined ? -1 : profile.recipientIdColumn);
            setValue(idColumnBox, profile.idColumn);
            setValue(categoryColumnBox, profile.categoryColumn);
            setValue(directionColumnBox, profile.directionColumn);
            setValue(currencyColumnBox, profile.currencyColumn);
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
            recipientColumn: pdfFile ? -1 : recipientColumnBox.currentValue,
            recipientIdColumn: pdfFile ? -1 : recipientIdColumnBox.currentValue,
            idColumn: idColumnBox.currentValue,
            categoryColumn: categoryColumnBox.currentValue,
            directionColumn: directionColumnBox.currentValue,
            currencyColumn: currencyColumnBox.currentValue,
            positiveMeansIncome: positiveIncomeCheck.checked
        };
    }

    function formatMinor(value, currency) {
        return (Number(value) / 100).toLocaleString(Qt.locale(), "f", 2) + " " + currency;
    }

    function previewImport() {
        if (inspecting) return false;
        const configuration = JSON.stringify(currentProfileValues());
        if (configuration !== previewConfiguration) categoryChoices = ({});
        previewConfiguration = configuration;
        const result = controller.previewBankImport(csvFile, currentProfileValues());
        statusOk = result.ok;
        if (!result.ok) {
            previewSummary = "";
            operationRows = [];
            statusText = result.error;
            return false;
        }
        statusText = "";
        operationRows = result.operationRows || [];
        const keptChoices = ({});
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            const choice = categoryChoices[row.rowKey];
            if (choice && choice.fingerprint === row.fingerprint)
                keptChoices[row.rowKey] = choice;
        }
        categoryChoices = Object.keys(keptChoices).length ? applyPendingRules(keptChoices) : ({});
        previewSummary = qsTr("Найдено: %1 · Новых: %2 · Дубликатов: %3 · Возможных переводов: %4 · %5 — %6 · Доходы: %7 · Расходы: %8 · Ошибок: %9 · Другая валюта: %10")
            .arg(result.operationCount)
            .arg(result.newCount)
            .arg(result.duplicateCount)
            .arg(result.possibleTransfers)
            .arg(result.firstDate)
            .arg(result.lastDate)
            .arg(formatMinor(result.incomeMinor, result.currency))
            .arg(formatMinor(result.expenseMinor, result.currency))
            .arg(result.rejected)
            .arg(result.currencyMismatches);
        if (result.warnings && result.warnings.length)
            statusText = result.warnings.join("\n");
        return result.operationCount > 0;
    }

    function saveProfile() {
        if (inspecting) return { ok: false };
        const result = controller.saveBankCsvProfile(currentProfileValues());
        statusOk = result.ok;
        statusText = result.ok ? qsTr("Профиль сохранён") : result.error;
        if (result.ok) {
            editingProfileId = result.id;
            setValue(profileBox, result.id);
            previewConfiguration = JSON.stringify(currentProfileValues());
        }
        return result;
    }

    function openForFile(fileUrl) {
        csvFile = fileUrl;
        operationRows = []; categoryChoices = ({}); previewConfiguration = "";
        showReviewOnly = true; showSavedRules = false;
        reloadRules();
        statusText = "";
        detectedInfo = "";
        previewSummary = "";
        profileBox.currentIndex = 0;
        resetForNewProfile();
        open();
    }

    component FormLabel: Text {
        Layout.preferredWidth: 180
        Layout.maximumWidth: 180
        color: dialog.mutedColor
        font.pixelSize: 14
        wrapMode: Text.WordWrap
    }

    component FormField: StyledTextField {
        neo: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: Math.max(0, (importScroll.width - 390) / 2)
        Layout.maximumWidth: Layout.preferredWidth
        controlHeight: 42
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appOnAccentColor: dialog.panelColor
    }

    component FormCombo: StyledComboBox {
        neo: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: Math.max(0, (importScroll.width - 390) / 2)
        Layout.maximumWidth: Layout.preferredWidth
        controlHeight: 42
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

    component FormCheckBox: StyledCheckBox {
        neo: true
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: "#E4E8F1"
        appOnAccentColor: dialog.panelColor
    }

    component FormButton: StyledButton {
        neo: true
        cornerRadius: 8
        font.weight: Font.DemiBold
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: "#E4E8F1"
        appOnAccentColor: dialog.panelColor
    }

    background: Rectangle {
        HardShadow { depth: 6; shadowColor: dialog.accentColor }

        color: panelColor
        radius: 16
        border.width: 1
        border.color: dialog.accentColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 22
            Text {
                text: qsTr("Импорт банковской выписки")
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

        RowLayout {
            visible: dialog.inspecting
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 16
            BusyIndicator {
                running: dialog.inspecting
                implicitWidth: 40
                implicitHeight: 40
            }
            Text {
                text: qsTr("Читаем выписку…")
                color: dialog.mutedColor
                font.pixelSize: 14
            }
            Item { Layout.fillWidth: true }
        }

        Flickable {
            id: importScroll
            objectName: "bankImportScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            contentWidth: width
            contentHeight: importContent.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: importContent
                width: importScroll.width
                spacing: 12

                GridLayout {
                    enabled: !dialog.inspecting
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 8

                    FormLabel { text: qsTr("Профиль"); color: dialog.mutedColor }
                    FormCombo {
                        id: profileBox
                        Layout.fillWidth: true
                        model: dialog.profileItems()
                        onActivated: dialog.loadProfile(currentValue)
                    }
                    FormLabel { text: qsTr("Название профиля"); color: dialog.mutedColor }
                    FormField {
                        id: profileNameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Например, Альфа-Банк")
                    }

                    FormLabel { text: qsTr("Кодировка"); color: dialog.mutedColor }
                    FormCombo {
                        id: encodingBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Определить автоматически"), value: "auto" },
                            { label: "UTF-8", value: "utf8" },
                            { label: "Windows-1251", value: "windows1251" },
                            { label: "UTF-16LE", value: "utf16le" },
                            { label: "UTF-16BE", value: "utf16be" }
                        ]
                    }
                    FormLabel { text: qsTr("Разделитель"); color: dialog.mutedColor }
                    FormCombo {
                        id: delimiterBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Определить автоматически"), value: "auto" },
                            { label: qsTr("Точка с запятой"), value: "semicolon" },
                            { label: qsTr("Запятая"), value: "comma" },
                            { label: qsTr("Табуляция"), value: "tab" }
                        ]
                    }

                    FormLabel { text: qsTr("Строка заголовков"); color: dialog.mutedColor }
                    FormField {
                        id: headerRowField
                        enabled: !dialog.pdfFile
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
                    text: dialog.pdfFile
                          ? dialog.detectedInfo + "\n" + qsTr("Сверьте операции и итоговые суммы перед импортом. Поддерживаются PDF с текстом; сканы и выписки с паролем не поддерживаются.")
                          : dialog.detectedInfo
                    wrapMode: Text.WordWrap
                    color: dialog.mutedColor
                    font.pixelSize: 14
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 8

                    FormLabel { text: qsTr("Счёт"); color: dialog.mutedColor }
                    FormCombo {
                        id: accountBox
                        Layout.fillWidth: true
                        model: dialog.fiatAccounts()
                    }
                    FormLabel { text: qsTr("Формат даты"); color: dialog.mutedColor }
                    FormCombo {
                        id: dateFormatBox
                        enabled: !dialog.pdfFile
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

                    FormLabel { text: qsTr("Столбец даты"); color: dialog.mutedColor }
                    FormCombo {
                        id: dateColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    FormLabel { text: qsTr("Хранение суммы"); color: dialog.mutedColor }
                    FormCombo {
                        id: amountModeBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: [
                            { label: qsTr("Один столбец со знаком"), value: "signed" },
                            { label: qsTr("Отдельно приход и расход"), value: "separate" }
                        ]
                    }

                    FormLabel {
                        visible: amountModeBox.currentValue === "signed"
                        text: qsTr("Столбец суммы")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: amountColumnBox
                        enabled: !dialog.pdfFile
                        visible: amountModeBox.currentValue === "signed"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    FormCheckBox {
                        id: positiveIncomeCheck
                        enabled: !dialog.pdfFile
                        visible: amountModeBox.currentValue === "signed"
                        Layout.columnSpan: 2
                        checked: true
                        text: qsTr("Положительное значение — доход")
                    }

                    FormLabel {
                        visible: amountModeBox.currentValue === "separate"
                        text: qsTr("Столбец прихода")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: incomeColumnBox
                        enabled: !dialog.pdfFile
                        visible: amountModeBox.currentValue === "separate"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }
                    FormLabel {
                        visible: amountModeBox.currentValue === "separate"
                        text: qsTr("Столбец расхода")
                        color: dialog.mutedColor
                    }
                    FormCombo {
                        id: expenseColumnBox
                        enabled: !dialog.pdfFile
                        visible: amountModeBox.currentValue === "separate"
                        Layout.fillWidth: true
                        model: dialog.columnItems(false)
                    }

                    FormLabel { text: qsTr("Описание"); color: dialog.mutedColor }
                    FormCombo {
                        id: descriptionColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("Получатель из файла"); color: dialog.mutedColor }
                    FormCombo {
                        id: recipientColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("ИНН / ID получателя"); color: dialog.mutedColor }
                    FormCombo {
                        id: recipientIdColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("Идентификатор операции"); color: dialog.mutedColor }
                    FormCombo {
                        id: idColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }

                    FormLabel { text: qsTr("Категория из файла"); color: dialog.mutedColor }
                    FormCombo {
                        id: categoryColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("Направление операции"); color: dialog.mutedColor }
                    FormCombo {
                        id: directionColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("Валюта строки"); color: dialog.mutedColor }
                    FormCombo {
                        id: currencyColumnBox
                        enabled: !dialog.pdfFile
                        Layout.fillWidth: true
                        model: dialog.columnItems(true)
                    }
                    FormLabel { text: qsTr("Категория дохода по умолчанию"); color: dialog.mutedColor }
                    FormCombo {
                        id: incomeCategoryBox
                        Layout.fillWidth: true
                        model: dialog.categoryItems("income")
                    }

                    Item { Layout.preferredHeight: 1 }
                    Item { Layout.preferredHeight: 1 }
                    FormLabel { text: qsTr("Категория расхода по умолчанию"); color: dialog.mutedColor }
                    FormCombo {
                        id: expenseCategoryBox
                        Layout.fillWidth: true
                        model: dialog.categoryItems("expense")
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.operationRows.length > 0
                    text: qsTr("Получатели и категории · Требуют проверки: %1 операций")
                        .arg(dialog.remainingReviews())
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: dialog.textColor
                }
                RowLayout {
                    visible: dialog.operationRows.length > 0
                    Layout.fillWidth: true
                    spacing: 16
                    StyledCheckBox {
                        text: qsTr("Только требующие проверки")
                        checked: dialog.showReviewOnly
                        onClicked: dialog.showReviewOnly = checked
                        appTextColor: dialog.textColor
                        appAccentColor: dialog.accentColor
                    }
                    Item { Layout.fillWidth: true }
                    FormButton {
                        text: qsTr("Принять предложения")
                        onClicked: dialog.acceptSuggestions()
                    }
                }
                Text {
                    visible: dialog.operationRows.length > 0
                    Layout.fillWidth: true
                    text: qsTr("Проверьте группы: одно исправление применяется ко всем операциям группы. Без имени получателя можно импортировать, сохранив полное описание отдельно. Переводы, возвраты и снятия проверьте по одному — импорт сохраняет их как доходы или расходы.")
                    color: dialog.mutedColor
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                ListView {
                    id: categoryReviewList
                    objectName: "recipientReviewList"
                    visible: dialog.operationRows.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(560, contentHeight)
                    clip: true
                    spacing: 8
                    model: dialog.reviewGroups()
                    ScrollBar.vertical: StyledScrollBar {}
                    delegate: Rectangle {
                        id: reviewGroup
                        required property var modelData
                        readonly property var items: dialog.categoryItems(modelData.type)
                        property bool expanded: false
                        property var ruleDrafts: ({})
                        // A user's checkbox choice is independent of the selected rule source.
                        property int rememberRecipientSelection: -1

                        function patternForField(field) {
                            if (Object.prototype.hasOwnProperty.call(ruleDrafts, field))
                                return ruleDrafts[field];
                            return dialog.initialRecipientRulePattern(modelData, field);
                        }

                        function fillRulePattern(field) {
                            const pattern = patternForField(field);
                            // Show the entire source before the user shortens it to a rule.
                            recipientPatternField.maximumLength = Math.max(240,
                                dialog.recipientRuleSource(modelData, field).length, pattern.length);
                            recipientPatternField.text = pattern;
                        }

                        width: categoryReviewList.width
                        height: reviewContent.implicitHeight + 32
                        radius: 8
                        color: dialog.panelColor
                        border.width: 1
                        border.color: dialog.lineColor
                        ColumnLayout {
                            id: reviewContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: reviewGroup.modelData.merchant || qsTr("Получатель не определён")
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    color: dialog.textColor
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: qsTr("Операций: %1 · %2").arg(reviewGroup.modelData.rows.length)
                                        .arg(dialog.formatMinor(reviewGroup.modelData.total, reviewGroup.modelData.currency))
                                    color: dialog.textColor
                                    font.pixelSize: 14
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: reviewGroup.modelData.recipientReason + " · " + reviewGroup.modelData.reason
                                    + (reviewGroup.modelData.mixedCategory ? qsTr(" · Разные категории — сохраните исключения ниже") : "")
                                color: dialog.mutedColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                            Repeater {
                                model: reviewGroup.modelData.rows.slice(0, 2)
                                delegate: Text {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: modelData.date + " · " + dialog.formatMinor(modelData.signedMinor, modelData.currency)
                                        + " · " + modelData.description
                                    color: dialog.mutedColor
                                    font.pixelSize: 13
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                FormField {
                                    id: recipientNameField
                                    objectName: "recipientNameField"
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 10000
                                    text: reviewGroup.modelData.merchant
                                    maximumLength: 160
                                    placeholderText: qsTr("Имя получателя (можно оставить пустым)")
                                }
                                FormCombo {
                                    id: groupCategoryBox
                                    objectName: "groupCategoryBox"
                                    Layout.preferredWidth: 240
                                    Layout.maximumWidth: 240
                                    model: reviewGroup.items
                                    currentIndex: reviewGroup.modelData.mixedCategory ? -1
                                        : dialog.categoryIndex(reviewGroup.items, reviewGroup.modelData.categoryId)
                                    emptyText: qsTr("Выберите категорию")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16
                                FormCheckBox {
                                    id: rememberCategoryCheck
                                    text: qsTr("Запомнить категорию получателя")
                                    checked: !!recipientNameField.text.trim() && !reviewGroup.modelData.special
                                    enabled: !!recipientNameField.text.trim()
                                }
                                FormCheckBox {
                                    id: rememberRecipientCheck
                                    text: qsTr("Запомнить имя по правилу")
                                    objectName: "rememberRecipientCheck"
                                    checked: reviewGroup.rememberRecipientSelection >= 0
                                        ? reviewGroup.rememberRecipientSelection === 1
                                        : reviewGroup.modelData.rows.some(function(row) {
                                            return dialog.choiceFor(row).rememberRecipient;
                                        }) || (!!recipientNameField.text.trim()
                                            && recipientNameField.text.trim() !== reviewGroup.modelData.merchant)
                                    onClicked: reviewGroup.rememberRecipientSelection = checked ? 1 : 0
                                    enabled: !!recipientNameField.text.trim()
                                }
                            }
                            GridLayout {
                                visible: rememberRecipientCheck.checked
                                    || recipientNameField.text.trim() !== reviewGroup.modelData.merchant
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 12
                                FormCombo {
                                    id: recipientRuleFieldBox
                                    objectName: "recipientRuleFieldBox"
                                    Layout.preferredWidth: 240
                                    Layout.maximumWidth: 240
                                    model: [{label: qsTr("Описание содержит"), value: "description"},
                                        {label: qsTr("Получатель из файла"), value: "bank_recipient"},
                                        {label: qsTr("ИНН / ID совпадает"), value: "recipient_id"},
                                        {label: qsTr("Распознанное имя"), value: "recipient"}]
                                    currentIndex: dialog.indexByValue(model, reviewGroup.modelData.recipientField)
                                    onActivated: reviewGroup.fillRulePattern(currentValue)
                                }
                                FormField {
                                    id: recipientPatternField
                                    objectName: "recipientPatternField"
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 10000
                                    maximumLength: Math.max(240,
                                        dialog.recipientRuleSource(reviewGroup.modelData, recipientRuleFieldBox.currentValue).length)
                                    text: reviewGroup.patternForField(recipientRuleFieldBox.currentValue)
                                    placeholderText: qsTr("Значение не найдено — введите правило")
                                    onTextEdited: {
                                        const drafts = Object.assign({}, reviewGroup.ruleDrafts);
                                        drafts[recipientRuleFieldBox.currentValue] = text;
                                        reviewGroup.ruleDrafts = drafts;
                                    }
                                }
                                Text {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    text: qsTr("Значение подставляется из операции. Отредактируйте его: для описания оставьте целые слова без даты, суммы и номера карты. Остальные поля должны совпасть целиком. Правило сразу применяется к этой выписке и сохраняется после импорта.")
                                    color: dialog.mutedColor
                                    font.pixelSize: 13
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Text {
                                visible: rememberRecipientCheck.checked && recipientPatternField.text.length > 240
                                Layout.fillWidth: true
                                text: qsTr("Сократите правило до 240 символов: оставьте слова, по которым узнаётся получатель.")
                                color: dialog.errorColor
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                FormButton {
                                    objectName: "applyRecipientGroup"
                                    text: reviewGroup.modelData.rows.length > 1 ? qsTr("Применить к группе") : qsTr("Подтвердить")
                                    enabled: !!groupCategoryBox.currentValue
                                    onClicked: dialog.applyGroup(reviewGroup.modelData, recipientNameField.text,
                                        groupCategoryBox.currentValue, rememberCategoryCheck.checked,
                                        rememberRecipientCheck.checked, recipientPatternField.text,
                                        recipientRuleFieldBox.currentValue,
                                        recipientRuleFieldBox.currentValue === "description" ? "contains" : "exact")
                                }
                                FormButton {
                                    text: reviewGroup.expanded ? qsTr("Скрыть операции") : qsTr("Операции и исключения")
                                    onClicked: reviewGroup.expanded = !reviewGroup.expanded
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Repeater {
                                model: reviewGroup.expanded ? reviewGroup.modelData.rows : []
                                delegate: ColumnLayout {
                                    id: exceptionRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Rectangle { Layout.fillWidth: true; height: 1; color: dialog.lineColor }
                                    Text {
                                        Layout.fillWidth: true
                                        text: exceptionRow.modelData.date + " · " + dialog.formatMinor(
                                            exceptionRow.modelData.signedMinor, exceptionRow.modelData.currency)
                                            + " · " + exceptionRow.modelData.description
                                        color: dialog.mutedColor
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        FormCombo {
                                            id: exceptionCategoryBox
                                            Layout.preferredWidth: 240
                                            Layout.maximumWidth: 240
                                            model: reviewGroup.items
                                            currentIndex: dialog.categoryIndex(model, exceptionRow.modelData.categoryId)
                                        }
                                        FormButton {
                                            text: qsTr("Только эта операция")
                                            enabled: !!exceptionCategoryBox.currentValue
                                            onClicked: dialog.setChoice(exceptionRow.modelData, {
                                                categoryId: exceptionCategoryBox.currentValue, confirmed: true
                                            })
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                Text {
                    visible: dialog.operationRows.length > 0 && dialog.showReviewOnly && dialog.reviewGroups().length === 0
                    Layout.fillWidth: true
                    text: qsTr("Все операции готовы к импорту. Снимите фильтр, чтобы посмотреть распознанных получателей.")
                    color: dialog.successColor
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                FormButton {
                    text: dialog.showSavedRules ? qsTr("Скрыть сохранённые правила")
                                                : qsTr("Сохранённые правила (%1)").arg(dialog.savedCategoryRules.length + dialog.savedRecipientRules.length)
                    onClicked: { dialog.reloadRules(); dialog.showSavedRules = !dialog.showSavedRules; }
                }
                ListView {
                    id: savedRulesList
                    visible: dialog.showSavedRules
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(180, contentHeight)
                    model: dialog.savedCategoryRules
                    clip: true
                    spacing: 8
                    ScrollBar.vertical: StyledScrollBar { policy: ScrollBar.AlwaysOff }
                    delegate: RowLayout {
                        required property var modelData
                        width: savedRulesList.width
                        spacing: 16
                        Text {
                            Layout.fillWidth: true
                            text: (modelData.type === "income" ? qsTr("Доход: ") : qsTr("Расход: "))
                                + (modelData.field === "description" ? qsTr("Описание «")
                                    : modelData.field === "legacy" ? qsTr("Имя / описание «") : qsTr("Получатель «"))
                                + modelData.pattern + "» → " + modelData.categoryName
                            color: dialog.textColor
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                        FormButton {
                            text: qsTr("Удалить")
                            onClicked: {
                                const result = controller.deleteBankCategoryRule(
                                    modelData.pattern, modelData.matchMode, modelData.type);
                                if (result.ok) {
                                    dialog.reloadRules(); dialog.previewImport();
                                } else { dialog.statusOk = false; dialog.statusText = result.error; }
                            }
                        }
                    }
                }

                ListView {
                    id: savedRecipientRulesList
                    visible: dialog.showSavedRules
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(180, contentHeight)
                    model: dialog.savedRecipientRules
                    clip: true
                    spacing: 8
                    ScrollBar.vertical: StyledScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        width: savedRecipientRulesList.width
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Имя получателя: «%1» → %2 (%3)").arg(modelData.pattern)
                                .arg(modelData.recipientName).arg(modelData.type === "income" ? qsTr("доход") : qsTr("расход"))
                            color: dialog.textColor
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                        FormButton {
                            text: qsTr("Удалить")
                            onClicked: {
                                const result = controller.deleteBankRecipientRule(modelData.pattern, modelData.field,
                                    modelData.matchMode, modelData.type);
                                if (result.ok) { dialog.reloadRules(); dialog.previewImport(); }
                                else { dialog.statusOk = false; dialog.statusText = result.error; }
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("Предварительный просмотр")
                    font.pixelSize: 14
                    color: dialog.textColor
                    font.weight: Font.DemiBold
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(240, dialog.previewRows.length * 48)
                    clip: true
                    model: dialog.previewRows
                    ScrollBar.vertical: StyledScrollBar { policy: ScrollBar.AlwaysOff }
                    delegate: Text {
                        required property string modelData
                        width: ListView.view.width - 12
                        height: 48
                        text: modelData
                        color: dialog.mutedColor
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.previewSummary.length > 0
                    text: dialog.previewSummary
                    font.pixelSize: 14
                    color: dialog.textColor
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.statusText.length > 0
                    text: dialog.statusText
                    font.pixelSize: 14
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
                enabled: !dialog.inspecting && dialog.editingProfileId.length > 0
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
                enabled: !dialog.inspecting
                text: qsTr("Проверить")
                onClicked: dialog.previewImport()
            }
            FormButton {
                enabled: !dialog.inspecting
                text: qsTr("Сохранить профиль")
                onClicked: dialog.saveProfile()
            }
            FormButton {
                enabled: !dialog.inspecting
                text: qsTr("Импортировать")
                primary: true
                onClicked: {
                    if (!dialog.previewImport())
                        return;
                    if (dialog.remainingReviews() > 0) {
                        dialog.statusOk = false;
                        dialog.statusText = qsTr("Проверьте получателей и категории выше: осталось %1 операций")
                            .arg(dialog.remainingReviews());
                        return;
                    }
                    const saved = dialog.saveProfile();
                    if (!saved.ok)
                        return;
                    const result = controller.importBankCsv(dialog.csvFile, saved.id, {
                        choices: dialog.categoryChoices, requireReview: true
                    });
                    dialog.statusOk = result.ok;
                    dialog.statusText = result.ok
                        ? qsTr("Импортировано: %1, дубликатов: %2, отклонено строк: %3")
                            .arg(result.imported).arg(result.skipped).arg(result.rejected)
                        : result.error;
                    if (result.ok) {
                        dialog.operationRows = []; dialog.categoryChoices = ({});
                        dialog.reloadRules();
                        dialog.importFinished(result);
                    }
                }
            }
        }
    }
}
