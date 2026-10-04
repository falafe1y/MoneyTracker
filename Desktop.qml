import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1600
    height: 960
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: "Ledgera"
    color: root.canvas
    font.family: "Inter"
    font.pixelSize: 14

    readonly property int pageGap: 24
    readonly property int cardGap: 16
    readonly property int panelPadding: 24
    readonly property int cardRadius: 12

    // Outlined card style: solid dark outline + hard offset shadow (no blur).
    // Panels / buttons / fields opt in with `neo: true`.
    readonly property int outlineWidth: 2
    readonly property int shadowDepth: 6

    // Color palette
    // Ivory + indigo foundation. Indigo is the only primary accent;
    // Income and terracotta colors below are reserved for financial semantics.
    readonly property color canvas: "#F5F3E4"
    readonly property color panel: "#FFFFF8"
    readonly property color soft: "#FFFFF0"
    readonly property color line: "#D8D7C7"
    readonly property color accent: "#031528"   // indigo
    readonly property color muted: "#687483"

    // readonly property color accentDark: "#020D1A"
    // readonly property color accent2: "#294477"
    readonly property color accentSoft: "#536A98"
    readonly property color pale: "#E4E8F1"
    readonly property color paleText: "#E9EDF6"
    readonly property color red: "#B94F48"

    readonly property color navSelected: "#294477"
    readonly property color navHovered: "#142B48"

    readonly property color chartAccent1: "#072a50"
    readonly property color chartAccent2: "#0e54a0"
    readonly property color chartAccent3: "#3a8eee"
    readonly property color chartAccent4: "#31c7ed"
    readonly property color chartAccent5: "#70d6f0"
    readonly property var chartColors: [chartAccent1, chartAccent2, chartAccent3, chartAccent4, chartAccent5]
    readonly property color tableHeader: "#EEEAD1"
    readonly property color tableRowAlt: "#FAF9EC"
    readonly property color categoryRow: "#FFFFF0"
    readonly property color categoryEditRow: "#F4F3E3"
    readonly property color controlHovered: "#F3F1E3"
    readonly property int chartPriceLevels: 7

    readonly property color incomePanel: "#E8F0E9"
    readonly property color expensePanel: "#F5E6E2"
    readonly property color income: "#3F735F"

    readonly property color white: "#FFFFF0"
    readonly property color transparentColor: "transparent"

    property string page: "overview"
    onPageChanged: browserScroll.stop()
    property var activeHistorySelection: null
    readonly property var scrollController: browserScroll
    AutoScrollController { id: browserScroll; parent: root.contentItem }
    property string searchText: ""
    property string csvStatus: ""
    property bool csvStatusOk: true
    readonly property var assets: [
        {
            code: "fiat",
            title: qsTr("Фиат"),
            icon: ""
        },
        {
            code: "crypto",
            title: qsTr("Криптовалюта"),
            icon: ""
        },
        {
            code: "investment",
            title: qsTr("Инвестиции"),
            icon: ""
        }
    ]

    function symbol(code) {
        return code === "USD" ? "$" : code === "EUR" ? "€" : "₽";
    }

    function uiLocale() {
        return Qt.locale(financeController.uiLanguage === "en" ? "en_US" : "ru_RU");
    }

    function money(minor, code, sign) {
        const numericMinor = Number(minor);
        const roundedMinor = Number.isFinite(numericMinor)
                           ? Math.round(numericMinor)
                           : 0;
        const absoluteMinor = Math.abs(roundedMinor);
        const whole = Math.floor(absoluteMinor / 100);
        const cents = absoluteMinor % 100;
        const decimals = Math.abs(roundedMinor) % 100 === 0 ? 0 : 2;
        const decimalSeparator = financeController.uiLanguage === "en" ? "." : ",";
        const groupedWhole = String(whole).replace(/\B(?=(\d{3})+(?!\d))/g, " ");
        const formatted = decimals === 0
                        ? groupedWhole
                        : groupedWhole + decimalSeparator
                          + (cents < 10 ? "0" : "") + String(cents);
        const prefix = sign
                     ? (roundedMinor >= 0 ? "+" : "−")
                     : (roundedMinor < 0 ? "−" : "");
        return prefix
             + formatted
             + " " + symbol(code || financeController.appCurrency);
    }

    function assetTitle(code) {
        for (let i = 0; i < assets.length; ++i)
            if (assets[i].code === code)
                return assets[i].title;
        return qsTr("Фиат");
    }

    function assetAmount(code) {
        const rows = financeController.assetSummaries;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].code === code)
                return rows[i].balanceMinor;
        return 0;
    }

    function selectedCryptoSymbol() {
        const rows = financeController.cryptoWallets;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === financeController.selectedCryptoWalletId)
                return rows[i].isExchange ? rows[i].name : rows[i].symbol;
        return "";
    }

    function exchangeSection(section) {
        const names = { FundingAccount: qsTr("Финансирование"), UnifiedTradingAccount: qsTr("Торговый счёт"),
            Earn: qsTr("Накопления"), TradingBot: qsTr("Торговые боты"), CopyTrading: qsTr("Копирование сделок"),
            CryptoLoans: qsTr("Криптозаймы"), CryptoLoans_legacy: qsTr("Криптозаймы"),
            Launchpool: qsTr("Пул вознаграждений") };
        return names[section] || section;
    }

    function selectedProject() {
        const rows = financeController.projects;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === financeController.selectedProjectId)
                return rows[i];
        return null;
    }

    function selectedBudget() {
        const rows = financeController.budgets;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === financeController.selectedBudgetId)
                return rows[i];
        return null;
    }

    function budgetMonthDate() {
        const parts = financeController.selectedBudgetMonth.split("-");
        return parts.length === 3
             ? new Date(Number(parts[0]), Number(parts[1]) - 1, 1)
             : new Date();
    }

    function budgetMonthLabel() {
        const value = budgetMonthDate();
        return root.expenseMonthLabel(value.getMonth())
            + " " + value.getFullYear();
    }

    function shiftBudgetMonth(offset) {
        const value = budgetMonthDate();
        value.setMonth(value.getMonth() + offset);
        const month = value.getMonth() + 1;
        financeController.selectedBudgetMonth = value.getFullYear() + "-"
            + (month < 10 ? "0" : "") + month + "-01";
    }

    function projectName(id) {
        const rows = financeController.projects;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === id)
                return rows[i].name;
        return "";
    }

    function accountTypeLabel(type) {
        if (type === "cash") return qsTr("Наличные");
        if (type === "debit_card") return qsTr("Дебетовая карта");
        if (type === "credit_card") return qsTr("Кредитная карта");
        if (type === "crypto_wallet") return qsTr("Криптокошелёк");
        if (type === "brokerage") return qsTr("Брокер");
        if (type === "deposit") return qsTr("Вклад");
        return qsTr("Другой");
    }

    function accountPrimaryAmount(row) {
        if (row && row.isExchange) return row.hasSnapshot ? root.money(row.valueMinor, row.currency, false) : row.refreshing ? qsTr("Загрузка…") : qsTr("Нет данных");
        if (row && row.isCrypto)
            return row.balanceText + " " + row.symbol;
        if (row && row.isCreditCard)
            return qsTr("Задолженность: %1").arg(
                root.money(row.debtMinor, row.currency, false)
            );
        return row ? root.money(row.balanceMinor, row.currency, false) : "";
    }

    function accountCompactAmount(row) {
        if (row && row.isExchange) return row.hasSnapshot ? root.money(row.valueMinor, row.currency, false) : row.refreshing ? qsTr("Загрузка…") : qsTr("Нет данных");
        if (row && row.isCrypto)
            return row.balanceText + " " + row.symbol;
        if (row && row.isCreditCard)
            return qsTr("Долг %1 · доступно %2")
                .arg(root.money(row.debtMinor, row.currency, false))
                .arg(root.money(row.availableCreditMinor, row.currency, false));
        return row ? root.money(row.balanceMinor, row.currency, false) : "";
    }

    function accountAvailableCredit(row) {
        return row && row.isCreditCard
             ? qsTr("Доступно: %1 из %2")
                   .arg(root.money(row.availableCreditMinor, row.currency, false))
                   .arg(root.money(row.creditLimitMinor, row.currency, false))
             : "";
    }

    function overviewAccountSelected(row) {
        if (!row)
            return false;
        if (financeController.selectedAsset === "crypto")
            return financeController.selectedCryptoWalletId === (row.id || "");
        return financeController.selectedAccountId === (row.id || "");
    }

    function cryptoWalletStatus(row) {
        if (!row || !row.isCrypto)
            return "";
        if (row.refreshing)
            return qsTr("Обновление баланса…");
        if (row.connectionError) return row.connectionError;
        if (!row.hasSnapshot)
            return qsTr("Баланс ещё не обновлён");
        const updated = qsTr("Обновлено: %1").arg(
            Qt.formatDateTime(row.updatedAt, "dd.MM.yyyy HH:mm")
        );
        return row.priceHasSnapshot
             ? updated + " · 1 " + row.symbol + " = "
               + Number(row.priceUsd).toLocaleString(
                     root.uiLocale(), "f", row.symbol === "USDT" ? 4 : 2
                 ) + " $"
             : updated;
    }

    function accountName(id) {
        const rows = financeController.allAccounts;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === id)
                return rows[i].name;
        return qsTr("Другой счёт");
    }

    function amountForInput(minor) {
        const value = Math.round(Math.abs(Number(minor)));
        const whole = Math.floor(value / 100);
        const cents = value % 100;
        return cents === 0 ? String(whole)
                           : String(whole) + "." + (cents < 10 ? "0" : "") + String(cents);
    }

    function signedAmountForInput(minor) {
        return Number(minor) < 0 ? "-" + amountForInput(minor) : amountForInput(minor);
    }

    function transactionSignedAmount(row) {
        if (row.type === "investment_position")
            return row.amount;
        return row.type === "income" || (row.type === "transfer" && row.direction === "in")
             ? row.amount : -row.amount;
    }

    function transactionTypeLabel(row) {
        if (row.type === "investment_position")
            return qsTr("Позиция");
        if (row.type === "transfer")
            return qsTr("Перевод");
        return row.type === "income" ? qsTr("Доход") : qsTr("Расход");
    }

    function indexByRole(model, role, value) {
        for (let i = 0; i < model.length; ++i)
            if (model[i][role] === value)
                return i;
        return model.length > 0 ? 0 : -1;
    }

    function openTransactionContextMenu(row, sourceItem, localX, localY) {
        const point = sourceItem.mapToItem(root.contentItem, localX, localY);
        transactionContextMenu.transactionData = row;
        transactionContextMenu.x = Math.max(
            8,
            Math.min(point.x, root.contentItem.width - transactionContextMenu.width - 8)
        );
        transactionContextMenu.y = Math.max(
            8,
            Math.min(point.y, root.contentItem.height - transactionContextMenu.implicitHeight - 8)
        );
        transactionContextMenu.open();
    }

    function openAccountContextMenu(row, sourceItem, localX, localY) {
        if (!row || !row.id)
            return;
        const point = sourceItem.mapToItem(root.contentItem, localX, localY);
        accountContextMenu.accountData = row;
        accountContextMenu.x = Math.max(
            8,
            Math.min(point.x, root.contentItem.width - accountContextMenu.width - 8)
        );
        accountContextMenu.y = Math.max(
            8,
            Math.min(point.y, root.contentItem.height - accountContextMenu.implicitHeight - 8)
        );
        accountContextMenu.open();
    }

    function openBudgetContextMenu(row, sourceItem, localX, localY) {
        if (!row || !row.id)
            return;
        const point = sourceItem.mapToItem(root.contentItem, localX, localY);
        budgetContextMenu.budgetData = row;
        budgetContextMenu.x = Math.max(
            8,
            Math.min(point.x, root.contentItem.width - budgetContextMenu.width - 8)
        );
        budgetContextMenu.y = Math.max(
            8,
            Math.min(point.y, root.contentItem.height - budgetContextMenu.implicitHeight - 8)
        );
        budgetContextMenu.open();
    }

    function openProjectContextMenu(row, sourceItem, localX, localY) {
        if (!row || !row.id)
            return;
        const point = sourceItem.mapToItem(root.contentItem, localX, localY);
        projectContextMenu.projectData = row;
        projectContextMenu.x = Math.max(
            8,
            Math.min(point.x, root.contentItem.width - projectContextMenu.width - 8)
        );
        projectContextMenu.y = Math.max(
            8,
            Math.min(point.y, root.contentItem.height - projectContextMenu.implicitHeight - 8)
        );
        projectContextMenu.open();
    }

    function openCategoryContextMenu(row, sourceItem, localX, localY) {
        if (!row || !row.value)
            return;
        const point = sourceItem.mapToItem(root.contentItem, localX, localY);
        categoryContextMenu.categoryData = row;
        categoryContextMenu.x = Math.max(
            8,
            Math.min(point.x, root.contentItem.width - categoryContextMenu.width - 8)
        );
        categoryContextMenu.y = Math.max(
            8,
            Math.min(point.y, root.contentItem.height - categoryContextMenu.implicitHeight - 8)
        );
        categoryContextMenu.open();
    }

    function dateFromIso(value) {
        if (!value)
            return null;
        const parts = value.split("-");
        if (parts.length !== 3)
            return null;
        return new Date(
            Number(parts[0]),
            Number(parts[1]) - 1,
            Number(parts[2]),
            12, 0, 0, 0
        );
    }

    function dateFilterLabel() {
        if (!financeController.dateFilterActive)
            return qsTr("Все время");
        const from = dateFromIso(financeController.dateFilterFrom);
        const to = dateFromIso(financeController.dateFilterTo);
        if (!from || !to)
            return qsTr("Все время");
        if (from.getTime() === to.getTime())
            return Qt.formatDate(from, "dd.MM.yyyy");
        return Qt.formatDate(from, "dd.MM.yyyy")
             + " — " + Qt.formatDate(to, "dd.MM.yyyy");
    }

    function capitalHistoryResolutionLabel() {
        return historyResolutionLabel(financeController.capitalHistory);
    }

    function historyResolutionLabel(rows) {
        if (rows.length === 0)
            return "";
        if (rows[0].resolution === "year")
            return qsTr("По годам");
        if (rows[0].resolution === "month")
            return qsTr("По месяцам");
        return qsTr("По дням");
    }

    function averageCompletedMonthExpense(rows) {
        let total = 0;
        let count = 0;
        for (let i = 0; i < rows.length; ++i) {
            if (!rows[i].tracked || !rows[i].complete)
                continue;
            total += Number(rows[i].totalMinor);
            ++count;
        }
        if (count === 0) {
            for (let j = 0; j < rows.length; ++j) {
                if (!rows[j].tracked)
                    continue;
                total += Number(rows[j].totalMinor);
                ++count;
            }
        }
        return count > 0 ? Math.round(total / count) : 0;
    }

    function expenseHistoryFirstYear() {
        const rows = financeController.expenseHistoryByMonthRub;
        if (rows.length === 0)
            return new Date().getFullYear();
        const date = root.dateFromIso(rows[0].date);
        return date ? date.getFullYear() : new Date().getFullYear();
    }

    function expenseHistoryLastYear() {
        const rows = financeController.expenseHistoryByMonthRub;
        if (rows.length === 0)
            return new Date().getFullYear();
        const date = root.dateFromIso(rows[rows.length - 1].date);
        return date ? Math.max(new Date().getFullYear(), date.getFullYear())
                    : new Date().getFullYear();
    }

    function expenseRowsForYear(year) {
        const source = financeController.expenseHistoryByMonthRub;
        const byMonth = {};
        for (let i = 0; i < source.length; ++i) {
            const date = root.dateFromIso(source[i].date);
            if (date && date.getFullYear() === year)
                byMonth[date.getMonth()] = source[i];
        }
        const result = [];
        for (let month = 0; month < 12; ++month) {
            const row = byMonth[month];
            result.push({
                date: year + "-" + (month < 9 ? "0" : "") + (month + 1) + "-01",
                monthIndex: month,
                totalMinor: row ? Number(row.totalMinor) : 0,
                complete: row ? Boolean(row.complete) : false,
                tracked: Boolean(row)
            });
        }
        return result;
    }

    function expenseMonthLabel(month) {
        return [
            qsTr("Янв."), qsTr("Фев."), qsTr("Мар."), qsTr("Апр."),
            qsTr("Май"), qsTr("Июн."), qsTr("Июл."), qsTr("Авг."),
            qsTr("Сен."), qsTr("Окт."), qsTr("Ноя."), qsTr("Дек.")
        ][month];
    }

    function niceChartStep(range, targetIntervals) {
        if (!Number.isFinite(range) || range <= 0)
            return 1;
        const raw = range / Math.max(1, targetIntervals);
        const magnitude = Math.pow(10, Math.floor(Math.log(raw) / Math.LN10));
        const normalized = raw / magnitude;
        const factor = normalized <= 1 ? 1
                     : normalized <= 2 ? 2
                     : normalized <= 2.5 ? 2.5
                     : normalized <= 3 ? 3
                     : normalized <= 4 ? 4
                     : normalized <= 5 ? 5
                     : normalized <= 7.5 ? 7.5 : 10;
        return factor * magnitude;
    }

    function niceChartAxis(minimum, maximum) {
        let low = Number(minimum);
        let high = Number(maximum);
        const intervals = Math.max(1, chartPriceLevels - 1);
        if (!Number.isFinite(low) || !Number.isFinite(high))
            return { minimum: 0, maximum: intervals, step: 1, intervals: intervals };
        if (low > high) {
            const swap = low;
            low = high;
            high = swap;
        }
        let span = high - low;
        if (span === 0)
            span = Math.max(100, Math.abs(high) * 0.1);
        const padding = span * 0.04;
        let step = niceChartStep(span + padding * 2, intervals);
        let axisMinimum;
        while (true) {
            const lowerStart = Math.floor((low - padding) / step + 1e-9) * step;
            const upperStart = Math.ceil((high + padding) / step - 1e-9) * step
                             - intervals * step;
            axisMinimum = Math.max(lowerStart, upperStart);
            if (axisMinimum <= low + step * 1e-9)
                break;
            step = niceChartStep(step * (1 + 1e-9), 1);
        }
        return {
            minimum: axisMinimum,
            maximum: axisMinimum + intervals * step,
            step: step,
            intervals: intervals
        };
    }

    function capitalAxisMoney(minor, code) {
        const amount = Math.abs(Number(minor)) < 0.5 ? 0 : Number(minor) / 100;
        const absolute = Math.abs(amount);
        let divisor = 1;
        let suffix = "";
        const westernStyle = code !== "RUB";
        if (absolute >= 1000000000) {
            divisor = 1000000000;
            suffix = westernStyle ? "B" : qsTr("млрд");
        } else if (absolute >= 1000000) {
            divisor = 1000000;
            suffix = westernStyle ? "M" : qsTr("млн");
        } else if (absolute >= 1000) {
            divisor = 1000;
            suffix = westernStyle ? "K" : qsTr("тыс.");
        }
        if (westernStyle) {
            const scaledWestern = amount / divisor;
            const roundedWestern = Math.round(scaledWestern);
            const wholeWestern = Math.abs(scaledWestern - roundedWestern) < 0.000001;
            const westernDigits = wholeWestern || Math.abs(scaledWestern) >= 100 ? 0
                                : Math.abs(scaledWestern) >= 10 ? 1 : 2;
            const westernNumber = Math.abs(scaledWestern).toFixed(westernDigits)
                .replace(/\.0+$/, "").replace(/(\.\d*[1-9])0+$/, "$1");
            return (scaledWestern < 0 ? "−" : "")
                 + root.symbol(code) + westernNumber + suffix;
        }
        if (divisor === 1)
            return root.money(Math.round(Number(minor)), code, false);
        const scaled = amount / divisor;
        const roundedScaled = Math.round(scaled);
        const whole = Math.abs(scaled - roundedScaled) < 0.000001;
        const digits = whole || Math.abs(scaled) >= 100 ? 0
                     : Math.abs(scaled) >= 10 ? 1 : 2;
        return scaled.toLocaleString(root.uiLocale(), "f", digits)
             + " " + suffix + " " + root.symbol(code);
    }

    function capitalDateLabel(row) {
        const date = root.dateFromIso(row.date);
        if (!date)
            return "";
        return Qt.formatDate(date, "dd.MM.yyyy");
    }

    function visibleTransactions() {
        const result = [], ids = {}, accounts = financeController.accounts;
        for (let i = 0; i < accounts.length; ++i)
            ids[accounts[i].id] = true;
        const rows = financeController.transactions, query = searchText.trim().toLowerCase();
        for (let j = 0; j < rows.length; ++j) {
            const row = rows[j];
            if (!ids[row.accountId])
                continue;
            if (financeController.selectedAccountId && row.accountId !== financeController.selectedAccountId)
                continue;
            const text = (row.description + " " + row.categoryName + " " + accountName(row.accountId)).toLowerCase();
            if (!query || text.indexOf(query) >= 0)
                result.push(row);
        }
        return result;
    }

    function investmentOperationRow(position) {
        const description = position.symbol + " · " + position.name
                          + " · " + qsTr("Количество: %1")
                                .arg(position.quantityText);
        return {
            id: position.id,
            accountId: position.accountId,
            accountName: position.accountName,
            type: "investment_position",
            amount: position.averageValueMinor,
            currency: position.currency,
            categoryName: position.typeName,
            rawDescription: description,
            date: new Date(position.createdAt),
            instrumentId: position.instrumentId,
            symbol: position.symbol,
            isin: position.isin,
            name: position.name,
            typeName: position.typeName,
            quantityText: position.quantityText,
            averagePriceText: position.averagePriceText,
            hasQuote: position.hasQuote,
            priceText: position.priceText
        };
    }

    function visibleInvestmentOperations() {
        const result = [];
        const rows = financeController.investmentPositions;
        const query = searchText.trim().toLowerCase();
        const from = root.dateFromIso(financeController.dateFilterFrom);
        const to = root.dateFromIso(financeController.dateFilterTo);
        if (to)
            to.setHours(23, 59, 59, 999);
        for (let i = rows.length - 1; i >= 0; --i) {
            const row = root.investmentOperationRow(rows[i]);
            if (financeController.dateFilterActive
                && (isNaN(row.date.getTime())
                    || row.date < from || row.date > to))
                continue;
            const text = (row.rawDescription + " " + row.categoryName + " "
                          + row.accountName).toLowerCase();
            if (query && text.indexOf(query) < 0)
                continue;
            result.push(row);
        }
        return result;
    }

    function dashboardHistoryRows() {
        return financeController.selectedAsset === "investment"
             ? visibleInvestmentOperations()
             : visibleTransactions();
    }

    function categoryTotals() {
        const values = {}, names = {}, rows = visibleTransactions();
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].type !== "expense")
                continue;
            const id = rows[i].categoryId || "none";
            values[id] = (values[id] || 0) + Number(rows[i].displayAmount);
            names[id] = rows[i].categoryName || qsTr("Без категории");
        }
        const result = [];
        for (const id in values)
            result.push({
                label: names[id],
                amount: values[id]
            });
        result.sort(function (a, b) {
            return b.amount - a.amount;
        });
        return result.slice(0, 5);
    }

    function modalDialogVisible() {
        return cryptoWalletDialog.visible
            || accountDialog.visible
            || deleteAccountDialog.visible
            || deleteCategoryDialog.visible
            || deleteTransactionDialog.visible
            || deleteSelectedHistoryDialog.visible
            || projectDialog.visible
            || deleteProjectDialog.visible
            || operationDialog.visible
            || dateFilterDialog.visible
            || operationDateDialog.visible
            || categoryDialog.visible
            || investmentPositionDialog.visible
            || bankCsvImportDialog.visible
            || recurringTransactionsDialog.visible
            || clearDataDialog.visible;
    }

    function selectedInvestmentAccountId() {
        const accounts = financeController.investmentAccounts;
        for (let i = 0; i < accounts.length; ++i)
            if (accounts[i].id === financeController.selectedAccountId)
                return accounts[i].id;
        return "";
    }

    function selectedInvestmentAccountAcceptsPositions() {
        return selectedInvestmentAccountId().length > 0;
    }

    function openNewOperationForSelectedAsset() {
        if (financeController.selectedAsset === "fiat") {
            operationDialog.openForNew();
            return;
        }
        if (financeController.selectedAsset === "investment"
            && selectedInvestmentAccountAcceptsPositions()) {
            investmentPositionDialog.openForNewPosition(
                selectedInvestmentAccountId()
            );
        }
    }

    Shortcut {
        sequences: [StandardKey.New]
        context: Qt.ApplicationShortcut
        enabled: !root.modalDialogVisible()
              && !accountContextMenu.visible
              && !categoryContextMenu.visible
              && !transactionContextMenu.visible
              && (financeController.selectedAsset === "fiat"
                  || (financeController.selectedAsset === "investment"
                      && root.selectedInvestmentAccountAcceptsPositions()))
        onActivated: root.openNewOperationForSelectedAsset()
    }

    component Panel: Rectangle {
        id: panelRoot
        property bool neo: false

        SurfaceShadow { visible: !panelRoot.neo }
        HardShadow { visible: panelRoot.neo; depth: root.shadowDepth }

        color: root.panel
        radius: 16
        border.width: neo ? root.outlineWidth : 1
        border.color: neo ? root.accent : root.line
    }

    component AnalyticsLineChart: Item {
        id: analyticsLineRoot
        property var points: []
        property string currency: "RUB"
        property color seriesColor: root.chartAccent3
        property string hoveredValue: ""

        Canvas {
            id: analyticsLineCanvas
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            Connections {
                target: analyticsLineRoot
                function onPointsChanged() {
                    analyticsLineRoot.hoveredValue = "";
                    analyticsLineCanvas.requestPaint();
                }
                function onCurrencyChanged() { analyticsLineCanvas.requestPaint(); }
                function onSeriesColorChanged() { analyticsLineCanvas.requestPaint(); }
            }

            onPaint: {
                const ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                const data = parent.points || [];
                if (data.length === 0 || width < 180 || height < 100)
                    return;
                const left = 90, right = 12, top = 10, bottom = 38;
                const plotWidth = Math.max(1, width - left - right);
                const plotHeight = Math.max(1, height - top - bottom);
                let minimum = Number(data[0].totalMinor);
                let maximum = minimum;
                for (let i = 1; i < data.length; ++i) {
                    const value = Number(data[i].totalMinor);
                    minimum = Math.min(minimum, value);
                    maximum = Math.max(maximum, value);
                }
                const axis = root.niceChartAxis(minimum, maximum);
                minimum = axis.minimum;
                maximum = axis.maximum;
                const range = Math.max(1, maximum - minimum);
                const firstDate = root.dateFromIso(data[0].date);
                const lastDate = root.dateFromIso(data[data.length - 1].date);
                const firstTime = firstDate ? firstDate.getTime() : 0;
                const lastTime = lastDate ? lastDate.getTime() : firstTime;
                const timeRange = Math.max(1, lastTime - firstTime);
                function xFor(row) {
                    if (data.length === 1 || lastTime === firstTime)
                        return left + plotWidth / 2;
                    const date = root.dateFromIso(row.date);
                    return left + ((date ? date.getTime() : firstTime) - firstTime)
                        / timeRange * plotWidth;
                }
                function yFor(value) {
                    return top + (maximum - value) / range * plotHeight;
                }
                ctx.font = "14px sans-serif";
                for (let tick = 0; tick <= axis.intervals; ++tick) {
                    const ratio = tick / axis.intervals;
                    const y = top + ratio * plotHeight;
                    ctx.strokeStyle = root.line;
                    ctx.lineWidth = 1;
                    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + plotWidth, y); ctx.stroke();
                    ctx.fillStyle = root.muted;
                    ctx.textAlign = "right";
                    ctx.textBaseline = "middle";
                    ctx.fillText(root.capitalAxisMoney(
                        maximum - tick * axis.step, parent.currency), left - 7, y);
                }
                ctx.strokeStyle = parent.seriesColor;
                ctx.lineWidth = 2.5;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";
                ctx.beginPath();
                for (let point = 0; point < data.length; ++point) {
                    const x = xFor(data[point]);
                    const y = yFor(Number(data[point].totalMinor));
                    if (point === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.stroke();
                // The plot uses elapsed time, so its labels must use the same scale.
                const sampleLabel = root.capitalDateLabel(data[0]);
                const labelWidth = ctx.measureText(sampleLabel).width;
                const labelCount = data.length === 1 || lastTime === firstTime ? 1
                    : Math.max(2, Math.min(5,
                        Math.floor(plotWidth / (labelWidth + 32))));
                for (let label = 0; label < labelCount; ++label) {
                    const fraction = labelCount === 1 ? 0.5
                        : label / (labelCount - 1);
                    const x = left + fraction * plotWidth;
                    const date = labelCount === 1 ? firstDate
                        : new Date(firstTime + fraction * (lastTime - firstTime));
                    const labelText = date ? Qt.formatDate(date, "dd.MM.yyyy") : "";
                    ctx.fillStyle = root.muted;
                    ctx.textAlign = labelCount === 1 ? "center"
                        : label === 0 ? "left"
                        : label === labelCount - 1 ? "right" : "center";
                    ctx.textBaseline = "top";
                    ctx.fillText(labelText, x, top + plotHeight + 8);
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onPositionChanged: function(mouse) {
                const rows = analyticsLineRoot.points || [];
                if (rows.length === 0 || width < 180) {
                    analyticsLineRoot.hoveredValue = "";
                    return;
                }
                const first = root.dateFromIso(rows[0].date);
                const last = root.dateFromIso(rows[rows.length - 1].date);
                const start = first ? first.getTime() : 0;
                const range = last ? Math.max(1, last.getTime() - start) : 1;
                const position = Math.max(0, Math.min(1,
                    (mouse.x - 90) / Math.max(1, width - 102)));
                let closest = rows[0];
                let distance = Infinity;
                for (let i = 0; i < rows.length; ++i) {
                    const date = root.dateFromIso(rows[i].date);
                    const fraction = rows.length === 1 ? 0.5
                        : ((date ? date.getTime() : start) - start) / range;
                    if (Math.abs(fraction - position) < distance) {
                        distance = Math.abs(fraction - position);
                        closest = rows[i];
                    }
                }
                analyticsLineRoot.hoveredValue =
                    root.capitalDateLabel(closest) + " · "
                    + root.money(closest.totalMinor, analyticsLineRoot.currency, false);
            }
            onExited: analyticsLineRoot.hoveredValue = ""
            ToolTip.delay: 350
            ToolTip.visible: containsMouse && analyticsLineRoot.hoveredValue.length > 0
            ToolTip.text: analyticsLineRoot.hoveredValue
        }
        Text {
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 24)
            visible: analyticsLineRoot.points.length === 0
            text: qsTr("Добавьте операцию — здесь появится история капитала")
            color: root.muted
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    component AnalyticsBarChart: Item {
        id: analyticsBarRoot
        property var points: []
        property string currency: "RUB"
        property real averageMinor: 0

        Canvas {
            id: analyticsBarCanvas
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()
            Connections {
                target: analyticsBarRoot
                function onPointsChanged() { analyticsBarCanvas.requestPaint(); }
                function onAverageMinorChanged() { analyticsBarCanvas.requestPaint(); }
            }
            onPaint: {
                const ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                const data = parent.points || [];
                if (data.length === 0 || width < 180 || height < 100)
                    return;
                const left = 90, right = 12, top = 10, bottom = 38;
                const plotWidth = Math.max(1, width - left - right);
                const plotHeight = Math.max(1, height - top - bottom);
                let maximum = 0;
                for (let i = 0; i < data.length; ++i)
                    maximum = Math.max(maximum, Number(data[i].totalMinor));
                const paddedMaximum = Math.max(100, maximum * 1.08);
                const tickIntervals = Math.max(1, root.chartPriceLevels - 1);
                const tickStep = root.niceChartStep(paddedMaximum, tickIntervals);
                maximum = tickIntervals * tickStep;
                ctx.font = "14px sans-serif";
                for (let tick = 0; tick <= tickIntervals; ++tick) {
                    const ratio = tick / tickIntervals;
                    const y = top + ratio * plotHeight;
                    ctx.strokeStyle = root.line;
                    ctx.lineWidth = 1;
                    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + plotWidth, y); ctx.stroke();
                    ctx.fillStyle = root.muted;
                    ctx.textAlign = "right";
                    ctx.textBaseline = "middle";
                    ctx.fillText(root.capitalAxisMoney(
                        maximum - tick * tickStep, parent.currency), left - 7, y);
                }
                const slot = plotWidth / data.length;
                const barWidth = Math.max(2, slot * 0.68);
                ctx.fillStyle = root.red;
                for (let bar = 0; bar < data.length; ++bar) {
                    const value = Number(data[bar].totalMinor);
                    const barHeight = value / maximum * plotHeight;
                    ctx.fillRect(left + bar * slot + (slot - barWidth) / 2,
                                 top + plotHeight - barHeight, barWidth, barHeight);
                }
                if (parent.averageMinor > 0) {
                    const averageY = top + plotHeight
                        - Math.min(parent.averageMinor, maximum) / maximum * plotHeight;
                    ctx.save();
                    ctx.strokeStyle = root.accent;
                    ctx.lineWidth = 1.5;
                    ctx.setLineDash([6, 4]);
                    ctx.beginPath();
                    ctx.moveTo(left, averageY);
                    ctx.lineTo(left + plotWidth, averageY);
                    ctx.stroke();
                    ctx.restore();
                }
                ctx.font = "14px sans-serif";
                for (let index = 0; index < data.length; ++index) {
                    ctx.fillStyle = root.muted;
                    ctx.textBaseline = "top";
                    ctx.textAlign = "center";
                    ctx.fillText(root.expenseMonthLabel(data[index].monthIndex),
                                 left + (index + 0.5) * slot,
                                 top + plotHeight + 8);
                }
            }
        }
    }

    // Item.clip and ListView.clip are rectangular. These masks cover content
    // that would otherwise remain visible outside the rounded bottom corners.
    component BottomCornerMask: Canvas {
        id: cornerMask
        property bool mirrored: false
        // With the hard shadow the area outside the arc is partly shadow, not canvas.
        property bool neo: false
        property int depth: 0

        onNeoChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onMirroredChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            const radius = Math.min(width, height);
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = root.canvas;
            ctx.beginPath();

            if (mirrored) {
                ctx.moveTo(width, 0);
                ctx.lineTo(width, height);
                ctx.lineTo(0, height);
                ctx.arc(0, 0, radius, Math.PI / 2, 0, true);
            } else {
                ctx.moveTo(0, 0);
                ctx.lineTo(0, height);
                ctx.lineTo(width, height);
                ctx.arc(width, 0, radius, Math.PI / 2, Math.PI, false);
            }

            ctx.closePath();
            ctx.fill();

            if (neo) {
                ctx.save();
                ctx.clip();
                ctx.fillStyle = root.accent;
                ctx.beginPath();
                if (mirrored) {
                    ctx.moveTo(width, 0);
                    ctx.lineTo(width, depth);
                    ctx.arc(0, depth, radius, 0, Math.PI / 2, false);
                    ctx.lineTo(0, 0);
                } else {
                    ctx.moveTo(0, 0);
                    ctx.lineTo(0, depth);
                    ctx.arc(width, depth, radius, Math.PI, Math.PI / 2, true);
                    ctx.lineTo(width, 0);
                }
                ctx.closePath();
                ctx.fill();
                ctx.restore();
            }
        }
    }
    component SoftButton: StyledButton {
        cornerRadius: 8
        primary: highlighted
        controlHeight: 42
        appTextColor: root.accent
        appMutedColor: root.muted
        appPanelColor: root.panel
        appSoftColor: root.soft
        appLineColor: root.line
        appAccentColor: root.accent
        appHoverColor: root.controlHovered
        appOnAccentColor: root.white
        appErrorColor: root.red
    }

    component AppMenuItem: MenuItem {
        id: menuItem
        property bool destructive: false
        hoverEnabled: true
        implicitHeight: 40
        leftPadding: 12
        rightPadding: 12

        contentItem: Text {
            text: menuItem.text
            color: menuItem.destructive ? root.red : root.accent
            font.pixelSize: 14
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 8
            color: menuItem.highlighted || menuItem.hovered
                   ? (menuItem.destructive ? root.expensePanel : root.controlHovered)
                   : root.transparentColor
        }
    }
    component AppTextField: StyledTextField {
        appTextColor: root.accent
        appMutedColor: root.muted
        appPanelColor: root.panel
        appSoftColor: root.soft
        appLineColor: root.line
        appAccentColor: root.navSelected
        appOnAccentColor: root.white
    }
    component AppCheckBox: StyledCheckBox {
        appTextColor: root.accent
        appMutedColor: root.muted
        appSoftColor: root.soft
        appLineColor: root.line
        appAccentColor: root.accent
        appHoverColor: root.controlHovered
        appOnAccentColor: root.white
    }
    component AppComboBox: StyledComboBox {
        appTextColor: root.accent
        appMutedColor: root.muted
        appPanelColor: root.panel
        appSoftColor: root.soft
        appLineColor: root.line
        appAccentColor: root.navSelected
        appHoverColor: root.controlHovered
    }
    component NavButton: Button {
        id: nav
        property string glyph: ""
        property string iconSource: ""
        property string target: ""
        flat: true
        hoverEnabled: true
        implicitHeight: 48
        leftPadding: 16
        rightPadding: 16
        contentItem: RowLayout {
            spacing: 16
            Item {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20

                Text {
                    anchors.fill: parent
                    visible: nav.iconSource.length === 0
                    text: nav.glyph
                    color: root.page === nav.target ? root.white : root.paleText
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Image {
                    id: navIconImage
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    visible: nav.iconSource.length > 0
                    source: nav.iconSource
                    sourceSize: Qt.size(20, 20)
                    fillMode: Image.PreserveAspectFit
                }
            }
            Text {
                text: nav.text
                font.weight: root.page === nav.target ? Font.Bold : Font.Normal
                color: root.page === nav.target ? root.white : root.paleText
                font.pixelSize: 16
                Layout.fillWidth: true
            }
        }
        background: Rectangle {
            SurfaceShadow { }

            radius: 12
            color: root.page === nav.target ? root.navSelected : (nav.hovered ? root.navHovered : root.transparentColor)
        }
        onClicked: root.page = target
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 32
        Panel {
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            color: root.accent
            border.color: root.accent
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                RowLayout {
                    Layout.leftMargin: 8
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    spacing: 16
                    Rectangle {
                        SurfaceShadow { }

                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 12
                        color: root.soft
                        Text {
                            anchors.centerIn: parent
                            text: "L"
                            color: root.accent
                            font.pixelSize: 23
                            font.weight: Font.Bold
                        }
                    }
                    Text {
                        text: "Ledgera"
                        color: root.soft
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    Layout.bottomMargin: 16
                    Layout.preferredHeight: 1
                    color: root.navSelected
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Обзор")
                    iconSource: "qrc:/icons/dashboard.svg"
                    target: "overview"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Категории")
                    iconSource: "qrc:/icons/categories.svg"
                    target: "categories"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Цели")
                    iconSource: "qrc:/icons/goals.svg"
                    target: "goals"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Заметки")
                    iconSource: "qrc:/icons/notes.svg"
                    target: "notes"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Траектория")
                    iconSource: "qrc:/icons/trajectory.svg"
                    target: "trajectory"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Бюджеты")
                    iconSource: "qrc:/icons/budgets.svg"
                    target: "budgets"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Проекты")
                    iconSource: "qrc:/icons/projects.svg"
                    target: "projects"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Аналитика")
                    iconSource: "qrc:/icons/analytics.svg"
                    target: "analytics"
                }
                Item {
                    Layout.fillHeight: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    Layout.bottomMargin: 8
                    color: root.navSelected
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Настройки")
                    iconSource: "qrc:/icons/settings.svg"
                    target: "settings"
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 24
            Layout.rightMargin: 24
            Layout.bottomMargin: 24
            spacing: root.pageGap
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                spacing: root.cardGap
                Text {
                    text: page === "overview" ? qsTr("Мои финансы")
                        : page === "accounts" ? qsTr("Счета")
                        : page === "categories" ? qsTr("Категории")
                        : page === "operations" ? qsTr("Операции")
                        : page === "budgets" ? qsTr("Бюджеты")
                        : page === "goals" ? qsTr("Цели")
                        : page === "notes" ? qsTr("Заметки")
                        : page === "trajectory" ? qsTr("Финансовая траектория")
                        : page === "projects" ? qsTr("Проекты")
                        : page === "analytics" ? qsTr("Аналитика")
                        : qsTr("Настройки")
                    color: root.accent
                    font.pixelSize: root.width < 1360 ? 26 : 30
                    font.weight: Font.Bold
                }
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    visible: page === "overview" && financeController.selectedAsset === "fiat"
                    Layout.preferredWidth: root.width < 1360 ? 208 : 218
                    controlHeight: 40
                    font.weight: Font.Bold
                    text: qsTr("Плановые операции")
                    onClicked: recurringTransactionsDialog.openManager()
                }
                SoftButton {
                    visible: page === "overview" || page === "accounts"
                          || page === "operations" || page === "analytics"
                    Layout.preferredWidth: root.width < 1360 ? 144 : 218
                    controlHeight: 40
                    font.weight: Font.Bold
                    text: root.dateFilterLabel()
                    onClicked: dateFilterDialog.openForCurrent()
                }
                AppComboBox {
                    id: currencyBox
                    Layout.preferredWidth: 104
                    implicitHeight: 40
                    model: ["RUB", "USD", "EUR"]
                    currentIndex: Math.max(0, model.indexOf(financeController.appCurrency))
                    onActivated: financeController.appCurrency = currentText
                }
            }
            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: page === "overview" ? overviewPage
                               : page === "accounts" ? accountsPage
                               : page === "categories" ? categoriesPage
                               : page === "operations" ? operationsPage
                               : page === "budgets" ? budgetsPage
                               : page === "goals" ? goalsPage
                               : page === "notes" ? notesPage
                               : page === "trajectory" ? trajectoryPage
                               : page === "projects" ? projectsPage
                               : page === "analytics" ? analyticsPage
                               : settingsPage
            }
        }
    }

    component PlannedOperationsPanel: Panel {
        id: schedulePanel
        // This is the upcoming global schedule, independent of the history date filter.
        property var scheduledRows: financeController.scheduledTransactions

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.panelPadding
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Text {
                    text: qsTr("Плановые операции")
                    color: root.accent
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                SoftButton {
                    flat: true
                    neo: schedulePanel.neo
                    text: qsTr("Все")
                    implicitWidth: 72
                    Layout.preferredWidth: 72
                    controlHeight: 40
                    font.weight: Font.Bold
                    onClicked: recurringTransactionsDialog.openManager()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 8
                Text {
                    text: qsTr("Название")
                    color: root.muted
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                }
                Text {
                    text: qsTr("Когда")
                    color: root.muted
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    Layout.preferredWidth: 96
                }
                Text {
                    text: qsTr("Сумма")
                    color: root.muted
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    Layout.preferredWidth: 120
                    horizontalAlignment: Text.AlignRight
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: schedulePanel.scheduledRows.length === 0
                    text: qsTr("Плановых операций пока нет")
                    color: root.muted
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                ListView {
                    MiddleScrollArea { parent: upcomingOperations; scroller: browserScroll; scrollTarget: upcomingOperations }
                    id: upcomingOperations
                    objectName: "upcomingOperations"
                    anchors.fill: parent
                    clip: true
                    spacing: 8
                    bottomMargin: schedulePanel.neo ? 4 : 0
                    model: schedulePanel.visible ? schedulePanel.scheduledRows : []
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }

                    delegate: Rectangle {
                        SurfaceShadow { visible: !schedulePanel.neo }
                        HardShadow { visible: schedulePanel.neo; depth: 3 }

                        required property var modelData
                        readonly property bool isIncome: modelData.type === "income"
                        width: upcomingOperations.width
                        height: 56
                        radius: 8
                        color: isIncome ? root.incomePanel : root.expensePanel
                        border.width: schedulePanel.neo ? root.outlineWidth : 1
                        border.color: schedulePanel.neo ? root.accent
                                      : isIncome ? "#C9D9CE" : "#E1C9C2"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            Text {
                                text: modelData.name
                                color: root.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.nextDate
                                      ? Qt.formatDate(root.dateFromIso(modelData.nextDate), "dd.MM.yyyy")
                                      : qsTr("Не назначено")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: 96
                            }
                            Text {
                                text: root.money(isIncome ? modelData.amount : -modelData.amount,
                                                 modelData.currency, true)
                                color: isIncome ? root.income : root.red
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 120
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea {
                            id: scheduledRowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                recurringTransactionsDialog.openForEdit(modelData);
                                recurringTransactionsDialog.open();
                            }
                        }
                        ToolTip.visible: scheduledRowMouse.containsMouse
                        ToolTip.delay: 600
                        ToolTip.text: modelData.name + " · " + modelData.accountName
                                      + "\n" + modelData.scheduleText
                                      + " · " + root.money(isIncome ? modelData.amount : -modelData.amount,
                                                         modelData.currency, true)
                    }
                }
            }
        }
    }

    Component {
        id: overviewPage
        ScrollView {
            MiddleScrollArea { parent: overviewScroll; scroller: browserScroll; scrollTarget: overviewScroll.contentItem }
            id: overviewScroll
            objectName: "overviewScroll"
            clip: true

            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: StyledScrollBar {
                policy: ScrollBar.AlwaysOff
                appAccentColor: root.accentSoft
                appTrackColor: root.line
            }

            // ===== CONTENT CLIP SAFETY MARGIN =====
            // Keep panel borders one physical pixel away from ScrollView's clip edge.
            // If outlines ever need to touch the viewport again, set this to 0.
            readonly property int contentEdgeMargin: 0

            contentWidth: availableWidth
            contentHeight: dashboard.implicitHeight

            ColumnLayout {
                id: dashboard
                objectName: "dashboard"
                x: overviewScroll.contentEdgeMargin
                width: Math.max(0, overviewScroll.availableWidth - overviewScroll.contentEdgeMargin * 2)
                // Keep the default-size layout; expose the schedule only on larger windows.
                readonly property bool wideOverview: root.width > 1600 && root.height > 960
                spacing: root.pageGap

                RowLayout {
                    id: overviewTop
                    objectName: "overviewTop"
                    Layout.fillWidth: true
                    spacing: 32

                    ColumnLayout {
                        id: overviewLeft
                        objectName: "overviewLeft"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 688
                        spacing: root.pageGap

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.cardGap
                            Panel {
                                Layout.fillWidth: true
                                Layout.preferredWidth: (overviewLeft.width - 3 * root.cardGap) / 4
                                Layout.minimumWidth: 160
                                Layout.preferredHeight: 120
                                color: root.accent
                                neo: true
                                // light ring keeps the dark shadow readable under a dark card
                                border.color: root.canvas
                                Column {
                                    anchors.fill: parent
                                    anchors.margins: root.panelPadding
                                    spacing: 16
                                    Text {
                                        text: qsTr("Все активы")
                                        color: root.panel
                                        font.pixelSize: 16
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        width: parent.width
                                        text: root.money(financeController.balanceMinorUnits,
                                                         financeController.appCurrency, false)
                                        color: root.panel
                                        font.pixelSize: 28
                                        font.weight: Font.Bold
                                        fontSizeMode: Text.Fit
                                        minimumPixelSize: 16
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            Repeater {
                                model: root.assets
                                delegate: Panel {
                                    required property var modelData
                                    readonly property bool selected: financeController.selectedAsset === modelData.code
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: (overviewLeft.width - 3 * root.cardGap) / 4
                                    Layout.minimumWidth: 160
                                    Layout.preferredHeight: 120
                                    neo: true
                                    color: selected ? root.pale : root.panel
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: root.panelPadding
                                        spacing: selected ? 12 : 16
                                        Text {
                                            text: modelData.title
                                            color: parent.parent.selected ? root.accent : root.muted
                                            font.pixelSize: parent.parent.selected ? 20 : 16
                                            font.weight: parent.parent.selected ? Font.ExtraBold : Font.Bold
                                        }
                                        Text {
                                            width: parent.width
                                            text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false)
                                            color: root.accent
                                            font.pixelSize: parent.parent.selected ? 30 : 28
                                            font.weight: Font.Bold
                                            fontSizeMode: Text.Fit
                                            minimumPixelSize: 16
                                            elide: Text.ElideRight
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: financeController.selectedAsset = modelData.code
                                    }
                                }
                            }
                        }
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 249
                            neo: true
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: root.panelPadding
                                anchors.topMargin: root.panelPadding
                                spacing: root.cardGap
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: (financeController.selectedAsset === "crypto"
                                               ? qsTr("Криптовалюты")
                                               : qsTr("Счета"))
                                              + " · " + root.assetTitle(financeController.selectedAsset)
                                        color: root.accent
                                        font.pixelSize: 18
                                        font.weight: Font.Bold
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    SoftButton {
                                        id: addAccountButton
                                        flat: true
                                        neo: true
                                        text: financeController.selectedAsset === "crypto"
                                              ? qsTr("+  Добавить криптовалюту")
                                              : qsTr("+  Добавить счёт")

                                        contentItem: Text {
                                            text: addAccountButton.text
                                            color: root.accent
                                            font.weight: Font.Bold
                                            font.pixelSize: 14
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        onClicked: {
                                            if (financeController.selectedAsset === "crypto")
                                                cryptoWalletDialog.openForNewWallet();
                                            else
                                                accountDialog.openForSelectedAsset();
                                        }
                                    }
                                }
                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    orientation: ListView.Horizontal
                                    spacing: root.cardGap
                                    clip: true
                                    ScrollBar.horizontal: StyledScrollBar {
                                        policy: ScrollBar.AlwaysOff
                                        appAccentColor: root.accentSoft
                                        appTrackColor: root.line
                                    }
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                                    model: [
                                        {
                                            id: "",
                                            name: financeController.selectedAsset === "crypto"
                                                  ? qsTr("Все счета")
                                                  : qsTr("Все счета"),
                                            balanceMinor: root.assetAmount(financeController.selectedAsset),
                                            currency: financeController.appCurrency
                                        }
                                    ].concat(financeController.selectedAsset === "crypto"
                                             ? financeController.cryptoWallets
                                             : financeController.accounts)
                                    delegate: Rectangle {
                                        HardShadow { depth: 4 }

                                        required property var modelData
                                        width: dashboard.wideOverview
                                               ? Math.min(236, Math.max(192, (ListView.view.width - 2 * root.cardGap) / 3))
                                               : Math.min(252, Math.max(192, (ListView.view.width - 4 * root.cardGap) / 5))
                                        height: 115
                                        radius: root.cardRadius
                                        color: root.overviewAccountSelected(modelData) ? root.pale : root.soft
                                        border.width: root.outlineWidth
                                        border.color: root.accent
                                        MouseArea {
                                            id: overviewAccountMouseArea
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                                            onClicked: function (mouse) {
                                                if (mouse.button !== Qt.LeftButton)
                                                    return;
                                                if (financeController.selectedAsset === "crypto")
                                                    financeController.selectedCryptoWalletId = modelData.id || "";
                                                else
                                                    financeController.selectedAccountId = modelData.id || "";
                                            }
                                            onPressed: function (mouse) {
                                                if (mouse.button === Qt.RightButton && modelData.id)
                                                    root.openAccountContextMenu(
                                                        modelData,
                                                        overviewAccountMouseArea,
                                                        mouse.x,
                                                        mouse.y
                                                    );
                                            }
                                        }
                                        Column {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 16
                                            anchors.topMargin: 12
                                            spacing: 4
                                            Text {
                                                width: parent.width
                                                text: modelData.name
                                                color: root.overviewAccountSelected(modelData) ? root.accent : root.muted
                                                font.pixelSize: 14
                                                font.weight: Font.Bold
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                width: parent.width
                                                text: modelData.isCreditCard
                                                      ? root.money(modelData.availableCreditMinor, modelData.currency, false)
                                                      : root.accountCompactAmount(modelData)
                                                color: root.accent
                                                font.pixelSize: 22
                                                font.weight: Font.Bold
                                                fontSizeMode: Text.Fit
                                                minimumPixelSize: 18
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                width: parent.width
                                                visible: modelData.isCreditCard === true
                                                text: modelData.isCreditCard
                                                      ? qsTr("Долг: %1").arg(root.money(modelData.debtMinor, modelData.currency, false)) : ""
                                                color: root.muted
                                                font.pixelSize: 14
                                                font.weight: Font.Bold
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    PlannedOperationsPanel {
                        objectName: "dashboardSchedule"
                        neo: true
                        visible: dashboard.wideOverview
                        Layout.preferredWidth: Math.min(568, overviewTop.width * 0.355)
                        Layout.fillHeight: true
                        Layout.preferredHeight: 392
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.pageGap
                    Panel {
                        neo: true
                        Layout.preferredWidth: (dashboard.width - root.pageGap) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 232
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: root.panelPadding
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: qsTr("История капитала в рублях")
                                    color: root.accent
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    visible: parent.width > 440
                                    text: root.historyResolutionLabel(
                                        financeController.capitalHistoryRub
                                    )
                                    color: root.muted
                                    font.pixelSize: 14
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                AnalyticsLineChart {
                                    anchors.fill: parent
                                    points: financeController.capitalHistoryRub
                                    currency: "RUB"
                                    seriesColor: root.chartAccent2
                                }
                            }
                        }
                    }
                    Panel {
                        neo: true
                        Layout.preferredWidth: (dashboard.width - root.pageGap) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 232
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: root.panelPadding
                            Text {
                                text: qsTr("Структура расходов")
                                color: root.accent
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 24
                                Canvas {
                                    id: donut
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 120
                                    onPaint: {
                                        const ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);
                                        const data = root.categoryTotals();
                                        let total = 0;
                                        for (let i = 0; i < data.length; ++i)
                                            total += data[i].amount;
                                        let angle = -Math.PI / 2;
                                        if (total === 0) {
                                            ctx.strokeStyle = root.line;
                                            ctx.lineWidth = width * 0.2;
                                            ctx.beginPath();
                                            ctx.arc(width / 2, height / 2, width * 0.4, 0, Math.PI * 2);
                                            ctx.stroke();
                                            return;
                                        }
                                        for (let j = 0; j < data.length; ++j) {
                                            const next = angle + data[j].amount / total * Math.PI * 2;
                                            ctx.strokeStyle = root.chartColors[j % root.chartColors.length];
                                            ctx.lineWidth = width * 0.2;
                                            ctx.beginPath();
                                            ctx.arc(width / 2, height / 2, width * 0.4, angle, next);
                                            ctx.stroke();
                                            angle = next;
                                        }
                                    }
                                    Connections {
                                        target: financeController
                                        function onTransactionsChanged() {
                                            donut.requestPaint();
                                        }
                                        function onSelectedAccountIdChanged() {
                                            donut.requestPaint();
                                        }
                                        function onSelectedAssetChanged() {
                                            donut.requestPaint();
                                        }
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 8
                                    Repeater {
                                        model: root.categoryTotals()
                                        delegate: RowLayout {
                                            required property int index
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Rectangle {
                                                Layout.preferredWidth: 8
                                                Layout.preferredHeight: 8
                                                color: root.chartColors[index % root.chartColors.length]
                                            }
                                            Text {
                                                text: modelData.label
                                                color: root.muted
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: root.money(modelData.amount, financeController.appCurrency, false)
                                                color: root.accent
                                                font.pixelSize: 14
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                TransactionBlock {
                    neo: true
                    visible: financeController.selectedAsset !== "crypto"
                    Layout.fillWidth: true
                    expandToContent: true
                    title: qsTr("История операций")
                    rows: visible ? root.dashboardHistoryRows() : []
                    viewportFlickable: overviewScroll.contentItem
                    showSearch: true
                    addInvestmentPositionAction:
                        financeController.selectedAsset === "investment"
                }
                Text {
                    Layout.fillWidth: true
                    visible: financeController.selectedAsset === "crypto" && financeController.cryptoLastError.length > 0
                    text: financeController.cryptoLastError
                    color: root.red; font.pixelSize: 14; wrapMode: Text.WordWrap
                }
                ExchangeBalancesBlock { neo: true; Layout.fillWidth: true }
                DashboardCryptoHistoryBlock {
                    neo: true
                    viewportFlickable: overviewScroll.contentItem
                    visible: financeController.selectedAsset === "crypto"
                    Layout.fillWidth: true
                    expandToContent: true
                }
                Item {
                    Layout.preferredHeight: 8
                }
            }
        }
    }

    // One shared transaction container for both Overview and Operations.
    // The Panel owns the ONLY outer outline; its contents stay 1 px inside it
    // so opaque table rows never paint over the border.
    // Shared transaction container for Overview and Operations.
    // On Overview expandToContent=true: the transaction history grows with its rows,
    // so the OUTER overview ScrollView owns vertical scrolling.
    // On Operations expandToContent=false: the block fills the page and keeps its own ListView.
    component HistorySelection: QtObject {
        property var rows: []
        property var selectedKeys: ({})
        property var dragBase: ({})
        property int dragStart: -1
        property int dragEnd: -1
        readonly property var selectedRows: rows.filter(function(row) {
            return selectedKeys[key(row)] === true;
        })
        function key(row) {
            return row.transactionId ? "crypto:" + row.walletId + ":" + row.transactionId
                                     : (row.type || "operation") + ":" + row.id;
        }
        function contains(row) { return selectedKeys[key(row)] === true; }
        function clear() { selectedKeys = ({}); dragStart = -1; }
        onRowsChanged: clear()
        function press(index) {
            if (index < 0 || index >= rows.length) return;
            if (root.activeHistorySelection && root.activeHistorySelection !== this) root.activeHistorySelection.clear();
            root.activeHistorySelection = this;
            dragBase = Object.assign({}, selectedKeys);
            dragStart = index; dragEnd = index;
            const next = Object.assign({}, selectedKeys);
            const id = key(rows[index]);
            if (next[id]) delete next[id]; else next[id] = true;
            selectedKeys = next;
        }
        function extend(index) {
            if (dragStart < 0 || rows.length === 0) return;
            index = Math.max(0, Math.min(rows.length - 1, index));
            if (index === dragEnd) return;
            dragEnd = index;
            const next = Object.assign({}, dragBase);
            for (let i = Math.min(dragStart, index); i <= Math.max(dragStart, index); ++i)
                next[key(rows[i])] = true;
            selectedKeys = next;
        }
        function context(index, area, x, y) {
            if (index < 0 || index >= rows.length) return;
            root.activeHistorySelection = this;
            if (selectedRows.length < 2) {
                root.openTransactionContextMenu(rows[index], area, x, y);
                return;
            }
            historySelectionMenu.selection = this;
            const point = area.mapToItem(root.contentItem, x, y);
            historySelectionMenu.x = Math.max(8, Math.min(point.x, root.contentItem.width - historySelectionMenu.width - 8));
            historySelectionMenu.y = Math.max(8, Math.min(point.y, root.contentItem.height - historySelectionMenu.implicitHeight - 8));
            historySelectionMenu.open();
        }
    }

    component HistoryRowSelectionArea: MouseArea {
        id: selectionArea
        required property var selection
        required property var rowData
        required property int rowIndex
        required property var body
        property real contentOffset: 0
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton) selection.press(rowIndex);
            else selection.context(rowIndex, selectionArea, mouse.x, mouse.y);
        }
        onPositionChanged: function(mouse) {
            if (!(mouse.buttons & Qt.LeftButton)) return;
            const point = mapToItem(body, mouse.x, mouse.y);
            selection.extend(Math.floor((point.y + contentOffset) / 48));
        }
        onReleased: selection.dragStart = -1
        onCanceled: selection.dragStart = -1
    }

    component HistoryPanel: Panel {
        id: historyPanel
        clip: false

        BottomCornerMask {
            width: historyPanel.radius
            height: historyPanel.radius
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            z: 999
            neo: historyPanel.neo
            depth: root.shadowDepth
        }

        BottomCornerMask {
            width: historyPanel.radius
            height: historyPanel.radius
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            mirrored: true
            z: 999
            neo: historyPanel.neo
            depth: root.shadowDepth
        }

        // IMPORTANT: frame is deliberately rendered ABOVE all table content.
        Rectangle {
            anchors.fill: parent
            color: root.transparentColor
            radius: historyPanel.radius
            border.width: historyPanel.border.width
            border.color: historyPanel.border.color
            z: 1000
        }
    }

    component ExchangeBalancesBlock: HistoryPanel {
        id: exchangeBalances
        property var rows: {
            const accounts = financeController.cryptoWallets;
            const selected = financeController.selectedCryptoWalletId;
            return financeController.cryptoExchangeHoldings();
        }
        visible: financeController.selectedAsset === "crypto" && rows.length > 0
        implicitHeight: 64 + Math.min(4, rows.length) * 40
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 1; spacing: 0; clip: true
            RowLayout {
                Layout.fillWidth: true; Layout.preferredHeight: 64
                Layout.leftMargin: 24; Layout.rightMargin: 24
                Text { text: qsTr("Остатки Bybit"); color: root.accent; font.pixelSize: 18; font.weight: Font.Bold }
                Item { Layout.fillWidth: true }
                Text { text: qsTr("Стоимость по данным биржи"); color: root.muted; font.pixelSize: 14 }
            }
            ListView {
                id: exchangeBalancesList
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: exchangeBalances.rows
                MiddleScrollArea { scroller: browserScroll; scrollTarget: exchangeBalancesList }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 40
                    color: modelData.summary ? root.tableHeader : root.panel
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24
                        Text {
                            Layout.fillWidth: true
                            text: modelData.accountName + " · " + root.exchangeSection(modelData.section)
                                + (modelData.category ? " · " + modelData.category : "")
                            color: root.accent; font.pixelSize: 14; elide: Text.ElideRight
                            font.weight: modelData.summary ? Font.DemiBold : Font.Normal
                        }
                        Text {
                            text: modelData.summary ? Number(modelData.usdValue).toLocaleString(root.uiLocale(), "f", 2) + " $"
                                : String(modelData.amountText).replace(".", financeController.uiLanguage === "en" ? "." : ",") + " " + modelData.coin
                            color: root.accent; font.pixelSize: 14; font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }

    component DashboardCryptoHistoryBlock: HistoryPanel {
        id: dashboardCryptoHistory

        property var rows: financeController.cryptoTransactions
        property bool expandToContent: false
        property var viewportFlickable: null
        function bodyOffsetInScrollContent() {
            let offset = 0; let item = cryptoHistoryBody;
            const target = viewportFlickable ? viewportFlickable.contentItem : null;
            while (item && item !== target) { offset += item.y; item = item.parent; }
            return offset;
        }
        readonly property int firstVisibleRow: viewportFlickable
            ? Math.min(rows.length, Math.max(0, Math.floor((viewportFlickable.contentY - bodyOffsetInScrollContent()) / 48) - 3)) : 0
        readonly property int lastVisibleRow: viewportFlickable
            ? Math.max(firstVisibleRow, Math.min(rows.length, Math.ceil((viewportFlickable.contentY + viewportFlickable.height - bodyOffsetInScrollContent()) / 48) + 3)) : rows.length
        HistorySelection { id: cryptoSelection; rows: dashboardCryptoHistory.rows }
        onVisibleChanged: if (!visible) cryptoSelection.clear()

        readonly property int dateColumnWidth: 150
        readonly property int directionColumnWidth: 125
        readonly property int hashColumnWidth: 175
        readonly property int amountColumnWidth: 135

        implicitHeight: expandToContent
                      ? 114 + Math.max(58, rows.length * 48)
                      : 230

        ColumnLayout {
            clip: true
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: 40
                Layout.preferredHeight: 40
                Layout.maximumHeight: 40
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 16
                Layout.bottomMargin: 16

                Text {
                    text: qsTr("История %1").arg(
                        root.selectedCryptoSymbol() || qsTr("криптовалюты")
                    )
                    color: root.accent
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("Последние операции: %1")
                        .arg(dashboardCryptoHistory.rows.length)
                    color: root.muted
                    font.pixelSize: 14
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: root.tableHeader

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.line
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24

                    Text {
                        text: qsTr("Дата")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: dashboardCryptoHistory.dateColumnWidth
                    }
                    Text {
                        text: qsTr("Направление")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: dashboardCryptoHistory.directionColumnWidth
                    }
                    Text {
                        text: qsTr("Описание / адрес")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Счёт / хеш")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: dashboardCryptoHistory.hashColumnWidth
                    }
                    Text {
                        text: qsTr("Сумма")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: dashboardCryptoHistory.amountColumnWidth
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Item {
                id: cryptoHistoryBody
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    58,
                    dashboardCryptoHistory.rows.length * 48
                )

                Column {
                    width: parent.width
                    y: dashboardCryptoHistory.firstVisibleRow * 48

                    Repeater {
                        model: dashboardCryptoHistory.rows.slice(dashboardCryptoHistory.firstVisibleRow, dashboardCryptoHistory.lastVisibleRow)

                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: parent.width
                            height: 48
                            color: root.panel
                            Rectangle { anchors.fill: parent; color: "#332D6CC0"; visible: cryptoSelection.contains(modelData) }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 24
                                anchors.rightMargin: 24
                                height: 1
                                color: root.line
                            }

                            HistoryRowSelectionArea {
                                selection: cryptoSelection
                                rowData: modelData
                                rowIndex: dashboardCryptoHistory.firstVisibleRow + index
                                body: cryptoHistoryBody
                                z: 2
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 24
                                anchors.rightMargin: 24

                                Text {
                                    text: Qt.formatDateTime(
                                        modelData.occurredAt,
                                        "dd.MM.yyyy HH:mm"
                                    )
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.preferredWidth: dashboardCryptoHistory.dateColumnWidth
                                }
                                Text {
                                    text: modelData.direction === "in"
                                          ? qsTr("Получено")
                                          : modelData.direction === "neutral" ? qsTr("Без изменения") : qsTr("Отправлено")
                                    color: modelData.direction === "in"
                                         ? root.income
                                         : modelData.direction === "neutral" ? root.muted : root.red
                                    font.pixelSize: 14
                                    font.weight: Font.Normal
                                    Layout.preferredWidth: dashboardCryptoHistory.directionColumnWidth
                                }
                                Text {
                                    text: modelData.counterparty
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }
                                Text {
                                    text: modelData.isExchange ? root.exchangeSection(modelData.section) : modelData.transactionId
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.preferredWidth: dashboardCryptoHistory.hashColumnWidth
                                    elide: Text.ElideMiddle
                                }
                                Text {
                                    readonly property string localizedAmount:
                                        financeController.uiLanguage === "en"
                                        ? modelData.amountText
                                        : modelData.amountText.replace(".", ",")
                                    text: (modelData.direction === "in" ? "+" : modelData.direction === "out" ? "−" : "")
                                          + localizedAmount + " " + modelData.symbol
                                    color: modelData.direction === "in"
                                         ? root.income
                                         : modelData.direction === "neutral" ? root.muted : root.red
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    Layout.preferredWidth: dashboardCryptoHistory.amountColumnWidth
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: dashboardCryptoHistory.rows.length === 0
                    text: financeController.cryptoWallets.length === 0
                          ? qsTr("Сначала добавьте криптокошелёк")
                          : financeController.cryptoRefreshing
                            ? qsTr("Загружаем историю переводов…")
                            : qsTr("У этого кошелька пока нет переводов %1")
                                  .arg(root.selectedCryptoSymbol())
                    color: root.muted
                    font.pixelSize: 14
                }
            }
        }
    }

    component TransactionBlock: HistoryPanel {
        id: transactionBlock
        objectName: "transactionBlock"

        property string title: qsTr("История операций")
        property var rows: []
        property bool expandToContent: false
        property var viewportFlickable: null
        property bool addInvestmentPositionAction: false
        property bool showSearch: false
        property string projectId: ""

        // 72 px block header + 40 px table header + 48 px per transaction + 2 px frame inset.
        // Keep these values in sync with TransactionTable row/header heights below.
        implicitHeight: expandToContent ? 112 + border.width * 2 + rows.length * 48
                                        : 243 + border.width * 2

        ColumnLayout {
            clip: true
            anchors.fill: parent
            anchors.margins: transactionBlock.border.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: 40
                Layout.preferredHeight: 40
                Layout.maximumHeight: 40
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 16
                Layout.bottomMargin: 16

                Text {
                    text: transactionBlock.title
                    color: root.accent
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }

                Item {
                    Layout.fillWidth: true
                }

                AppTextField {
                    visible: transactionBlock.showSearch
                    neo: transactionBlock.neo
                    Layout.preferredWidth: 265
                    implicitHeight: 40
                    placeholderText: qsTr("Поиск по операциям...")
                    text: root.searchText
                    onTextEdited: root.searchText = text
                }

                SoftButton {
                    text: qsTr("+ Добавить операцию")
                    flat: true
                    neo: transactionBlock.neo
                    controlHeight: 40
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    implicitWidth: 224
                    visible: !transactionBlock.addInvestmentPositionAction
                          || root.selectedInvestmentAccountAcceptsPositions()
                    enabled: !transactionBlock.addInvestmentPositionAction
                          || root.selectedInvestmentAccountAcceptsPositions()
                    onClicked: {
                        if (transactionBlock.projectId.length > 0)
                            operationDialog.openForNew(transactionBlock.projectId);
                        else if (transactionBlock.addInvestmentPositionAction)
                            investmentPositionDialog.openForNewPosition(
                                financeController.selectedAccountId
                            );
                        else
                            operationDialog.openForNew();
                    }
                }
            }

            TransactionTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                rows: transactionBlock.rows
                expandToContent: transactionBlock.expandToContent
                viewportFlickable: transactionBlock.viewportFlickable
                bottomCornerRadius: transactionBlock.radius - transactionBlock.border.width
                neo: transactionBlock.neo
            }
        }

    }

    component TransactionTable: Item {
        id: transactionTable

        property var rows: []
        HistorySelection { id: tableSelection; rows: transactionTable.rows }
        onVisibleChanged: if (!visible) tableSelection.clear()
        property real bottomCornerRadius: 0
        property bool neo: false
        property bool expandToContent: false
        property var viewportFlickable: null

        // Keep the overview's single scroll surface without constructing off-screen rows.
        function bodyOffsetInScrollContent() {
            let offset = 0;
            let item = overviewRows.parent;
            const target = viewportFlickable ? viewportFlickable.contentItem : null;
            while (item && item !== target) {
                offset += item.y;
                item = item.parent;
            }
            return offset;
        }
        readonly property int firstVisibleRow: viewportFlickable
            ? Math.min(rows.length, Math.max(0, Math.floor(
                (viewportFlickable.contentY - bodyOffsetInScrollContent()) / rowHeight) - 3))
            : 0
        readonly property int lastVisibleRow: viewportFlickable
            ? Math.max(firstVisibleRow, Math.min(rows.length, Math.ceil(
                (viewportFlickable.contentY + viewportFlickable.height
                 - bodyOffsetInScrollContent()) / rowHeight) + 3))
            : rows.length

        readonly property int tableHeaderHeight: 40
        readonly property int rowHeight: 48
        readonly property int operationColumnWidth: 100
        readonly property int accountColumnWidth: 150
        readonly property int categoryColumnWidth: 160
        readonly property int dateColumnWidth: 110
        readonly property int amountColumnWidth: 130

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: transactionTable.tableHeaderHeight
                color: root.tableHeader

                // Only the header/content divider is drawn here.
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: transactionTable.neo ? root.outlineWidth : 1
                    color: transactionTable.neo ? root.accent : root.line
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24

                    Text {
                        text: qsTr("Дата")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: transactionTable.dateColumnWidth
                    }
                    Text {
                        text: qsTr("Операция")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: transactionTable.operationColumnWidth
                    }
                    Text {
                        text: qsTr("Счёт")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: transactionTable.accountColumnWidth
                    }
                    Text {
                        text: qsTr("Категория")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: transactionTable.categoryColumnWidth
                    }
                    Text {
                        text: qsTr("Описание")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Сумма")
                        color: root.muted
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        Layout.preferredWidth: transactionTable.amountColumnWidth
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Item {
                id: transactionBody
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: root.white
                    radius: transactionTable.bottomCornerRadius

                    // Rectangle.radius rounds all four corners; cover upper pair so the
                    // body remains square where it touches the table header.
                    Rectangle {
                        visible: parent.radius > 0
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: Math.min(parent.radius, parent.height)
                        color: root.white
                    }
                }

                // OVERVIEW MODE: ordinary non-flickable content.
                // The whole overview page scrolls as one document.
                Column {
                    id: overviewRows
                    width: parent.width
                    y: transactionTable.firstVisibleRow * transactionTable.rowHeight
                    visible: transactionTable.expandToContent

                    Repeater {
                        model: transactionTable.expandToContent && transactionTable.visible
                               ? transactionTable.rows.slice(transactionTable.firstVisibleRow,
                                                             transactionTable.lastVisibleRow)
                               : []

                        delegate: Item {
                            required property int index
                            required property var modelData
                            width: parent.width
                            height: transactionTable.rowHeight

                            Rectangle {
                                anchors.fill: parent
                                color: root.panel
                            Rectangle { anchors.fill: parent; color: "#332D6CC0"; visible: tableSelection.contains(modelData) }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 24
                                anchors.rightMargin: 24
                                height: 1
                                color: root.line
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 24
                                anchors.rightMargin: 24

                                Text {
                                    text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.preferredWidth: transactionTable.dateColumnWidth
                                }
                                Text {
                                    text: root.transactionTypeLabel(modelData)
                                    color: root.accent
                                    font.pixelSize: 14
                                    font.weight: Font.Normal
                                    Layout.preferredWidth: transactionTable.operationColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: root.accountName(modelData.accountId)
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.preferredWidth: transactionTable.accountColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.categoryName
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.preferredWidth: transactionTable.categoryColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.rawDescription || "—"
                                    color: root.accent
                                    font.pixelSize: 14
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                    color: modelData.type === "investment_position"
                                         ? root.navSelected
                                         : modelData.type === "transfer"
                                           ? root.navSelected
                                           : modelData.type === "income"
                                             ? root.income : root.red
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    Layout.preferredWidth: transactionTable.amountColumnWidth
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            HistoryRowSelectionArea {
                                objectName: "overviewRowMenuArea"
                                selection: tableSelection
                                rowData: modelData
                                rowIndex: transactionTable.firstVisibleRow + index
                                body: transactionBody
                                contentOffset: 0
                            }
                        }
                    }
                }

                // OPERATIONS PAGE MODE: virtualized scrollable list remains useful for
                // potentially large transaction histories. Scrollbar itself stays hidden.
                ListView {
                    MiddleScrollArea { parent: transactionList; scroller: browserScroll; scrollTarget: transactionList }
                    id: transactionList
                    anchors.fill: parent
                    visible: !transactionTable.expandToContent
                    clip: true
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                    ScrollBar.vertical: StyledScrollBar {
                        policy: ScrollBar.AlwaysOff
                        appAccentColor: root.accentSoft
                        appTrackColor: root.line
                    }
                    reuseItems: true
                    model: !transactionTable.expandToContent && transactionTable.visible
                           ? transactionTable.rows : []

                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        height: transactionTable.rowHeight

                        Rectangle {
                            anchors.fill: parent
                            color: root.panel
                            Rectangle { anchors.fill: parent; color: "#332D6CC0"; visible: tableSelection.contains(modelData) }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 24
                            anchors.rightMargin: 24
                            height: 1
                            color: root.line
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 24
                            anchors.rightMargin: 24

                            Text {
                                text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                color: root.accent
                                font.pixelSize: 14
                                Layout.preferredWidth: transactionTable.dateColumnWidth
                            }
                            Text {
                                text: root.transactionTypeLabel(modelData)
                                color: root.accent
                                font.pixelSize: 14
                                font.weight: Font.Normal
                                Layout.preferredWidth: transactionTable.operationColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.accountName(modelData.accountId)
                                color: root.accent
                                font.pixelSize: 14
                                Layout.preferredWidth: transactionTable.accountColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.categoryName
                                color: root.accent
                                font.pixelSize: 14
                                Layout.preferredWidth: transactionTable.categoryColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.rawDescription || "—"
                                color: root.accent
                                font.pixelSize: 14
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                color: modelData.type === "investment_position"
                                     ? root.navSelected
                                     : modelData.type === "transfer"
                                       ? root.navSelected
                                       : modelData.type === "income"
                                         ? root.income : root.red
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: transactionTable.amountColumnWidth
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        HistoryRowSelectionArea {
                            objectName: "operationsRowMenuArea"
                            selection: tableSelection
                            rowData: modelData
                            rowIndex: index
                            body: transactionList
                            contentOffset: transactionList.contentY
                        }
                    }
                }

                // Label {
                //     anchors.centerIn: parent
                //     visible: transactionTable.rows.length === 0
                //     text: qsTr("Операций пока нет")
                //     color: root.muted
                // }
            }
        }
    }

    Component {
        id: budgetsPage
        RowLayout {
            spacing: 14

            Panel {
                Layout.preferredWidth: 320
                Layout.minimumWidth: 280
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Мои бюджеты")
                            color: root.accent
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                        SoftButton {
                            text: qsTr("+ Бюджет")
                            highlighted: true
                            onClicked: budgetDialog.openForNew()
                        }
                    }

                    ListView {
                        MiddleScrollArea { parent: budgetList; scroller: browserScroll; scrollTarget: budgetList }
                        id: budgetList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        clip: true
                        model: financeController.budgets
                        ScrollBar.vertical: StyledScrollBar {
                            policy: ScrollBar.AlwaysOff
                            appAccentColor: root.accentSoft
                            appTrackColor: root.line
                        }

                        delegate: Rectangle {
                            SurfaceShadow { }

                            id: budgetCard
                            required property var modelData
                            width: ListView.view.width
                            height: 108
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name
                            ToolTip.delay: 650
                            ToolTip.visible: budgetRowMenuArea.containsMouse || activeFocus
                            ToolTip.text: qsTr("Правый клик или Enter: изменить или удалить")
                            Keys.onPressed: function(event) {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                    || event.key === Qt.Key_Menu
                                    || (event.key === Qt.Key_F10
                                        && (event.modifiers & Qt.ShiftModifier))) {
                                    financeController.selectedBudgetId = modelData.id;
                                    root.openBudgetContextMenu(modelData, budgetCard,
                                                               budgetCard.width - 24, 24);
                                    event.accepted = true;
                                }
                            }
                            radius: 12
                            color: financeController.selectedBudgetId === modelData.id
                                 ? root.pale : root.soft
                            border.width: budgetCard.activeFocus
                                        || financeController.selectedBudgetId === modelData.id
                                        ? 2 : 1
                            border.color: budgetCard.activeFocus
                                        || financeController.selectedBudgetId === modelData.id
                                        ? root.accentSoft : root.line

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: root.accent
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: Math.round(modelData.progress * 100) + "%"
                                        color: modelData.progress > 1 ? root.red : root.muted
                                        font.pixelSize: 14
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.money(modelData.displaySpentMinor,
                                                     modelData.displayCurrency, false)
                                        + " / "
                                        + root.money(modelData.displayLimitMinor,
                                                     modelData.displayCurrency, false)
                                    color: modelData.progress > 1 ? root.red : root.muted
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 7
                                    radius: 4
                                    color: root.line
                                    Rectangle {
                                        width: parent.width * Math.min(1, modelData.progress)
                                        height: parent.height
                                        radius: parent.radius
                                        color: modelData.progress > 1 ? root.red : root.accentSoft
                                    }
                                }
                            }
                            MouseArea {
                                id: budgetRowMenuArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    budgetCard.forceActiveFocus();
                                    financeController.selectedBudgetId = modelData.id;
                                    if (mouse.button === Qt.RightButton) {
                                        root.openBudgetContextMenu(
                                            modelData,
                                            budgetRowMenuArea,
                                            mouse.x,
                                            mouse.y
                                        );
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14
                visible: root.selectedBudget() !== null

                RowLayout {
                    Layout.fillWidth: true
                    SoftButton {
                        text: "‹"
                        Layout.preferredWidth: 58
                        onClicked: root.shiftBudgetMonth(-1)
                    }
                    Text {
                        Layout.preferredWidth: 180
                        text: root.budgetMonthLabel()
                        color: root.accent
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    SoftButton {
                        text: "›"
                        Layout.preferredWidth: 58
                        onClicked: root.shiftBudgetMonth(1)
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Repeater {
                        model: [
                            {
                                title: qsTr("Лимит"),
                                amount: root.selectedBudget()
                                      ? root.selectedBudget().displayLimitMinor : 0,
                                color: root.accent
                            },
                            {
                                title: qsTr("Потрачено"),
                                amount: root.selectedBudget()
                                      ? root.selectedBudget().displaySpentMinor : 0,
                                color: root.red
                            },
                            {
                                title: qsTr("Осталось"),
                                amount: root.selectedBudget()
                                      ? root.selectedBudget().displayLimitMinor
                                        - root.selectedBudget().displaySpentMinor : 0,
                                color: root.selectedBudget()
                                    && root.selectedBudget().remainingMinor < 0
                                    ? root.red : root.income
                            }
                        ]
                        delegate: Panel {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 106
                            color: root.soft
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 8
                                Text { text: modelData.title; color: root.muted }
                                Text {
                                    text: root.money(modelData.amount,
                                        root.selectedBudget()
                                        ? root.selectedBudget().displayCurrency
                                        : financeController.appCurrency, false)
                                    color: modelData.color
                                    font.pixelSize: 22
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.selectedBudget() && root.selectedBudget().rateMissing
                    text: qsTr("Не удалось пересчитать часть сумм: проверьте валютные курсы")
                    color: root.red
                    font.pixelSize: 14
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 14

                    Panel {
                        Layout.preferredWidth: 370
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10
                            Text {
                                text: qsTr("Категории")
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.Bold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.selectedBudget()
                                    ? (root.selectedBudget().allAccounts
                                       ? qsTr("Все счета")
                                       : qsTr("Выбрано счетов: %1")
                                         .arg(root.selectedBudget().accountIds.length))
                                    : ""
                                color: root.muted
                                font.pixelSize: 14
                            }
                            ListView {
                                MiddleScrollArea { parent: budgetCategoryList; scroller: browserScroll; scrollTarget: budgetCategoryList }
                                id: budgetCategoryList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 8
                                clip: true
                                model: root.selectedBudget()
                                     ? root.selectedBudget().categoryLimits : []
                                delegate: Rectangle {
                                    SurfaceShadow { }

                                    required property var modelData
                                    width: ListView.view.width
                                    height: modelData.limitMinor > 0 ? 74 : 50
                                    radius: 10
                                    color: root.soft
                                    border.width: 1
                                    border.color: root.line
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 5
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: root.accent
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                text: modelData.limitMinor > 0
                                                    ? root.money(modelData.spentMinor,
                                                        root.selectedBudget().currency, false)
                                                      + " / "
                                                      + root.money(modelData.limitMinor,
                                                        root.selectedBudget().currency, false)
                                                    : root.money(modelData.spentMinor,
                                                        root.selectedBudget().currency, false)
                                                color: modelData.progress > 1 ? root.red : root.muted
                                                font.pixelSize: 14
                                            }
                                        }
                                        Rectangle {
                                            visible: modelData.limitMinor > 0
                                            Layout.fillWidth: true
                                            height: 6
                                            radius: 3
                                            color: root.line
                                            Rectangle {
                                                width: parent.width * Math.min(1, modelData.progress)
                                                height: parent.height
                                                radius: parent.radius
                                                color: modelData.progress > 1
                                                     ? root.red : root.accentSoft
                                            }
                                        }
                                    }
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: budgetCategoryList.count === 0
                                    text: qsTr("В этом месяце расходов пока нет")
                                    color: root.muted
                                }
                            }
                        }
                    }

                    Panel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10
                            Text {
                                text: qsTr("Операции бюджета")
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.Bold
                            }
                            ListView {
                                MiddleScrollArea { parent: budgetOperationList; scroller: browserScroll; scrollTarget: budgetOperationList }
                                id: budgetOperationList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 7
                                clip: true
                                model: financeController.budgetTransactions
                                delegate: Rectangle {
                                    SurfaceShadow { }

                                    required property var modelData
                                    width: ListView.view.width
                                    height: 64
                                    radius: 10
                                    color: root.soft
                                    border.width: 1
                                    border.color: root.line
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.description.length > 0
                                                    ? modelData.description : modelData.categoryName
                                                color: root.accent
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                text: modelData.categoryName + " · "
                                                    + Qt.formatDateTime(
                                                        new Date(modelData.date), "dd.MM.yyyy")
                                                color: root.muted
                                                font.pixelSize: 14
                                            }
                                        }
                                        Text {
                                            text: "−" + root.money(
                                                modelData.budgetAmountMinor,
                                                modelData.budgetCurrency, false)
                                            color: root.red
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                                // Label {
                                //     anchors.centerIn: parent
                                //     visible: budgetOperationList.count === 0
                                //     text: qsTr("Операций пока нет")
                                //     color: root.muted
                                // }
                            }
                        }
                    }
                }
            }

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.selectedBudget() === null
                color: root.soft
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 60, 430)
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Бюджет не выбран")
                        color: root.accent
                        font.pixelSize: 21
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Создайте бюджет и выберите счета и категории, расходы которых нужно контролировать")
                        color: root.muted
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Component {
        id: goalsPage
        FinancialGoalsPage { controller: financeController; theme: root }
    }

    Component {
        id: trajectoryPage
        FinancialTrajectoryPage { controller: financeController; theme: root }
    }

    Component {
        id: projectsPage
        RowLayout {
            spacing: 14

            Panel {
                Layout.preferredWidth: 320
                Layout.minimumWidth: 280
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Мои проекты")
                            color: root.accent
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        SoftButton {
                            text: qsTr("+ Проект")
                            highlighted: true
                            onClicked: projectDialog.openForNew()
                        }
                    }

                    ListView {
                        MiddleScrollArea { parent: projectList; scroller: browserScroll; scrollTarget: projectList }
                        id: projectList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: financeController.projects
                        ScrollBar.vertical: StyledScrollBar {
                            policy: ScrollBar.AlwaysOff
                            appAccentColor: root.accentSoft
                            appTrackColor: root.line
                        }

                        delegate: Rectangle {
                            SurfaceShadow { }

                            id: projectCard
                            required property var modelData
                            width: ListView.view.width
                            height: 108
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name
                            ToolTip.delay: 650
                            ToolTip.visible: projectRowMenuArea.containsMouse || activeFocus
                            ToolTip.text: qsTr("Правый клик или Enter: изменить или удалить")
                            Keys.onPressed: function(event) {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                    || event.key === Qt.Key_Menu
                                    || (event.key === Qt.Key_F10
                                        && (event.modifiers & Qt.ShiftModifier))) {
                                    financeController.selectedProjectId = modelData.id;
                                    root.openProjectContextMenu(modelData, projectCard,
                                                                projectCard.width - 24, 24);
                                    event.accepted = true;
                                }
                            }
                            radius: 12
                            color: financeController.selectedProjectId
                                   === modelData.id
                                   ? root.pale
                                   : root.soft
                            border.width: projectCard.activeFocus ? 2 : 1
                            border.color: projectCard.activeFocus
                                          || financeController.selectedProjectId
                                          === modelData.id
                                          ? root.accentSoft
                                          : root.line

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: root.accent
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("%1 операций · результат %2")
                                          .arg(modelData.operationCount)
                                          .arg(root.money(
                                              modelData.resultMinor,
                                              financeController.appCurrency,
                                              true
                                          ))
                                    color: root.muted
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: projectRowMenuArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    projectCard.forceActiveFocus();
                                    financeController.selectedProjectId = modelData.id;
                                    if (mouse.button === Qt.RightButton) {
                                        root.openProjectContextMenu(
                                            modelData,
                                            projectRowMenuArea,
                                            mouse.x,
                                            mouse.y
                                        );
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14
                visible: root.selectedProject() !== null

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedProject()
                              ? root.selectedProject().name
                              : ""
                        color: root.accent
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Repeater {
                        model: [
                            {
                                title: qsTr("Доходы проекта"),
                                amount: root.selectedProject()
                                        ? root.selectedProject().incomeMinor : 0,
                                color: root.income
                            },
                            {
                                title: qsTr("Расходы проекта"),
                                amount: root.selectedProject()
                                        ? root.selectedProject().expenseMinor : 0,
                                color: root.red
                            },
                            {
                                title: qsTr("Результат проекта"),
                                amount: root.selectedProject()
                                        ? root.selectedProject().resultMinor : 0,
                                color: root.selectedProject()
                                       && root.selectedProject().resultMinor < 0
                                       ? root.red : root.income
                            }
                        ]

                        delegate: Panel {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 106
                            color: root.soft

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 9
                                Text {
                                    text: modelData.title
                                    color: root.muted
                                    font.pixelSize: 14
                                }
                                Text {
                                    text: root.money(
                                        modelData.amount,
                                        financeController.appCurrency,
                                        false
                                    )
                                    color: modelData.color
                                    font.pixelSize: 23
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }

                TransactionBlock {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Операции проекта")
                    rows: financeController.projectTransactions
                    projectId: financeController.selectedProjectId
                }
            }

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.selectedProject() === null
                color: root.soft

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 60, 430)
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Проект не выбран")
                        color: root.accent
                        font.pixelSize: 21
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Создайте проект слева, а затем добавляйте его доходы и расходы прямо здесь")
                        color: root.muted
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        // font.pixelSize: 14
                    }
                }
            }
        }
    }

    Component {
        id: accountsPage
        ColumnLayout {
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: root.assets
                    delegate: SoftButton {
                        required property var modelData
                        text: modelData.title
                        highlighted: financeController.selectedAsset === modelData.code
                        implicitWidth: 130
                        onClicked: financeController.selectedAsset = modelData.code
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    visible: financeController.selectedAsset !== "investment"
                          || financeController.investmentAccounts.length > 0
                    text: financeController.selectedAsset === "crypto"
                          ? qsTr("+ Добавить криптовалюту")
                          : financeController.selectedAsset === "investment"
                            ? qsTr("+ Добавить позицию")
                            : qsTr("+ Добавить счёт")
                    highlighted: true
                    implicitWidth: financeController.selectedAsset === "crypto" ? 220 : 170
                    onClicked: {
                        if (financeController.selectedAsset === "crypto")
                            cryptoWalletDialog.openForNewWallet();
                        else if (financeController.selectedAsset === "investment")
                            investmentPositionDialog.openForNewPosition();
                        else
                            accountDialog.openForSelectedAsset();
                    }
                }
                SoftButton {
                    visible: financeController.selectedAsset === "investment"
                    text: qsTr("+ Добавить счёт")
                    onClicked: accountDialog.openForSelectedAsset()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: financeController.selectedAsset === "crypto"
                      && (financeController.cryptoRefreshing
                          || financeController.cryptoLastError.length > 0)

                Text {
                    Layout.fillWidth: true
                    text: financeController.cryptoRefreshing
                          ? qsTr("Обновляем балансы, цены и историю криптовалют…")
                          : qsTr("Не удалось обновить криптоданные: %1")
                                .arg(financeController.cryptoLastError)
                    color: financeController.cryptoRefreshing ? root.muted : root.red
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
                SoftButton {
                    visible: !financeController.cryptoRefreshing
                    text: qsTr("Повторить")
                    onClicked: financeController.refreshCryptoWallets()
                }
            }
            GridView {
                Layout.fillWidth: true
                Layout.fillHeight: financeController.selectedAsset === "fiat"
                Layout.preferredHeight: financeController.selectedAsset !== "fiat"
                                      ? 188
                                      : -1
                Layout.minimumHeight: financeController.selectedAsset !== "fiat"
                                    ? 188
                                    : 0
                cellWidth: 320
                cellHeight: financeController.selectedAsset === "crypto" ? 174 : 150
                clip: true
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: StyledScrollBar {
                    policy: ScrollBar.AlwaysOff
                    appAccentColor: root.accentSoft
                    appTrackColor: root.line
                }
                model: financeController.selectedAsset === "crypto"
                       ? financeController.cryptoWallets
                       : financeController.accounts
                delegate: Panel {
                    id: accountCard
                    required property var modelData
                    readonly property bool selected:
                        (modelData.isCrypto === true
                         && financeController.selectedCryptoWalletId === modelData.id)
                        || (financeController.selectedAsset === "investment"
                            && financeController.selectedAccountId === modelData.id)
                    width: 300
                    height: modelData.isCrypto ? 156 : 132
                    color: selected ? root.accent : root.panel
                    border.color: selected ? root.accent : root.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Text {
                                text: "▣"
                                color: accountCard.selected
                                     ? root.white
                                     : root.accent
                                font.pixelSize: 24
                            }
                            Text {
                                text: modelData.name
                                color: accountCard.selected
                                     ? root.white
                                     : root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            text: root.accountPrimaryAmount(modelData)
                            color: accountCard.selected
                                 ? root.white
                                 : root.accent
                            font.pixelSize: modelData.isCreditCard ? 20 : 25
                            font.weight: Font.Bold
                        }
                        Text {
                            visible: modelData.isCreditCard
                            text: root.accountAvailableCredit(modelData)
                            color: accountCard.selected
                                 ? root.paleText
                                 : root.navSelected
                            font.pixelSize: 14
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.isCrypto
                                  ? modelData.network + " · " + modelData.address
                                  : modelData.currency + " · " + root.accountTypeLabel(modelData.type)
                            color: accountCard.selected
                                 ? root.paleText
                                 : root.muted
                            font.pixelSize: 14
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.isCrypto
                            text: root.cryptoWalletStatus(modelData)
                            color: accountCard.selected
                                 ? root.paleText
                                 : modelData.refreshing
                                   ? root.navSelected
                                   : root.muted
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: accountsPageMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: modelData.isCrypto
                                   || financeController.selectedAsset === "investment"
                                   ? Qt.PointingHandCursor
                                   : Qt.ArrowCursor
                        onClicked: function (mouse) {
                            if (mouse.button !== Qt.LeftButton)
                                return;
                            if (modelData.isCrypto)
                                financeController.selectedCryptoWalletId = modelData.id;
                            else if (financeController.selectedAsset === "investment")
                                financeController.selectedAccountId =
                                    financeController.selectedAccountId === modelData.id
                                    ? "" : modelData.id;
                        }
                        onPressed: function (mouse) {
                            if (mouse.button === Qt.RightButton)
                                root.openAccountContextMenu(
                                    modelData,
                                    accountsPageMouseArea,
                                    mouse.x,
                                    mouse.y
                                );
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                    text: financeController.selectedAsset === "crypto"
                          ? qsTr("Добавьте публичный адрес криптокошелька")
                          : qsTr("У этого актива пока нет счетов")
                    color: root.muted
                }
            }
            InvestmentPositionsPanel {
                visible: financeController.selectedAsset === "investment"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 250
                controller: financeController
                confirmDeletion: true
                panelColor: root.panel
                textColor: root.accent
                mutedColor: root.muted
                lineColor: root.line
                errorColor: root.red
                onContextMenuRequested: function(position, sourceItem, x, y) {
                    root.openTransactionContextMenu(
                        root.investmentOperationRow(position),
                        sourceItem,
                        x,
                        y
                    );
                }
                onDeleteRequested: function(position) {
                    deleteTransactionDialog.openFor(
                        root.investmentOperationRow(position)
                    );
                }
            }
            ExchangeBalancesBlock { Layout.fillWidth: true }
            Panel {
                id: cryptoHistoryPanel
                visible: financeController.selectedAsset === "crypto"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 230

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 1
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.topMargin: 12
                        Layout.bottomMargin: 12

                        Text {
                            text: qsTr("История %1").arg(
                                root.selectedCryptoSymbol() || qsTr("криптовалюты")
                            )
                            color: root.accent
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Последние операции: %1")
                                .arg(financeController.cryptoTransactions.length)
                            color: root.muted
                            font.pixelSize: 14
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        color: root.tableHeader

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18

                            Text {
                                text: qsTr("Дата")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: 125
                            }
                            Text {
                                text: qsTr("Направление")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: 105
                            }
                            Text {
                                text: qsTr("Описание / адрес")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.fillWidth: true
                            }
                            Text {
                                text: qsTr("Счёт / хеш")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: 175
                            }
                            Text {
                                text: qsTr("Сумма")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: 135
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
            id: middleViewport8
            MiddleScrollArea { parent: middleViewport8; scroller: browserScroll; scrollTarget: middleViewport8 }

                            anchors.fill: parent
                            clip: true
                            model: financeController.cryptoTransactions
                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }
                            ScrollBar.vertical: StyledScrollBar {
                                policy: ScrollBar.AlwaysOff
                                appAccentColor: root.accentSoft
                                appTrackColor: root.line
                            }

                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                width: ListView.view.width
                                height: 42
                                color: index % 2 ? root.tableRowAlt : root.panel

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 18
                                    anchors.rightMargin: 18

                                    Text {
                                        text: Qt.formatDateTime(
                                            modelData.occurredAt,
                                            "dd.MM.yyyy HH:mm"
                                        )
                                        color: root.muted
                                        font.pixelSize: 14
                                        Layout.preferredWidth: 125
                                    }
                                    Text {
                                        text: modelData.direction === "in"
                                              ? qsTr("Получено")
                                              : modelData.direction === "neutral" ? qsTr("Без изменения") : qsTr("Отправлено")
                                        color: modelData.direction === "in"
                                             ? root.income
                                             : modelData.direction === "neutral" ? root.muted : root.red
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        Layout.preferredWidth: 105
                                    }
                                    Text {
                                        text: modelData.counterparty
                                        color: root.muted
                                        font.pixelSize: 14
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        text: modelData.isExchange ? root.exchangeSection(modelData.section) : modelData.transactionId
                                        color: root.muted
                                        font.pixelSize: 14
                                        Layout.preferredWidth: 175
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        readonly property string localizedAmount:
                                            financeController.uiLanguage === "en"
                                            ? modelData.amountText
                                            : modelData.amountText.replace(".", ",")
                                        text: (modelData.direction === "in" ? "+" : modelData.direction === "out" ? "−" : "")
                                              + localizedAmount + " " + modelData.symbol
                                        color: modelData.direction === "in"
                                             ? root.income
                                             : modelData.direction === "neutral" ? root.muted : root.red
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        Layout.preferredWidth: 135
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: financeController.cryptoTransactions.length === 0
                            text: financeController.cryptoWallets.length === 0
                                  ? qsTr("Сначала добавьте криптокошелёк")
                                  : financeController.cryptoRefreshing
                                    ? qsTr("Загружаем историю переводов…")
                                    : qsTr("У этого кошелька пока нет переводов %1")
                                          .arg(root.selectedCryptoSymbol())
                            color: root.muted
                        }
                    }
                }
            }
        }
    }

    Component {
        id: categoriesPage

        ColumnLayout {
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                SoftButton {
                    text: qsTr("+ Управление категориями")
                    highlighted: true
                    implicitWidth: 210
                    onClicked: categoryDialog.openForManagement()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                // =========================
                // Доходы
                // =========================
                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.incomePanel

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        // Text {
                        //     text: "Доходы"
                        //     color: root.accent
                        //     font.pixelSize: 17
                        //     font.weight: Font.DemiBold
                        // }

                        ListView {
            id: middleViewport9
            MiddleScrollArea { parent: middleViewport9; scroller: browserScroll; scrollTarget: middleViewport9 }

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            clip: true
                            spacing: 6

                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            ScrollBar.vertical: StyledScrollBar {
                                policy: ScrollBar.AlwaysOff
                                appAccentColor: root.accentSoft
                                appTrackColor: root.line
                            }

                            model: financeController.categories.filter(function(category) {
                                return category.type === "income";
                            })

                            delegate: Rectangle {
                                SurfaceShadow { }

                                required property var modelData

                                width: ListView.view.width
                                height: 52
                                radius: 10
                                color: root.panel
                                border.width: 1
                                border.color: "#C9D9CE"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12

                                    Text {
                                        text: "◇"
                                        color: root.income
                                        font.pixelSize: 20
                                    }

                                    Text {
                                        text: modelData.label
                                        color: root.accent
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: qsTr("Доход")
                                        color: root.muted
                                    }
                                }

                                MouseArea {
                                    id: incomeCategoryMenuArea
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onPressed: function(mouse) {
                                        if (mouse.button === Qt.RightButton)
                                            root.openCategoryContextMenu(
                                                modelData,
                                                incomeCategoryMenuArea,
                                                mouse.x,
                                                mouse.y
                                            );
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0

                                text: qsTr("Категорий доходов пока нет")
                                color: root.muted
                            }
                        }
                    }
                }

                // =========================
                // Расходы
                // =========================
                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.expensePanel

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        // Text {
                        //     text: "Расходы"
                        //     color: root.accent
                        //     font.pixelSize: 17
                        //     font.weight: Font.DemiBold
                        // }

                        ListView {
            id: middleViewport10
            MiddleScrollArea { parent: middleViewport10; scroller: browserScroll; scrollTarget: middleViewport10 }

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            clip: true
                            spacing: 6

                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            ScrollBar.vertical: StyledScrollBar {
                                policy: ScrollBar.AlwaysOff
                                appAccentColor: root.accentSoft
                                appTrackColor: root.line
                            }

                            model: financeController.categories.filter(function(category) {
                                return category.type === "expense";
                            })

                            delegate: Rectangle {
                                SurfaceShadow { }

                                required property var modelData

                                width: ListView.view.width
                                height: 52
                                radius: 10
                                color: root.panel
                                border.width: 1
                                border.color: "#E1C9C2"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12

                                    Text {
                                        text: "◇"
                                        color: root.red
                                        font.pixelSize: 20
                                    }

                                    Text {
                                        text: modelData.label
                                        color: root.accent
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: qsTr("Расход")
                                        color: root.muted
                                    }
                                }

                                MouseArea {
                                    id: expenseCategoryMenuArea
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onPressed: function(mouse) {
                                        if (mouse.button === Qt.RightButton)
                                            root.openCategoryContextMenu(
                                                modelData,
                                                expenseCategoryMenuArea,
                                                mouse.x,
                                                mouse.y
                                            );
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0

                                text: qsTr("Категорий расходов пока нет")
                                color: root.muted
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: operationsPage
        TransactionBlock {
            anchors.fill: parent
            title: qsTr("История операций")
            rows: root.visibleTransactions()
        }
    }

    Component {
        id: analyticsPage
        ScrollView {
            MiddleScrollArea { parent: analyticsScroll; scroller: browserScroll; scrollTarget: analyticsScroll.contentItem }
            id: analyticsScroll
            property int expenseYear: new Date().getFullYear()
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: StyledScrollBar {
                policy: ScrollBar.AlwaysOff
                appAccentColor: root.accentSoft
                appTrackColor: root.line
            }

            ColumnLayout {
                width: analyticsScroll.availableWidth
                spacing: 14

                Panel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 275
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("История расходов по месяцам")
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            SoftButton {
                                text: "‹"
                                controlHeight: 30
                                Layout.preferredWidth: 34
                                enabled: analyticsScroll.expenseYear
                                    > root.expenseHistoryFirstYear()
                                onClicked: analyticsScroll.expenseYear -= 1
                            }
                            Text {
                                text: analyticsScroll.expenseYear
                                color: root.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.minimumWidth: 42
                                horizontalAlignment: Text.AlignHCenter
                            }
                            SoftButton {
                                text: "›"
                                controlHeight: 30
                                Layout.preferredWidth: 34
                                enabled: analyticsScroll.expenseYear
                                    < root.expenseHistoryLastYear()
                                onClicked: analyticsScroll.expenseYear += 1
                            }
                            Text { text: "RUB"; color: root.muted; font.pixelSize: 14 }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("В среднем %1")
                                .arg(root.money(
                                    root.averageCompletedMonthExpense(
                                        root.expenseRowsForYear(
                                            analyticsScroll.expenseYear)),
                                    "RUB", false))
                            color: root.muted
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                        AnalyticsBarChart {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            points: root.expenseRowsForYear(analyticsScroll.expenseYear)
                            currency: "RUB"
                            averageMinor: root.averageCompletedMonthExpense(points)
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            visible: financeController.expenseHistoryByMonthRub.length === 0
                            text: qsTr("Добавьте расходы — здесь появится история по месяцам")
                            color: root.muted
                            font.pixelSize: 14
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                Panel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 360
                    Layout.preferredHeight: 300
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Text {
                                text: qsTr("История капитала в рублях")
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: root.historyResolutionLabel(financeController.capitalHistoryRub)
                                color: root.muted
                                font.pixelSize: 14
                            }
                        }
                        AnalyticsLineChart {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            points: financeController.capitalHistoryRub
                            currency: "RUB"
                            seriesColor: root.chartAccent2
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.minimumWidth: 360
                    Layout.preferredHeight: 300
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Капитал в выбранной валюте")
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            AppComboBox {
                                model: ["USD", "EUR"]
                                currentIndex: Math.max(0, model.indexOf(financeController.analyticsCurrency))
                                onActivated: financeController.analyticsCurrency = currentText
                                implicitWidth: 105
                            }
                        }
                        AnalyticsLineChart {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            points: financeController.capitalHistoryAnalyticsCurrency
                            currency: financeController.analyticsCurrency
                            seriesColor: root.chartAccent4
                        }
                    }
                }
                }
            }
        }
    }

    Component {
        id: notesPage

        Panel {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Один документ. Изменения сохраняются автоматически.")
                        color: root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }

                    Text {
                        text: financeController.notesError.length > 0
                              ? qsTr("Ошибка заметок")
                              : financeController.notesDirty
                                ? qsTr("Есть несохранённые изменения")
                                : qsTr("Сохранено")
                        color: financeController.notesError.length > 0
                               ? root.red : root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        Layout.maximumWidth: 310
                    }

                    SoftButton {
                        text: qsTr("Сохранить")
                        highlighted: true
                        enabled: financeController.notesAvailable
                                 && financeController.notesDirty
                        onClicked: financeController.saveNotes()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: financeController.notesError.length > 0
                    text: financeController.notesError
                    color: root.red
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }

                ScrollView {
                    MiddleScrollArea { parent: notesScroll; scroller: browserScroll; scrollTarget: notesScroll.contentItem }
                    id: notesScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    background: Rectangle {
                        SurfaceShadow { }

                        color: root.soft
                        radius: 12
                        border.width: 1
                        border.color: root.line
                    }
                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AlwaysOff
                    }
                    ScrollBar.vertical: StyledScrollBar {
                        policy: ScrollBar.AlwaysOff
                        appAccentColor: root.accentSoft
                        appTrackColor: root.line
                    }

                    TextArea {
                        id: notesEditor
                        property bool loaded: false
                        width: notesScroll.availableWidth
                        height: Math.max(notesScroll.availableHeight,
                                         contentHeight + topPadding + bottomPadding)
                        readOnly: !financeController.notesAvailable
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        placeholderText: qsTr("Запишите мысли, планы или важные детали…")
                        color: root.accent
                        placeholderTextColor: root.muted
                        selectionColor: root.navSelected
                        selectedTextColor: root.white
                        font.pixelSize: 15
                        leftPadding: 18
                        rightPadding: 18
                        topPadding: 16
                        bottomPadding: 16
                        background: null

                        Component.onCompleted: {
                            text = financeController.notesText;
                            loaded = true;
                        }
                        onTextChanged: {
                            if (loaded)
                                financeController.setNotesText(text);
                        }
                        onActiveFocusChanged: {
                            if (!activeFocus && loaded)
                                financeController.saveNotes();
                        }
                    }
                }

                Shortcut {
                    sequences: [StandardKey.Save]
                    context: Qt.WindowShortcut
                    enabled: root.page === "notes"
                    onActivated: financeController.saveNotes()
                }
            }

            Component.onDestruction: financeController.saveNotes()
        }
    }

    Component {
        id: settingsPage
        Panel {
            ScrollView {
                MiddleScrollArea { parent: settingsScroll; scroller: browserScroll; scrollTarget: settingsScroll.contentItem }
                id: settingsScroll

                anchors.fill: parent
                clip: true
                contentWidth: availableWidth
                contentHeight: settingsContent.implicitHeight + 56

                ScrollBar.horizontal: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOff
                }

                ColumnLayout {
                    id: settingsContent

                    x: 28
                    y: 28
                    width: Math.max(0, settingsScroll.availableWidth - 56)
                Text {
                    text: qsTr("Основная валюта")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Все итоговые суммы пересчитываются в эту валюту.")
                    color: root.muted
                }
                AppComboBox {
                    model: ["RUB", "USD", "EUR"]
                    currentIndex: Math.max(0, model.indexOf(financeController.appCurrency))
                    onActivated: financeController.appCurrency = currentText
                    implicitWidth: 180
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.preferredWidth: 360
                    height: 1
                    color: root.line
                }
                Text {
                    Layout.topMargin: 8
                    text: qsTr("Курсы валют")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.preferredWidth: 460
                    text: automaticRatesCheck.checked
                          ? qsTr("Курсы загружаются из ЦБ РФ дважды в сутки. При ошибке используется последний успешный результат.")
                          : qsTr("Автоматические запросы отключены. Для пересчёта используются сохранённые ниже значения.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    SurfaceShadow { }

                    id: currentRatesBlock

                    Layout.topMargin: 4
                    Layout.preferredWidth: 460
                    implicitHeight: ratesHeader.height + ratesList.implicitHeight
                    radius: 10
                    color: root.soft
                    border.color: root.line
                    clip: true

                    Rectangle {
                        id: ratesHeader
                        width: parent.width
                        height: 44
                        color: root.transparentColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Текущие курсы в %1")
                                    .arg("RUB")
                                color: root.accent
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Column {
                        id: ratesList
                        anchors.top: ratesHeader.bottom
                        width: parent.width

                        Repeater {
                            model: financeController.currentCurrencyRates

                            Rectangle {
                                required property var modelData
                                width: ratesList.width
                                height: 38
                                color: root.transparentColor

                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    height: 1
                                    color: root.line
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14

                                    Text {
                                        Layout.fillWidth: true
                                        text: "1 " + modelData.code
                                        color: root.accent
                                    }
                                    Text {
                                        readonly property int decimals:
                                            modelData.rate >= 100 ? 2
                                          : modelData.rate >= 1 ? 4 : 6
                                        text: Number(modelData.rate).toLocaleString(
                                                  root.uiLocale(), "f", decimals)
                                              + " "
                                              + root.symbol("RUB")
                                        color: root.accent
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }
                    }
                }
                AppCheckBox {
                    id: automaticRatesCheck
                    text: qsTr("Обновлять курсы автоматически")
                    checked: financeController.automaticCurrencyRates
                    onToggled: {
                        financeController.automaticCurrencyRates = checked;
                        rateSaveStatus.text = "";
                    }
                }
                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    enabled: !automaticRatesCheck.checked
                    opacity: enabled ? 1.0 : 0.55

                    Text {
                        text: qsTr("1 RUB в рублях")
                        color: root.accent
                    }
                    AppTextField {
                        id: manualRubRateField
                        implicitWidth: 180
                        text: financeController.manualRubToRubRate.toLocaleString(
                            root.uiLocale(), "f", 4)
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator {
                            bottom: 0.0001
                            top: 999999999
                            decimals: 4
                            locale: root.uiLocale().name
                        }
                        onTextEdited: rateSaveStatus.text = ""
                    }

                    Text {
                        text: qsTr("1 USD в рублях")
                        color: root.accent
                    }
                    AppTextField {
                        id: manualUsdRateField
                        implicitWidth: 180
                        text: financeController.manualUsdToRubRate.toLocaleString(
                            root.uiLocale(), "f", 4)
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator {
                            bottom: 0.0001
                            top: 999999999
                            decimals: 4
                            locale: root.uiLocale().name
                        }
                        onTextEdited: rateSaveStatus.text = ""
                    }

                    Text {
                        text: qsTr("1 EUR в рублях")
                        color: root.accent
                    }
                    AppTextField {
                        id: manualEurRateField
                        implicitWidth: 180
                        text: financeController.manualEurToRubRate.toLocaleString(
                            root.uiLocale(), "f", 4)
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator {
                            bottom: 0.0001
                            top: 999999999
                            decimals: 4
                            locale: root.uiLocale().name
                        }
                        onTextEdited: rateSaveStatus.text = ""
                    }
                }
                RowLayout {
                    enabled: !automaticRatesCheck.checked
                    opacity: enabled ? 1.0 : 0.55

                    SoftButton {
                        text: qsTr("Сохранить курсы")
                        highlighted: true
                        enabled: manualRubRateField.acceptableInput
                              && manualUsdRateField.acceptableInput
                              && manualEurRateField.acceptableInput
                        onClicked: {
                            const rubRate = Number(manualRubRateField.text.replace(",", "."));
                            const usdRate = Number(manualUsdRateField.text.replace(",", "."));
                            const eurRate = Number(manualEurRateField.text.replace(",", "."));
                            const saved = financeController.saveManualCurrencyRates(
                                rubRate, usdRate, eurRate);
                            rateSaveStatus.color = saved ? root.income : root.red;
                            rateSaveStatus.text = saved
                                ? qsTr("Курсы сохранены")
                                : qsTr("Введите положительные числовые значения");
                        }
                    }
                    Text {
                        id: rateSaveStatus
                        color: root.income
                        font.pixelSize: 14
                    }
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.preferredWidth: 360
                    height: 1
                    color: root.line
                }
                Text {
                    Layout.topMargin: 8
                    text: qsTr("Импорт и экспорт")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.preferredWidth: 520
                    text: qsTr("CSV Ledgera переносит операции. Банковский импорт поддерживает CSV, XLSX и PDF-выписки Альфы и Сбера, сопоставление столбцов и сохранённые профили.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    spacing: 10
                    SoftButton {
                        text: qsTr("Экспортировать CSV")
                        highlighted: true
                        implicitWidth: 160
                        onClicked: exportCsvDialog.open()
                    }
                    SoftButton {
                        text: qsTr("Выписка банка")
                        implicitWidth: 160
                        onClicked: bankCsvFileDialog.open()
                    }
                    SoftButton {
                        text: qsTr("Импорт Ledgera")
                        implicitWidth: 160
                        onClicked: importCsvDialog.open()
                    }
                }
                Text {
                    Layout.preferredWidth: 520
                    visible: root.csvStatus.length > 0
                    text: root.csvStatus
                    color: root.csvStatusOk ? root.income : root.red
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.preferredWidth: 360
                    height: 1
                    color: root.line
                }
                Text {
                    Layout.topMargin: 8
                    text: qsTr("Резервная копия")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.preferredWidth: 520
                    text: qsTr("Сохранить полную копию базы данных со счетами, операциями и настройками. Функция доступна в настольной версии для Linux и Windows.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                SoftButton {
                    text: qsTr("Создать резервную копию")
                    highlighted: true
                    implicitWidth: 230
                    onClicked: databaseBackupDialog.open()
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.preferredWidth: 360
                    height: 1
                    color: root.line
                }
                Text {
                    Layout.topMargin: 8
                    text: qsTr("Удаление данных")
                    color: root.red
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.preferredWidth: 520
                    text: qsTr("Удалить все счета, операции и остальные пользовательские данные. Это действие нельзя отменить.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                SoftButton {
                    text: qsTr("Очистить все данные")
                    implicitWidth: 210
                    onClicked: clearDataDialog.open()
                }
                }
            }
        }
    }

    FileDialog {
        id: databaseBackupDialog
        title: qsTr("Сохранить резервную копию базы данных")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("База данных SQLite (*.sqlite3 *.db)")]
        defaultSuffix: "sqlite3"
        onAccepted: {
            const result = financeController.backupDatabase(selectedFile);
            root.csvStatusOk = result.ok;
            root.csvStatus = result.ok
                ? qsTr("Резервная копия сохранена: %1").arg(result.path)
                : qsTr("Не удалось создать копию: %1").arg(result.error);
        }
    }

    Dialog {
        id: clearDataDialog
        width: 470
        modal: true
        anchors.centerIn: parent
        padding: 24
        closePolicy: Popup.CloseOnEscape

        function confirmClear() {
            const result = financeController.clearAllData();
            if (result.ok) {
                root.csvStatusOk = true;
                root.csvStatus = qsTr("Все пользовательские данные удалены");
                close();
                return;
            }
            clearDataError.text = result.error
                ? qsTr("Не удалось очистить данные: %1").arg(result.error)
                : qsTr("Не удалось очистить данные");
        }

        onAboutToShow: clearDataError.text = ""

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: clearDataDialog.visible
                  && !clearDataCancelButton.activeFocus
                  && !clearDataSubmitButton.activeFocus
            onActivated: clearDataDialog.confirmClear()
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: qsTr("Удалить все данные?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Все счета, операции, цели, бюджеты, кошельки и другие пользовательские данные будут удалены. Это действие нельзя отменить.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Rectangle {
                SurfaceShadow { }

                Layout.fillWidth: true
                implicitHeight: 52
                radius: 10
                color: root.expensePanel

                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    text: qsTr("Перед удалением рекомендуем создать резервную копию базы данных.")
                    color: root.red
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }
            }

            Text {
                id: clearDataError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: clearDataCancelButton
                    text: qsTr("Отмена")
                    onClicked: clearDataDialog.close()
                }
                SoftButton {
                    id: clearDataSubmitButton
                    text: qsTr("Удалить всё")
                    destructive: true
                    onClicked: clearDataDialog.confirmClear()
                }
            }
        }
    }

    FileDialog {
        id: exportCsvDialog
        title: qsTr("Экспорт операций в CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV-файлы (*.csv)")]
        defaultSuffix: "csv"
        onAccepted: {
            const result = financeController.exportTransactionsCsv(selectedFile);
            root.csvStatusOk = result.ok;
            root.csvStatus = result.ok
                ? qsTr("Экспортировано операций: %1. Файл: %2")
                    .arg(result.count).arg(result.path)
                : qsTr("Не удалось экспортировать CSV: %1").arg(result.error);
        }
    }

    FileDialog {
        id: bankCsvFileDialog
        title: qsTr("Выберите банковскую выписку")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Банковские выписки (*.csv *.xlsx *.pdf)"), qsTr("Все файлы (*)")]
        onAccepted: bankCsvImportDialog.openForFile(selectedFile)
    }

    FileDialog {
        id: importCsvDialog
        title: qsTr("Импорт резервной копии Ledgera")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("CSV-файлы (*.csv)")]
        onAccepted: {
            const result = financeController.importTransactionsCsv(selectedFile);
            root.csvStatusOk = result.ok;
            root.csvStatus = result.ok
                ? qsTr("Импортировано: %1, пропущено дубликатов: %2")
                    .arg(result.imported).arg(result.skipped)
                : qsTr("Не удалось импортировать CSV: %1").arg(result.error);
        }
    }

    BankCsvImportDialog {
        id: bankCsvImportDialog
        controller: financeController
        panelColor: root.panel
        softColor: root.soft
        textColor: root.accent
        mutedColor: root.muted
        lineColor: root.line
        accentColor: root.accent
        errorColor: root.red
        successColor: root.income
        onImportFinished: function(result) {
            root.csvStatusOk = true;
            root.csvStatus = qsTr("Импортировано: %1, пропущено дубликатов: %2, отклонено строк: %3")
                .arg(result.imported).arg(result.skipped).arg(result.rejected);
        }
    }

    InvestmentPositionDialog {
        id: investmentPositionDialog
        controller: financeController
        panelColor: root.panel
        textColor: root.accent
        mutedColor: root.muted
        lineColor: root.line
        accentColor: root.accent
        errorColor: root.red
    }

    RecurringTransactionsDialog {
        id: recurringTransactionsDialog
        controller: financeController
        moneyFormatter: function(minor, currency, sign) {
            return root.money(minor, currency, sign);
        }
        panelColor: root.panel
        softColor: root.soft
        textColor: root.accent
        mutedColor: root.muted
        lineColor: root.line
        accentColor: root.accent
        errorColor: root.red
        successColor: root.income
    }

    BudgetDialog {
        id: budgetDialog
        controller: financeController
        panelColor: root.panel
        softColor: root.soft
        textColor: root.accent
        mutedColor: root.muted
        lineColor: root.line
        accentColor: root.accent
        errorColor: root.red
    }

    Dialog {
        id: cryptoWalletDialog
        objectName: "cryptoConnectionDialog"
        width: Math.min(520, root.width - 48)
        modal: true
        anchors.centerIn: parent
        padding: 24
        property string editingId: ""

        function openForNewWallet() {
            editingId = "";
            cryptoNameField.clear(); cryptoSourceBox.currentIndex = 0;
            cryptoTypeBox.currentIndex = 0; cryptoAddressField.clear();
            cryptoApiKeyField.clear(); cryptoApiSecretField.clear(); cryptoWalletError.text = "";
            open(); Qt.callLater(function() { cryptoNameField.forceActiveFocus(); });
        }
        function openForEdit(row) {
            editingId = row.id; cryptoNameField.text = row.name;
            cryptoSourceBox.currentIndex = row.isExchange ? 1 : 0;
            cryptoTypeBox.currentIndex = row.symbol === "BTC" ? 1 : row.symbol === "ETH" ? 2 : 0;
            cryptoAddressField.text = row.address || "";
            cryptoApiKeyField.clear(); cryptoApiSecretField.clear(); cryptoWalletError.text = "";
            open(); Qt.callLater(function() { cryptoNameField.forceActiveFocus(); });
        }
        function submit() {
            const result = financeController.saveCryptoConnection({
                id: editingId, name: cryptoNameField.text,
                kind: cryptoSourceBox.currentIndex === 0 ? "wallet" : "exchange",
                symbol: cryptoTypeBox.model[cryptoTypeBox.currentIndex].value,
                address: cryptoAddressField.text,
                apiKey: cryptoApiKeyField.text, apiSecret: cryptoApiSecretField.text
            });
            if (result.ok) close();
            else cryptoWalletError.text = result.error || qsTr("Не удалось сохранить счёт");
        }
        onClosed: { cryptoApiKeyField.clear(); cryptoApiSecretField.clear(); }
        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: cryptoWalletDialog.visible && !cryptoTypeBox.popup.visible
                  && !cryptoSourceBox.popup.visible && !cryptoCancelButton.activeFocus
                  && !cryptoSubmitButton.activeFocus
            onActivated: cryptoWalletDialog.submit()
        }
        background: Rectangle {
            SurfaceShadow { }
            color: root.panel; radius: 16; border.width: 1; border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 16
            Text {
                text: cryptoWalletDialog.editingId ? qsTr("Редактировать криптовалюту") : qsTr("Добавить криптовалюту")
                color: root.accent; font.pixelSize: 21; font.weight: Font.Bold
            }
            AppTextField {
                id: cryptoNameField
                objectName: "cryptoNameField"
                Layout.fillWidth: true
                placeholderText: qsTr("Название счёта")
                maximumLength: 80
            }
            AppComboBox {
                id: cryptoSourceBox
                objectName: "cryptoSourceBox"
                Layout.fillWidth: true
                model: [qsTr("Кошелёк"), qsTr("Биржа")]
                enabled: !cryptoWalletDialog.editingId
                onActivated: cryptoWalletError.text = ""
            }
            AppComboBox {
                id: cryptoTypeBox
                Layout.fillWidth: true
                visible: cryptoSourceBox.currentIndex === 0
                enabled: !cryptoWalletDialog.editingId
                model: [{ label: "USDT · TRC-20", value: "USDT" },
                        { label: "BTC · Bitcoin", value: "BTC" },
                        { label: "ETH · Ethereum", value: "ETH" }]
                textRole: "label"
                onActivated: { cryptoAddressField.clear(); cryptoWalletError.text = ""; }
            }
            AppTextField {
                id: cryptoAddressField
                Layout.fillWidth: true
                visible: cryptoSourceBox.currentIndex === 0
                enabled: !cryptoWalletDialog.editingId
                placeholderText: cryptoTypeBox.currentIndex === 0 ? qsTr("Публичный адрес TRON (T…)")
                    : cryptoTypeBox.currentIndex === 1 ? qsTr("Публичный адрес Bitcoin (1…, 3… или bc1…)")
                    : qsTr("Публичный адрес Ethereum (0x…)")
                maximumLength: 90
            }
            AppComboBox {
                Layout.fillWidth: true
                visible: cryptoSourceBox.currentIndex === 1
                model: ["Bybit"]
            }
            AppTextField {
                id: cryptoApiKeyField
                objectName: "cryptoApiKeyField"
                Layout.fillWidth: true
                visible: cryptoSourceBox.currentIndex === 1
                placeholderText: qsTr("Ключ API")
                maximumLength: 256
                echoMode: TextInput.Password
            }
            AppTextField {
                id: cryptoApiSecretField
                objectName: "cryptoApiSecretField"
                Layout.fillWidth: true
                visible: cryptoSourceBox.currentIndex === 1
                placeholderText: qsTr("Секретный ключ API")
                maximumLength: 256
                echoMode: TextInput.Password
            }
            Text {
                Layout.fillWidth: true
                text: cryptoSourceBox.currentIndex === 0
                    ? qsTr("Введите публичный адрес кошелька.")
                    : (cryptoWalletDialog.editingId ? qsTr("Оставьте оба поля ключей пустыми, чтобы сохранить подключение.\n") : "")
                      + qsTr("Bybit: основной сайт, ключ с доступом только на чтение. История торгового счёта и финансирования за последние 30 дней. Ключи сохраняются в защищённом хранилище системы; если оно недоступно — до закрытия приложения.")
                color: root.muted; font.pixelSize: 14; wrapMode: Text.WordWrap
            }
            Text {
                id: cryptoWalletError
                Layout.fillWidth: true
                visible: text.length > 0
                color: root.red; font.pixelSize: 14; wrapMode: Text.WordWrap
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                SoftButton { id: cryptoCancelButton; text: qsTr("Отмена"); onClicked: cryptoWalletDialog.close() }
                SoftButton {
                    id: cryptoSubmitButton
                    text: cryptoWalletDialog.editingId ? qsTr("Сохранить") : qsTr("Добавить")
                    highlighted: true; onClicked: cryptoWalletDialog.submit()
                }
            }
        }
    }

    Dialog {
        id: accountDialog
        width: 500
        modal: true
        anchors.centerIn: parent
        padding: 24
        property var accountTypes: []
        property string editingId: ""
        property var editingAccount: null
        property string editingAsset: financeController.selectedAsset
        property var depositPayoutTypes: [
            { label: qsTr("Ежемесячно"), value: "monthly" },
            { label: qsTr("Ежедневно"), value: "daily" }
        ]
        readonly property bool creditCardSelected:
            accountTypeBox.currentIndex >= 0
            && accountTypeBox.currentIndex < accountTypes.length
            && accountTypes[accountTypeBox.currentIndex].value === "credit_card"
        readonly property bool depositSelected:
            accountTypeBox.currentIndex >= 0
            && accountTypeBox.currentIndex < accountTypes.length
            && accountTypes[accountTypeBox.currentIndex].value === "deposit"
        readonly property bool monthlyDepositSelected:
            depositSelected && depositPayoutBox.currentIndex === 0

        function typesForAsset(asset) {
            return asset === "fiat" ? [
                {
                    label: qsTr("Наличные"),
                    value: "cash"
                },
                {
                    label: qsTr("Дебетовая карта"),
                    value: "debit_card"
                },
                {
                    label: qsTr("Кредитная карта"),
                    value: "credit_card"
                },
                {
                    label: qsTr("Другой"),
                    value: "other"
                }
            ] : asset === "crypto" ? [
                {
                    label: qsTr("Криптокошелёк"),
                    value: "crypto_wallet"
                },
                {
                    label: qsTr("Другой"),
                    value: "other"
                }
            ] : [
                {
                    label: qsTr("Брокер"),
                    value: "brokerage"
                },
                {
                    label: qsTr("Вклад"),
                    value: "deposit"
                },
                {
                    label: qsTr("Другой"),
                    value: "other"
                }
            ];
        }

        function openForSelectedAsset() {
            editingId = "";
            editingAccount = null;
            editingAsset = financeController.selectedAsset;
            accountTypes = typesForAsset(editingAsset);
            accountNameField.clear();
            accountBalanceField.clear();
            accountCreditLimitField.clear();
            depositRateField.clear();
            depositPayoutBox.currentIndex = 0;
            depositPayoutDayField.text = "1";
            accountTypeBox.currentIndex = 0;
            accountCurrencyBox.currentIndex = 0;
            accountError.text = "";
            open();
            Qt.callLater(function() { accountNameField.forceActiveFocus(); });
        }

        function openForEdit(row) {
            editingId = row.id;
            editingAccount = row;
            editingAsset = row.asset;
            accountTypes = typesForAsset(editingAsset);
            accountNameField.text = row.name;
            accountTypeBox.currentIndex = root.indexByRole(
                accountTypes,
                "value",
                row.type
            );
            accountCurrencyBox.currentIndex = Math.max(
                0,
                accountCurrencyBox.model.indexOf(row.currency)
            );
            accountBalanceField.text = row.isCreditCard
                                     ? root.amountForInput(row.initialDebtMinor)
                                     : root.signedAmountForInput(row.initialBalanceMinor);
            accountCreditLimitField.text = root.amountForInput(row.creditLimitMinor);
            depositRateField.text = row.depositAnnualRatePercent
                                  ? String(row.depositAnnualRatePercent).replace(".", ",")
                                  : "";
            depositPayoutBox.currentIndex = row.depositPayoutFrequency === "daily"
                                          ? 1 : 0;
            depositPayoutDayField.text = row.depositPayoutDay > 0
                                       ? String(row.depositPayoutDay) : "1";
            accountError.text = "";
            open();
            Qt.callLater(function() { accountNameField.forceActiveFocus(); });
        }

        function submit() {
            const type = accountTypes[accountTypeBox.currentIndex].value;
            const enteredMinor = Math.round(
                (Number(accountBalanceField.text.replace(",", ".")) || 0) * 100
            );
            const initialBalanceMinor = creditCardSelected
                                      ? -Math.abs(enteredMinor)
                                      : enteredMinor;
            const creditLimitMinor = creditCardSelected
                                   ? Math.round(
                                       (Number(accountCreditLimitField.text.replace(",", ".")) || 0)
                                       * 100
                                     )
                                   : 0;
            if (creditCardSelected && creditLimitMinor <= 0) {
                accountError.text = qsTr("Укажите кредитный лимит");
                return;
            }
            const depositAnnualRatePercent = Number(
                depositRateField.text.replace(",", ".")
            );
            const depositPayoutFrequency = depositSelected
                ? depositPayoutTypes[depositPayoutBox.currentIndex].value
                : "";
            const depositPayoutDay = monthlyDepositSelected
                ? Number(depositPayoutDayField.text)
                : 0;
            if (depositSelected && initialBalanceMinor <= 0) {
                accountError.text = qsTr("Укажите сумму вклада");
                return;
            }
            if (depositSelected && depositAnnualRatePercent <= 0) {
                accountError.text = qsTr("Укажите годовую процентную ставку");
                return;
            }
            if (monthlyDepositSelected &&
                (depositPayoutDay < 1 || depositPayoutDay > 31)) {
                accountError.text = qsTr("Укажите день выплаты от 1 до 31");
                return;
            }
            const ok = editingId
                     ? financeController.updateAccount(
                         editingId,
                         accountNameField.text,
                         type,
                         accountCurrencyBox.currentText,
                         initialBalanceMinor,
                         creditLimitMinor,
                         depositAnnualRatePercent,
                         depositPayoutFrequency,
                         depositPayoutDay
                     )
                     : financeController.addAccount(
                         accountNameField.text,
                         type,
                         accountCurrencyBox.currentText,
                         initialBalanceMinor,
                         creditLimitMinor,
                         depositAnnualRatePercent,
                         depositPayoutFrequency,
                         depositPayoutDay
                     );
            if (ok)
                close();
            else
                accountError.text = qsTr("Проверьте название и параметры счёта");
        }

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: accountDialog.visible
                  && !accountTypeBox.activeFocus
                  && !accountTypeBox.popup.visible
                  && !accountCurrencyBox.activeFocus
                  && !accountCurrencyBox.popup.visible
                  && !depositPayoutBox.activeFocus
                  && !depositPayoutBox.popup.visible
                  && !accountCancelButton.activeFocus
                  && !accountSubmitButton.activeFocus
            onActivated: accountDialog.submit()
        }

        onClosed: {
            editingId = "";
            editingAccount = null;
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: (accountDialog.editingId
                       ? qsTr("Редактирование счёта")
                       : qsTr("Новый счёт"))
                      + " · " + root.assetTitle(accountDialog.editingAsset)
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppTextField {
                id: accountNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Название счёта")
            }
            AppComboBox {
                id: accountTypeBox
                Layout.fillWidth: true
                model: accountDialog.accountTypes
                textRole: "label"
            }
            AppComboBox {
                id: accountCurrencyBox
                Layout.fillWidth: true
                model: ["RUB", "USD", "EUR"]
                enabled: !accountDialog.editingId
                      || !accountDialog.editingAccount
                      || accountDialog.editingAccount.transactionCount === 0
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.editingId
                      && accountDialog.editingAccount
                      && accountDialog.editingAccount.transactionCount > 0
                text: qsTr("Валюту счёта с операциями изменить нельзя")
                color: root.muted
                font.pixelSize: 14
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.creditCardSelected
                text: qsTr("Кредитный лимит")
                color: root.muted
                font.pixelSize: 14
            }
            AppTextField {
                id: accountCreditLimitField
                Layout.fillWidth: true
                visible: accountDialog.creditCardSelected
                placeholderText: qsTr("Кредитный лимит")
                validator: DoubleValidator {
                    bottom: 0.01
                    top: 999999999
                    decimals: 2
                }
            }
            Text {
                Layout.fillWidth: true
                text: accountDialog.creditCardSelected
                      ? qsTr("Задолженность на момент добавления")
                      : accountDialog.depositSelected
                        ? qsTr("Сумма вклада")
                      : qsTr("Начальный баланс")
                color: root.muted
                font.pixelSize: 14
            }
            AppTextField {
                id: accountBalanceField
                Layout.fillWidth: true
                placeholderText: accountDialog.creditCardSelected
                                 ? qsTr("Задолженность на момент добавления")
                                 : accountDialog.depositSelected
                                   ? qsTr("Сумма вклада")
                                 : qsTr("Начальный баланс")
                validator: DoubleValidator {
                    bottom: accountDialog.creditCardSelected
                            || accountDialog.depositSelected ? 0 : -999999999
                    top: 999999999
                    decimals: 2
                }
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.depositSelected
                text: qsTr("Годовая ставка, %")
                color: root.muted
                font.pixelSize: 14
            }
            AppTextField {
                id: depositRateField
                Layout.fillWidth: true
                visible: accountDialog.depositSelected
                placeholderText: qsTr("Например, 13")
                validator: DoubleValidator {
                    bottom: 0.01
                    top: 1000
                    decimals: 2
                }
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.depositSelected
                text: qsTr("Начисление процентов")
                color: root.muted
                font.pixelSize: 14
            }
            AppComboBox {
                id: depositPayoutBox
                Layout.fillWidth: true
                visible: accountDialog.depositSelected
                model: accountDialog.depositPayoutTypes
                textRole: "label"
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.monthlyDepositSelected
                text: qsTr("День выплаты")
                color: root.muted
                font.pixelSize: 14
            }
            AppTextField {
                id: depositPayoutDayField
                Layout.fillWidth: true
                visible: accountDialog.monthlyDepositSelected
                placeholderText: qsTr("От 1 до 31")
                validator: IntValidator { bottom: 1; top: 31 }
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.creditCardSelected
                text: qsTr("Кредитный лимит не считается активом. Расходы увеличивают задолженность, а перевод на кредитку её погашает.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text {
                id: accountError
                color: root.red
                font.pixelSize: 14
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: accountCancelButton
                    text: qsTr("Отмена")
                    onClicked: accountDialog.close()
                }
                SoftButton {
                    id: accountSubmitButton
                    text: accountDialog.editingId ? qsTr("Сохранить") : qsTr("Добавить")
                    highlighted: true
                    onClicked: accountDialog.submit()
                }
            }
        }
    }

    Menu {
        id: categoryContextMenu
        width: 224
        padding: 6
        property var categoryData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        AppMenuItem {
            width: categoryContextMenu.availableWidth
            text: qsTr("Редактировать")
            enabled: categoryContextMenu.categoryData !== null
            onTriggered: {
                if (categoryContextMenu.categoryData)
                    categoryDialog.openForEdit(categoryContextMenu.categoryData);
            }
        }

        MenuSeparator {
            width: categoryContextMenu.availableWidth
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.line
            }
        }

        AppMenuItem {
            width: categoryContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: categoryContextMenu.categoryData !== null
            onTriggered: {
                if (categoryContextMenu.categoryData)
                    deleteCategoryDialog.openFor(categoryContextMenu.categoryData);
            }
        }

        onClosed: categoryData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Dialog {
        id: deleteCategoryDialog
        width: 460
        modal: true
        anchors.centerIn: parent
        padding: 24
        closePolicy: Popup.CloseOnEscape
        property var categoryData: null

        function openFor(row) {
            categoryData = row;
            deleteCategoryError.text = "";
            open();
        }

        function confirmDelete() {
            const ok = categoryData
                    && financeController.deleteCategory(categoryData.value);
            if (ok)
                close();
            else
                deleteCategoryError.text = qsTr("Не удалось удалить категорию");
        }

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: deleteCategoryDialog.visible
                  && !deleteCategoryCancelButton.activeFocus
                  && !deleteCategorySubmitButton.activeFocus
            onActivated: deleteCategoryDialog.confirmDelete()
        }

        onClosed: categoryData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: qsTr("Удалить категорию?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Категория исчезнет из доступных для новых операций. Уже созданные операции сохранятся.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: 54
                color: root.soft

                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    text: deleteCategoryDialog.categoryData
                          ? deleteCategoryDialog.categoryData.label
                          : ""
                    color: root.accent
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            Text {
                id: deleteCategoryError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: deleteCategoryCancelButton
                    text: qsTr("Отмена")
                    onClicked: deleteCategoryDialog.close()
                }
                SoftButton {
                    id: deleteCategorySubmitButton
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteCategoryDialog.confirmDelete()
                }
            }
        }
    }

    Menu {
        id: budgetContextMenu
        width: 224
        padding: 6
        property var budgetData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        AppMenuItem {
            width: budgetContextMenu.availableWidth
            text: qsTr("Редактировать")
            enabled: budgetContextMenu.budgetData !== null
            onTriggered: {
                if (budgetContextMenu.budgetData)
                    budgetDialog.openForEdit(budgetContextMenu.budgetData);
            }
        }

        MenuSeparator {
            width: budgetContextMenu.availableWidth
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.line
            }
        }

        AppMenuItem {
            width: budgetContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: budgetContextMenu.budgetData !== null
            onTriggered: {
                if (budgetContextMenu.budgetData)
                    deleteBudgetDialog.openFor(budgetContextMenu.budgetData);
            }
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Menu {
        id: projectContextMenu
        width: 224
        padding: 6
        property var projectData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        AppMenuItem {
            width: projectContextMenu.availableWidth
            text: qsTr("Редактировать")
            enabled: projectContextMenu.projectData !== null
            onTriggered: {
                if (projectContextMenu.projectData)
                    projectDialog.openForEdit(projectContextMenu.projectData);
            }
        }

        MenuSeparator {
            width: projectContextMenu.availableWidth
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.line
            }
        }

        AppMenuItem {
            width: projectContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: projectContextMenu.projectData !== null
            onTriggered: {
                if (projectContextMenu.projectData)
                    deleteProjectDialog.openFor(projectContextMenu.projectData);
            }
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Menu {
        id: accountContextMenu
        width: 224
        padding: 6
        property var accountData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        AppMenuItem {
            width: accountContextMenu.availableWidth
            text: qsTr("Редактировать")
            visible: accountContextMenu.accountData !== null
            enabled: visible
            onTriggered: {
                if (accountContextMenu.accountData) {
                    if (accountContextMenu.accountData.isCrypto) cryptoWalletDialog.openForEdit(accountContextMenu.accountData);
                    else accountDialog.openForEdit(accountContextMenu.accountData);
                }
            }
        }

        MenuSeparator {
            width: accountContextMenu.availableWidth
            visible: accountContextMenu.accountData !== null
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.line
            }
        }

        AppMenuItem {
            width: accountContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: accountContextMenu.accountData !== null
            onTriggered: {
                if (accountContextMenu.accountData)
                    deleteAccountDialog.openFor(accountContextMenu.accountData);
            }
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Dialog {
        id: deleteAccountDialog
        width: 460
        modal: true
        anchors.centerIn: parent
        padding: 24
        closePolicy: Popup.CloseOnEscape
        property var accountData: null

        function openFor(row) {
            accountData = row;
            deleteAccountError.text = "";
            open();
        }

        function confirmDelete() {
            const row = accountData;
            const ok = row && row.isCrypto
                     ? financeController.deleteCryptoWallet(row.id)
                     : row && financeController.deleteAccount(row.id);
            if (ok)
                close();
            else
                deleteAccountError.text = row && row.isCrypto
                                        ? qsTr("Не удалось удалить криптовалюту")
                                        : qsTr("Не удалось удалить счёт");
        }

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: deleteAccountDialog.visible
                  && !deleteAccountCancelButton.activeFocus
                  && !deleteAccountSubmitButton.activeFocus
            onActivated: deleteAccountDialog.confirmDelete()
        }

        onClosed: accountData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: deleteAccountDialog.accountData
                      && deleteAccountDialog.accountData.isCrypto
                      ? qsTr("Удалить криптовалюту?")
                      : qsTr("Удалить счёт?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: deleteAccountDialog.accountData
                      && deleteAccountDialog.accountData.isCrypto
                      ? qsTr("Публичный адрес и сохранённые данные баланса и истории будут удалены. Средства в блокчейне это не затронет.")
                      : deleteAccountDialog.accountData
                      && deleteAccountDialog.accountData.transactionCount > 0
                      ? qsTr("Счёт и все связанные операции будут удалены. Связанные переводы удалятся целиком. Это действие нельзя отменить.")
                      : qsTr("Счёт будет удалён. Это действие нельзя отменить.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: 78
                color: root.soft

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: deleteAccountDialog.accountData
                              ? deleteAccountDialog.accountData.name
                              : ""
                        color: root.accent
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: deleteAccountDialog.accountData
                              ? root.accountCompactAmount(
                                    deleteAccountDialog.accountData
                                )
                                + (deleteAccountDialog.accountData.isCrypto
                                   ? ""
                                   : " · " + qsTr("операций: ")
                                     + deleteAccountDialog.accountData.transactionCount)
                              : ""
                        color: root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                id: deleteAccountError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: deleteAccountCancelButton
                    text: qsTr("Отмена")
                    onClicked: deleteAccountDialog.close()
                }
                SoftButton {
                    id: deleteAccountSubmitButton
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteAccountDialog.confirmDelete()
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: root.activeHistorySelection !== null
                 && root.activeHistorySelection.selectedRows.length > 0
                 && !root.modalDialogVisible()
                 && !browserScroll.active
        onActivated: {
            root.activeHistorySelection.clear();
            historySelectionMenu.close();
            transactionContextMenu.close();
        }
    }

    Menu {
        id: historySelectionMenu
        objectName: "historySelectionMenu"
        property var selection: null
        width: 224
        padding: 6
        AppMenuItem {
            text: qsTr("Отменить")
            onTriggered: {
                if (historySelectionMenu.selection) historySelectionMenu.selection.clear();
            }
        }
        AppMenuItem {
            text: qsTr("Удалить")
            destructive: true
            onTriggered: {
                deleteSelectedHistoryDialog.rows = historySelectionMenu.selection.selectedRows.slice();
                deleteSelectedHistoryDialog.errorText = "";
                deleteSelectedHistoryDialog.open();
            }
        }
        background: Rectangle {
            color: root.panel; radius: 12; border.color: root.line
            SurfaceShadow { }
        }
    }

    Dialog {
        id: deleteSelectedHistoryDialog
        objectName: "deleteSelectedHistoryDialog"
        property var rows: []
        property string errorText: ""
        modal: true
        anchors.centerIn: parent
        width: 440
        padding: 24
        title: qsTr("Удалить выделенные операции?")
        background: Rectangle {
            color: root.panel; radius: 16; border.color: root.line
            SurfaceShadow { }
        }
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: qsTr("Выделено операций: %1").arg(deleteSelectedHistoryDialog.rows.length); color: root.accent; font.pixelSize: 14 }
            Text { Layout.fillWidth: true; text: qsTr("Переводы будут удалены вместе с парной операцией."); color: root.muted; font.pixelSize: 14; wrapMode: Text.WordWrap }
            Text { visible: text.length > 0; text: deleteSelectedHistoryDialog.errorText; color: root.red; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 16
                SoftButton { text: qsTr("Отмена"); onClicked: deleteSelectedHistoryDialog.close() }
                SoftButton {
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: {
                        if (financeController.deleteHistoryRows(deleteSelectedHistoryDialog.rows)) {
                            if (historySelectionMenu.selection) historySelectionMenu.selection.clear();
                            deleteSelectedHistoryDialog.close();
                        } else deleteSelectedHistoryDialog.errorText = qsTr("Не удалось удалить операции. Данные не изменены.");
                    }
                }
            }
        }
    }

    Menu {
        id: transactionContextMenu
        width: 224
        padding: 6
        property var transactionData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        AppMenuItem {
            width: transactionContextMenu.availableWidth
            text: qsTr("Редактировать")
            enabled: transactionContextMenu.transactionData !== null
                  && !transactionContextMenu.transactionData.transactionId
            onTriggered: {
                const row = transactionContextMenu.transactionData;
                if (!row)
                    return;
                if (row.type === "investment_position")
                    investmentPositionDialog.openForEdit(row);
                else
                    operationDialog.openForEdit(row);
            }
        }

        MenuSeparator {
            width: transactionContextMenu.availableWidth
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.line
            }
        }

        AppMenuItem {
            width: transactionContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: transactionContextMenu.transactionData !== null
            onTriggered: {
                const row = transactionContextMenu.transactionData;
                if (!row) return;
                if (row.transactionId) {
                    deleteSelectedHistoryDialog.rows = [row];
                    deleteSelectedHistoryDialog.errorText = "";
                    deleteSelectedHistoryDialog.open();
                } else deleteTransactionDialog.openFor(row);
            }
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Dialog {
        id: deleteTransactionDialog
        width: 460
        modal: true
        anchors.centerIn: parent
        padding: 24
        closePolicy: Popup.CloseOnEscape
        property var transactionData: null

        function openFor(row) {
            transactionData = row;
            deleteTransactionError.text = "";
            open();
        }

        function confirmDelete() {
            const row = transactionData;
            const ok = row && row.type === "investment_position"
                     ? financeController.deleteInvestmentPosition(row.id)
                     : row && financeController.deleteTransaction(row.id);
            if (ok)
                close();
            else
                deleteTransactionError.text = row
                    && row.type === "investment_position"
                    ? qsTr("Не удалось удалить инвестиционную позицию")
                    : qsTr("Не удалось удалить операцию");
        }

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: deleteTransactionDialog.visible
                  && !deleteTransactionCancelButton.activeFocus
                  && !deleteTransactionSubmitButton.activeFocus
            onActivated: deleteTransactionDialog.confirmDelete()
        }

        onClosed: transactionData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: deleteTransactionDialog.transactionData
                      && deleteTransactionDialog.transactionData.type
                         === "investment_position"
                      ? qsTr("Удалить инвестиционную позицию?")
                      : qsTr("Удалить операцию?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: deleteTransactionDialog.transactionData
                      && deleteTransactionDialog.transactionData.type === "transfer"
                      ? qsTr("Будут удалены обе части перевода. Баланс и статистика будут пересчитаны.")
                      : deleteTransactionDialog.transactionData
                        && deleteTransactionDialog.transactionData.type
                           === "investment_position"
                        ? qsTr("Позиция будет удалена из списка и базы данных. Общая стоимость активов будет пересчитана.")
                      : qsTr("Это действие нельзя отменить. Баланс и статистика будут пересчитаны.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Panel {
                Layout.fillWidth: true
                implicitHeight: 78
                color: root.soft

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: deleteTransactionDialog.transactionData
                              ? (deleteTransactionDialog.transactionData.rawDescription
                                 || deleteTransactionDialog.transactionData.description
                                 || (deleteTransactionDialog.transactionData.type === "transfer"
                                     ? qsTr("Перевод")
                                     : deleteTransactionDialog.transactionData.type === "income"
                                         ? qsTr("Доход")
                                         : qsTr("Расход")))
                              : ""
                        color: root.accent
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: deleteTransactionDialog.transactionData
                              ? root.accountName(deleteTransactionDialog.transactionData.accountId)
                                + " · "
                                + root.money(
                                    root.transactionSignedAmount(deleteTransactionDialog.transactionData),
                                    deleteTransactionDialog.transactionData.currency,
                                    true
                                )
                              : ""
                        color: root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                id: deleteTransactionError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: deleteTransactionCancelButton
                    text: qsTr("Отмена")
                    onClicked: deleteTransactionDialog.close()
                }
                SoftButton {
                    id: deleteTransactionSubmitButton
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteTransactionDialog.confirmDelete()
                }
            }
        }
    }

    Dialog {
        id: projectDialog
        width: 460
        modal: true
        anchors.centerIn: parent
        padding: 24
        property string editingId: ""

        function openForNew() {
            editingId = "";
            projectNameField.clear();
            projectError.text = "";
            open();
            Qt.callLater(function() { projectNameField.forceActiveFocus(); });
        }

        function openForEdit(project) {
            if (!project)
                return;
            editingId = project.id;
            projectNameField.text = project.name;
            projectError.text = "";
            open();
            Qt.callLater(function() {
                projectNameField.forceActiveFocus();
                projectNameField.selectAll();
            });
        }

        function submit() {
            const ok = editingId.length > 0
                     ? financeController.renameProject(
                           editingId,
                           projectNameField.text
                       )
                     : financeController.addProject(projectNameField.text);
            if (ok)
                close();
            else
                projectError.text = qsTr(
                    "Укажите уникальное название проекта"
                );
        }

        onClosed: editingId = ""

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: projectDialog.editingId.length > 0
                      ? qsTr("Редактирование проекта")
                      : qsTr("Новый проект")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppTextField {
                id: projectNameField
                Layout.fillWidth: true
                placeholderText: qsTr("Название проекта")
                maximumLength: 80
                onAccepted: projectDialog.submit()
            }
            Text {
                id: projectError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: projectDialog.close()
                }
                SoftButton {
                    text: qsTr("Сохранить")
                    highlighted: true
                    onClicked: projectDialog.submit()
                }
            }
        }
    }

    Dialog {
        id: deleteProjectDialog
        width: 470
        modal: true
        anchors.centerIn: parent
        padding: 24
        property var projectData: null

        function openFor(project) {
            if (!project)
                return;
            projectData = project;
            deleteProjectError.text = "";
            open();
        }

        function confirmDelete() {
            if (projectData && financeController.deleteProject(projectData.id)) {
                close();
                return;
            }
            deleteProjectError.text = qsTr("Не удалось удалить проект");
        }

        onClosed: projectData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: qsTr("Удалить проект?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                text: deleteProjectDialog.projectData
                      ? deleteProjectDialog.projectData.name
                      : ""
                color: root.accent
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Операции останутся на своих счетах и продолжат учитываться в общем балансе. Проект будет скрыт из списка.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text {
                id: deleteProjectError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: deleteProjectDialog.close()
                }
                SoftButton {
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteProjectDialog.confirmDelete()
                }
            }
        }
    }

    Dialog {
        id: deleteBudgetDialog
        width: 470
        modal: true
        anchors.centerIn: parent
        padding: 24
        property var budgetData: null

        function openFor(budget) {
            if (!budget)
                return;
            budgetData = budget;
            deleteBudgetError.text = "";
            open();
        }

        function confirmDelete() {
            if (budgetData && financeController.deleteBudget(budgetData.id)) {
                close();
                return;
            }
            deleteBudgetError.text = qsTr("Не удалось удалить бюджет");
        }

        onClosed: budgetData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: qsTr("Удалить бюджет?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                text: deleteBudgetDialog.budgetData
                    ? deleteBudgetDialog.budgetData.name : ""
                color: root.accent
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("История операций и счета не изменятся. Будет удалён только бюджет и его месячные лимиты.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text {
                id: deleteBudgetError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: deleteBudgetDialog.close()
                }
                SoftButton {
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: deleteBudgetDialog.confirmDelete()
                }
            }
        }
    }

    Dialog {
        id: operationDialog
        width: 520
        modal: true
        anchors.centerIn: parent
        padding: 24
        property string editingId: ""
        property var editingTransaction: null
        property string editingSourceAccountId: ""
        property string editingTargetAccountId: ""
        property string editingProjectId: ""
        property date selectedDate: new Date()

        function selectDate(day) {
            const time = selectedDate;
            selectedDate = new Date(
                day.getFullYear(),
                day.getMonth(),
                day.getDate(),
                time.getHours(),
                time.getMinutes(),
                time.getSeconds(),
                time.getMilliseconds()
            );
        }

        function exactIndexByRole(model, role, value) {
            for (let i = 0; i < model.length; ++i)
                if (model[i][role] === value)
                    return i;
            return -1;
        }

        function selectTransferAccounts(sourceId, targetId) {
            const rows = financeController.allAccounts;
            let sourceIndex = exactIndexByRole(rows, "id", sourceId);
            if (sourceIndex < 0)
                sourceIndex = rows.length > 0 ? 0 : -1;
            operationAccount.currentIndex = sourceIndex;

            const resolvedSourceId = sourceIndex >= 0 ? rows[sourceIndex].id : "";
            let targetIndex = exactIndexByRole(rows, "id", targetId);
            if (targetIndex < 0 || rows[targetIndex].id === resolvedSourceId) {
                targetIndex = -1;
                for (let i = 0; i < rows.length; ++i) {
                    if (rows[i].id !== resolvedSourceId) {
                        targetIndex = i;
                        break;
                    }
                }
            }
            transferTargetAccount.currentIndex = targetIndex;
        }

        function openForNew(projectId) {
            editingId = "";
            editingTransaction = null;
            editingSourceAccountId = "";
            editingTargetAccountId = "";
            editingProjectId = projectId || "";
            operationError.text = "";
            operationAmount.clear();
            operationDescription.clear();
            selectedDate = new Date();
            operationType.currentIndex = 1;
            const availableAccounts = editingProjectId.length > 0
                                    ? financeController.allAccounts
                                    : financeController.accounts;
            operationAccount.currentIndex = root.indexByRole(
                availableAccounts,
                "id",
                financeController.selectedAccountId
            );
            if (operationAccount.currentIndex < 0
                && availableAccounts.length > 0)
                operationAccount.currentIndex = 0;
            operationCategory.currentIndex = 0;
            const source = operationAccount.currentIndex >= 0
                         ? financeController.accounts[operationAccount.currentIndex]
                         : null;
            const allAccounts = financeController.allAccounts;
            transferTargetAccount.currentIndex = -1;
            for (let i = 0; i < allAccounts.length; ++i) {
                if (!source || allAccounts[i].id !== source.id) {
                    transferTargetAccount.currentIndex = i;
                    break;
                }
            }
            open();
            Qt.callLater(function() { operationAmount.forceActiveFocus(); });
        }

        function openForEdit(row) {
            editingId = row.id;
            editingTransaction = row;
            editingProjectId = row.projectId || "";
            operationError.text = "";
            const restoredDate = new Date(row.date);
            selectedDate = isNaN(restoredDate.getTime()) ? new Date() : restoredDate;
            if (row.type === "transfer") {
                const details = financeController.transferDetails(row.id);
                editingSourceAccountId = details.sourceAccountId || "";
                editingTargetAccountId = details.targetAccountId || "";
                operationType.currentIndex = 2;
                selectTransferAccounts(
                    editingSourceAccountId,
                    editingTargetAccountId
                );
                operationAmount.text = root.amountForInput(details.sourceAmount || row.amount);
            } else {
                editingSourceAccountId = row.accountId;
                editingTargetAccountId = "";
                operationType.currentIndex = row.type === "income" ? 0 : 1;
                operationAccount.currentIndex = root.indexByRole(
                    editingProjectId.length > 0
                    ? financeController.allAccounts
                    : financeController.accounts,
                    "id",
                    row.accountId
                );
                operationCategory.currentIndex = root.indexByRole(
                    operationCategory.model,
                    "value",
                    row.categoryId
                );
                operationAmount.text = root.amountForInput(row.amount);
            }
            operationDescription.text = row.rawDescription || "";
            open();
            Qt.callLater(function() { operationAmount.forceActiveFocus(); });
        }

        function submit() {
            if (!financeController.allAccounts.length) {
                operationError.text = qsTr("Сначала добавьте счёт");
                return;
            }
            const isTransfer = operationType.currentIndex === 2;
            if (!operationAccount.model.length
                || operationAccount.currentIndex < 0) {
                operationError.text = qsTr("Для выбранной операции нет доступного счёта");
                return;
            }
            if (isTransfer && financeController.allAccounts.length < 2) {
                operationError.text = qsTr("Для перевода нужны два счёта");
                return;
            }
            if (isTransfer && transferTargetAccount.currentIndex < 0) {
                operationError.text = qsTr("Выберите счёт назначения");
                return;
            }
            if (!isTransfer && !operationCategory.model.length) {
                operationError.text = qsTr("Сначала добавьте категорию");
                return;
            }
            const account = operationAccount.model[operationAccount.currentIndex];
            const category = isTransfer
                         ? null
                         : operationCategory.model[operationCategory.currentIndex];
            const minor = Math.round(
                (Number(operationAmount.text.replace(",", ".")) || 0) * 100
            );
            const type = operationType.currentIndex === 0 ? "income"
                       : operationType.currentIndex === 1 ? "expense"
                       : "transfer";
            const targetAccount = isTransfer
                                ? transferTargetAccount.model[
                                      transferTargetAccount.currentIndex
                                  ]
                                : null;
            const ok = editingId
                     ? financeController.updateOperation(
                         editingId,
                         minor,
                         operationDescription.text,
                         isTransfer ? "" : category.value,
                         account.id,
                         type,
                         isTransfer ? targetAccount.id : "",
                         selectedDate
                     )
                     : isTransfer
                       ? financeController.addTransfer(
                           minor,
                           operationDescription.text,
                           account.id,
                           targetAccount.id,
                           selectedDate
                       )
                       : editingProjectId.length > 0
                         ? financeController.addProjectTransaction(
                             editingProjectId,
                             minor,
                             operationDescription.text,
                             category.value,
                             account.id,
                             type,
                             selectedDate
                         )
                       : type === "income"
                         ? financeController.addIncome(
                             minor,
                             operationDescription.text,
                             category.value,
                             account.currency,
                             account.id,
                             selectedDate
                         )
                         : financeController.addExpense(
                             minor,
                             operationDescription.text,
                             category.value,
                             account.currency,
                             account.id,
                             selectedDate
                         );
            if (ok)
                close();
            else
                operationError.text = isTransfer
                    ? qsTr("Проверьте сумму и выбранные счета")
                    : qsTr("Проверьте сумму, счёт и категорию");
        }

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.ApplicationShortcut
            enabled: operationDialog.visible
                  && !operationDateDialog.visible
                  && !operationType.activeFocus
                  && !operationType.popup.visible
                  && !operationAccount.activeFocus
                  && !operationAccount.popup.visible
                  && !transferTargetAccount.activeFocus
                  && !transferTargetAccount.popup.visible
                  && !operationCategory.activeFocus
                  && !operationCategory.popup.visible
                  && !operationDateButton.activeFocus
                  && !operationCancelButton.activeFocus
                  && !operationSubmitButton.activeFocus
            onActivated: operationDialog.submit()
        }

        onClosed: {
            editingId = "";
            editingTransaction = null;
            editingSourceAccountId = "";
            editingTargetAccountId = "";
            editingProjectId = "";
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: operationDialog.editingId
                      ? qsTr("Редактирование операции")
                      : operationDialog.editingProjectId.length > 0
                        ? qsTr("Новая операция проекта · %1").arg(
                              root.projectName(operationDialog.editingProjectId)
                          )
                        : qsTr("Новая операция")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppComboBox {
                id: operationType
                Layout.fillWidth: true
                model: operationDialog.editingProjectId.length > 0
                     ? [qsTr("Доход"), qsTr("Расход")]
                     : [qsTr("Доход"), qsTr("Расход"), qsTr("Перевод")]
                onActivated: {
                    operationCategory.currentIndex = 0;
                    if (!operationDialog.editingTransaction)
                        return;

                    if (currentIndex === 2) {
                        operationDialog.selectTransferAccounts(
                            operationDialog.editingSourceAccountId,
                            operationDialog.editingTargetAccountId
                        );
                    } else {
                        operationAccount.currentIndex = root.indexByRole(
                            operationDialog.editingProjectId.length > 0
                            ? financeController.allAccounts
                            : financeController.accounts,
                            "id",
                            operationDialog.editingTransaction.accountId
                        );
                    }
                }
            }
            Text {
                visible: operationType.currentIndex === 2
                text: qsTr("Откуда")
                color: root.muted
                font.pixelSize: 14
            }
            AppComboBox {
                id: operationAccount
                Layout.fillWidth: true
                model: operationDialog.editingProjectId.length > 0
                     ? financeController.allAccounts
                     : operationType.currentIndex === 2
                     ? financeController.allAccounts
                     : financeController.accounts
                textRole: operationDialog.editingProjectId.length > 0
                        || operationType.currentIndex === 2
                        ? "displayName" : "name"
                enabled: count > 0
                emptyText: qsTr("Добавьте счёт")
            }
            Text {
                visible: operationType.currentIndex === 2
                text: qsTr("Куда")
                color: root.muted
                font.pixelSize: 14
            }
            AppComboBox {
                id: transferTargetAccount
                Layout.fillWidth: true
                visible: operationType.currentIndex === 2
                model: financeController.allAccounts
                textRole: "displayName"
                enabled: count > 0
                emptyText: qsTr("Добавьте счёт")
            }
            AppComboBox {
                id: operationCategory
                Layout.fillWidth: true
                visible: operationType.currentIndex !== 2
                model: {
                    const selectedType = operationType.currentIndex === 0
                                       ? "income"
                                       : "expense";
                    const result = financeController.categories.filter(function (category) {
                        return category.type === selectedType;
                    });
                    const editing = operationDialog.editingTransaction;
                    if (editing && editing.type === selectedType) {
                        let found = false;
                        for (let i = 0; i < result.length; ++i)
                            found = found || result[i].value === editing.categoryId;
                        if (!found)
                            result.push({
                                label: editing.categoryName,
                                value: editing.categoryId,
                                type: editing.type
                            });
                    }
                    return result;
                }
                textRole: "label"
                enabled: count > 0
                emptyText: qsTr("Добавьте категорию")
            }
            AppTextField {
                id: operationAmount
                Layout.fillWidth: true
                placeholderText: qsTr("Сумма")
                validator: DoubleValidator {
                    bottom: 0.01
                    top: 999999999
                    decimals: 2
                }
            }
            AppTextField {
                id: operationDescription
                Layout.fillWidth: true
                placeholderText: qsTr("Описание")
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Дата операции")
                    color: root.muted
                    font.pixelSize: 14
                }

                Button {
                    id: operationDateButton
                    Layout.fillWidth: true
                    implicitHeight: 44
                    hoverEnabled: true
                    text: Qt.formatDate(
                        operationDialog.selectedDate,
                        "dd.MM.yyyy"
                    )
                    onClicked: operationDateDialog.openFor(operationDialog.selectedDate)

                    contentItem: RowLayout {
                        spacing: 10
                        Text {
                            Layout.fillWidth: true
                            text: operationDateButton.text
                            color: root.accent
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                        }
                        Text {
                            text: "▦"
                            color: root.navSelected
                            font.pixelSize: 18
                        }
                    }

                    background: Rectangle {
                        SurfaceShadow { }

                        radius: 11
                        color: operationDateButton.down
                               ? root.panel
                               : operationDateButton.hovered
                                 ? root.controlHovered
                                 : root.soft
                        border.width: operationDateButton.activeFocus ? 2 : 1
                        border.color: operationDateButton.activeFocus
                                      ? root.navSelected
                                      : root.line
                    }
                }
            }
            Text {
                id: operationError
                color: root.red
                font.pixelSize: 14
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    id: operationCancelButton
                    text: qsTr("Отмена")
                    onClicked: operationDialog.close()
                }
                SoftButton {
                    id: operationSubmitButton
                    text: qsTr("Сохранить")
                    highlighted: true
                    onClicked: operationDialog.submit()
                }
            }
        }
    }

    Dialog {
        id: dateFilterDialog
        width: 450
        modal: true
        anchors.centerIn: parent
        padding: 22
        closePolicy: Popup.CloseOnEscape

        property var pendingFrom: null
        property var pendingTo: null
        property int displayedMonth: new Date().getMonth()
        property int displayedYear: new Date().getFullYear()

        function normalizedDate(date) {
            return new Date(
                date.getFullYear(),
                date.getMonth(),
                date.getDate(),
                12, 0, 0, 0
            );
        }

        function openForCurrent() {
            const now = normalizedDate(new Date());
            pendingFrom = financeController.dateFilterActive
                ? root.dateFromIso(financeController.dateFilterFrom)
                : new Date(now.getFullYear(), now.getMonth(), 1, 12);
            pendingTo = financeController.dateFilterActive
                ? root.dateFromIso(financeController.dateFilterTo)
                : now;
            displayedMonth = pendingFrom.getMonth();
            displayedYear = pendingFrom.getFullYear();
            open();
        }

        function shiftMonth(offset) {
            const shifted = new Date(displayedYear, displayedMonth + offset, 1);
            displayedMonth = shifted.getMonth();
            displayedYear = shifted.getFullYear();
        }

        function isSameDay(left, right) {
            return left && right
                && left.getFullYear() === right.getFullYear()
                && left.getMonth() === right.getMonth()
                && left.getDate() === right.getDate();
        }

        function isInsideRange(date) {
            if (!pendingFrom || !pendingTo)
                return false;
            const value = normalizedDate(date).getTime();
            return value >= pendingFrom.getTime()
                && value <= pendingTo.getTime();
        }

        function chooseDate(date) {
            const chosen = normalizedDate(date);
            if (!pendingFrom || pendingTo) {
                pendingFrom = chosen;
                pendingTo = null;
            } else if (chosen.getTime() < pendingFrom.getTime()) {
                pendingTo = pendingFrom;
                pendingFrom = chosen;
            } else {
                pendingTo = chosen;
            }
        }

        function applyRange(from, to) {
            financeController.setDateFilter(
                normalizedDate(from),
                normalizedDate(to)
            );
            close();
        }

        function applyCurrentSelection() {
            if (!pendingFrom)
                return;
            applyRange(pendingFrom, pendingTo || pendingFrom);
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: qsTr("Период")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                SoftButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Этот месяц")
                    onClicked: {
                        const to = dateFilterDialog.normalizedDate(new Date());
                        dateFilterDialog.applyRange(
                            new Date(to.getFullYear(), to.getMonth(), 1, 12),
                            to
                        );
                    }
                }
                SoftButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Этот год")
                    onClicked: {
                        const to = dateFilterDialog.normalizedDate(new Date());
                        dateFilterDialog.applyRange(
                            new Date(to.getFullYear(), 0, 1, 12),
                            to
                        );
                    }
                }
                SoftButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Все время")
                    onClicked: {
                        financeController.clearDateFilter();
                        dateFilterDialog.close();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SoftButton {
                    Layout.preferredWidth: 44
                    text: "‹"
                    onClicked: dateFilterDialog.shiftMonth(-1)
                }

                Text {
                    Layout.fillWidth: true
                    text: dateFilterCalendar.title
                    color: root.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                SoftButton {
                    Layout.preferredWidth: 44
                    text: "›"
                    onClicked: dateFilterDialog.shiftMonth(1)
                }
            }

            DayOfWeekRow {
                Layout.fillWidth: true
                locale: root.uiLocale()

                delegate: Text {
                    required property string shortName
                    text: shortName
                    color: root.muted
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MonthGrid {
                id: dateFilterCalendar
                Layout.fillWidth: true
                Layout.preferredHeight: 258
                month: dateFilterDialog.displayedMonth
                year: dateFilterDialog.displayedYear
                locale: root.uiLocale()

                delegate: Button {
                    id: filterDayButton
                    required property var model
                    flat: true
                    hoverEnabled: true
                    opacity: model.month === dateFilterCalendar.month ? 1 : 0.42
                    onClicked: dateFilterDialog.chooseDate(model.date)

                    readonly property bool rangeEdge:
                        dateFilterDialog.isSameDay(model.date, dateFilterDialog.pendingFrom)
                        || dateFilterDialog.isSameDay(model.date, dateFilterDialog.pendingTo)

                    contentItem: Text {
                        text: filterDayButton.model.day
                        color: filterDayButton.rangeEdge ? root.white : root.accent
                        font.pixelSize: 14
                        font.weight: filterDayButton.model.today
                                     ? Font.DemiBold
                                     : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        SurfaceShadow { }

                        radius: 9
                        color: filterDayButton.rangeEdge
                               ? root.accent
                               : dateFilterDialog.isInsideRange(filterDayButton.model.date)
                                 ? root.pale
                                 : filterDayButton.hovered
                                   ? root.controlHovered
                                   : root.transparentColor
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: (dateFilterDialog.pendingFrom && dateFilterDialog.pendingTo)
                    ? Qt.formatDate(dateFilterDialog.pendingFrom, "dd.MM.yyyy") + " — " + Qt.formatDate(dateFilterDialog.pendingTo, "dd.MM.yyyy")
                    : ""
                color: root.muted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: dateFilterDialog.close()
                }
                SoftButton {
                    text: qsTr("Применить")
                    highlighted: true
                    enabled: dateFilterDialog.pendingFrom !== null
                    onClicked: dateFilterDialog.applyCurrentSelection()
                }
            }
        }
    }

    Dialog {
        id: operationDateDialog
        width: 400
        modal: true
        anchors.centerIn: parent
        padding: 22
        closePolicy: Popup.CloseOnEscape
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

        function isSameDay(left, right) {
            return left.getFullYear() === right.getFullYear()
                && left.getMonth() === right.getMonth()
                && left.getDate() === right.getDate();
        }

        function chooseDate(date) {
            operationDialog.selectDate(date);
            close();
        }

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: qsTr("Дата операции")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                SoftButton {
                    Layout.preferredWidth: 44
                    text: "‹"
                    onClicked: operationDateDialog.shiftMonth(-1)
                }

                Text {
                    Layout.fillWidth: true
                    text: operationCalendar.title
                    color: root.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                SoftButton {
                    Layout.preferredWidth: 44
                    text: "›"
                    onClicked: operationDateDialog.shiftMonth(1)
                }
            }

            DayOfWeekRow {
                Layout.fillWidth: true
                locale: root.uiLocale()

                delegate: Text {
                    required property string shortName
                    text: shortName
                    color: root.muted
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MonthGrid {
                id: operationCalendar
                Layout.fillWidth: true
                Layout.preferredHeight: 258
                month: operationDateDialog.displayedMonth
                year: operationDateDialog.displayedYear
                locale: root.uiLocale()

                delegate: Button {
                    id: dayButton
                    required property var model
                    flat: true
                    hoverEnabled: true
                    opacity: model.month === operationCalendar.month ? 1 : 0.42
                    onClicked: operationDateDialog.chooseDate(model.date)

                    contentItem: Text {
                        text: dayButton.model.day
                        color: operationDateDialog.isSameDay(
                                   dayButton.model.date,
                                   operationDialog.selectedDate
                               )
                               ? root.white
                               : root.accent
                        font.pixelSize: 14
                        font.weight: dayButton.model.today ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        SurfaceShadow { }

                        radius: 9
                        color: operationDateDialog.isSameDay(
                                   dayButton.model.date,
                                   operationDialog.selectedDate
                               )
                               ? root.accent
                               : dayButton.hovered
                                 ? root.controlHovered
                                 : dayButton.model.today
                                   ? root.pale
                                   : root.transparentColor
                        border.width: dayButton.model.today
                                      && !operationDateDialog.isSameDay(
                                          dayButton.model.date,
                                          operationDialog.selectedDate
                                      ) ? 1 : 0
                        border.color: root.accentSoft
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: qsTr("Сегодня")
                    onClicked: operationDateDialog.chooseDate(new Date())
                }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: operationDateDialog.close()
                }
            }
        }
    }

    Dialog {
        id: categoryDialog
        width: 560
        modal: true
        anchors.centerIn: parent
        padding: 22
        property var editingData: null
        readonly property string editingId: editingData ? editingData.value : ""

        function openForManagement() {
            editingData = null;
            categoryNameField.clear();
            categoryType.currentIndex = 0;
            categoryError.text = "";
            open();
        }

        function openForEdit(row) {
            editingData = row;
            categoryNameField.text = row.label || "";
            categoryType.currentIndex = row.type === "expense" ? 1 : 0;
            categoryError.text = "";
            open();
            categoryNameField.forceActiveFocus();
            categoryNameField.selectAll();
        }

        function submit() {
            const ok = editingId.length > 0
                    ? financeController.renameCategory(
                          editingId,
                          categoryNameField.text
                      )
                    : financeController.addCategory(
                          categoryNameField.text,
                          categoryType.currentIndex === 0 ? "income" : "expense"
                      );
            if (ok) {
                if (editingId.length > 0) {
                    close();
                    return;
                }
                categoryNameField.clear();
                categoryError.text = "";
            } else {
                categoryError.text = qsTr("Не удалось сохранить категорию");
            }
        }

        onClosed: editingData = null

        background: Rectangle {
            SurfaceShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: categoryDialog.editingId.length > 0
                      ? qsTr("Изменить категорию")
                      : qsTr("Категории")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            RowLayout {
                Layout.fillWidth: true
                AppTextField {
                    id: categoryNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Название категории")
                    onAccepted: categoryDialog.submit()
                }
                AppComboBox {
                    id: categoryType
                    model: [qsTr("Доход"), qsTr("Расход")]
                    enabled: categoryDialog.editingId.length === 0
                }
            }
            Text {
                id: categoryError
                color: root.red
                font.pixelSize: 14
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: qsTr("Закрыть")
                    onClicked: categoryDialog.close()
                }
                SoftButton {
                    text: qsTr("Сохранить")
                    highlighted: true
                    onClicked: categoryDialog.submit()
                }
            }
        }
    }
}
