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
    property bool showReviewOnly: false
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
            pattern: row.merchant, matchMode: "exact"
        };
    }

    function setChoice(row, changes) {
        const next = Object.assign({}, categoryChoices);
        // Restore the choices underneath previous automatic assignments.
        for (const key in next)
            if (next[key].ruleBaseChoice)
                next[key] = next[key].ruleBaseChoice;
        const edited = Object.assign({}, choiceFor(row), changes);
        delete edited.ruleBaseChoice;
        next[row.rowKey] = edited;
        categoryChoices = applyPendingRules(next);
    }

    function ruleMatches(row, rule, pattern) {
        if (row.type !== rule.type || !pattern) return false;
        const description = controller.normalizeBankCategoryText(row.description || "");
        const merchant = controller.normalizeBankCategoryText(row.merchant || "");
        if (rule.matchMode === "exact")
            return pattern === merchant || pattern === description;
        // Normalized descriptions contain only words separated by single spaces.
        return rule.matchMode === "contains"
            && (" " + description + " ").indexOf(" " + pattern + " ") >= 0;
    }

    function applyPendingRules(base) {
        const rules = [];
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            const choice = base[row.rowKey];
            if (!choice || !choice.remember || !choice.confirmed
                    || categoryIndex(categoryItems(row.type), choice.categoryId) < 0)
                continue;
            const pattern = controller.normalizeBankCategoryText(choice.pattern || "");
            const rule = Object.assign({}, choice, { type: row.type });
            if (pattern.length <= 240 && ruleMatches(row, rule, pattern))
                rules.push({ rule: rule, pattern: pattern });
        }
        const next = Object.assign({}, base);
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            const original = base[row.rowKey] || choiceFor(row);
            // Explicit confirmations remain available as one-off exceptions.
            if (original.confirmed || original.remember) continue;
            let selected = "";
            let best = -1;
            let conflict = false;
            for (let j = 0; j < rules.length; ++j) {
                const entry = rules[j];
                if (!ruleMatches(row, entry.rule, entry.pattern)) continue;
                const score = entry.rule.matchMode === "exact" ? 241 : entry.pattern.length;
                if (score > best) {
                    best = score;
                    selected = entry.rule.categoryId;
                    conflict = false;
                } else if (score === best && selected !== entry.rule.categoryId) {
                    conflict = true;
                }
            }
            if (selected && !conflict)
                next[row.rowKey] = Object.assign({}, original, {
                    categoryId: selected, confirmed: !row.special,
                    ruleBaseChoice: original
                });
        }
        return next;
    }

    function categoryIndex(items, id) {
        for (let i = 0; i < items.length; ++i)
            if (items[i].value === id) return i;
        return -1;
    }

    function unresolved(row) {
        const choice = choiceFor(row);
        return categoryIndex(categoryItems(row.type), choice.categoryId) < 0
            || (row.needsReview && !choice.confirmed);
    }

    function reviewRows() {
        return operationRows.filter(function(row) {
            return !showReviewOnly || unresolved(row);
        });
    }

    function remainingReviews() {
        return operationRows.filter(function(row) { return unresolved(row); }).length;
    }

    function applyToMerchant(row) {
        const choice = choiceFor(row);
        if (!choice.categoryId) return;
        const next = Object.assign({}, categoryChoices);
        for (let i = 0; i < operationRows.length; ++i) {
            const other = operationRows[i];
            if (other.merchant === row.merchant && other.type === row.type && !other.special) {
                next[other.rowKey] = Object.assign({}, choiceFor(other), {
                    categoryId: choice.categoryId, confirmed: true
                });
                delete next[other.rowKey].ruleBaseChoice;
            }
        }
        categoryChoices = next;
    }

    function acceptSuggestions() {
        const next = Object.assign({}, categoryChoices);
        for (let i = 0; i < operationRows.length; ++i) {
            const row = operationRows[i];
            const choice = choiceFor(row);
            if (!row.special && categoryIndex(categoryItems(row.type), choice.categoryId) >= 0) {
                next[row.rowKey] = Object.assign({}, choice, { confirmed: true });
                delete next[row.rowKey].ruleBaseChoice;
            }
        }
        categoryChoices = next;
    }

    function reloadRules() {
        const result = controller.bankCategoryRules();
        if (result.ok) savedCategoryRules = result.items;
        else { statusOk = false; statusText = result.error; }
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

    function findHeader(words) {
        for (let word = 0; word < words.length; ++word) {
            for (let i = 0; i < headers.length; ++i) {
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
                 findHeader(["описание", "назначение", "merchant", "description"]));
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
                keptChoices[row.rowKey] = choice.ruleBaseChoice || choice;
        }
        categoryChoices = applyPendingRules(keptChoices);
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
        showReviewOnly = false; showSavedRules = false;
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
                    text: qsTr("Категории новых операций · Требуют проверки: %1")
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
                    text: qsTr("«Принять предложения» подтверждает категории обычных операций. Переводы, возвраты и снятия проверьте отдельно: импорт сохраняет их как доходы или расходы и не связывает счета автоматически.")
                    color: dialog.mutedColor
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                ListView {
                    id: categoryReviewList
                    visible: dialog.operationRows.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(420, contentHeight)
                    clip: true
                    spacing: 8
                    model: dialog.reviewRows()
                    ScrollBar.vertical: StyledScrollBar { policy: ScrollBar.AlwaysOff }
                    delegate: Rectangle {
                        id: reviewRow
                        required property var modelData
                        readonly property var choice: dialog.choiceFor(modelData)
                        readonly property var items: dialog.categoryItems(modelData.type)
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
                            spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: reviewRow.modelData.merchant || qsTr("Неизвестный получатель")
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: dialog.textColor
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: reviewRow.modelData.date + " · " + dialog.formatMinor(
                                        reviewRow.modelData.signedMinor, reviewRow.modelData.currency)
                                    color: dialog.textColor
                                    font.pixelSize: 14
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: reviewRow.modelData.description
                                color: dialog.mutedColor
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: dialog.unresolved(reviewRow.modelData)
                                    ? qsTr("Требует проверки · %1").arg(reviewRow.modelData.reason)
                                    : reviewRow.choice.confirmed ? qsTr("Выбор подтверждён") : reviewRow.modelData.reason
                                color: dialog.mutedColor
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16
                                FormCombo {
                                    Layout.preferredWidth: 240
                                    Layout.maximumWidth: 240
                                    model: reviewRow.items
                                    textRole: "label"
                                    valueRole: "value"
                                    emptyText: qsTr("Добавьте категорию")
                                    currentIndex: dialog.categoryIndex(reviewRow.items, reviewRow.choice.categoryId)
                                    onActivated: dialog.setChoice(reviewRow.modelData, {
                                        categoryId: currentValue, confirmed: true
                                    })
                                }
                                StyledCheckBox {
                                    text: qsTr("Подтверждено")
                                    checked: reviewRow.choice.confirmed
                                    enabled: dialog.categoryIndex(reviewRow.items, reviewRow.choice.categoryId) >= 0
                                    onClicked: dialog.setChoice(reviewRow.modelData, { confirmed: checked })
                                    appTextColor: dialog.textColor
                                    appAccentColor: dialog.accentColor
                                }
                                StyledCheckBox {
                                    text: qsTr("Запомнить")
                                    checked: reviewRow.choice.remember
                                    enabled: reviewRow.choice.confirmed && reviewRow.modelData.merchant.length > 0
                                    onClicked: dialog.setChoice(reviewRow.modelData, { remember: checked })
                                    appTextColor: dialog.textColor
                                    appAccentColor: dialog.accentColor
                                }
                                Item { Layout.fillWidth: true }
                            }
                            FormButton {
                                text: qsTr("Применить к этому магазину в выписке")
                                enabled: !!reviewRow.choice.categoryId && !reviewRow.modelData.special
                                onClicked: dialog.applyToMerchant(reviewRow.modelData)
                            }
                            RowLayout {
                                visible: reviewRow.choice.remember
                                Layout.fillWidth: true
                                spacing: 16
                                FormCombo {
                                    Layout.preferredWidth: 240
                                    Layout.maximumWidth: 240
                                    model: [ { label: qsTr("Получатель совпадает"), value: "exact" },
                                             { label: qsTr("Описание содержит"), value: "contains" } ]
                                    textRole: "label"
                                    valueRole: "value"
                                    currentIndex: reviewRow.choice.matchMode === "contains" ? 1 : 0
                                    onActivated: dialog.setChoice(reviewRow.modelData, { matchMode: currentValue })
                                }
                                FormField {
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 10000
                                    text: reviewRow.choice.pattern
                                    maximumLength: 240
                                    placeholderText: qsTr("Название или слова из описания")
                                    onTextEdited: dialog.setChoice(reviewRow.modelData, { pattern: text })
                                }
                            }
                        }
                    }
                }
                FormButton {
                    text: dialog.showSavedRules ? qsTr("Скрыть сохранённые правила")
                                                : qsTr("Сохранённые правила (%1)").arg(dialog.savedCategoryRules.length)
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
                                + (modelData.matchMode === "contains" ? qsTr("Содержит «") : qsTr("Получатель «"))
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
                        dialog.statusText = qsTr("Проверьте категории новых операций выше: осталось %1")
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
