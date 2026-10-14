import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 390
    height: 844
    minimumWidth: 320
    minimumHeight: 560
    visible: true
    title: "Vexa"
    color: root.canvas
    font.pixelSize: 14

    // Palette values live in MobileTheme so the pages and the shell share one source.
    readonly property color canvas: MobileTheme.canvas
    readonly property color panel: MobileTheme.panel
    readonly property color soft: MobileTheme.soft
    readonly property color line: MobileTheme.line
    readonly property color accent: MobileTheme.accent
    readonly property color muted: MobileTheme.muted
    readonly property color accentSoft: MobileTheme.accentSoft
    readonly property color pale: MobileTheme.pale
    readonly property color paleText: "#E9EDF6"
    readonly property color red: MobileTheme.red
    readonly property color navSelected: MobileTheme.navSelected
    readonly property color navHovered: "#142B48"

    readonly property color chartAccent1: MobileTheme.chartColors[0]
    readonly property color chartAccent2: MobileTheme.chartColors[1]
    readonly property color chartAccent3: MobileTheme.chartColors[2]
    readonly property color chartAccent4: MobileTheme.chartColors[3]
    readonly property color chartAccent5: MobileTheme.chartColors[4]
    readonly property var chartColors: [
        chartAccent1,
        chartAccent2,
        chartAccent3,
        chartAccent4,
        chartAccent5
    ]

    readonly property color tableHeader: MobileTheme.tableHeader
    readonly property color tableRowAlt: "#FAF9EC"
    readonly property color categoryRow: "#FFFFF0"
    readonly property color categoryEditRow: "#F4F3E3"
    readonly property color controlHovered: MobileTheme.hover
    readonly property int chartPriceLevels: 7

    readonly property color incomePanel: MobileTheme.incomePanel
    readonly property color expensePanel: MobileTheme.expensePanel
    readonly property color income: MobileTheme.income

    readonly property color white: MobileTheme.accentText
    readonly property color transparentColor: "transparent"

    property string page: "overview"
    onPageChanged: {
        browserScroll.stop();
        Qt.inputMethod.hide();
    }
    readonly property var scrollController: browserScroll
    AutoScrollController { id: browserScroll; parent: root.contentItem }
    property string searchText: ""
    property bool hideAmounts: false
    readonly property int pageMargin: MobileTheme.margin
    readonly property int touchHeight: 48
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

    function symbol(code) { return MobileFormat.symbol(code); }
    function uiLocale() { return MobileFormat.locale(); }
    function money(minor, code, sign) { return MobileFormat.money(minor, code, sign); }

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
        if (row && row.isCrypto)
            return row.balanceText + " " + row.symbol;
        if (row && row.isCreditCard)
            return qsTr("Задолженность: %1").arg(
                root.money(row.debtMinor, row.currency, false)
            );
        return row ? root.money(row.balanceMinor, row.currency, false) : "";
    }

    function accountCompactAmount(row) {
        if (row && row.isCrypto)
            return row.balanceText + " " + row.symbol
                 + " · ≈ " + root.money(row.valueMinor, financeController.appCurrency, false);
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

    function cryptoWalletStatus(row) {
        if (!row || !row.isCrypto)
            return "";
        if (row.refreshing)
            return qsTr("Обновление баланса…");
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
        if (row.type === "investment_cash") return row.amount;
        return row.type === "income" || (row.type === "transfer" && row.direction === "in")
             ? row.amount : -row.amount;
    }

    function transactionTypeLabel(row) {
        if (row.type === "investment_cash") return qsTr("Инвестиция");
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

    function transactionSummary(row) {
        if (row.recipientSource)
            return row.recipientName || qsTr("Получатель не определён");
        return row.rawDescription || row.description || "—";
    }

    function visibleTransactions() {
        const result = [], ids = {}, accounts = financeController.accounts;
        for (let i = 0; i < accounts.length; ++i)
            ids[accounts[i].id] = true;
        const rows = financeController.transactions.concat(financeController.selectedAsset === "investment" ? financeController.investmentOperations : []), query = searchText.trim().toLowerCase();
        for (let j = 0; j < rows.length; ++j) {
            const row = rows[j];
            if (!ids[row.accountId])
                continue;
            if (financeController.selectedAccountId && row.accountId !== financeController.selectedAccountId)
                continue;
            const text = ((row.description || row.rawDescription || "") + " " + (row.recipientName || "") + " " + row.categoryName + " " + accountName(row.accountId)).toLowerCase();
            if (!query || text.indexOf(query) >= 0)
                result.push(row);
        }
        result.sort(function(a, b) { return new Date(b.date).getTime() - new Date(a.date).getTime(); });
        return result;
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

    // Neobrutalism shadow: a solid accent copy of the shape 4 px below, without blur (as in the Figma design)
    component HardShadow: Rectangle {
        x: 0
        y: 4
        z: -1
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        radius: parent ? parent.radius : 0
        color: root.accent
    }

    component Panel: Rectangle {
        HardShadow { }

        color: root.panel
        radius: 16
        border.width: 1
        border.color: root.accent
    }

    // Item.clip and ListView.clip are rectangular. These masks cover content
    // that would otherwise remain visible outside the rounded bottom corners.
    component BottomCornerMask: Canvas {
        id: cornerMask
        property bool mirrored: false

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
        }
    }
    component SoftButton: StyledButton {
        primary: highlighted
        controlHeight: root.touchHeight
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
        implicitHeight: root.touchHeight
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
        controlHeight: root.touchHeight
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
        controlHeight: root.touchHeight
        appTextColor: root.accent
        appMutedColor: root.muted
        appPanelColor: root.panel
        appSoftColor: root.soft
        appLineColor: root.line
        appAccentColor: root.navSelected
        appHoverColor: root.controlHovered
    }
    readonly property var tabItems: [
        { key: "overview", title: qsTr("Обзор"), svg: "qrc:/icons/dashboard.svg" },
        { key: "accounts", title: qsTr("Счета"), icon: "wallet" },
        { key: "add", title: qsTr("Операция"), icon: "plus", disabled: financeController.selectedAsset !== "fiat" },
        { key: "analytics", title: qsTr("Аналитика"), svg: "qrc:/icons/analytics.svg" },
        { key: "more", title: qsTr("Ещё"), icon: "more" }
    ]
    readonly property var pageTitles: ({
        "overview": qsTr("Мои финансы"),
        "accounts": qsTr("Счета"),
        "operations": qsTr("Операции"),
        "analytics": qsTr("Аналитика"),
        "more": qsTr("Ещё"),
        "categories": qsTr("Категории"),
        "budgets": qsTr("Бюджеты"),
        "goals": qsTr("Цели"),
        "trajectory": qsTr("Траектория"),
        "scheduled": qsTr("Регулярные операции"),
        "projects": qsTr("Проекты"),
        "notes": qsTr("Заметки"),
        "settings": qsTr("Настройки"),
        "data": qsTr("Импорт и экспорт")
    })
    readonly property var globalFilterPages: ["overview", "accounts", "operations", "analytics"]
    readonly property bool globalFilters: globalFilterPages.indexOf(page) >= 0
    readonly property string tabKey: globalFilters ? page : "more"
    property var pageHistory: []

    function pageTitle() {
        if (page === "project") {
            const rows = financeController.projects;
            for (let i = 0; i < rows.length; ++i)
                if (rows[i].id === financeController.selectedProjectId)
                    return rows[i].name;
            return qsTr("Проект");
        }
        return pageTitles[page] || "";
    }

    function openTab(key) {
        pageHistory = [];
        page = key;
    }

    function openPage(key) {
        if (key === page)
            return;
        pageHistory = pageHistory.concat([page]);
        page = key;
    }

    function goBack() {
        if (pageHistory.length > 0) {
            const previous = pageHistory.slice();
            page = previous.pop();
            pageHistory = previous;
            return true;
        }
        if (page !== "overview") {
            page = "overview";
            return true;
        }
        return false;
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onBackPressed: function(event) {
            event.accepted = root.goBack();
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: tabBar.top
            anchors.leftMargin: root.pageMargin
            anchors.rightMargin: root.pageMargin
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                visible: root.page !== "overview"
                spacing: 8

                MobileIconButton {
                    visible: root.pageHistory.length > 0
                    icon: "chevronLeft"
                    onClicked: root.goBack()
                }

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.pageTitle()
                    elide: Text.ElideRight
                    color: root.accent
                    font.pixelSize: MobileTheme.sizeTitle
                    font.weight: Font.DemiBold
                }

                MobileChip {
                    visible: root.globalFilters
                    text: financeController.appCurrency
                    onClicked: currencySheet.open()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.globalFilters && root.page !== "overview"
                spacing: 8

                MobileChip {
                    Layout.fillWidth: true
                    text: root.dateFilterLabel()
                    onClicked: dateFilterDialog.openForCurrent()
                }

                MobileField {
                    Layout.fillWidth: true
                    visible: root.page === "operations"
                    placeholder: qsTr("Поиск")
                    onTextChanged: root.searchText = text
                }
            }

            CachedPages {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentKey: root.page
                pages: [
                    { key: "overview", component: overviewPage },
                    { key: "accounts", component: accountsPage },
                    { key: "operations", component: operationsPage },
                    { key: "analytics", component: analyticsPage },
                    { key: "more", component: morePage },
                    { key: "categories", component: categoriesPage },
                    { key: "settings", component: settingsPage },
                    { key: "data", component: dataPage },
                    { key: "budgets", component: budgetsPage },
                    { key: "goals", component: goalsPage },
                    { key: "trajectory", component: trajectoryPage },
                    { key: "scheduled", component: scheduledPage },
                    { key: "projects", component: projectsPage },
                    { key: "project", component: projectPage },
                    { key: "notes", component: notesPage }
                ]
            }
        }

        MobileTabBar {
            id: tabBar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 64
            items: root.tabItems
            activeKey: root.tabKey
            onSelected: function(key) { if (key === "add") operationDialog.openForNew(); else root.openTab(key) }
        }
    }

    MobileSheet {
        id: currencySheet

        title: qsTr("Валюта")

        MobileCard {
            Layout.fillWidth: true
            cardPadding: 0
            gap: 0

            Repeater {
                model: ["RUB", "USD", "EUR", "CNY"]

                delegate: MobileListRow {
                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    first: index === 0
                    title: modelData
                    marked: financeController.appCurrency === modelData
                    onClicked: {
                        financeController.appCurrency = modelData;
                        currencySheet.close();
                    }
                }
            }
        }
    }

    Component {
        id: morePage

        MobileMorePage {
            onNavigate: function(key) { root.openPage(key) }
        }
    }

    Component {
        id: budgetsPage

        MobileBudgetsPage {}
    }

    Component {
        id: goalsPage

        MobileGoalsPage {}
    }

    Component {
        id: trajectoryPage

        MobileTrajectoryPage {}
    }

    Component {
        id: scheduledPage

        MobileScheduledPage {}
    }

    Component {
        id: dataPage

        MobileDataPage {}
    }

    Component {
        id: projectsPage

        MobileProjectsPage {
            onNavigate: function(key) { root.openPage(key) }
        }
    }

    Component {
        id: projectPage

        MobileProjectPage {
            onDone: root.goBack()
        }
    }

    Component {
        id: notesPage

        MobileNotesPage {}
    }

    // ---- Overview (design): balance card, asset strip, accounts of the asset, history ----
    readonly property color designIncome: "#2E8963"
    readonly property color designExpense: "#2D0300"
    property bool overviewSearchOpen: false

    function shownAmount(text) {
        return root.hideAmounts ? "••••••" : text;
    }

    function assetRows() {
        return financeController.selectedAsset === "crypto" ? financeController.cryptoWallets : financeController.accounts;
    }

    function rowPrimaryAmount(row) {
        if (row.isCrypto)
            return row.balanceText + " " + row.symbol;
        if (row.isCreditCard)
            return root.money(row.availableCreditMinor, row.currency, false);
        return root.money(row.balanceMinor, row.currency, false);
    }

    function dayKey(date) {
        return Qt.formatDate(date, "yyyy-MM-dd");
    }

    // Operations grouped by day: header items ("Сегодня", "Вчера", dd.MM.yyyy) and row items
    function overviewHistoryItems() {
        const rows = visibleTransactions();
        const today = new Date();
        const yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
        const todayKey = dayKey(today);
        const yesterdayKey = dayKey(yesterday);
        const items = [];
        let lastKey = "";
        for (let i = 0; i < rows.length; ++i) {
            const date = new Date(rows[i].date);
            const key = dayKey(date);
            if (key !== lastKey) {
                items.push({ header: true, label: key === todayKey ? qsTr("Сегодня") : key === yesterdayKey ? qsTr("Вчера") : Qt.formatDate(date, "dd.MM.yyyy") });
                lastKey = key;
            }
            items.push({ header: false, row: rows[i] });
        }
        return items;
    }

    // Round tool button of the balance card. selected = pressed look (e.g. an active date filter)
    component RoundTool: Item {
        id: tool

        property string icon: ""
        property string glyph: ""
        property bool selected: false
        signal clicked()

        implicitWidth: 38
        implicitHeight: 42

        Rectangle {
            x: 0
            y: 4
            width: 38
            height: 38
            radius: 19
            color: root.accent
            visible: !tool.selected
        }

        Rectangle {
            y: tool.selected ? 4 : 0
            width: 38
            height: 38
            radius: 19
            color: tool.selected ? root.accent : tap.pressed ? root.controlHovered : root.panel
            border.width: 1
            border.color: root.accent

            MobileIcon {
                anchors.centerIn: parent
                visible: tool.icon.length > 0
                name: tool.icon
                width: 20
                height: 20
                color: tool.selected ? root.panel : root.accent
            }

            Text {
                anchors.centerIn: parent
                visible: tool.glyph.length > 0
                text: tool.glyph
                color: tool.selected ? root.panel : root.accent
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
        }

        TapHandler {
            id: tap
            onTapped: tool.clicked()
        }
    }

    Component {
        id: overviewPage
        ScrollView {
            id: overviewScroll
            clip: true

            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

            // Keep panel borders one physical pixel away from ScrollView's clip edge.
            readonly property int contentEdgeMargin: 2

            contentWidth: availableWidth
            contentHeight: dashboard.implicitHeight

            ColumnLayout {
                id: dashboard
                x: overviewScroll.contentEdgeMargin
                width: Math.max(0, overviewScroll.availableWidth - overviewScroll.contentEdgeMargin * 2)
                spacing: 12

                // "Общий баланс" card with the round tools on the right
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 6

                            Text {
                                text: qsTr("Общий баланс")
                                color: root.accent
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.shownAmount(root.money(financeController.balanceMinorUnits, financeController.appCurrency, false))
                                color: root.designIncome
                                font.pixelSize: 22
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            RoundTool {
                                icon: "filter"
                                selected: financeController.dateFilterActive
                                onClicked: dateFilterDialog.openForCurrent()
                            }
                            RoundTool {
                                icon: root.hideAmounts ? "eyeOff" : "eye"
                                onClicked: root.hideAmounts = !root.hideAmounts
                            }
                        }

                        Item {
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 42

                            RoundTool {
                                x: 23
                                glyph: MobileFormat.symbol(financeController.appCurrency)
                                onClicked: currencySheet.open()
                            }
                        }
                    }
                }

                // Asset strip: the selected asset is the wide bar, the others are squares.
                // Order is fixed: фиат, крипта, инвестиции. Swipe left/right or tap a square.
                Item {
                    id: assetStrip

                    readonly property var assetOrder: ["fiat", "crypto", "investment"]
                    readonly property var assetGlyphs: ["₽", "₿", ""]
                    readonly property int squareSize: 36
                    readonly property int gap: 8

                    Layout.fillWidth: true
                    Layout.preferredHeight: 36

                    function selectedIndex() {
                        return assetOrder.indexOf(financeController.selectedAsset);
                    }

                    function widthOf(index) {
                        return index === selectedIndex() ? width - 2 * squareSize - 2 * gap : squareSize;
                    }

                    function xOf(index) {
                        let x = 0;
                        for (let i = 0; i < index; ++i)
                            x += widthOf(i) + gap;
                        return x;
                    }

                    function indexAt(x) {
                        for (let i = 0; i < assetOrder.length; ++i)
                            if (x >= xOf(i) && x < xOf(i) + widthOf(i))
                                return i;
                        return -1;
                    }

                    function selectAt(index) {
                        if (index >= 0 && index < assetOrder.length)
                            financeController.selectedAsset = assetOrder[index];
                    }

                    Repeater {
                        model: assetStrip.assetOrder

                        delegate: Rectangle {
                            id: assetTile

                            required property string modelData
                            required property int index
                            readonly property bool selected: financeController.selectedAsset === modelData

                            x: assetStrip.xOf(index)
                            y: 0
                            width: assetStrip.widthOf(index)
                            height: 36
                            radius: 9
                            color: selected ? root.accent : root.panel
                            border.width: selected ? 0 : 1
                            border.color: root.accent

                            Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                            // Selected asset: "all accounts" checkbox and the total
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 12
                                spacing: 10
                                visible: assetTile.selected

                                Rectangle {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    radius: 4
                                    color: financeController.selectedAccountId === "" ? root.panel : root.accent
                                    border.width: 1
                                    border.color: root.panel

                                    MobileIcon {
                                        anchors.centerIn: parent
                                        visible: financeController.selectedAccountId === ""
                                        name: "check"
                                        width: 14
                                        height: 14
                                        color: root.accent
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.shownAmount(root.money(root.assetAmount(assetTile.modelData), financeController.appCurrency, false))
                                    color: root.white
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                }
                            }

                            // Other assets: squares with a glyph
                            Text {
                                anchors.centerIn: parent
                                visible: !assetTile.selected && assetStrip.assetGlyphs[assetTile.index] !== ""
                                text: assetStrip.assetGlyphs[assetTile.index]
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }

                            MobileIcon {
                                anchors.centerIn: parent
                                visible: !assetTile.selected && assetStrip.assetGlyphs[assetTile.index] === ""
                                name: "chart"
                                width: 20
                                height: 20
                                color: root.accent
                            }
                        }
                    }

                    // Taps and swipes on the whole strip. A tap on the checkbox of the selected asset clears the account filter.
                    MouseArea {
                        id: assetSwipe

                        property real pressX: 0

                        anchors.fill: parent
                        preventStealing: true
                        onPressed: function (mouse) {
                            pressX = mouse.x;
                        }
                        onReleased: function (mouse) {
                            const dx = mouse.x - pressX;
                            if (dx < -40)
                                assetStrip.selectAt(assetStrip.selectedIndex() + 1);
                            else if (dx > 40)
                                assetStrip.selectAt(assetStrip.selectedIndex() - 1);
                        }
                        onClicked: function (mouse) {
                            if (Math.abs(mouse.x - pressX) > 40)
                                return;
                            const index = assetStrip.indexAt(mouse.x);
                            if (index < 0)
                                return;
                            if (index !== assetStrip.selectedIndex()) {
                                assetStrip.selectAt(index);
                                return;
                            }
                            if (mouse.x - assetStrip.xOf(index) < 40)
                                financeController.selectedAccountId = "";
                        }
                    }
                }

                // Accounts of the selected asset, one row per account
                Panel {
                    Layout.fillWidth: true
                    implicitHeight: assetColumn.implicitHeight + 32

                    ColumnLayout {
                        id: assetColumn
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 16

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: root.assetTitle(financeController.selectedAsset)
                                color: root.accent
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                id: assetAddButton
                                text: qsTr("+ Добавить")
                                color: root.navSelected
                                font.pixelSize: 14
                                opacity: addTap.pressed ? 0.6 : 1

                                TapHandler {
                                    id: addTap
                                    onTapped: {
                                        if (financeController.selectedAsset === "crypto")
                                            cryptoWalletDialog.openForNewWallet();
                                        else
                                            accountDialog.openForSelectedAsset();
                                    }
                                }
                            }
                        }

                        Repeater {
                            model: root.assetRows()

                            delegate: Rectangle {
                                id: accountRow

                                required property var modelData
                                readonly property bool selected: !modelData.isCrypto && financeController.selectedAccountId === modelData.id

                                Layout.fillWidth: true
                                Layout.preferredHeight: 69
                                radius: 10
                                color: selected ? root.accent : root.canvas

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 6

                                        Text {
                                            Layout.fillWidth: true
                                            text: accountRow.modelData.name
                                            color: accountRow.selected ? root.white : root.accent
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.shownAmount(root.rowPrimaryAmount(accountRow.modelData))
                                            color: accountRow.selected ? root.white : root.accent
                                            font.pixelSize: 20
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                        }
                                    }

                                    ColumnLayout {
                                        visible: accountRow.modelData.isCreditCard === true
                                        spacing: 6

                                        Text {
                                            Layout.alignment: Qt.AlignRight
                                            text: qsTr("Долг")
                                            color: accountRow.selected ? root.paleText : root.muted
                                            font.pixelSize: 14
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignRight
                                            text: root.shownAmount(root.money(accountRow.modelData.debtMinor, accountRow.modelData.currency, false))
                                            color: accountRow.selected ? root.white : root.accent
                                            font.pixelSize: 20
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (!accountRow.modelData.isCrypto)
                                            financeController.selectedAccountId = accountRow.selected ? "" : accountRow.modelData.id;
                                    }
                                    onPressAndHold: root.openAccountContextMenu(accountRow.modelData, accountRow, mouseX, mouseY)
                                }
                            }
                        }
                    }
                }

                // "История операций" grouped by day; "Поиск" is a plain text link
                Panel {
                    Layout.fillWidth: true
                    implicitHeight: historyColumn.implicitHeight + 32

                    ColumnLayout {
                        id: historyColumn
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("История операций")
                                color: root.accent
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            Text {
                                id: overviewSearchButton
                                text: qsTr("Поиск")
                                color: root.navSelected
                                font.pixelSize: 14
                                opacity: searchTap.pressed ? 0.6 : 1

                                TapHandler {
                                    id: searchTap
                                    onTapped: {
                                        root.overviewSearchOpen = !root.overviewSearchOpen;
                                        if (!root.overviewSearchOpen) {
                                            root.searchText = "";
                                            Qt.inputMethod.hide();
                                        }
                                    }
                                }
                            }
                        }

                        Loader {
                            id: overviewSearchLoader

                            Layout.fillWidth: true
                            Layout.bottomMargin: root.overviewSearchOpen ? 6 : 0
                            active: root.overviewSearchOpen
                            visible: root.overviewSearchOpen

                            sourceComponent: TextField {
                                id: overviewSearchField

                                width: overviewSearchLoader.width
                                placeholderText: qsTr("Поиск по операциям")
                                placeholderTextColor: root.muted
                                color: root.accent
                                font.pixelSize: 14
                                leftPadding: 0
                                rightPadding: 0
                                background: Item {}
                                text: root.searchText
                                onTextChanged: root.searchText = text
                                Component.onCompleted: {
                                    forceActiveFocus();
                                    Qt.inputMethod.show();
                                }
                            }
                        }

                        Repeater {
                            model: root.overviewHistoryItems()

                            delegate: Item {
                                id: historyItem

                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: modelData.header ? 34 : 50

                                Text {
                                    visible: modelData.header
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 4
                                    text: modelData.label || ""
                                    color: root.accent
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }

                                RowLayout {
                                    visible: !historyItem.modelData.header
                                    anchors.fill: parent
                                    spacing: 8

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: historyItem.modelData.header ? "" : root.transactionSummary(historyItem.modelData.row)
                                            color: root.accent
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: historyItem.modelData.header ? "" : root.accountName(historyItem.modelData.row.accountId)
                                            color: root.muted
                                            font.pixelSize: 13
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        text: historyItem.modelData.header ? "" : root.shownAmount(root.money(
                                                  root.transactionSignedAmount(historyItem.modelData.row),
                                                  historyItem.modelData.row.currency, true))
                                        color: historyItem.modelData.header ? root.accent
                                             : historyItem.modelData.row.type === "transfer" ? root.navSelected
                                             : (historyItem.modelData.row.type === "income" || (historyItem.modelData.row.type === "investment_cash" && historyItem.modelData.row.amount >= 0)) ? root.designIncome
                                             : root.designExpense
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !historyItem.modelData.header
                                    onPressAndHold: root.openTransactionContextMenu(historyItem.modelData.row, historyItem, mouseX, mouseY)
                                    onPressed: function (mouse) {
                                        if (mouse.button === Qt.RightButton && !historyItem.modelData.header)
                                            root.openTransactionContextMenu(historyItem.modelData.row, historyItem, mouse.x, mouse.y);
                                    }
                                }
                            }
                        }
                    }
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
    component TransactionBlock: Panel {
        id: transactionBlock

        property string title: qsTr("История операций")
        property var rows: []
        property bool expandToContent: false

        // 68 px block header + 34 px table header + 38 px per transaction + 2 px frame inset.
        // Keep these values in sync with TransactionTable row/header heights below.
        implicitHeight: expandToContent ? 104 + rows.length * 38 : 245

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 13
                Layout.rightMargin: 13
                Layout.topMargin: 13
                Layout.bottomMargin: 13

                Text {
                    text: transactionBlock.title
                    color: root.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Item {
                    Layout.fillWidth: true
                }

                SoftButton {
                    text: qsTr("+  Операция")
                    highlighted: true
                    implicitWidth: 132
                    onClicked: operationDialog.openForNew()
                }
            }

            TransactionTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                rows: transactionBlock.rows
                expandToContent: transactionBlock.expandToContent
                bottomCornerRadius: transactionBlock.radius - 1
            }
        }

        BottomCornerMask {
            width: transactionBlock.radius
            height: transactionBlock.radius
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            z: 999
        }

        BottomCornerMask {
            width: transactionBlock.radius
            height: transactionBlock.radius
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            mirrored: true
            z: 999
        }

        // IMPORTANT: frame is deliberately rendered ABOVE all table content.
        Rectangle {
            anchors.fill: parent
            color: root.transparentColor
            radius: transactionBlock.radius
            border.width: 1
            border.color: root.line
            z: 1000
        }
    }

    component TransactionTable: Item {
        id: transactionTable

        property var rows: []
        property real bottomCornerRadius: 0
        property bool expandToContent: false

        readonly property int tableHeaderHeight: 34
        readonly property int rowHeight: 38
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
                height: transactionTable.tableHeaderHeight
                color: root.tableHeader

                // Only the header/content divider is drawn here.
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: root.line
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22

                    Text {
                        text: qsTr("Операция")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.preferredWidth: transactionTable.operationColumnWidth
                    }
                    Text {
                        text: qsTr("Счёт")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.preferredWidth: transactionTable.accountColumnWidth
                    }
                    Text {
                        text: qsTr("Категория")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.preferredWidth: transactionTable.categoryColumnWidth
                    }
                    Text {
                        text: qsTr("Описание")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Дата")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.preferredWidth: transactionTable.dateColumnWidth
                    }
                    Text {
                        text: qsTr("Сумма")
                        color: root.muted
                        font.pixelSize: 14
                        Layout.preferredWidth: transactionTable.amountColumnWidth
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Item {
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
                    anchors.fill: parent
                    visible: transactionTable.expandToContent

                    Repeater {
                        model: transactionTable.rows

                        delegate: Item {
                            required property int index
                            required property var modelData
                            width: parent.width
                            height: transactionTable.rowHeight

                            Rectangle {
                                anchors.fill: parent
                                color: index % 2 ? root.tableRowAlt : root.panel
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 22
                                anchors.rightMargin: 22

                                Text {
                                    text: root.transactionTypeLabel(modelData)
                                    color: root.accent
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    Layout.preferredWidth: transactionTable.operationColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: root.accountName(modelData.accountId)
                                    color: root.muted
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
                                    text: root.transactionSummary(modelData)
                                    color: root.muted
                                    font.pixelSize: 14
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                    color: root.muted
                                    font.pixelSize: 14
                                    Layout.preferredWidth: transactionTable.dateColumnWidth
                                }
                                Text {
                                    text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                    color: modelData.type === "transfer" ? root.navSelected : (modelData.type === "income" || (modelData.type === "investment_cash" && modelData.amount >= 0)) ? root.income : root.red
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    Layout.preferredWidth: transactionTable.amountColumnWidth
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            MouseArea {
                                id: overviewRowMenuArea
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onPressed: function (mouse) {
                                    if (mouse.button === Qt.RightButton)
                                        root.openTransactionContextMenu(
                                            modelData,
                                            overviewRowMenuArea,
                                            mouse.x,
                                            mouse.y
                                        );
                                }
                            }
                        }
                    }
                }

                // OPERATIONS PAGE MODE: virtualized scrollable list remains useful for
                // potentially large transaction histories. Scrollbar itself stays hidden.
                ListView {
                    id: transactionList
                    anchors.fill: parent
                    visible: !transactionTable.expandToContent
                    clip: true
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                    model: transactionTable.rows

                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        height: transactionTable.rowHeight

                        Rectangle {
                            anchors.fill: parent
                            color: index % 2 ? root.tableRowAlt : root.panel
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 22
                            anchors.rightMargin: 22

                            Text {
                                text: root.transactionTypeLabel(modelData)
                                color: root.accent
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                Layout.preferredWidth: transactionTable.operationColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.accountName(modelData.accountId)
                                color: root.muted
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
                                text: root.transactionSummary(modelData)
                                color: root.muted
                                font.pixelSize: 14
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                color: root.muted
                                font.pixelSize: 14
                                Layout.preferredWidth: transactionTable.dateColumnWidth
                            }
                            Text {
                                text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                color: modelData.type === "transfer" ? root.navSelected : (modelData.type === "income" || (modelData.type === "investment_cash" && modelData.amount >= 0)) ? root.income : root.red
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: transactionTable.amountColumnWidth
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        MouseArea {
                            id: operationsRowMenuArea
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onPressed: function (mouse) {
                                if (mouse.button === Qt.RightButton)
                                    root.openTransactionContextMenu(
                                        modelData,
                                        operationsRowMenuArea,
                                        mouse.x,
                                        mouse.y
                                    );
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: transactionTable.rows.length === 0
                    text: qsTr("Операций пока нет")
                    color: root.muted
                }
            }
        }
    }

    component MobileTransactionCard: Rectangle {
        HardShadow { }

        id: mobileTransactionCard
        property var rowData: null
        property int rowIndex: 0

        width: parent ? parent.width : 0
        height: 92
        radius: 13
        color: rowIndex % 2 ? root.tableRowAlt : root.panel
        border.width: 1
        border.color: root.line

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 4
            height: 52
            radius: 2
            color: !mobileTransactionCard.rowData ? root.line
                 : mobileTransactionCard.rowData.type === "transfer" ? root.navSelected
                 : (mobileTransactionCard.rowData.type === "income" || (mobileTransactionCard.rowData.type === "investment_cash" && mobileTransactionCard.rowData.amount >= 0)) ? root.income
                 : root.red
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 8
            anchors.topMargin: 9
            anchors.bottomMargin: 9
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: mobileTransactionCard.rowData
                              ? root.transactionTypeLabel(mobileTransactionCard.rowData)
                              : ""
                        color: root.accent
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: mobileTransactionCard.rowData
                              ? mobileTransactionCard.rowData.categoryName
                              : ""
                        color: root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: mobileTransactionCard.rowData
                          ? root.transactionSummary(mobileTransactionCard.rowData)
                          : ""
                    color: root.accent
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: mobileTransactionCard.rowData
                          ? root.accountName(mobileTransactionCard.rowData.accountId)
                            + " · "
                            + Qt.formatDateTime(
                                  new Date(mobileTransactionCard.rowData.date),
                                  "dd.MM.yyyy"
                              )
                          : ""
                    color: root.muted
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
            }

            ColumnLayout {
                Layout.preferredWidth: Math.min(130, mobileTransactionCard.width * 0.38)
                Layout.fillHeight: true
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    text: mobileTransactionCard.rowData
                          ? root.money(
                                root.transactionSignedAmount(mobileTransactionCard.rowData),
                                mobileTransactionCard.rowData.currency,
                                true
                            )
                          : ""
                    color: !mobileTransactionCard.rowData ? root.accent
                         : mobileTransactionCard.rowData.type === "transfer" ? root.navSelected
                         : (mobileTransactionCard.rowData.type === "income" || (mobileTransactionCard.rowData.type === "investment_cash" && mobileTransactionCard.rowData.amount >= 0)) ? root.income
                         : root.red
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }

                SoftButton {
                    id: transactionMoreButton
                    Layout.alignment: Qt.AlignRight
                    implicitWidth: 42
                    implicitHeight: 36
                    flat: true
                    text: "⋮"
                    font.pixelSize: 22
                    onClicked: {
                        if (mobileTransactionCard.rowData)
                            root.openTransactionContextMenu(
                                mobileTransactionCard.rowData,
                                transactionMoreButton,
                                transactionMoreButton.width / 2,
                                transactionMoreButton.height / 2
                            );
                    }
                }
            }
        }

        MouseArea {
            id: transactionTouchArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.rightMargin: 50
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressAndHold: {
                if (mobileTransactionCard.rowData)
                    root.openTransactionContextMenu(
                        mobileTransactionCard.rowData,
                        transactionTouchArea,
                        mouseX,
                        mouseY
                    );
            }
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton && mobileTransactionCard.rowData)
                    root.openTransactionContextMenu(
                        mobileTransactionCard.rowData,
                        transactionTouchArea,
                        mouse.x,
                        mouse.y
                    );
            }
        }
    }

    component MobileTransactionList: Item {
        id: mobileTransactionList
        property var rows: []
        property bool expandToContent: false

        Column {
            anchors.fill: parent
            spacing: 6
            visible: mobileTransactionList.expandToContent

            Repeater {
                model: mobileTransactionList.rows
                delegate: MobileTransactionCard {
                    required property int index
                    required property var modelData
                    width: parent.width
                    rowData: modelData
                    rowIndex: index
                }
            }
        }

        ListView {
            anchors.fill: parent
            visible: !mobileTransactionList.expandToContent
            clip: true
            spacing: 8
            model: mobileTransactionList.rows
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

            delegate: MobileTransactionCard {
                required property int index
                required property var modelData
                width: ListView.view.width
                rowData: modelData
                rowIndex: index
            }
        }

        Label {
            anchors.centerIn: parent
            visible: mobileTransactionList.rows.length === 0
            text: qsTr("Операций пока нет")
            color: root.muted
        }
    }

    component MobileTransactionBlock: Panel {
        id: mobileTransactionBlock
        property string title: qsTr("История операций")
        property var rows: []
        property bool expandToContent: false

        implicitHeight: expandToContent
                      ? 72 + Math.max(72, rows.length * 98)
                      : 260

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: mobileTransactionBlock.title
                    color: root.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                SoftButton {
                    text: qsTr("+ Операция")
                    highlighted: true
                    implicitWidth: 122
                    onClicked: operationDialog.openForNew()
                }
            }

            MobileTransactionList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                rows: mobileTransactionBlock.rows
                expandToContent: mobileTransactionBlock.expandToContent
            }
        }
    }

    Component {
        id: accountsPage
        ColumnLayout {
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: root.assets
                        delegate: SoftButton {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: modelData.title
                            highlighted: financeController.selectedAsset === modelData.code
                            onClicked: financeController.selectedAsset = modelData.code
                        }
                    }
                }

                SoftButton {
                    Layout.fillWidth: true
                    visible: financeController.selectedAsset !== "investment"
                          || financeController.investmentAccounts.length > 0
                    text: financeController.selectedAsset === "crypto"
                          ? qsTr("+ Добавить криптовалюту")
                          : financeController.selectedAsset === "investment"
                            ? qsTr("+ Добавить позицию")
                            : qsTr("+ Добавить счёт")
                    highlighted: true
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
                    Layout.fillWidth: true
                    visible: financeController.selectedAsset === "investment"
                    text: qsTr("+ Добавить счёт")
                    onClicked: accountDialog.openForSelectedAsset()
                }
            }

            ColumnLayout {
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
                    wrapMode: Text.WordWrap
                }
                SoftButton {
                    Layout.fillWidth: true
                    visible: !financeController.cryptoRefreshing
                    text: qsTr("Повторить")
                    onClicked: financeController.refreshCryptoWallets()
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: financeController.selectedAsset !== "investment"
                Layout.preferredHeight: financeController.selectedAsset === "investment"
                                      ? Math.min(contentHeight, 300) : -1
                Layout.minimumHeight: financeController.selectedAsset === "investment"
                                    ? Math.min(Math.max(contentHeight, 148), 300) : 0
                clip: true
                spacing: 10
                // Scrollbars intentionally hidden application-wide.
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                model: financeController.selectedAsset === "crypto"
                       ? financeController.cryptoWallets
                       : financeController.accounts
                delegate: Panel {
                    required property var modelData
                    width: ListView.view.width
                    height: modelData.isCrypto ? 174 : 138
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 54
                        anchors.topMargin: 14
                        anchors.bottomMargin: 14
                        RowLayout {
                            Text {
                                text: "▣"
                                color: root.accent
                                font.pixelSize: 24
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.accountPrimaryAmount(modelData)
                            color: root.accent
                            font.pixelSize: modelData.isCreditCard ? 18 : 23
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.isCreditCard
                            text: root.accountAvailableCredit(modelData)
                            color: root.navSelected
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.isCrypto
                            text: "≈ " + root.money(
                                modelData.valueMinor,
                                financeController.appCurrency,
                                false
                            )
                            color: root.navSelected
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.isCrypto
                                  ? modelData.network + " · " + modelData.address
                                  : modelData.currency + " · " + root.accountTypeLabel(modelData.type)
                            color: root.muted
                            font.pixelSize: 14
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.isCrypto
                            text: root.cryptoWalletStatus(modelData)
                            color: modelData.refreshing ? root.navSelected : root.muted
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: accountsPageMouseArea
                        anchors.fill: parent
                        anchors.rightMargin: 48
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressAndHold: root.openAccountContextMenu(
                            modelData,
                            accountsPageMouseArea,
                            mouseX,
                            mouseY
                        )
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

                    SoftButton {
                        id: accountMoreButton
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        width: 42
                        height: 42
                        flat: true
                        text: "⋮"
                        font.pixelSize: 22
                        onClicked: root.openAccountContextMenu(
                            modelData,
                            accountMoreButton,
                            width / 2,
                            height / 2
                        )
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
                Layout.minimumHeight: 260
                controller: financeController
                showEditButton: true
                onOperationRequested: function(position) { investmentOperationDialog.openFor(position); }
                onEditRequested: function(position) { investmentPositionDialog.openForEdit(position); }
                panelColor: root.panel
                textColor: root.accent
                mutedColor: root.muted
                lineColor: root.line
                errorColor: root.red
            }
        }
    }

    Component {
        id: categoriesPage

        ColumnLayout {
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                SoftButton {
                    Layout.fillWidth: true
                    text: qsTr("+ Управление категориями")
                    highlighted: true
                    onClicked: categoryDialog.openForManagement()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

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

                        Text {
                            text: qsTr("Доходы")
                            color: root.income
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            clip: true
                            spacing: 6

                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            model: financeController.categories.filter(function(category) {
                                return category.type === "income";
                            })

                            delegate: Rectangle {
                                HardShadow { }

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

                        Text {
                            text: qsTr("Расходы")
                            color: root.red
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            clip: true
                            spacing: 6

                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }

                            model: financeController.categories.filter(function(category) {
                                return category.type === "expense";
                            })

                            delegate: Rectangle {
                                HardShadow { }

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
        MobileTransactionBlock {
            anchors.fill: parent
            title: qsTr("История операций")
            rows: root.visibleTransactions()
        }
    }

    Component {
        id: analyticsPage

        MobileAnalyticsPage {}
    }
    Component {
        id: settingsPage
        Panel {
            ScrollView {
                id: settingsScroll
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth
                contentHeight: settingsContent.implicitHeight + 32
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

                ColumnLayout {
                    id: settingsContent
                    x: 16
                    y: 16
                    width: Math.max(0, settingsScroll.availableWidth - 32)
                    PasswordSettings {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 24
                    }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Основная валюта")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Все итоговые суммы пересчитываются в эту валюту.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                AppComboBox {
                    Layout.fillWidth: true
                    model: ["RUB", "USD", "EUR", "CNY"]
                    currentIndex: Math.max(0, model.indexOf(financeController.appCurrency))
                    onActivated: financeController.appCurrency = currentText
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.fillWidth: true
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
                    Layout.fillWidth: true
                    text: automaticRatesCheck.checked
                          ? qsTr("Курсы загружаются из ЦБ РФ дважды в сутки. При ошибке используется последний успешный результат.")
                          : qsTr("Автоматические запросы отключены. Для пересчёта используются сохранённые ниже значения.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    HardShadow { }

                    id: currentRatesBlock

                    Layout.topMargin: 4
                    Layout.fillWidth: true
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
                    Layout.fillWidth: true
                    columns: root.width < 380 ? 1 : 2
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
                        Layout.fillWidth: true
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
                        Layout.fillWidth: true
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
                        Layout.fillWidth: true
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

                    Text {
                        text: qsTr("1 CNY в рублях")
                        color: root.accent
                    }
                    AppTextField {
                        id: manualCnyRateField
                        Layout.fillWidth: true
                        text: financeController.manualCnyToRubRate.toLocaleString(
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
                              && manualCnyRateField.acceptableInput
                        onClicked: {
                            const rubRate = Number(manualRubRateField.text.replace(",", "."));
                            const usdRate = Number(manualUsdRateField.text.replace(",", "."));
                            const eurRate = Number(manualEurRateField.text.replace(",", "."));
                            const cnyRate = Number(manualCnyRateField.text.replace(",", "."));
                            const saved = financeController.saveManualCurrencyRates(
                                rubRate, usdRate, eurRate, cnyRate);
                            rateSaveStatus.color = saved ? root.income : root.red;
                            rateSaveStatus.text = saved
                                ? qsTr("Курсы сохранены")
                                : qsTr("Введите положительные числовые значения");
                        }
                    }
                    Text {
                        id: rateSaveStatus
                        Layout.fillWidth: true
                        color: root.income
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
                Rectangle {
                    Layout.topMargin: 12
                    Layout.fillWidth: true
                    height: 1
                    color: root.line
                }
                Text {
                    Layout.topMargin: 8
                    text: qsTr("Язык интерфейса")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Пользовательские названия счетов и категорий не переводятся.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                AppComboBox {
                    id: languageBox
                    Layout.fillWidth: true
                    model: [
                        { label: qsTr("Русский"), value: "ru" },
                        { label: qsTr("Английский"), value: "en" }
                    ]
                    textRole: "label"
                    currentIndex: root.indexByRole(model, "value", financeController.uiLanguage)
                    onActivated: financeController.uiLanguage = model[currentIndex].value
                }
                Item { Layout.preferredHeight: 4 }
                }
            }
        }
    }

    InvestmentOperationDialog {
        id: investmentOperationDialog
        controller: financeController
        panelColor: root.panel; softColor: root.soft; textColor: root.accent
        mutedColor: root.muted; lineColor: root.line; accentColor: root.accent; errorColor: root.red
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

    Dialog {
        id: cryptoWalletDialog
        width: Math.min(500, root.width - 20)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 16 : 20

        function openForNewWallet() {
            cryptoTypeBox.currentIndex = 0;
            cryptoAddressField.clear();
            cryptoWalletError.text = "";
            open();
            cryptoAddressField.forceActiveFocus();
        }

        function submit() {
            const result = financeController.addCryptoWallet(
                cryptoTypeBox.model[cryptoTypeBox.currentIndex].value,
                cryptoAddressField.text
            );
            if (result.ok)
                close();
            else
                cryptoWalletError.text = result.error || qsTr("Не удалось добавить кошелёк");
        }

        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: qsTr("Добавить криптовалюту")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppComboBox {
                id: cryptoTypeBox
                Layout.fillWidth: true
                model: [
                    { label: "USDT · TRC-20", value: "USDT" },
                    { label: "BTC · Bitcoin", value: "BTC" },
                    { label: "ETH · Ethereum", value: "ETH" }
                ]
                textRole: "label"
                onActivated: {
                    cryptoAddressField.clear();
                    cryptoWalletError.text = "";
                    cryptoAddressField.forceActiveFocus();
                }
            }
            AppTextField {
                id: cryptoAddressField
                Layout.fillWidth: true
                placeholderText: cryptoTypeBox.currentIndex === 0
                    ? qsTr("Публичный адрес TRON (T…)")
                    : cryptoTypeBox.currentIndex === 1
                        ? qsTr("Публичный адрес Bitcoin (1…, 3… или bc1…)")
                        : qsTr("Публичный адрес Ethereum (0x…)")
                maximumLength: 90
                onAccepted: cryptoWalletDialog.submit()
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Вводите только публичный адрес. Никогда не указывайте seed-фразу или приватный ключ.")
                color: root.muted
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text {
                id: cryptoWalletError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                SoftButton {
                    text: qsTr("Отмена")
                    onClicked: cryptoWalletDialog.close()
                }
                SoftButton {
                    text: qsTr("Добавить")
                    highlighted: true
                    onClicked: cryptoWalletDialog.submit()
                }
            }
        }
    }

    Dialog {
        id: accountDialog
        width: Math.min(500, root.width - 20)
        height: Math.min(700, root.height - 24)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 16 : 20
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
        }

        onClosed: {
            editingId = "";
            editingAccount = null;
        }

        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ScrollView {
            id: accountDialogScroll
            clip: true
            contentWidth: availableWidth
            contentHeight: accountDialogForm.implicitHeight
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

            ColumnLayout {
                id: accountDialogForm
                width: accountDialogScroll.availableWidth
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
                model: ["RUB", "USD", "EUR", "CNY"]
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
                      : accountTypeBox.currentIndex >= 0 && accountTypeBox.currentIndex < accountDialog.accountTypes.length && accountDialog.accountTypes[accountTypeBox.currentIndex].value === "brokerage"
                        ? qsTr("Деньги на счёте, включая обеспечение") : qsTr("Начальный баланс")
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
                                 : accountTypeBox.currentIndex >= 0 && accountTypeBox.currentIndex < accountDialog.accountTypes.length && accountDialog.accountTypes[accountTypeBox.currentIndex].value === "brokerage"
                                   ? qsTr("Деньги на счёте, включая обеспечение") : qsTr("Начальный баланс")
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
                    text: qsTr("Отмена")
                    onClicked: accountDialog.close()
                }
                SoftButton {
                    text: accountDialog.editingId ? qsTr("Сохранить") : qsTr("Добавить")
                    highlighted: true
                    onClicked: {
                        const type = accountDialog.accountTypes[accountTypeBox.currentIndex].value;
                        const enteredMinor = Math.round((Number(accountBalanceField.text.replace(",", ".")) || 0) * 100);
                        const initialBalanceMinor = accountDialog.creditCardSelected
                                                  ? -Math.abs(enteredMinor)
                                                  : enteredMinor;
                        const creditLimitMinor = accountDialog.creditCardSelected
                                               ? Math.round((Number(accountCreditLimitField.text.replace(",", ".")) || 0) * 100)
                                               : 0;
                        if (accountDialog.creditCardSelected && creditLimitMinor <= 0) {
                            accountError.text = qsTr("Укажите кредитный лимит");
                            return;
                        }
                        const depositAnnualRatePercent = Number(
                            depositRateField.text.replace(",", ".")
                        );
                        const depositPayoutFrequency = accountDialog.depositSelected
                            ? accountDialog.depositPayoutTypes[
                                  depositPayoutBox.currentIndex].value
                            : "";
                        const depositPayoutDay = accountDialog.monthlyDepositSelected
                            ? Number(depositPayoutDayField.text)
                            : 0;
                        if (accountDialog.depositSelected && initialBalanceMinor <= 0) {
                            accountError.text = qsTr("Укажите сумму вклада");
                            return;
                        }
                        if (accountDialog.depositSelected && depositAnnualRatePercent <= 0) {
                            accountError.text = qsTr("Укажите годовую процентную ставку");
                            return;
                        }
                        if (accountDialog.monthlyDepositSelected &&
                            (depositPayoutDay < 1 || depositPayoutDay > 31)) {
                            accountError.text = qsTr("Укажите день выплаты от 1 до 31");
                            return;
                        }
                        const ok = accountDialog.editingId
                                 ? financeController.updateAccount(
                                     accountDialog.editingId,
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
                            accountDialog.close();
                        else
                            accountError.text = qsTr("Проверьте название и параметры счёта");
                    }
                }
            }
            }
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
                  && !accountContextMenu.accountData.isCrypto
            enabled: visible
            onTriggered: {
                if (accountContextMenu.accountData)
                    accountDialog.openForEdit(accountContextMenu.accountData);
            }
        }

        MenuSeparator {
            width: accountContextMenu.availableWidth
            visible: accountContextMenu.accountData !== null
                  && !accountContextMenu.accountData.isCrypto
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
            HardShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Dialog {
        id: deleteAccountDialog
        width: Math.min(460, root.width - 20)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 16 : 20
        closePolicy: Popup.CloseOnEscape
        property var accountData: null

        function openFor(row) {
            accountData = row;
            deleteAccountError.text = "";
            open();
        }

        onClosed: accountData = null

        background: Rectangle {
            HardShadow { }

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
                    text: qsTr("Отмена")
                    onClicked: deleteAccountDialog.close()
                }
                SoftButton {
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: {
                        const row = deleteAccountDialog.accountData;
                        const ok = row && row.isCrypto
                                 ? financeController.deleteCryptoWallet(row.id)
                                 : row && financeController.deleteAccount(row.id);
                        if (ok)
                            deleteAccountDialog.close();
                        else
                            deleteAccountError.text = row && row.isCrypto
                                                    ? qsTr("Не удалось удалить криптовалюту")
                                                    : qsTr("Не удалось удалить счёт");
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
                     && transactionContextMenu.transactionData.type !== "investment_cash"
            onTriggered: {
                if (transactionContextMenu.transactionData)
                    operationDialog.openForEdit(transactionContextMenu.transactionData);
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
                     && transactionContextMenu.transactionData.type !== "investment_cash"
            onTriggered: {
                if (transactionContextMenu.transactionData)
                    deleteTransactionDialog.openFor(transactionContextMenu.transactionData);
            }
        }

        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 12
            border.width: 1
            border.color: root.line
        }
    }

    Dialog {
        id: deleteTransactionDialog
        width: Math.min(460, root.width - 20)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 16 : 20
        closePolicy: Popup.CloseOnEscape
        property var transactionData: null

        function openFor(row) {
            transactionData = row;
            deleteTransactionError.text = "";
            open();
        }

        onClosed: transactionData = null

        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: qsTr("Удалить операцию?")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: deleteTransactionDialog.transactionData
                      && deleteTransactionDialog.transactionData.type === "transfer"
                      ? qsTr("Будут удалены обе части перевода. Баланс и статистика будут пересчитаны.")
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
                              ? (deleteTransactionDialog.transactionData.description
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
                    text: qsTr("Отмена")
                    onClicked: deleteTransactionDialog.close()
                }
                SoftButton {
                    text: qsTr("Удалить")
                    destructive: true
                    onClicked: {
                        const row = deleteTransactionDialog.transactionData;
                        if (row && financeController.deleteTransaction(row.id))
                            deleteTransactionDialog.close();
                        else
                            deleteTransactionError.text = qsTr("Не удалось удалить операцию");
                    }
                }
            }
        }
    }

    Dialog {
        id: operationDialog
        width: Math.min(520, root.width - 20)
        height: Math.min(720, root.height - 24)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 16 : 20
        property string editingId: ""
        property var editingTransaction: null
        property string editingSourceAccountId: ""
        property string editingTargetAccountId: ""
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

        function openForNew() {
            editingId = "";
            editingTransaction = null;
            editingSourceAccountId = "";
            editingTargetAccountId = "";
            operationError.text = "";
            operationAmount.clear();
            operationDescription.clear();
            selectedDate = new Date();
            operationType.currentIndex = 1;
            operationAccount.currentIndex = root.indexByRole(
                financeController.accounts,
                "id",
                financeController.selectedAccountId
            );
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
        }

        function openForEdit(row) {
            editingId = row.id;
            editingTransaction = row;
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
                    financeController.accounts,
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
        }

        onClosed: {
            editingId = "";
            editingTransaction = null;
            editingSourceAccountId = "";
            editingTargetAccountId = "";
        }

        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ScrollView {
            id: operationDialogScroll
            clip: true
            contentWidth: availableWidth
            contentHeight: operationDialogForm.implicitHeight
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

            ColumnLayout {
                id: operationDialogForm
                width: operationDialogScroll.availableWidth
                spacing: 14
            Text {
                text: operationDialog.editingId
                      ? qsTr("Редактирование операции")
                      : qsTr("Новая операция")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppComboBox {
                id: operationType
                Layout.fillWidth: true
                model: [qsTr("Доход"), qsTr("Расход"), qsTr("Перевод")]
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
                            financeController.accounts,
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
                model: operationType.currentIndex === 2
                     ? financeController.allAccounts
                     : financeController.accounts
                textRole: operationType.currentIndex === 2 ? "displayName" : "name"
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
                        HardShadow { }

                        radius: 11
                        color: operationDateButton.down
                               ? root.panel
                               : operationDateButton.hovered
                                 ? root.controlHovered
                                 : root.soft
                        border.width: 1
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
                    text: qsTr("Отмена")
                    onClicked: operationDialog.close()
                }
                SoftButton {
                    text: qsTr("Сохранить")
                    highlighted: true
                    onClicked: {
                        if (!financeController.allAccounts.length) {
                            operationError.text = qsTr("Сначала добавьте счёт");
                            return;
                        }
                        const isTransfer = operationType.currentIndex === 2;
                        if (!operationAccount.model.length ||
                            operationAccount.currentIndex < 0) {
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
                                            ? transferTargetAccount.model[transferTargetAccount.currentIndex]
                                            : null;
                        const ok = operationDialog.editingId
                                 ? financeController.updateOperation(
                                     operationDialog.editingId,
                                     minor,
                                     operationDescription.text,
                                     isTransfer ? "" : category.value,
                                     account.id,
                                     type,
                                     isTransfer ? targetAccount.id : "",
                                     operationDialog.selectedDate
                                 )
                                 : isTransfer
                                   ? financeController.addTransfer(
                                     minor,
                                     operationDescription.text,
                                     account.id,
                                     targetAccount.id,
                                     operationDialog.selectedDate
                                 )
                                 : type === "income"
                                   ? financeController.addIncome(
                                       minor,
                                       operationDescription.text,
                                       category.value,
                                       account.currency,
                                       account.id,
                                       operationDialog.selectedDate
                                   )
                                   : financeController.addExpense(
                                       minor,
                                       operationDescription.text,
                                       category.value,
                                       account.currency,
                                       account.id,
                                       operationDialog.selectedDate
                                   );
                        if (ok)
                            operationDialog.close();
                        else
                            operationError.text = isTransfer
                                ? qsTr("Проверьте сумму и выбранные счета")
                                : qsTr("Проверьте сумму, счёт и категорию");
                    }
                }
            }
            }
        }
    }

    Dialog {
        id: dateFilterDialog
        width: Math.min(450, root.width - 20)
        height: Math.min(650, root.height - 24)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 14 : 18
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
            HardShadow { }

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
                spacing: 4

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
                text: !dateFilterDialog.pendingFrom
                      ? qsTr("Выберите начало периода")
                      : !dateFilterDialog.pendingTo
                        ? qsTr("Теперь выберите конец периода")
                        : Qt.formatDate(dateFilterDialog.pendingFrom, "dd.MM.yyyy")
                          + " — "
                          + Qt.formatDate(dateFilterDialog.pendingTo, "dd.MM.yyyy")
                color: root.muted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }
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
        width: Math.min(400, root.width - 20)
        height: Math.min(540, root.height - 24)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 14 : 18
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
            HardShadow { }

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
        width: Math.min(560, root.width - 20)
        modal: true
        anchors.centerIn: parent
        padding: root.width < 380 ? 14 : 18
        function openForManagement() {
            categoryNameField.clear();
            categoryError.text = "";
            open();
        }
        background: Rectangle {
            HardShadow { }

            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: qsTr("Категории")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            ColumnLayout {
                Layout.fillWidth: true
                AppTextField {
                    id: categoryNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Название категории")
                }
                AppComboBox {
                    id: categoryType
                    Layout.fillWidth: true
                    model: [qsTr("Доход"), qsTr("Расход")]
                }
                SoftButton {
                    Layout.fillWidth: true
                    text: qsTr("Сохранить")
                    highlighted: true
                    onClicked: {
                        const ok = financeController.addCategory(categoryNameField.text, categoryType.currentIndex === 0 ? "income" : "expense");
                        if (ok) {
                            categoryNameField.clear();
                            categoryError.text = "";
                        } else
                            categoryError.text = qsTr("Не удалось сохранить категорию");
                    }
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
            }
        }
    }
}
