import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: "Ledgera"
    color: "#F4F2ED"

    readonly property color panel: "#FBFAF7"
    readonly property color soft: "#F0F0E9"
    readonly property color line: "#E2E0D9"
    readonly property color ink: "#17201B"
    readonly property color muted: "#687069"
    readonly property color green: "#344B3B"
    readonly property color green2: "#56745B"
    readonly property color pale: "#DDE4D5"
    readonly property color red: "#D74D36"

    property string page: "overview"
    property string searchText: ""
    readonly property var assets: [
        { code: "fiat", title: "Фиат", icon: "▣" },
        { code: "crypto", title: "Крипта", icon: "₿" },
        { code: "investment", title: "Инвестиции", icon: "▥" }
    ]

    function symbol(code) { return code === "USD" ? "$" : code === "EUR" ? "€" : "₽" }
    function money(minor, code, sign) {
        const value = Number(minor) / 100
        const prefix = sign ? (value >= 0 ? "+" : "−") : (value < 0 ? "−" : "")
        return prefix + Math.abs(value).toLocaleString(Qt.locale("ru_RU"), "f", 0)
                + " " + symbol(code || financeController.appCurrency)
    }
    function assetTitle(code) {
        for (let i = 0; i < assets.length; ++i) if (assets[i].code === code) return assets[i].title
        return "Фиат"
    }
    function assetAmount(code) {
        const rows = financeController.assetSummaries
        for (let i = 0; i < rows.length; ++i) if (rows[i].code === code) return rows[i].balanceMinor
        return 0
    }
    function accountName(id) {
        const rows = financeController.accounts
        for (let i = 0; i < rows.length; ++i) if (rows[i].id === id) return rows[i].name
        return "Другой счёт"
    }
    function visibleTransactions() {
        const result = [], ids = {}, accounts = financeController.accounts
        for (let i = 0; i < accounts.length; ++i) ids[accounts[i].id] = true
        const rows = financeController.transactions, query = searchText.trim().toLowerCase()
        for (let j = 0; j < rows.length; ++j) {
            const row = rows[j]
            if (!ids[row.accountId]) continue
            if (financeController.selectedAccountId && row.accountId !== financeController.selectedAccountId) continue
            const text = (row.description + " " + row.categoryName + " " + accountName(row.accountId)).toLowerCase()
            if (!query || text.indexOf(query) >= 0) result.push(row)
        }
        return result
    }
    function categoryTotals() {
        const values = {}, names = {}, rows = visibleTransactions()
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].type !== "expense") continue
            const id = rows[i].categoryId || "none"
            values[id] = (values[id] || 0) + Number(rows[i].displayAmount)
            names[id] = rows[i].categoryName || "Без категории"
        }
        const result = []
        for (const id in values) result.push({ label: names[id], amount: values[id] })
        result.sort(function(a, b) { return b.amount - a.amount })
        return result.slice(0, 5)
    }

    component Panel: Rectangle { color: root.panel; radius: 16; border.width: 1; border.color: root.line }
    component SoftButton: Button {
        id: control
        hoverEnabled: true; implicitHeight: 42
        contentItem: Text { text: control.text; color: control.highlighted ? "white" : root.ink; font.pixelSize: 14; font.weight: Font.Medium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 10; color: control.highlighted ? (control.down ? "#283C2F" : root.green) : (control.hovered ? root.soft : root.panel); border.width: control.highlighted ? 0 : 1; border.color: root.line }
    }
    component NavButton: Button {
        id: nav
        property string glyph: ""
        property string target: ""
        flat: true; hoverEnabled: true; implicitHeight: 54
        contentItem: RowLayout { spacing: 14
            Text { text: nav.glyph; color: root.ink; font.pixelSize: 20; Layout.preferredWidth: 26; horizontalAlignment: Text.AlignHCenter }
            Text { text: nav.text; color: root.ink; font.pixelSize: 15; Layout.fillWidth: true }
        }
        background: Rectangle { radius: 14; color: root.page === nav.target ? "#E5E5DB" : (nav.hovered ? "#F0EFE9" : "transparent") }
        onClicked: root.page = target
    }

    RowLayout {
        anchors.fill: parent; anchors.margins: 6; spacing: 24
        Panel {
            Layout.preferredWidth: 252; Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 8
                RowLayout { Layout.bottomMargin: 34; spacing: 12
                    Rectangle { width: 36; height: 36; radius: 12; color: root.green; Text { anchors.centerIn: parent; text: "◆"; color: "white"; font.pixelSize: 17 } }
                    Text { text: "Ledgera"; color: root.ink; font.pixelSize: 25; font.weight: Font.Bold }
                }
                NavButton { Layout.fillWidth: true; text: "Обзор"; glyph: "▦"; target: "overview" }
                NavButton { Layout.fillWidth: true; text: "Счета"; glyph: "▣"; target: "accounts" }
                NavButton { Layout.fillWidth: true; text: "Категории"; glyph: "◇"; target: "categories" }
                NavButton { Layout.fillWidth: true; text: "Операции"; glyph: "⇄"; target: "operations" }
                NavButton { Layout.fillWidth: true; text: "Аналитика"; glyph: "▥"; target: "analytics" }
                Item { Layout.fillHeight: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: root.line }
                NavButton { Layout.fillWidth: true; text: "Настройки"; glyph: "⚙"; target: "settings" }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.topMargin: 12; Layout.rightMargin: 18; spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Text { text: page === "overview" ? "Мои финансы" : page === "accounts" ? "Счета" : page === "categories" ? "Категории" : page === "operations" ? "Операции" : page === "analytics" ? "Аналитика" : "Настройки"; color: root.ink; font.pixelSize: 28; font.weight: Font.Bold }
                Item { Layout.fillWidth: true }
                TextField { visible: page === "overview" || page === "operations"; Layout.preferredWidth: 265; implicitHeight: 42; placeholderText: "Поиск по операциям..."; onTextChanged: root.searchText = text; leftPadding: 16; background: Rectangle { color: root.panel; radius: 12; border.width: 1; border.color: root.line } }
                ComboBox { id: currencyBox; Layout.preferredWidth: 126; implicitHeight: 42; model: ["RUB", "USD", "EUR"]; currentIndex: Math.max(0, model.indexOf(financeController.appCurrency)); onActivated: financeController.appCurrency = currentText; background: Rectangle { color: root.panel; radius: 12; border.width: 1; border.color: root.line } }
            }
            Loader { Layout.fillWidth: true; Layout.fillHeight: true; sourceComponent: page === "overview" ? overviewPage : page === "accounts" ? accountsPage : page === "categories" ? categoriesPage : page === "operations" ? operationsPage : page === "analytics" ? analyticsPage : settingsPage }
        }
    }

    Component {
        id: overviewPage
        ScrollView {
            id: overviewScroll
            clip: true
            contentWidth: availableWidth
            contentHeight: dashboard.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: dashboard
                width: overviewScroll.availableWidth
                spacing: 14

                Panel {
                    Layout.fillWidth: true; Layout.preferredHeight: 108
                    RowLayout { anchors.fill: parent; anchors.margins: 20; spacing: 28
                        Rectangle { width: 58; height: 58; radius: 17; color: root.green; Text { anchors.centerIn: parent; text: "▣"; color: "white"; font.pixelSize: 27 } }
                        ColumnLayout { spacing: 0; Text { text: "Все активы"; color: root.ink; font.pixelSize: 15 } Text { text: root.money(financeController.balanceMinorUnits, financeController.appCurrency, false); color: root.ink; font.pixelSize: 30; font.weight: Font.Bold } }
                        Item { Layout.fillWidth: true }
                        Repeater { model: root.assets; delegate: ColumnLayout { required property var modelData; Layout.preferredWidth: 120; Text { text: modelData.title; color: root.muted; font.pixelSize: 13 } Text { text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false); color: root.green2; font.pixelSize: 17; font.weight: Font.DemiBold } } }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Repeater { model: root.assets; delegate: Panel {
                        required property var modelData
                        Layout.preferredWidth: (dashboard.width - 28) / 3
                        Layout.minimumWidth: 240
                        Layout.preferredHeight: 118
                        color: financeController.selectedAsset === modelData.code ? root.green : root.panel
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: financeController.selectedAsset = modelData.code }
                        ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 10
                            RowLayout { Rectangle { width: 38; height: 38; radius: 19; color: financeController.selectedAsset === modelData.code ? "#59705E" : root.pale; Text { anchors.centerIn: parent; text: modelData.icon; color: financeController.selectedAsset === modelData.code ? "white" : root.green; font.pixelSize: 19 } } Text { text: modelData.title; color: financeController.selectedAsset === modelData.code ? "white" : root.ink; font.pixelSize: 17; font.weight: Font.DemiBold } }
                            Text { text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false); color: financeController.selectedAsset === modelData.code ? "white" : root.ink; font.pixelSize: 25; font.weight: Font.Bold }
                        }
                    } }
                }
                Panel {
                    Layout.fillWidth: true; Layout.preferredHeight: 145
                    ColumnLayout { anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout { Layout.fillWidth: true
                            Text { text: "Счета · " + root.assetTitle(financeController.selectedAsset); color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            Button {
                                id: addAccountButton
                                flat: true
                                text: "+  Добавить счёт"

                                contentItem: Text {
                                    text: addAccountButton.text
                                    color: root.green2
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: accountDialog.openForSelectedAsset()
                            }
                        }
                        ListView { Layout.fillWidth: true; Layout.fillHeight: true; orientation: ListView.Horizontal; spacing: 12; clip: true; model: [{id:"",name:"Все счета",balanceMinor:root.assetAmount(financeController.selectedAsset),currency:financeController.appCurrency}].concat(financeController.accounts)
                            delegate: Rectangle { required property var modelData; width: 245; height: 64; radius: 13; color: financeController.selectedAccountId === modelData.id ? root.green : root.panel; border.width: 1; border.color: financeController.selectedAccountId === modelData.id ? root.green : root.line
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: financeController.selectedAccountId = modelData.id }
                                RowLayout { anchors.fill: parent; anchors.margins: 13
                                    Text { text: "▣"; color: financeController.selectedAccountId === modelData.id ? "white" : root.green; font.pixelSize: 20 }
                                    ColumnLayout { spacing: 1; Text { text: modelData.name; color: financeController.selectedAccountId === modelData.id ? "white" : root.ink; font.pixelSize: 14; font.weight: Font.DemiBold } Text { text: root.money(modelData.balanceMinor, modelData.currency, false); color: financeController.selectedAccountId === modelData.id ? "#DDE7DF" : root.muted; font.pixelSize: 12 } }
                                    Item { Layout.fillWidth: true } Text { text: financeController.selectedAccountId === modelData.id ? "✓" : "›"; color: financeController.selectedAccountId === modelData.id ? "white" : root.ink; font.pixelSize: 18 }
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Panel {
                        Layout.preferredWidth: (dashboard.width - 14) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 210
                        ColumnLayout { anchors.fill: parent; anchors.margins: 16
                            Text { text: "Расходы по категориям"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                            RowLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
                                Repeater { model: root.categoryTotals(); delegate: ColumnLayout { required property var modelData; Layout.fillWidth: true; Layout.fillHeight: true; Item { Layout.fillHeight: true } Rectangle { Layout.alignment: Qt.AlignHCenter; width: 44; height: Math.max(5, Math.min(120, modelData.amount / Math.max(1, root.categoryTotals()[0].amount) * 120)); radius: 5; color: index === 0 ? root.green : "#9EAD96" } Text { Layout.alignment: Qt.AlignHCenter; text: modelData.label; color: root.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.maximumWidth: 80 } } }
                                Text { visible: root.categoryTotals().length === 0; text: "Добавьте расходы — здесь появится график"; color: root.muted; Layout.alignment: Qt.AlignCenter }
                            }
                        }
                    }
                    Panel {
                        Layout.preferredWidth: (dashboard.width - 14) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 210
                        ColumnLayout { anchors.fill: parent; anchors.margins: 16
                            Text { text: "Структура расходов"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold }
                            RowLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 24
                                Canvas { id: donut; width: 145; height: 145
                                    onPaint: { const ctx=getContext("2d");ctx.clearRect(0,0,width,height);const data=root.categoryTotals();let total=0;for(let i=0;i<data.length;++i)total+=data[i].amount;const colors=[root.green,"#71866F","#A2B19A","#CCD2BC","#E5DDC8"];let angle=-Math.PI/2;if(total===0){ctx.strokeStyle=root.line;ctx.lineWidth=25;ctx.beginPath();ctx.arc(72,72,48,0,Math.PI*2);ctx.stroke();return}for(let j=0;j<data.length;++j){const next=angle+data[j].amount/total*Math.PI*2;ctx.strokeStyle=colors[j];ctx.lineWidth=25;ctx.beginPath();ctx.arc(72,72,48,angle,next);ctx.stroke();angle=next} }
                                    Connections { target: financeController; function onTransactionsChanged() { donut.requestPaint() } function onSelectedAccountIdChanged() { donut.requestPaint() } function onSelectedAssetChanged() { donut.requestPaint() } }
                                }
                                ColumnLayout { Layout.fillWidth: true
                                    Repeater { model: root.categoryTotals(); delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Rectangle { width: 9; height: 9; radius: 5; color: [root.green,"#71866F","#A2B19A","#CCD2BC","#E5DDC8"][index] } Text { text: modelData.label; color: root.muted; Layout.fillWidth: true } Text { text: root.money(modelData.amount, financeController.appCurrency, false); color: root.ink; font.pixelSize: 11 } } }
                                    Item { Layout.fillHeight: true }
                                }
                            }
                        }
                    }
                }
                Panel { Layout.fillWidth: true; Layout.preferredHeight: 245
                    ColumnLayout { anchors.fill: parent; spacing: 0
                        RowLayout { Layout.fillWidth: true; Layout.margins: 14; Text { text: "История операций"; color: root.ink; font.pixelSize: 15; font.weight: Font.DemiBold } Item { Layout.fillWidth: true } SoftButton { text: "+  Операция"; highlighted: true; implicitWidth: 132; onClicked: operationDialog.openForNew() } }
                        TransactionTable { Layout.fillWidth: true; Layout.fillHeight: true; rows: root.visibleTransactions() }
                    }
                }
                Item { Layout.preferredHeight: 8 }
            }
        }
    }

    component TransactionTable: Item {
        property var rows: []
        ColumnLayout { anchors.fill: parent; spacing: 0
            Rectangle { Layout.fillWidth: true; height: 34; color: "#F6F5F1"; border.width: 1; border.color: root.line
                RowLayout { anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
                    Text { text: "Операция"; color: root.muted; font.pixelSize: 11; Layout.preferredWidth: 250 } Text { text: "Счёт"; color: root.muted; font.pixelSize: 11; Layout.preferredWidth: 170 } Text { text: "Категория"; color: root.muted; font.pixelSize: 11; Layout.fillWidth: true } Text { text: "Дата"; color: root.muted; font.pixelSize: 11; Layout.preferredWidth: 120 } Text { text: "Сумма"; color: root.muted; font.pixelSize: 11; Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                }
            }
            ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: parent.parent.rows
                delegate: Rectangle { required property var modelData; width: ListView.view.width; height: 38; color: index % 2 ? "#FAF9F6" : root.panel
                    RowLayout { anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
                        Text { text: modelData.description || (modelData.type === "income" ? "Доход" : "Расход"); color: root.ink; font.pixelSize: 12; Layout.preferredWidth: 250; elide: Text.ElideRight }
                        Text { text: root.accountName(modelData.accountId); color: root.muted; font.pixelSize: 12; Layout.preferredWidth: 170; elide: Text.ElideRight }
                        Text { text: modelData.categoryName; color: root.green; font.pixelSize: 11; Layout.fillWidth: true }
                        Text { text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy"); color: root.muted; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        Text { text: root.money(modelData.type === "income" ? modelData.amount : -modelData.amount, modelData.currency, true); color: modelData.type === "income" ? root.green2 : root.red; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.preferredWidth: 130; horizontalAlignment: Text.AlignRight }
                    }
                }
                Label { anchors.centerIn: parent; visible: parent.count === 0; text: "Операций пока нет"; color: root.muted }
            }
        }
    }

    Component { id: accountsPage; ColumnLayout { spacing: 14
        RowLayout { Layout.fillWidth: true; Repeater { model: root.assets; delegate: SoftButton { required property var modelData; text: modelData.title; highlighted: financeController.selectedAsset === modelData.code; implicitWidth: 130; onClicked: financeController.selectedAsset = modelData.code } } Item { Layout.fillWidth: true } SoftButton { text: "+ Добавить счёт"; highlighted: true; implicitWidth: 150; onClicked: accountDialog.openForSelectedAsset() } }
        GridView { Layout.fillWidth: true; Layout.fillHeight: true; cellWidth: 320; cellHeight: 150; clip: true; model: financeController.accounts
            delegate: Panel { required property var modelData; width: 300; height: 132; ColumnLayout { anchors.fill: parent; anchors.margins: 18; RowLayout { Text { text: "▣"; color: root.green; font.pixelSize: 24 } Text { text: modelData.name; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold } } Text { text: root.money(modelData.balanceMinor, modelData.currency, false); color: root.ink; font.pixelSize: 25; font.weight: Font.Bold } Text { text: modelData.currency + " · " + modelData.type; color: root.muted; font.pixelSize: 12 } } }
            Label { anchors.centerIn: parent; visible: parent.count === 0; text: "У этого актива пока нет счетов"; color: root.muted }
        }
    } }

    Component { id: categoriesPage; ColumnLayout {
        RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } SoftButton { text: "+ Управление категориями"; highlighted: true; implicitWidth: 210; onClicked: categoryDialog.openForManagement() } }
        Panel { Layout.fillWidth: true; Layout.fillHeight: true; ListView { anchors.fill: parent; anchors.margins: 12; clip: true; spacing: 6; model: financeController.categories
            delegate: Rectangle { required property var modelData; width: ListView.view.width; height: 52; radius: 10; color: "#F7F6F2"; RowLayout { anchors.fill: parent; anchors.margins: 12; Text { text: "◇"; color: root.green; font.pixelSize: 20 } Text { text: modelData.label; color: root.ink; Layout.fillWidth: true } Text { text: modelData.type === "income" ? "Доход" : "Расход"; color: root.muted } } }
        } }
    } }

    Component { id: operationsPage; ColumnLayout {
        RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } SoftButton { text: "+ Операция"; highlighted: true; implicitWidth: 140; onClicked: operationDialog.openForNew() } }
        Panel { Layout.fillWidth: true; Layout.fillHeight: true; TransactionTable { anchors.fill: parent; anchors.margins: 1; rows: root.visibleTransactions() } }
    } }

    Component { id: analyticsPage; RowLayout { spacing: 14
        Panel { Layout.fillWidth: true; Layout.fillHeight: true; ColumnLayout { anchors.fill: parent; anchors.margins: 24; Text { text: "Доходы"; color: root.muted } Text { text: root.money(financeController.incomeMinorUnits, financeController.appCurrency, false); color: root.green2; font.pixelSize: 30; font.weight: Font.Bold } Rectangle { Layout.fillWidth: true; height: 1; color: root.line } Text { text: "Расходы"; color: root.muted } Text { text: root.money(financeController.expenseMinorUnits, financeController.appCurrency, false); color: root.red; font.pixelSize: 30; font.weight: Font.Bold } Item { Layout.fillHeight: true } } }
        Panel { Layout.fillWidth: true; Layout.fillHeight: true; ColumnLayout { anchors.fill: parent; anchors.margins: 24; Text { text: "Категории расходов"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold } Repeater { model: root.categoryTotals(); delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Text { text: modelData.label; color: root.muted; Layout.fillWidth: true } Text { text: root.money(modelData.amount, financeController.appCurrency, false); color: root.ink } } } Item { Layout.fillHeight: true } } }
    } }

    Component { id: settingsPage; Panel { ColumnLayout { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 28; Text { text: "Основная валюта"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold } Text { text: "Все итоговые суммы пересчитываются в эту валюту."; color: root.muted } ComboBox { model: ["RUB","USD","EUR"]; currentIndex: Math.max(0, model.indexOf(financeController.appCurrency)); onActivated: financeController.appCurrency = currentText; implicitWidth: 180 } } } }

    Dialog { id: accountDialog; width: 500; modal: true; anchors.centerIn: parent; padding: 24; property var accountTypes: []
        function openForSelectedAsset() { accountTypes = financeController.selectedAsset === "fiat" ? [{label:"Наличные",value:"cash"},{label:"Дебетовая карта",value:"debit_card"},{label:"Кредитная карта",value:"credit_card"},{label:"Накопительный",value:"savings"}] : financeController.selectedAsset === "crypto" ? [{label:"Криптокошелёк",value:"crypto_wallet"},{label:"Другой",value:"other"}] : [{label:"Брокер",value:"brokerage"},{label:"Вклад",value:"deposit"},{label:"Другой",value:"other"}]; accountNameField.clear(); accountBalanceField.clear(); accountTypeBox.currentIndex=0; accountError.text=""; open() }
        background: Rectangle { color: root.panel; radius: 18; border.width: 1; border.color: root.line }
        contentItem: ColumnLayout { spacing: 14
            Text { text: "Новый счёт · " + root.assetTitle(financeController.selectedAsset); color: root.ink; font.pixelSize: 21; font.weight: Font.Bold }
            TextField { id: accountNameField; Layout.fillWidth: true; placeholderText: "Название счёта" }
            ComboBox { id: accountTypeBox; Layout.fillWidth: true; model: accountDialog.accountTypes; textRole: "label" }
            ComboBox { id: accountCurrencyBox; Layout.fillWidth: true; model: ["RUB","USD","EUR"] }
            TextField { id: accountBalanceField; Layout.fillWidth: true; placeholderText: "Начальный баланс"; validator: DoubleValidator { bottom: -999999999; top: 999999999; decimals: 2 } }
            Text { id: accountError; color: root.red; font.pixelSize: 12 }
            RowLayout { Item { Layout.fillWidth: true } SoftButton { text: "Отмена"; onClicked: accountDialog.close() } SoftButton { text: "Добавить"; highlighted: true; onClicked: { const minor=Math.round((Number(accountBalanceField.text.replace(",","."))||0)*100);const type=accountDialog.accountTypes[accountTypeBox.currentIndex].value;if(financeController.addAccount(accountNameField.text,type,accountCurrencyBox.currentText,minor))accountDialog.close();else accountError.text="Проверьте название: оно должно быть уникальным" } } }
        }
    }

    Dialog { id: operationDialog; width: 520; modal: true; anchors.centerIn: parent; padding: 24
        function openForNew() { operationError.text="";operationAmount.clear();operationDescription.clear();operationType.currentIndex=1;open() }
        background: Rectangle { color: root.panel; radius: 18; border.width: 1; border.color: root.line }
        contentItem: ColumnLayout { spacing: 14
            Text { text: "Новая операция"; color: root.ink; font.pixelSize: 21; font.weight: Font.Bold }
            ComboBox { id: operationType; Layout.fillWidth: true; model: ["Доход","Расход"] }
            ComboBox { id: operationAccount; Layout.fillWidth: true; model: financeController.accounts; textRole: "name" }
            ComboBox { id: operationCategory; Layout.fillWidth: true; model: financeController.categories.filter(function(c){return c.type===(operationType.currentIndex===0?"income":"expense")}); textRole: "label" }
            TextField { id: operationAmount; Layout.fillWidth: true; placeholderText: "Сумма"; validator: DoubleValidator { bottom: 0.01; top: 999999999; decimals: 2 } }
            TextField { id: operationDescription; Layout.fillWidth: true; placeholderText: "Описание" }
            Text { id: operationError; color: root.red; font.pixelSize: 12 }
            RowLayout { Item { Layout.fillWidth: true } SoftButton { text: "Отмена"; onClicked: operationDialog.close() } SoftButton { text: "Сохранить"; highlighted: true; onClicked: { if(!financeController.accounts.length){operationError.text="Сначала добавьте счёт";return}if(!operationCategory.model.length){operationError.text="Сначала добавьте категорию";return}const account=financeController.accounts[operationAccount.currentIndex],category=operationCategory.model[operationCategory.currentIndex],minor=Math.round((Number(operationAmount.text.replace(",","."))||0)*100);const ok=operationType.currentIndex===0?financeController.addIncome(minor,operationDescription.text,category.value,account.currency,account.id):financeController.addExpense(minor,operationDescription.text,category.value,account.currency,account.id);if(ok)operationDialog.close();else operationError.text="Укажите корректную сумму и счёт" } } }
        }
    }

    Dialog { id: categoryDialog; width: 560; height: 620; modal: true; anchors.centerIn: parent; padding: 22; property string editingId: ""
        function openForManagement(){editingId="";categoryNameField.clear();categoryError.text="";open()}
        function startEdit(row){editingId=row.value;categoryNameField.text=row.label;categoryType.currentIndex=row.type==="income"?0:1}
        background: Rectangle { color: root.panel; radius: 18; border.width: 1; border.color: root.line }
        contentItem: ColumnLayout { spacing: 12
            Text { text: "Категории"; color: root.ink; font.pixelSize: 21; font.weight: Font.Bold }
            RowLayout { Layout.fillWidth: true; TextField { id: categoryNameField; Layout.fillWidth: true; placeholderText: "Название категории" } ComboBox { id: categoryType; model: ["Доход","Расход"]; enabled: !categoryDialog.editingId } SoftButton { text: categoryDialog.editingId ? "Сохранить" : "Добавить"; highlighted: true; onClicked: { const ok=categoryDialog.editingId?financeController.renameCategory(categoryDialog.editingId,categoryNameField.text):financeController.addCategory(categoryNameField.text,categoryType.currentIndex===0?"income":"expense");if(ok){categoryDialog.editingId="";categoryNameField.clear();categoryError.text=""}else categoryError.text="Не удалось сохранить категорию" } } }
            Text { id: categoryError; color: root.red; font.pixelSize: 12 }
            ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: financeController.categories
                delegate: Rectangle { required property var modelData; width: ListView.view.width; height: 48; radius: 9; color: "#F4F3EE"; RowLayout { anchors.fill: parent; anchors.margins: 10; Text { text: modelData.label; color: root.ink; Layout.fillWidth: true } Text { text: modelData.type === "income" ? "Доход" : "Расход"; color: root.muted; font.pixelSize: 11 } Button { flat: true; text: "✎"; onClicked: categoryDialog.startEdit(modelData) } Button { flat: true; text: "×"; onClicked: { financeController.deleteCategory(modelData.value); if(categoryDialog.editingId===modelData.value){categoryDialog.editingId="";categoryNameField.clear()} } } } }
            }
            RowLayout { Item { Layout.fillWidth: true } SoftButton { text: "Закрыть"; onClicked: categoryDialog.close() } }
        }
    }
}
