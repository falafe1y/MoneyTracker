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
    title: "Test Tracker"
    color: root.canvas

    // Color palette
    // Ivory + indigo foundation. Indigo is the only primary accent;
    // green and terracotta below are reserved for financial semantics.
    readonly property color canvas: "#F5F3E4"
    readonly property color panel: "#FFFFF8"
    readonly property color soft: "#FFFFF0"
    readonly property color line: "#D8D7C7"
    readonly property color ink: "#031528"
    readonly property color muted: "#687483"

    readonly property color green: "#031528"
    readonly property color greenDark: "#020D1A"
    readonly property color green2: "#294477"
    readonly property color greenSoft: "#536A98"
    readonly property color pale: "#E4E8F1"
    readonly property color paleText: "#E9EDF6"
    readonly property color red: "#B94F48"

    readonly property color navSelected: "#294477"
    readonly property color navHovered: "#142B48"

    readonly property color chartGreen1: "#294477"
    readonly property color chartGreen2: "#536A98"
    readonly property color chartGreen3: "#8795B3"
    readonly property color chartGreen4: "#B68C62"
    readonly property color chartGreen5: "#708C88"

    readonly property color tableHeader: "#F1F0DF"
    readonly property color tableRowAlt: "#FAF9EC"
    readonly property color categoryRow: "#FFFFF0"
    readonly property color categoryEditRow: "#F4F3E3"
    readonly property color controlHovered: "#F3F1E3"

    readonly property color incomePanel: "#E8F0E9"
    readonly property color expensePanel: "#F5E6E2"
    readonly property color income: "#3F735F"

    readonly property color white: "#FFFFF0"
    readonly property color transparentColor: "transparent"

    property string page: "overview"
    property string searchText: ""
    readonly property var assets: [
        {
            code: "fiat",
            title: "Фиат",
            icon: ""
        },
        {
            code: "crypto",
            title: "Крипта",
            icon: ""
        },
        {
            code: "investment",
            title: "Инвестиции",
            icon: ""
        }
    ]

    function symbol(code) {
        return code === "USD" ? "$" : code === "EUR" ? "€" : "₽";
    }

    function money(minor, code, sign) {
        const value = Number(minor) / 100;
        const prefix = sign ? (value >= 0 ? "+" : "−") : (value < 0 ? "−" : "");
        return prefix + Math.abs(value).toLocaleString(Qt.locale("ru_RU"), "f", 0) + " " + symbol(code || financeController.appCurrency);
    }

    function assetTitle(code) {
        for (let i = 0; i < assets.length; ++i)
            if (assets[i].code === code)
                return assets[i].title;
        return "Фиат";
    }

    function assetAmount(code) {
        const rows = financeController.assetSummaries;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].code === code)
                return rows[i].balanceMinor;
        return 0;
    }

    function accountName(id) {
        const rows = financeController.allAccounts;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === id)
                return rows[i].name;
        return "Другой счёт";
    }

    function amountForInput(minor) {
        const value = Math.round(Math.abs(Number(minor)));
        const whole = Math.floor(value / 100);
        const cents = value % 100;
        return cents === 0 ? String(whole)
                           : String(whole) + "." + (cents < 10 ? "0" : "") + String(cents);
    }

    function transactionSignedAmount(row) {
        return row.type === "income" || (row.type === "transfer" && row.direction === "in")
             ? row.amount : -row.amount;
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

    function categoryTotals() {
        const values = {}, names = {}, rows = visibleTransactions();
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].type !== "expense")
                continue;
            const id = rows[i].categoryId || "none";
            values[id] = (values[id] || 0) + Number(rows[i].displayAmount);
            names[id] = rows[i].categoryName || "Без категории";
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

    component Panel: Rectangle {
        color: root.panel
        radius: 16
        border.width: 1
        border.color: root.line
    }
    component SoftButton: Button {
        id: control
        property bool destructive: false
        hoverEnabled: true
        implicitHeight: 42
        contentItem: Text {
            text: control.text
            color: control.destructive || control.highlighted ? root.white : root.ink
            font.pixelSize: 14
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 10
            color: control.destructive
                   ? (control.down || control.hovered ? Qt.darker(root.red, 1.08) : root.red)
                   : control.highlighted
                     ? (control.down ? root.greenDark : root.green)
                     : (control.hovered ? root.soft : root.panel)
            border.width: control.destructive || control.highlighted ? 0 : 1
            border.color: root.line
        }
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
            color: menuItem.destructive ? root.red : root.ink
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
    component AppTextField: TextField {
        id: field
        implicitHeight: 44
        leftPadding: 14
        rightPadding: 14
        color: root.ink
        placeholderTextColor: root.muted
        selectionColor: root.greenSoft
        selectedTextColor: root.white
        font.pixelSize: 14

        background: Rectangle {
            radius: 11
            color: field.activeFocus ? root.panel : root.soft
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus ? root.green2 : root.line
        }
    }
    component AppComboBox: ComboBox {
        id: combo
        hoverEnabled: true
        implicitHeight: 44
        leftPadding: 14
        rightPadding: 42
        font.pixelSize: 14

        contentItem: Text {
            leftPadding: 0
            rightPadding: 0
            text: combo.displayText
            color: root.ink
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: combo.width - width - 15
            y: (combo.height - height) / 2 - 1
            text: combo.popup.visible ? "⌃" : "⌄"
            color: root.green2
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        background: Rectangle {
            radius: 11
            color: combo.pressed || combo.popup.visible ? root.panel : combo.hovered ? root.controlHovered : root.soft
            border.width: combo.popup.visible ? 2 : 1
            border.color: combo.popup.visible ? root.green2 : root.line
        }

        delegate: ItemDelegate {
            id: optionDelegate
            required property var modelData
            width: combo.width - 12
            height: 40
            leftPadding: 12
            hoverEnabled: true
            highlighted: combo.highlightedIndex === index
            contentItem: Text {
                text: combo.textRole ? modelData[combo.textRole] : modelData
                color: root.ink
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                radius: 8
                color: optionDelegate.hovered ? root.controlHovered : root.transparentColor
            }
        }

        popup: Popup {
            y: combo.height + 6
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 12, 260)
            padding: 6
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                spacing: 2
                ScrollIndicator.vertical: ScrollIndicator { }
            }

            background: Rectangle {
                radius: 12
                color: root.panel
                border.width: 1
                border.color: root.line
            }
        }
    }
    component NavButton: Button {
        id: nav
        property string glyph: ""
        property string target: ""
        flat: true
        hoverEnabled: true
        implicitHeight: 54
        contentItem: RowLayout {
            spacing: 14
            Text {
                text: nav.glyph
                color: root.page === nav.target ? root.white : root.paleText
                font.pixelSize: 20
                Layout.preferredWidth: 26
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: nav.text
                color: root.page === nav.target ? root.white : root.paleText
                font.pixelSize: 15
                Layout.fillWidth: true
            }
        }
        background: Rectangle {
            radius: 14
            color: root.page === nav.target ? root.navSelected : (nav.hovered ? root.navHovered : root.transparentColor)
        }
        onClicked: root.page = target
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 24
        Panel {
            Layout.preferredWidth: 252
            Layout.fillHeight: true
            color: root.ink
            border.color: root.ink
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8
                RowLayout {
                    Layout.bottomMargin: 34
                    spacing: 12
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 12
                        color: root.soft
                        Text {
                            anchors.centerIn: parent
                            text: "◆"
                            color: root.ink
                            font.pixelSize: 17
                        }
                    }
                    Text {
                        text: "Ledgera"
                        color: root.soft
                        font.pixelSize: 25
                        font.weight: Font.Bold
                    }
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Обзор"
                    glyph: "▦"
                    target: "overview"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Счета"
                    glyph: "▣"
                    target: "accounts"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Категории"
                    glyph: "◇"
                    target: "categories"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Операции"
                    glyph: "⇄"
                    target: "operations"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Аналитика"
                    glyph: "▥"
                    target: "analytics"
                }
                Item {
                    Layout.fillHeight: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: root.greenSoft
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Настройки"
                    glyph: "⚙"
                    target: "settings"
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.rightMargin: 18
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: page === "overview" ? "Мои финансы" : page === "accounts" ? "Счета" : page === "categories" ? "Категории" : page === "operations" ? "Операции" : page === "analytics" ? "Аналитика" : "Настройки"
                    color: root.ink
                    font.pixelSize: 28
                    font.weight: Font.Bold
                }
                Item {
                    Layout.fillWidth: true
                }
                AppTextField {
                    visible: page === "overview" || page === "operations"
                    Layout.preferredWidth: 265
                    implicitHeight: 42
                    placeholderText: "Поиск по операциям..."
                    onTextChanged: root.searchText = text
                }
                AppComboBox {
                    id: currencyBox
                    Layout.preferredWidth: 126
                    implicitHeight: 42
                    model: ["RUB", "USD", "EUR"]
                    currentIndex: Math.max(0, model.indexOf(financeController.appCurrency))
                    onActivated: financeController.appCurrency = currentText
                }
            }
            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: page === "overview" ? overviewPage : page === "accounts" ? accountsPage : page === "categories" ? categoriesPage : page === "operations" ? operationsPage : page === "analytics" ? analyticsPage : settingsPage
            }
        }
    }

    Component {
        id: overviewPage
        ScrollView {
            id: overviewScroll
            clip: true

            // ===== SCROLLBARS DISABLED =====
            // Keep scrolling itself enabled, but never draw scrollbars.
            // To restore them later, change AlwaysOff to AsNeeded.
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
            // ===============================

            // ===== CONTENT CLIP SAFETY MARGIN =====
            // Keep panel borders one physical pixel away from ScrollView's clip edge.
            // If outlines ever need to touch the viewport again, set this to 0.
            readonly property int contentEdgeMargin: 2

            contentWidth: availableWidth
            contentHeight: dashboard.implicitHeight

            ColumnLayout {
                id: dashboard
                x: overviewScroll.contentEdgeMargin
                width: Math.max(0, overviewScroll.availableWidth - overviewScroll.contentEdgeMargin * 2)
                spacing: 14

                Panel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 108
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 28
                        Rectangle {
                            width: 58
                            height: 58
                            radius: 17
                            color: root.green
                            Text {
                                anchors.centerIn: parent
                                text: "▣"
                                color: root.white
                                font.pixelSize: 27
                            }
                        }
                        ColumnLayout {
                            spacing: 0
                            Text {
                                text: "Все активы"
                                color: root.ink
                                font.pixelSize: 15
                            }
                            Text {
                                text: root.money(financeController.balanceMinorUnits, financeController.appCurrency, false)
                                color: root.ink
                                font.pixelSize: 30
                                font.weight: Font.Bold
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: root.assets
                            delegate: ColumnLayout {
                                required property var modelData
                                Layout.preferredWidth: 120
                                Text {
                                    text: modelData.title
                                    color: root.muted
                                    font.pixelSize: 13
                                }
                                Text {
                                    text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false)
                                    color: root.green2
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Repeater {
                        model: root.assets
                        delegate: Panel {
                            required property var modelData
                            Layout.preferredWidth: (dashboard.width - 28) / 3
                            Layout.minimumWidth: 240
                            Layout.preferredHeight: 118
                            color: financeController.selectedAsset === modelData.code ? root.green : root.panel
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: financeController.selectedAsset = modelData.code
                            }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 10
                                RowLayout {
                                    Rectangle {
                                        width: 38
                                        height: 38
                                        radius: 19
                                        color: financeController.selectedAsset === modelData.code ? root.greenSoft : root.pale
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.icon
                                            color: financeController.selectedAsset === modelData.code ? root.white : root.green
                                            font.pixelSize: 19
                                        }
                                    }
                                    Text {
                                        text: modelData.title
                                        color: financeController.selectedAsset === modelData.code ? root.white : root.ink
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Text {
                                    text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false)
                                    color: financeController.selectedAsset === modelData.code ? root.white : root.ink
                                    font.pixelSize: 25
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }
                Panel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 145
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Счета · " + root.assetTitle(financeController.selectedAsset)
                                color: root.ink
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Item {
                                Layout.fillWidth: true
                            }
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
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            orientation: ListView.Horizontal
                            spacing: 12
                            clip: true
                            // Scrollbars intentionally hidden application-wide.
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                            model: [
                                {
                                    id: "",
                                    name: "Все счета",
                                    balanceMinor: root.assetAmount(financeController.selectedAsset),
                                    currency: financeController.appCurrency
                                }
                            ].concat(financeController.accounts)
                            delegate: Rectangle {
                                required property var modelData
                                width: 245
                                height: 64
                                radius: 13
                                color: financeController.selectedAccountId === modelData.id ? root.green : root.panel
                                border.width: 1
                                border.color: financeController.selectedAccountId === modelData.id ? root.green : root.line
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: financeController.selectedAccountId = modelData.id
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 13
                                    Text {
                                        text: "▣"
                                        color: financeController.selectedAccountId === modelData.id ? root.white : root.green
                                        font.pixelSize: 20
                                    }
                                    ColumnLayout {
                                        spacing: 1
                                        Text {
                                            text: modelData.name
                                            color: financeController.selectedAccountId === modelData.id ? root.white : root.ink
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            text: root.money(modelData.balanceMinor, modelData.currency, false)
                                            color: financeController.selectedAccountId === modelData.id ? root.paleText : root.muted
                                            font.pixelSize: 12
                                        }
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: financeController.selectedAccountId === modelData.id ? "✓" : "›"
                                        color: financeController.selectedAccountId === modelData.id ? root.white : root.ink
                                        font.pixelSize: 18
                                    }
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Panel {
                        Layout.preferredWidth: (dashboard.width - 14) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 210
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            Text {
                                text: "Расходы по категориям"
                                color: root.ink
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 18
                                Repeater {
                                    model: root.categoryTotals()
                                    delegate: ColumnLayout {
                                        required property int index
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Item {
                                            Layout.fillHeight: true
                                        }
                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 44
                                            height: Math.max(5, Math.min(120, modelData.amount / Math.max(1, root.categoryTotals()[0].amount) * 120))
                                            radius: 5
                                            color: index === 0 ? root.green : root.chartGreen2
                                            border.width: 1
                                            border.color: root.line
                                        }
                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: modelData.label
                                            color: root.muted
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.maximumWidth: 80
                                        }
                                    }
                                }
                                Text {
                                    visible: root.categoryTotals().length === 0
                                    text: "Добавьте расходы — здесь появится график"
                                    color: root.muted
                                    Layout.alignment: Qt.AlignCenter
                                }
                            }
                        }
                    }
                    Panel {
                        Layout.preferredWidth: (dashboard.width - 14) / 2
                        Layout.minimumWidth: 360
                        Layout.preferredHeight: 210
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            Text {
                                text: "Структура расходов"
                                color: root.ink
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 24
                                Canvas {
                                    id: donut
                                    width: 145
                                    height: 145
                                    onPaint: {
                                        const ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);
                                        const data = root.categoryTotals();
                                        let total = 0;
                                        for (let i = 0; i < data.length; ++i)
                                            total += data[i].amount;
                                        const colors = [root.green, root.chartGreen1, root.chartGreen3, root.chartGreen4, root.chartGreen5];
                                        let angle = -Math.PI / 2;
                                        if (total === 0) {
                                            ctx.strokeStyle = root.line;
                                            ctx.lineWidth = 25;
                                            ctx.beginPath();
                                            ctx.arc(72, 72, 48, 0, Math.PI * 2);
                                            ctx.stroke();
                                            return;
                                        }
                                        for (let j = 0; j < data.length; ++j) {
                                            const next = angle + data[j].amount / total * Math.PI * 2;
                                            ctx.strokeStyle = colors[j];
                                            ctx.lineWidth = 25;
                                            ctx.beginPath();
                                            ctx.arc(72, 72, 48, angle, next);
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
                                    Repeater {
                                        model: root.categoryTotals()
                                        delegate: RowLayout {
                                            required property int index
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Rectangle {
                                                width: 9
                                                height: 9
                                                radius: 5
                                                color: [root.green, root.chartGreen1, root.chartGreen3, root.chartGreen4, root.chartGreen5][index]
                                            }
                                            Text {
                                                text: modelData.label
                                                color: root.muted
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: root.money(modelData.amount, financeController.appCurrency, false)
                                                color: root.ink
                                                font.pixelSize: 11
                                            }
                                        }
                                    }
                                    Item {
                                        Layout.fillHeight: true
                                    }
                                }
                            }
                        }
                    }
                }
                TransactionBlock {
                    Layout.fillWidth: true
                    expandToContent: true
                    title: "История операций"
                    rows: root.visibleTransactions()
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

        property string title: "История операций"
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
                    color: root.ink
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Item {
                    Layout.fillWidth: true
                }

                SoftButton {
                    text: "+  Операция"
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
                        text: "Операция"
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: 250
                    }
                    Text {
                        text: "Счёт"
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: 170
                    }
                    Text {
                        text: "Категория"
                        color: root.muted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Дата"
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: 120
                    }
                    Text {
                        text: "Сумма"
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: 130
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
                    radius: transactionTable.expandToContent
                            ? transactionTable.bottomCornerRadius
                            : (transactionList.contentHeight >= parent.height - 0.5
                               ? transactionTable.bottomCornerRadius
                               : 0)

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
                                    text: modelData.description || (modelData.type === "transfer" ? "Перевод" : modelData.type === "income" ? "Доход" : "Расход")
                                    color: root.ink
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 250
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: root.accountName(modelData.accountId)
                                    color: root.muted
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 170
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.categoryName
                                    color: root.green
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                    color: root.muted
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 120
                                }
                                Text {
                                    text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                    color: modelData.type === "transfer" ? root.green2 : modelData.type === "income" ? root.income : root.red
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    Layout.preferredWidth: 130
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
                                text: modelData.description || (modelData.type === "transfer" ? "Перевод" : modelData.type === "income" ? "Доход" : "Расход")
                                color: root.ink
                                font.pixelSize: 12
                                Layout.preferredWidth: 250
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.accountName(modelData.accountId)
                                color: root.muted
                                font.pixelSize: 12
                                Layout.preferredWidth: 170
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.categoryName
                                color: root.green
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                            Text {
                                text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                color: root.muted
                                font.pixelSize: 12
                                Layout.preferredWidth: 120
                            }
                            Text {
                                text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                color: modelData.type === "transfer" ? root.green2 : modelData.type === "income" ? root.income : root.red
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 130
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
                    text: "Операций пока нет"
                    color: root.muted
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
                    text: "+ Добавить счёт"
                    highlighted: true
                    implicitWidth: 150
                    onClicked: accountDialog.openForSelectedAsset()
                }
            }
            GridView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                cellWidth: 320
                cellHeight: 150
                clip: true
                // Scrollbars intentionally hidden application-wide.
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                model: financeController.accounts
                delegate: Panel {
                    required property var modelData
                    width: 300
                    height: 132
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Text {
                                text: "▣"
                                color: root.green
                                font.pixelSize: 24
                            }
                            Text {
                                text: modelData.name
                                color: root.ink
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            text: root.money(modelData.balanceMinor, modelData.currency, false)
                            color: root.ink
                            font.pixelSize: 25
                            font.weight: Font.Bold
                        }
                        Text {
                            text: modelData.currency + " · " + modelData.type
                            color: root.muted
                            font.pixelSize: 12
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                    text: "У этого актива пока нет счетов"
                    color: root.muted
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
                    text: "+ Управление категориями"
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
                        //     color: root.ink
                        //     font.pixelSize: 17
                        //     font.weight: Font.DemiBold
                        // }

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
                                        color: root.ink
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: "Доход"
                                        color: root.muted
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0

                                text: "Категорий доходов пока нет"
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
                        //     color: root.ink
                        //     font.pixelSize: 17
                        //     font.weight: Font.DemiBold
                        // }

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
                                        color: root.ink
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: "Расход"
                                        color: root.muted
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0

                                text: "Категорий расходов пока нет"
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
            title: "История операций"
            rows: root.visibleTransactions()
        }
    }

    Component {
        id: analyticsPage
        RowLayout {
            spacing: 14
            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    Text {
                        text: "Доходы"
                        color: root.muted
                    }
                    Text {
                        text: root.money(financeController.incomeMinorUnits, financeController.appCurrency, false)
                        color: root.income
                        font.pixelSize: 30
                        font.weight: Font.Bold
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: root.line
                    }
                    Text {
                        text: "Расходы"
                        color: root.muted
                    }
                    Text {
                        text: root.money(financeController.expenseMinorUnits, financeController.appCurrency, false)
                        color: root.red
                        font.pixelSize: 30
                        font.weight: Font.Bold
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    Text {
                        text: "Категории расходов"
                        color: root.ink
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: root.categoryTotals()
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text {
                                text: modelData.label
                                color: root.muted
                                Layout.fillWidth: true
                            }
                            Text {
                                text: root.money(modelData.amount, financeController.appCurrency, false)
                                color: root.ink
                            }
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }

    Component {
        id: settingsPage
        Panel {
            ColumnLayout {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 28
                Text {
                    text: "Основная валюта"
                    color: root.ink
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Все итоговые суммы пересчитываются в эту валюту."
                    color: root.muted
                }
                AppComboBox {
                    model: ["RUB", "USD", "EUR"]
                    currentIndex: Math.max(0, model.indexOf(financeController.appCurrency))
                    onActivated: financeController.appCurrency = currentText
                    implicitWidth: 180
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
        function openForSelectedAsset() {
            accountTypes = financeController.selectedAsset === "fiat" ? [
                {
                    label: "Наличные",
                    value: "cash"
                },
                {
                    label: "Дебетовая карта",
                    value: "debit_card"
                },
                {
                    label: "Кредитная карта",
                    value: "credit_card"
                },
                {
                    label: "Накопительный",
                    value: "savings"
                }
            ] : financeController.selectedAsset === "crypto" ? [
                {
                    label: "Криптокошелёк",
                    value: "crypto_wallet"
                },
                {
                    label: "Другой",
                    value: "other"
                }
            ] : [
                {
                    label: "Брокер",
                    value: "brokerage"
                },
                {
                    label: "Вклад",
                    value: "deposit"
                },
                {
                    label: "Другой",
                    value: "other"
                }
            ];
            accountNameField.clear();
            accountBalanceField.clear();
            accountTypeBox.currentIndex = 0;
            accountError.text = "";
            open();
        }
        background: Rectangle {
            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: "Новый счёт · " + root.assetTitle(financeController.selectedAsset)
                color: root.ink
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppTextField {
                id: accountNameField
                Layout.fillWidth: true
                placeholderText: "Название счёта"
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
            }
            AppTextField {
                id: accountBalanceField
                Layout.fillWidth: true
                placeholderText: "Начальный баланс"
                validator: DoubleValidator {
                    bottom: -999999999
                    top: 999999999
                    decimals: 2
                }
            }
            Text {
                id: accountError
                color: root.red
                font.pixelSize: 12
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: "Отмена"
                    onClicked: accountDialog.close()
                }
                SoftButton {
                    text: "Добавить"
                    highlighted: true
                    onClicked: {
                        const minor = Math.round((Number(accountBalanceField.text.replace(",", ".")) || 0) * 100);
                        const type = accountDialog.accountTypes[accountTypeBox.currentIndex].value;
                        if (financeController.addAccount(accountNameField.text, type, accountCurrencyBox.currentText, minor))
                            accountDialog.close();
                        else
                            accountError.text = "Проверьте название: оно должно быть уникальным";
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
            text: "Редактировать"
            enabled: transactionContextMenu.transactionData
                  && transactionContextMenu.transactionData.type !== "transfer"
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
            text: "Удалить"
            destructive: true
            enabled: transactionContextMenu.transactionData
                  && transactionContextMenu.transactionData.type !== "transfer"
            onTriggered: {
                if (transactionContextMenu.transactionData)
                    deleteTransactionDialog.openFor(transactionContextMenu.transactionData);
            }
        }

        background: Rectangle {
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

        onClosed: transactionData = null

        background: Rectangle {
            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                text: "Удалить операцию?"
                color: root.ink
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                text: "Это действие нельзя отменить. Баланс и статистика будут пересчитаны."
                color: root.muted
                font.pixelSize: 13
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
                                     ? "Перевод"
                                     : deleteTransactionDialog.transactionData.type === "income"
                                         ? "Доход"
                                         : "Расход"))
                              : ""
                        color: root.ink
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
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                id: deleteTransactionError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: "Отмена"
                    onClicked: deleteTransactionDialog.close()
                }
                SoftButton {
                    text: "Удалить"
                    destructive: true
                    onClicked: {
                        const row = deleteTransactionDialog.transactionData;
                        if (row && financeController.deleteTransaction(row.id))
                            deleteTransactionDialog.close();
                        else
                            deleteTransactionError.text = "Не удалось удалить операцию";
                    }
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

        function openForNew() {
            editingId = "";
            editingTransaction = null;
            operationError.text = "";
            operationAmount.clear();
            operationDescription.clear();
            operationType.currentIndex = 1;
            operationAccount.currentIndex = root.indexByRole(
                financeController.accounts,
                "id",
                financeController.selectedAccountId
            );
            operationCategory.currentIndex = 0;
            transferTargetAccount.currentIndex = financeController.allAccounts.length > 1 ? 1 : 0;
            open();
        }

        function openForEdit(row) {
            editingId = row.id;
            editingTransaction = row;
            operationError.text = "";
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
            operationDescription.text = row.description || "";
            open();
        }

        onClosed: {
            editingId = "";
            editingTransaction = null;
        }

        background: Rectangle {
            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 14
            Text {
                text: operationDialog.editingId
                      ? "Редактирование операции"
                      : "Новая операция"
                color: root.ink
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            AppComboBox {
                id: operationType
                Layout.fillWidth: true
                model: ["Доход", "Расход", "Перевод"]
                enabled: !operationDialog.editingId
                onActivated: operationCategory.currentIndex = 0
            }
            Text {
                visible: operationType.currentIndex === 2
                text: "Откуда"
                color: root.muted
                font.pixelSize: 12
            }
            AppComboBox {
                id: operationAccount
                Layout.fillWidth: true
                model: operationType.currentIndex === 2
                     ? financeController.allAccounts
                     : financeController.accounts
                textRole: operationType.currentIndex === 2 ? "displayName" : "name"
            }
            Text {
                visible: operationType.currentIndex === 2
                text: "Куда"
                color: root.muted
                font.pixelSize: 12
            }
            AppComboBox {
                id: transferTargetAccount
                Layout.fillWidth: true
                visible: operationType.currentIndex === 2
                model: financeController.allAccounts
                textRole: "displayName"
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
            }
            AppTextField {
                id: operationAmount
                Layout.fillWidth: true
                placeholderText: "Сумма"
                validator: DoubleValidator {
                    bottom: 0.01
                    top: 999999999
                    decimals: 2
                }
            }
            AppTextField {
                id: operationDescription
                Layout.fillWidth: true
                placeholderText: "Описание"
            }
            Text {
                id: operationError
                color: root.red
                font.pixelSize: 12
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: "Отмена"
                    onClicked: operationDialog.close()
                }
                SoftButton {
                    text: "Сохранить"
                    highlighted: true
                    onClicked: {
                        if (!financeController.allAccounts.length) {
                            operationError.text = "Сначала добавьте счёт";
                            return;
                        }
                        const isTransfer = operationType.currentIndex === 2;
                        if (isTransfer && financeController.allAccounts.length < 2) {
                            operationError.text = "Для перевода нужны два счёта";
                            return;
                        }
                        if (!isTransfer && !operationCategory.model.length) {
                            operationError.text = "Сначала добавьте категорию";
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
                        const ok = isTransfer
                                 ? financeController.addTransfer(
                                     minor,
                                     operationDescription.text,
                                     account.id,
                                     targetAccount.id
                                 )
                                 : operationDialog.editingId
                                 ? financeController.updateTransaction(
                                     operationDialog.editingId,
                                     minor,
                                     operationDescription.text,
                                     category.value,
                                     account.id,
                                     type
                                 )
                                 : type === "income"
                                   ? financeController.addIncome(
                                       minor,
                                       operationDescription.text,
                                       category.value,
                                       account.currency,
                                       account.id
                                   )
                                   : financeController.addExpense(
                                       minor,
                                       operationDescription.text,
                                       category.value,
                                       account.currency,
                                       account.id
                                   );
                        if (ok)
                            operationDialog.close();
                        else
                            operationError.text = isTransfer
                                ? "Проверьте сумму и выбранные счета"
                                : "Проверьте сумму, счёт и категорию";
                    }
                }
            }
        }
    }

    Dialog {
        id: categoryDialog
        width: 560
        height: 620
        modal: true
        anchors.centerIn: parent
        padding: 22
        property string editingId: ""
        function openForManagement() {
            editingId = "";
            categoryNameField.clear();
            categoryError.text = "";
            open();
        }
        function startEdit(row) {
            editingId = row.value;
            categoryNameField.text = row.label;
            categoryType.currentIndex = row.type === "income" ? 0 : 1;
        }
        background: Rectangle {
            color: root.panel
            radius: 18
            border.width: 1
            border.color: root.line
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "Категории"
                color: root.ink
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            RowLayout {
                Layout.fillWidth: true
                AppTextField {
                    id: categoryNameField
                    Layout.fillWidth: true
                    placeholderText: "Название категории"
                }
                AppComboBox {
                    id: categoryType
                    model: ["Доход", "Расход"]
                    enabled: !categoryDialog.editingId
                }
                SoftButton {
                    text: categoryDialog.editingId ? "Сохранить" : "Добавить"
                    highlighted: true
                    onClicked: {
                        const ok = categoryDialog.editingId ? financeController.renameCategory(categoryDialog.editingId, categoryNameField.text) : financeController.addCategory(categoryNameField.text, categoryType.currentIndex === 0 ? "income" : "expense");
                        if (ok) {
                            categoryDialog.editingId = "";
                            categoryNameField.clear();
                            categoryError.text = "";
                        } else
                            categoryError.text = "Не удалось сохранить категорию";
                    }
                }
            }
            Text {
                id: categoryError
                color: root.red
                font.pixelSize: 12
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 5
                // Scrollbars intentionally hidden application-wide.
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                model: financeController.categories
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 48
                    radius: 9
                    color: root.categoryEditRow
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text {
                            text: modelData.label
                            color: root.ink
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.type === "income" ? "Доход" : "Расход"
                            color: root.muted
                            font.pixelSize: 11
                        }
                        Button {
                            flat: true
                            text: "✎"
                            onClicked: categoryDialog.startEdit(modelData)
                        }
                        Button {
                            flat: true
                            text: "×"
                            onClicked: {
                                financeController.deleteCategory(modelData.value);
                                if (categoryDialog.editingId === modelData.value) {
                                    categoryDialog.editingId = "";
                                    categoryNameField.clear();
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    text: "Закрыть"
                    onClicked: categoryDialog.close()
                }
            }
        }
    }
}
