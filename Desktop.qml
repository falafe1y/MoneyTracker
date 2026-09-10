import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: "Ledgera"
    color: root.canvas

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
    readonly property var chartColors: [
        chartAccent1,
        chartAccent2,
        chartAccent3,
        chartAccent4,
        chartAccent5
    ]

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
            title: qsTr("Крипта"),
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
        const formatted = decimals === 0
                        ? String(whole)
                        : String(whole) + decimalSeparator
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

    function accountTypeLabel(type) {
        if (type === "cash") return qsTr("Наличные");
        if (type === "debit_card") return qsTr("Дебетовая карта");
        if (type === "credit_card") return qsTr("Кредитная карта");
        if (type === "savings") return qsTr("Накопительный");
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
        if (!row.hasSnapshot)
            return qsTr("Баланс ещё не обновлён");
        const updated = qsTr("Обновлено: %1").arg(
            Qt.formatDateTime(row.updatedAt, "dd.MM.yyyy HH:mm")
        );
        return row.priceHasSnapshot
             ? updated + " · 1 USDT = "
               + Number(row.priceUsd).toLocaleString(root.uiLocale(), "f", 4) + " $"
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
        return row.type === "income" || (row.type === "transfer" && row.direction === "in")
             ? row.amount : -row.amount;
    }

    function transactionTypeLabel(row) {
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

    component Panel: Rectangle {
        color: root.panel
        radius: 16
        border.width: 1
        border.color: root.line
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
    component SoftButton: Button {
        id: control
        property bool destructive: false
        hoverEnabled: true
        implicitHeight: 42
        contentItem: Text {
            text: control.text
            color: control.destructive || control.highlighted ? root.white : root.accent
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
                     ? (control.down ? root.navHovered : root.accent)
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
    component AppTextField: TextField {
        id: field
        implicitHeight: 44
        leftPadding: 14
        rightPadding: 14
        color: root.accent
        placeholderTextColor: root.muted
        selectionColor: root.accentSoft
        selectedTextColor: root.white
        font.pixelSize: 14

        background: Rectangle {
            radius: 11
            color: field.activeFocus ? root.panel : root.soft
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus ? root.navSelected : root.line
        }
    }
    component AppCheckBox: CheckBox {
        id: check
        hoverEnabled: true
        spacing: 10
        implicitHeight: 32

        indicator: Rectangle {
            implicitWidth: 22
            implicitHeight: 22
            x: check.leftPadding
            y: (check.height - height) / 2
            radius: 6
            color: check.checked
                   ? (check.hovered ? root.navSelected : root.accent)
                   : (check.hovered ? root.controlHovered : root.soft)
            border.width: check.checked ? 0 : 1
            border.color: root.line

            Text {
                anchors.centerIn: parent
                visible: check.checked
                text: "✓"
                color: root.white
                font.pixelSize: 15
                font.weight: Font.Bold
            }
        }

        contentItem: Text {
            leftPadding: check.indicator.width + check.spacing
            text: check.text
            color: check.enabled ? root.accent : root.muted
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
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
            color: root.accent
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: combo.width - width - 15
            y: (combo.height - height) / 2 - 1
            text: combo.popup.visible ? "⌃" : "⌄"
            color: root.navSelected
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        background: Rectangle {
            radius: 11
            color: combo.pressed || combo.popup.visible ? root.panel : combo.hovered ? root.controlHovered : root.soft
            border.width: combo.popup.visible ? 2 : 1
            border.color: combo.popup.visible ? root.navSelected : root.line
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
                color: root.accent
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
            color: root.accent
            border.color: root.accent
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
                            color: root.accent
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
                    text: qsTr("Обзор")
                    glyph: "▦"
                    target: "overview"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Счета")
                    glyph: "▣"
                    target: "accounts"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Категории")
                    glyph: "◇"
                    target: "categories"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Операции")
                    glyph: "⇄"
                    target: "operations"
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Аналитика")
                    glyph: "▥"
                    target: "analytics"
                }
                Item {
                    Layout.fillHeight: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: root.accentSoft
                }
                NavButton {
                    Layout.fillWidth: true
                    text: qsTr("Настройки")
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
                    text: page === "overview" ? qsTr("Мои финансы")
                        : page === "accounts" ? qsTr("Счета")
                        : page === "categories" ? qsTr("Категории")
                        : page === "operations" ? qsTr("Операции")
                        : page === "analytics" ? qsTr("Аналитика")
                        : qsTr("Настройки")
                    color: root.accent
                    font.pixelSize: 28
                    font.weight: Font.Bold
                }
                Item {
                    Layout.fillWidth: true
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
            RowLayout {
                Layout.fillWidth: true
                visible: page === "overview"
                      || page === "accounts"
                      || page === "operations"
                      || page === "analytics"

                Item {
                    Layout.fillWidth: true
                }
                SoftButton {
                    implicitWidth: 205
                    implicitHeight: 42
                    text: "◷  " + root.dateFilterLabel()
                    onClicked: dateFilterDialog.openForCurrent()
                }
                AppTextField {
                    visible: page === "overview" || page === "operations"
                    Layout.preferredWidth: 265
                    implicitHeight: 42
                    placeholderText: qsTr("Поиск по операциям...")
                    onTextChanged: root.searchText = text
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Panel {
                        Layout.preferredWidth: (dashboard.width - 42) / 4
                        Layout.minimumWidth: 180
                        Layout.preferredHeight: 118
                        color: root.soft
                        border.color: root.accentSoft

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            RowLayout {
                                Rectangle {
                                    width: 38
                                    height: 38
                                    radius: 19
                                    color: root.accent

                                    Text {
                                        anchors.centerIn: parent
                                        text: "∑"
                                        color: root.white
                                        font.pixelSize: 19
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    text: qsTr("Все активы")
                                    color: root.accent
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                text: root.money(
                                    financeController.balanceMinorUnits,
                                    financeController.appCurrency,
                                    false
                                )
                                color: root.navSelected
                                font.pixelSize: 25
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Repeater {
                        model: root.assets
                        delegate: Panel {
                            required property var modelData
                            Layout.preferredWidth: (dashboard.width - 42) / 4
                            Layout.minimumWidth: 180
                            Layout.preferredHeight: 118
                            color: financeController.selectedAsset === modelData.code ? root.accent : root.panel
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
                                        color: financeController.selectedAsset === modelData.code ? root.accentSoft : root.pale
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.icon
                                            color: financeController.selectedAsset === modelData.code ? root.white : root.accent
                                            font.pixelSize: 19
                                        }
                                    }
                                    Text {
                                        text: modelData.title
                                        color: financeController.selectedAsset === modelData.code ? root.white : root.accent
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Text {
                                    text: root.money(root.assetAmount(modelData.code), financeController.appCurrency, false)
                                    color: financeController.selectedAsset === modelData.code ? root.white : root.accent
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
                                text: (financeController.selectedAsset === "crypto"
                                       ? qsTr("Криптовалюты")
                                       : qsTr("Счета"))
                                      + " · " + root.assetTitle(financeController.selectedAsset)
                                color: root.accent
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                id: addAccountButton
                                flat: true
                                text: financeController.selectedAsset === "crypto"
                                      ? qsTr("+  Добавить криптовалюту")
                                      : qsTr("+  Добавить счёт")

                                contentItem: Text {
                                    text: addAccountButton.text
                                    color: root.navSelected
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
                            spacing: 12
                            clip: true
                            // Scrollbars intentionally hidden application-wide.
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                            model: [
                                {
                                    id: "",
                                    name: financeController.selectedAsset === "crypto"
                                          ? qsTr("Все кошельки")
                                          : qsTr("Все счета"),
                                    balanceMinor: root.assetAmount(financeController.selectedAsset),
                                    currency: financeController.appCurrency
                                }
                            ].concat(financeController.selectedAsset === "crypto"
                                     ? financeController.cryptoWallets
                                     : financeController.accounts)
                            delegate: Rectangle {
                                required property var modelData
                                width: 245
                                height: 64
                                radius: 13
                                color: root.overviewAccountSelected(modelData)
                                     ? root.accent
                                     : root.panel
                                border.width: 1
                                border.color: root.overviewAccountSelected(modelData)
                                            ? root.accent
                                            : root.line
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
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 13
                                    Text {
                                        text: "▣"
                                        color: root.overviewAccountSelected(modelData)
                                             ? root.white
                                             : root.accent
                                        font.pixelSize: 20
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            color: root.overviewAccountSelected(modelData)
                                                 ? root.white
                                                 : root.accent
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: root.accountCompactAmount(modelData)
                                            color: root.overviewAccountSelected(modelData)
                                                 ? root.paleText
                                                 : root.muted
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                    Text {
                                        text: root.overviewAccountSelected(modelData) ? "✓" : "›"
                                        color: root.overviewAccountSelected(modelData)
                                             ? root.white
                                             : root.accent
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
                                text: qsTr("Расходы по категориям")
                                color: root.accent
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
                                        Layout.preferredWidth: 0
                                        Layout.minimumWidth: 0
                                        Layout.fillHeight: true
                                        Item {
                                            Layout.fillHeight: true
                                        }
                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 44
                                            height: Math.max(5, Math.min(120, modelData.amount / Math.max(1, root.categoryTotals()[0].amount) * 120))
                                            radius: 5
                                            color: root.chartColors[index % root.chartColors.length]
                                            border.width: 1
                                            border.color: root.line
                                        }
                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 0
                                            Layout.maximumWidth: 80
                                            text: modelData.label
                                            color: root.muted
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                Text {
                                    visible: root.categoryTotals().length === 0
                                    text: qsTr("Добавьте расходы — здесь появится график")
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
                                text: qsTr("Структура расходов")
                                color: root.accent
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
                                            ctx.strokeStyle = root.chartColors[j % root.chartColors.length];
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
                    visible: financeController.selectedAsset !== "crypto"
                    Layout.fillWidth: true
                    expandToContent: true
                    title: qsTr("История операций")
                    rows: root.visibleTransactions()
                }
                DashboardCryptoHistoryBlock {
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
    component DashboardCryptoHistoryBlock: Panel {
        id: dashboardCryptoHistory

        property var rows: financeController.cryptoTransactions
        property bool expandToContent: false

        readonly property int dateColumnWidth: 125
        readonly property int directionColumnWidth: 105
        readonly property int hashColumnWidth: 175
        readonly property int amountColumnWidth: 135

        implicitHeight: expandToContent
                      ? 84 + Math.max(58, rows.length * 42)
                      : 230

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
                    text: qsTr("История USDT")
                    color: root.accent
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("Последние операции: %1")
                        .arg(dashboardCryptoHistory.rows.length)
                    color: root.muted
                    font.pixelSize: 12
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
                        font.pixelSize: 11
                        Layout.preferredWidth: dashboardCryptoHistory.dateColumnWidth
                    }
                    Text {
                        text: qsTr("Направление")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: dashboardCryptoHistory.directionColumnWidth
                    }
                    Text {
                        text: qsTr("Адрес")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Хеш")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: dashboardCryptoHistory.hashColumnWidth
                    }
                    Text {
                        text: qsTr("Сумма")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: dashboardCryptoHistory.amountColumnWidth
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    58,
                    dashboardCryptoHistory.rows.length * 42
                )

                Column {
                    anchors.fill: parent

                    Repeater {
                        model: dashboardCryptoHistory.rows

                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: parent.width
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
                                    font.pixelSize: 11
                                    Layout.preferredWidth: dashboardCryptoHistory.dateColumnWidth
                                }
                                Text {
                                    text: modelData.direction === "in"
                                          ? qsTr("Получено")
                                          : qsTr("Отправлено")
                                    color: modelData.direction === "in"
                                         ? root.income
                                         : root.red
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    Layout.preferredWidth: dashboardCryptoHistory.directionColumnWidth
                                }
                                Text {
                                    text: modelData.counterparty
                                    color: root.muted
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                }
                                Text {
                                    text: modelData.transactionId
                                    color: root.muted
                                    font.pixelSize: 11
                                    Layout.preferredWidth: dashboardCryptoHistory.hashColumnWidth
                                    elide: Text.ElideMiddle
                                }
                                Text {
                                    readonly property string localizedAmount:
                                        financeController.uiLanguage === "en"
                                        ? modelData.amountText
                                        : modelData.amountText.replace(".", ",")
                                    text: (modelData.direction === "in" ? "+" : "−")
                                          + localizedAmount + " USDT"
                                    color: modelData.direction === "in"
                                         ? root.income
                                         : root.red
                                    font.pixelSize: 12
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
                            : qsTr("У этого кошелька пока нет переводов USDT")
                    color: root.muted
                }
            }
        }
    }

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
                    font.pixelSize: 15
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
                        font.pixelSize: 11
                        Layout.preferredWidth: transactionTable.operationColumnWidth
                    }
                    Text {
                        text: qsTr("Счёт")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: transactionTable.accountColumnWidth
                    }
                    Text {
                        text: qsTr("Категория")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: transactionTable.categoryColumnWidth
                    }
                    Text {
                        text: qsTr("Описание")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Дата")
                        color: root.muted
                        font.pixelSize: 11
                        Layout.preferredWidth: transactionTable.dateColumnWidth
                    }
                    Text {
                        text: qsTr("Сумма")
                        color: root.muted
                        font.pixelSize: 11
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
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    Layout.preferredWidth: transactionTable.operationColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: root.accountName(modelData.accountId)
                                    color: root.muted
                                    font.pixelSize: 12
                                    Layout.preferredWidth: transactionTable.accountColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.categoryName
                                    color: root.accent
                                    font.pixelSize: 11
                                    Layout.preferredWidth: transactionTable.categoryColumnWidth
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.rawDescription || "—"
                                    color: root.muted
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                    color: root.muted
                                    font.pixelSize: 12
                                    Layout.preferredWidth: transactionTable.dateColumnWidth
                                }
                                Text {
                                    text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                    color: modelData.type === "transfer" ? root.navSelected : modelData.type === "income" ? root.income : root.red
                                    font.pixelSize: 12
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
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.preferredWidth: transactionTable.operationColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.accountName(modelData.accountId)
                                color: root.muted
                                font.pixelSize: 12
                                Layout.preferredWidth: transactionTable.accountColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.categoryName
                                color: root.accent
                                font.pixelSize: 11
                                Layout.preferredWidth: transactionTable.categoryColumnWidth
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.rawDescription || "—"
                                color: root.muted
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                                color: root.muted
                                font.pixelSize: 12
                                Layout.preferredWidth: transactionTable.dateColumnWidth
                            }
                            Text {
                                text: root.money(root.transactionSignedAmount(modelData), modelData.currency, true)
                                color: modelData.type === "transfer" ? root.navSelected : modelData.type === "income" ? root.income : root.red
                                font.pixelSize: 12
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
                    text: financeController.selectedAsset === "crypto"
                          ? qsTr("+ Добавить криптовалюту")
                          : qsTr("+ Добавить счёт")
                    highlighted: true
                    implicitWidth: financeController.selectedAsset === "crypto" ? 220 : 150
                    onClicked: {
                        if (financeController.selectedAsset === "crypto")
                            cryptoWalletDialog.openForNewWallet();
                        else
                            accountDialog.openForSelectedAsset();
                    }
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
                          ? qsTr("Обновляем баланс, цену и историю USDT…")
                          : qsTr("Не удалось обновить криптоданные: %1")
                                .arg(financeController.cryptoLastError)
                    color: financeController.cryptoRefreshing ? root.muted : root.red
                    font.pixelSize: 12
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
                Layout.fillHeight: financeController.selectedAsset !== "crypto"
                Layout.preferredHeight: financeController.selectedAsset === "crypto"
                                      ? 188
                                      : -1
                Layout.minimumHeight: financeController.selectedAsset === "crypto"
                                    ? 188
                                    : 0
                cellWidth: 320
                cellHeight: financeController.selectedAsset === "crypto" ? 174 : 150
                clip: true
                // Scrollbars intentionally hidden application-wide.
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                model: financeController.selectedAsset === "crypto"
                       ? financeController.cryptoWallets
                       : financeController.accounts
                delegate: Panel {
                    id: accountCard
                    required property var modelData
                    readonly property bool selectedCrypto:
                        modelData.isCrypto === true
                        && financeController.selectedCryptoWalletId === modelData.id
                    width: 300
                    height: modelData.isCrypto ? 156 : 132
                    color: selectedCrypto ? root.accent : root.panel
                    border.color: selectedCrypto ? root.accent : root.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Text {
                                text: "▣"
                                color: accountCard.selectedCrypto
                                     ? root.white
                                     : root.accent
                                font.pixelSize: 24
                            }
                            Text {
                                text: modelData.name
                                color: accountCard.selectedCrypto
                                     ? root.white
                                     : root.accent
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            text: root.accountPrimaryAmount(modelData)
                            color: accountCard.selectedCrypto
                                 ? root.white
                                 : root.accent
                            font.pixelSize: modelData.isCreditCard ? 20 : 25
                            font.weight: Font.Bold
                        }
                        Text {
                            visible: modelData.isCreditCard
                            text: root.accountAvailableCredit(modelData)
                            color: accountCard.selectedCrypto
                                 ? root.paleText
                                 : root.navSelected
                            font.pixelSize: 12
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.isCrypto
                                  ? modelData.network + " · " + modelData.address
                                  : modelData.currency + " · " + root.accountTypeLabel(modelData.type)
                            color: accountCard.selectedCrypto
                                 ? root.paleText
                                 : root.muted
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: modelData.isCrypto
                            text: root.cryptoWalletStatus(modelData)
                            color: accountCard.selectedCrypto
                                 ? root.paleText
                                 : modelData.refreshing
                                   ? root.navSelected
                                   : root.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: accountsPageMouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: modelData.isCrypto
                                   ? Qt.PointingHandCursor
                                   : Qt.ArrowCursor
                        onClicked: function (mouse) {
                            if (mouse.button === Qt.LeftButton
                                && modelData.isCrypto) {
                                financeController.selectedCryptoWalletId = modelData.id;
                            }
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
                          ? qsTr("Добавьте публичный адрес TRON-кошелька")
                          : qsTr("У этого актива пока нет счетов")
                    color: root.muted
                }
            }
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
                            text: qsTr("История USDT")
                            color: root.accent
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Последние операции: %1")
                                .arg(financeController.cryptoTransactions.length)
                            color: root.muted
                            font.pixelSize: 12
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
                                font.pixelSize: 11
                                Layout.preferredWidth: 125
                            }
                            Text {
                                text: qsTr("Направление")
                                color: root.muted
                                font.pixelSize: 11
                                Layout.preferredWidth: 105
                            }
                            Text {
                                text: qsTr("Адрес")
                                color: root.muted
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                            Text {
                                text: qsTr("Хеш")
                                color: root.muted
                                font.pixelSize: 11
                                Layout.preferredWidth: 175
                            }
                            Text {
                                text: qsTr("Сумма")
                                color: root.muted
                                font.pixelSize: 11
                                Layout.preferredWidth: 135
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            anchors.fill: parent
                            clip: true
                            model: financeController.cryptoTransactions
                            ScrollBar.horizontal: ScrollBar {
                                policy: ScrollBar.AlwaysOff
                            }
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOff
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
                                        font.pixelSize: 11
                                        Layout.preferredWidth: 125
                                    }
                                    Text {
                                        text: modelData.direction === "in"
                                              ? qsTr("Получено")
                                              : qsTr("Отправлено")
                                        color: modelData.direction === "in"
                                             ? root.income
                                             : root.red
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        Layout.preferredWidth: 105
                                    }
                                    Text {
                                        text: modelData.counterparty
                                        color: root.muted
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        text: modelData.transactionId
                                        color: root.muted
                                        font.pixelSize: 11
                                        Layout.preferredWidth: 175
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        readonly property string localizedAmount:
                                            financeController.uiLanguage === "en"
                                            ? modelData.amountText
                                            : modelData.amountText.replace(".", ",")
                                        text: (modelData.direction === "in" ? "+" : "−")
                                              + localizedAmount + " USDT"
                                        color: modelData.direction === "in"
                                             ? root.income
                                             : root.red
                                        font.pixelSize: 12
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
                                    : qsTr("У этого кошелька пока нет переводов USDT")
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

                        // Text {
                        //     text: "Расходы"
                        //     color: root.accent
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
        TransactionBlock {
            anchors.fill: parent
            title: qsTr("История операций")
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
                        text: qsTr("Доходы")
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
                        text: qsTr("Расходы")
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
                        text: qsTr("Категории расходов")
                        color: root.accent
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
                                color: root.accent
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
                    id: currentRatesBlock
                    property bool expanded: false

                    Layout.topMargin: 4
                    Layout.preferredWidth: 460
                    implicitHeight: ratesHeader.height
                                  + (expanded ? ratesList.implicitHeight : 0)
                    radius: 10
                    color: root.soft
                    border.color: root.line
                    clip: true

                    Rectangle {
                        id: ratesHeader
                        width: parent.width
                        height: 44
                        color: ratesMouse.containsMouse
                               ? root.controlHovered
                               : root.transparentColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Текущие курсы в %1")
                                    .arg(financeController.appCurrency)
                                color: root.accent
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: currentRatesBlock.expanded ? "⌃" : "⌄"
                                color: root.muted
                                font.pixelSize: 16
                            }
                        }

                        MouseArea {
                            id: ratesMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentRatesBlock.expanded =
                                           !currentRatesBlock.expanded
                        }
                    }

                    Column {
                        id: ratesList
                        anchors.top: ratesHeader.bottom
                        width: parent.width
                        visible: currentRatesBlock.expanded

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
                                              + root.symbol(financeController.appCurrency)
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
                        enabled: manualUsdRateField.acceptableInput
                              && manualEurRateField.acceptableInput
                        onClicked: {
                            const usdRate = Number(manualUsdRateField.text.replace(",", "."));
                            const eurRate = Number(manualEurRateField.text.replace(",", "."));
                            const saved = financeController.saveManualCurrencyRates(
                                usdRate, eurRate);
                            rateSaveStatus.color = saved ? root.income : root.red;
                            rateSaveStatus.text = saved
                                ? qsTr("Курсы сохранены")
                                : qsTr("Введите положительные числовые значения");
                        }
                    }
                    Text {
                        id: rateSaveStatus
                        color: root.income
                        font.pixelSize: 12
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
                    text: qsTr("CSV содержит операции, переводы, даты, исходные суммы, счета и категории.")
                    color: root.muted
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    spacing: 10
                    SoftButton {
                        text: qsTr("Экспортировать CSV")
                        highlighted: true
                        implicitWidth: 180
                        onClicked: exportCsvDialog.open()
                    }
                    SoftButton {
                        text: qsTr("Импортировать CSV")
                        implicitWidth: 180
                        onClicked: importCsvDialog.open()
                    }
                }
                Text {
                    Layout.preferredWidth: 520
                    visible: root.csvStatus.length > 0
                    text: root.csvStatus
                    color: root.csvStatusOk ? root.income : root.red
                    font.pixelSize: 12
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
                    text: qsTr("Язык интерфейса")
                    color: root.accent
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Пользовательские названия счетов и категорий не переводятся.")
                    color: root.muted
                }
                AppComboBox {
                    id: languageBox
                    model: [
                        { label: qsTr("Русский"), value: "ru" },
                        { label: qsTr("Английский"), value: "en" }
                    ]
                    textRole: "label"
                    currentIndex: root.indexByRole(model, "value", financeController.uiLanguage)
                    onActivated: financeController.uiLanguage = model[currentIndex].value
                    implicitWidth: 180
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
        id: importCsvDialog
        title: qsTr("Импорт операций из CSV")
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

    Dialog {
        id: cryptoWalletDialog
        width: 500
        modal: true
        anchors.centerIn: parent
        padding: 24

        function openForNewWallet() {
            cryptoAddressField.clear();
            cryptoWalletError.text = "";
            open();
            cryptoAddressField.forceActiveFocus();
        }

        function submit() {
            const result = financeController.addCryptoWallet(
                cryptoAddressField.text
            );
            if (result.ok)
                close();
            else
                cryptoWalletError.text = result.error || qsTr("Не удалось добавить кошелёк");
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
                text: qsTr("Добавить криптовалюту")
                color: root.accent
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                text: "USDT · TRC-20"
                color: root.navSelected
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            AppTextField {
                id: cryptoAddressField
                Layout.fillWidth: true
                placeholderText: qsTr("Публичный адрес TRON (T…)")
                maximumLength: 64
                onAccepted: cryptoWalletDialog.submit()
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Вводите только публичный адрес. Никогда не указывайте seed-фразу или приватный ключ.")
                color: root.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                id: cryptoWalletError
                Layout.fillWidth: true
                color: root.red
                font.pixelSize: 12
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
        width: 500
        modal: true
        anchors.centerIn: parent
        padding: 24
        property var accountTypes: []
        property string editingId: ""
        property var editingAccount: null
        property string editingAsset: financeController.selectedAsset
        readonly property bool creditCardSelected:
            accountTypeBox.currentIndex >= 0
            && accountTypeBox.currentIndex < accountTypes.length
            && accountTypes[accountTypeBox.currentIndex].value === "credit_card"

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
                    label: qsTr("Накопительный"),
                    value: "savings"
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
            accountError.text = "";
            open();
        }

        onClosed: {
            editingId = "";
            editingAccount = null;
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
                font.pixelSize: 11
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.creditCardSelected
                text: qsTr("Кредитный лимит")
                color: root.muted
                font.pixelSize: 12
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
                      : qsTr("Начальный баланс")
                color: root.muted
                font.pixelSize: 12
            }
            AppTextField {
                id: accountBalanceField
                Layout.fillWidth: true
                placeholderText: accountDialog.creditCardSelected
                                 ? qsTr("Задолженность на момент добавления")
                                 : qsTr("Начальный баланс")
                validator: DoubleValidator {
                    bottom: accountDialog.creditCardSelected ? 0 : -999999999
                    top: 999999999
                    decimals: 2
                }
            }
            Text {
                Layout.fillWidth: true
                visible: accountDialog.creditCardSelected
                text: qsTr("Кредитный лимит не считается активом. Расходы увеличивают задолженность, а перевод на кредитку её погашает.")
                color: root.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
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
                        const ok = accountDialog.editingId
                                 ? financeController.updateAccount(
                                     accountDialog.editingId,
                                     accountNameField.text,
                                     type,
                                     accountCurrencyBox.currentText,
                                     initialBalanceMinor,
                                     creditLimitMinor
                                 )
                                 : financeController.addAccount(
                                     accountNameField.text,
                                     type,
                                     accountCurrencyBox.currentText,
                                     initialBalanceMinor,
                                     creditLimitMinor
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

        onClosed: accountData = null

        background: Rectangle {
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
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            Text {
                id: deleteAccountError
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
        width: 520
        modal: true
        anchors.centerIn: parent
        padding: 24
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
                text: qsTr("Куда")
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
                    font.pixelSize: 12
                }

                Button {
                    id: operationDateButton
                    Layout.fillWidth: true
                    implicitHeight: 44
                    hoverEnabled: true
                    text: operationDialog.selectedDate.toLocaleDateString(
                        root.uiLocale(),
                        Locale.LongFormat
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
                font.pixelSize: 12
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
                    text: qsTr("30 дней")
                    onClicked: {
                        const to = dateFilterDialog.normalizedDate(new Date());
                        const from = new Date(to);
                        from.setDate(from.getDate() - 29);
                        dateFilterDialog.applyRange(from, to);
                    }
                }
                SoftButton {
                    Layout.fillWidth: true
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
                    text: qsTr("Этот год")
                    onClicked: {
                        const to = dateFilterDialog.normalizedDate(new Date());
                        dateFilterDialog.applyRange(
                            new Date(to.getFullYear(), 0, 1, 12),
                            to
                        );
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
                    font.pixelSize: 11
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
                        font.pixelSize: 13
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
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.fillWidth: true

                SoftButton {
                    text: qsTr("Все время")
                    onClicked: {
                        financeController.clearDateFilter();
                        dateFilterDialog.close();
                    }
                }
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
                    font.pixelSize: 11
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
                        font.pixelSize: 13
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
                text: qsTr("Категории")
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
                }
                AppComboBox {
                    id: categoryType
                    model: [qsTr("Доход"), qsTr("Расход")]
                    enabled: !categoryDialog.editingId
                }
                SoftButton {
                    text: categoryDialog.editingId ? qsTr("Сохранить") : qsTr("Добавить")
                    highlighted: true
                    onClicked: {
                        const ok = categoryDialog.editingId ? financeController.renameCategory(categoryDialog.editingId, categoryNameField.text) : financeController.addCategory(categoryNameField.text, categoryType.currentIndex === 0 ? "income" : "expense");
                        if (ok) {
                            categoryDialog.editingId = "";
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
                            color: root.accent
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.type === "income" ? qsTr("Доход") : qsTr("Расход")
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
                    text: qsTr("Закрыть")
                    onClicked: categoryDialog.close()
                }
            }
        }
    }
}
