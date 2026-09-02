import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 1180
    height: 800
    minimumWidth: 940
    minimumHeight: 680
    visible: true
    title: "Money Tracker"
    color: "#F4F6F8"

    readonly property color backgroundColor: "#F4F6F8"
    readonly property color surfaceColor: "#FFFFFF"
    readonly property color surfaceSoft: "#F8FAFC"
    readonly property color borderColor: "#E5E9EF"
    readonly property color textPrimary: "#101828"
    readonly property color textSecondary: "#788395"
    readonly property color accentColor: "#315EF4"
    readonly property color accentPressed: "#244AC7"
    readonly property color heroColor: "#111B31"
    readonly property color heroSoftColor: "#192640"
    readonly property color incomeColor: "#168A5B"
    readonly property color incomeSoft: "#E9F7F0"
    readonly property color expenseColor: "#C94A55"
    readonly property color expenseSoft: "#FCEDEF"

    property var currencyData: [
        { code: "RUB", symbol: "₽", name: "Российский рубль" },
        { code: "USD", symbol: "$", name: "Доллар США" },
        { code: "EUR", symbol: "€", name: "Евро" }
    ]

    readonly property var assetData: [
        { code: "fiat", title: "Фиат" },
        { code: "crypto", title: "Крипта" },
        { code: "investment", title: "Инвестиции" }
    ]

    function assetTitle(code) {
        for (let i = 0; i < assetData.length; ++i) {
            if (assetData[i].code === code)
                return assetData[i].title
        }
        return "Фиат"
    }

    readonly property var incomeCategories: financeController.categories.filter(
        function(category) { return category.type === "income" })
    readonly property var expenseCategories: financeController.categories.filter(
        function(category) { return category.type === "expense" })

    function currencyIndex(code) {
        for (let i = 0; i < currencyData.length; ++i) {
            if (currencyData[i].code === code)
                return i
        }
        return 0
    }

    function currencySymbol(code) {
        const index = currencyIndex(code)
        return currencyData[index].symbol
    }

    function formatAmount(minorUnits, currency, includeSign, transactionType) {
        const absoluteValue = Math.abs(Number(minorUnits)) / 100.0
        const formatted = absoluteValue.toLocaleString(Qt.locale("ru_RU"), "f", 2)
        let sign = ""

        if (includeSign)
            sign = transactionType === "income" ? "+" : "−"
        else if (Number(minorUnits) < 0)
            sign = "−"

        return sign + formatted + " " + currencySymbol(currency)
    }

    function categoryTitle(id) {
        const all = incomeCategories.concat(expenseCategories)
        for (let i = 0; i < all.length; ++i) {
            if (all[i].value === id)
                return all[i].label
        }
        return "Без категории"
    }

    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 76
        color: root.surfaceColor

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.borderColor
        }

        Item {
            width: Math.min(root.width - 64, 1120)
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter

            RowLayout {
                anchors.fill: parent
                spacing: 16

                RowLayout {
                    spacing: 11

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 12
                        color: root.heroColor

                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            anchors.centerIn: parent
                            color: root.accentColor

                            Rectangle {
                                width: 5
                                height: 5
                                radius: 2.5
                                anchors.centerIn: parent
                                color: "white"
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 0

                        Text {
                            text: "Money"
                            color: root.textPrimary
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }

                        Text {
                            text: "Личные финансы"
                            color: root.textSecondary
                            font.pixelSize: 11
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 5

                    Repeater {
                        model: root.assetData
                        delegate: Button {
                            id: assetButton
                            required property var modelData
                            implicitWidth: 92
                            implicitHeight: 38
                            flat: true

                            contentItem: Text {
                                text: assetButton.modelData.title
                                color: financeController.selectedAsset ===
                                       assetButton.modelData.code
                                       ? "white" : root.textSecondary
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 12
                                color: financeController.selectedAsset ===
                                       assetButton.modelData.code
                                       ? root.heroColor : root.surfaceSoft
                            }
                            onClicked: financeController.selectedAsset =
                                           assetButton.modelData.code
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 10

                    Button {
                        id: accountsButton
                        implicitWidth: 92
                        implicitHeight: 38
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Счета"
                            color: root.textPrimary
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 12
                            color: accountsButton.hovered
                                   ? root.surfaceSoft : root.surfaceColor
                            border.width: 1
                            border.color: root.borderColor
                        }

                        onClicked: accountDialog.openForSelectedAsset()
                    }

                    Button {
                        id: categoriesButton
                        implicitWidth: 108
                        implicitHeight: 38
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Категории"
                            color: root.textPrimary
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 12
                            color: categoriesButton.hovered
                                   ? root.surfaceSoft : root.surfaceColor
                            border.width: 1
                            border.color: root.borderColor
                        }

                        onClicked: categoryDialog.openForManagement()
                    }

                    Text {
                        text: "Валюта"
                        color: root.textSecondary
                        font.pixelSize: 12
                    }

                    BankComboBox {
                        id: appCurrencyBox
                        Layout.preferredWidth: 142
                        compact: true
                        model: root.currencyData
                        textRole: "code"
                        secondaryRole: "symbol"
                        currentIndex: root.currencyIndex(financeController.appCurrency)
                        surfaceColor: root.surfaceColor
                        surfaceHoverColor: root.surfaceSoft
                        borderColor: root.borderColor
                        focusColor: root.accentColor
                        textPrimary: root.textPrimary
                        textSecondary: root.textSecondary

                        onActivated: function(index) {
                            financeController.appCurrency = root.currencyData[index].code
                        }
                    }
                }
            }
        }
    }

    Item {
        id: page
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(root.width - 64, 1120)

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 26
            anchors.bottomMargin: 28
            spacing: 20

            Rectangle {
                id: balanceCard
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(650, page.width)
                Layout.preferredHeight: 194
                radius: 24
                color: root.heroColor
                clip: true

                Rectangle {
                    width: 220
                    height: 220
                    radius: 110
                    x: balanceCard.width - 110
                    y: -105
                    color: "#12FFFFFF"
                }

                Rectangle {
                    width: 120
                    height: 120
                    radius: 60
                    x: -54
                    y: balanceCard.height - 55
                    color: "#0CFFFFFF"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 7

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Общий баланс"
                            color: "#AEB8CA"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            Layout.preferredWidth: 76
                            Layout.preferredHeight: 28
                            radius: 14
                            color: "#16FFFFFF"

                            Text {
                                anchors.centerIn: parent
                                text: financeController.balanceCurrency
                                color: "#D8DEEA"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        text: root.formatAmount(
                                  financeController.balanceMinorUnits,
                                  financeController.balanceCurrency,
                                  false,
                                  "")
                        color: "white"
                        font.pixelSize: 40
                        minimumPixelSize: 25
                        fontSizeMode: Text.Fit
                        font.weight: Font.Bold
                        font.letterSpacing: -0.8
                        verticalAlignment: Text.AlignVCenter
                        maximumLineCount: 1
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            radius: 15
                            color: "#10FFFFFF"
                            border.width: 1
                            border.color: "#12FFFFFF"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 13
                                anchors.rightMargin: 13
                                spacing: 10

                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 9
                                    color: "#153CCB8B"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "↗"
                                        color: "#67D7A9"
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: "Доходы"
                                        color: "#96A3B7"
                                        font.pixelSize: 10
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.formatAmount(financeController.incomeMinorUnits, financeController.balanceCurrency, false, "")
                                        color: "#EAFBF4"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            radius: 15
                            color: "#10FFFFFF"
                            border.width: 1
                            border.color: "#12FFFFFF"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 13
                                anchors.rightMargin: 13
                                spacing: 10

                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 9
                                    color: "#18DC7180"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "↘"
                                        color: "#F398A3"
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: "Расходы"
                                        color: "#96A3B7"
                                        font.pixelSize: 10
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.formatAmount(financeController.expenseMinorUnits, financeController.balanceCurrency, false, "")
                                        color: "#FFF0F2"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                ColumnLayout {
                    spacing: 2

                    Text {
                        text: "Операции"
                        color: root.textPrimary
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }

                    Text {
                        text: financeController.transactions.length === 0
                              ? "История доходов и расходов появится здесь"
                              : "Последние транзакции"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    id: newTransactionButton
                    implicitHeight: 44
                    implicitWidth: 166
                    hoverEnabled: true

                    contentItem: Row {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: "+"
                            color: "white"
                            font.pixelSize: 17
                            font.weight: Font.Medium
                        }

                        Text {
                            text: "Новая операция"
                            color: "white"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    background: Rectangle {
                        radius: 14
                        color: newTransactionButton.down
                               ? root.accentPressed
                               : (newTransactionButton.hovered ? "#2B56E3" : root.accentColor)
                    }

                    onClicked: transactionDialog.openForNewTransaction()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 210
                radius: 20
                color: root.surfaceColor
                border.width: 1
                border.color: root.borderColor
                clip: true

                Item {
                    anchors.fill: parent
                    visible: financeController.transactions.length === 0

                    Column {
                        anchors.centerIn: parent
                        spacing: 9

                        Rectangle {
                            width: 48
                            height: 48
                            radius: 16
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#EEF2FF"

                            Text {
                                anchors.centerIn: parent
                                text: "↕"
                                color: root.accentColor
                                font.pixelSize: 20
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Пока без операций"
                            color: root.textPrimary
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Добавьте доход или расход"
                            color: root.textSecondary
                            font.pixelSize: 11
                        }
                    }
                }

                ListView {
                    id: transactionList
                    anchors.fill: parent
                    anchors.margins: 6
                    visible: financeController.transactions.length > 0
                    model: financeController.transactions
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        width: 6
                    }

                    delegate: Item {
                        id: transactionRow
                        required property var modelData
                        required property int index

                        width: transactionList.width
                        height: 76

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 5
                            anchors.rightMargin: 5
                            anchors.topMargin: 3
                            anchors.bottomMargin: 3
                            radius: 14
                            color: rowHover.containsMouse ? "#F8FAFC" : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 14
                                spacing: 13

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description.length > 0
                                              ? modelData.description
                                              : modelData.categoryName
                                        color: root.textPrimary
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        spacing: 7

                                        Text {
                                            text: modelData.categoryName
                                            color: root.textSecondary
                                            font.pixelSize: 10
                                        }

                                        Rectangle {
                                            width: 3
                                            height: 3
                                            radius: 1.5
                                            color: "#C4CAD3"
                                        }

                                        Text {
                                            text: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy · HH:mm")
                                            color: root.textSecondary
                                            font.pixelSize: 10
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.preferredWidth: 200
                                    Layout.maximumWidth: 200
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.formatAmount(modelData.amount, modelData.currency, true, modelData.type)
                                        color: modelData.type === "income" ? root.incomeColor : root.textPrimary
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideLeft
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: modelData.currency !== financeController.appCurrency
                                        text: "≈ " + root.formatAmount(
                                                  financeController.convertTransaction(transactionRow.index, financeController.appCurrency),
                                                  financeController.appCurrency,
                                                  false,
                                                  "")
                                        color: root.textSecondary
                                        font.pixelSize: 10
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideLeft
                                    }
                                }
                            }

                            MouseArea {
                                id: rowHover
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            height: index === transactionList.count - 1 ? 0 : 1
                            color: "#F0F2F5"
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: accountDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(560, root.width - 52)
        height: Math.min(630, root.height - 30)
        anchors.centerIn: parent
        padding: 0

        readonly property var accountTypes: {
            if (financeController.selectedAsset === "crypto")
                return [
                    { label: "Криптокошелёк", value: "crypto_wallet" },
                    { label: "Другое", value: "other" }
                ]
            if (financeController.selectedAsset === "investment")
                return [
                    { label: "Брокер", value: "brokerage" },
                    { label: "Вклад", value: "deposit" },
                    { label: "Другое", value: "other" }
                ]
            return [
                { label: "Наличные", value: "cash" },
                { label: "Дебетовая карта", value: "debit_card" },
                { label: "Кредитная карта", value: "credit_card" },
                { label: "Накопительный счёт", value: "savings" },
                { label: "Другое", value: "other" }
            ]
        }

        function openForSelectedAsset() {
            accountNameField.text = ""
            initialBalanceField.text = ""
            accountTypeBox.currentIndex = 0
            accountCurrencyBox.currentIndex = root.currencyIndex(
                financeController.appCurrency)
            accountError.text = ""
            open()
        }

        function initialBalanceMinor() {
            if (initialBalanceField.text.trim().length === 0)
                return 0
            const value = Number(initialBalanceField.text.trim().replace(",", "."))
            return isFinite(value) ? Math.round(value * 100) : 0
        }

        function saveAccount() {
            if (accountNameField.text.trim().length === 0)
                return

            const saved = financeController.addAccount(
                accountNameField.text,
                accountTypes[accountTypeBox.currentIndex].value,
                root.currencyData[accountCurrencyBox.currentIndex].code,
                initialBalanceMinor())
            if (saved) {
                accountNameField.text = ""
                initialBalanceField.text = ""
                accountError.text = ""
            } else {
                accountError.text = "Счёт с таким названием уже существует или не может быть сохранён"
            }
        }

        Overlay.modal: Rectangle { color: "#740B1220" }

        background: Rectangle {
            radius: 24
            color: root.surfaceColor
            border.width: 1
            border.color: root.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 13

            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Счета · " + root.assetTitle(financeController.selectedAsset)
                        color: root.textPrimary
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Новый счёт будет добавлен в выбранный актив"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    implicitWidth: 34
                    implicitHeight: 34
                    flat: true
                    contentItem: Text {
                        text: "×"
                        color: root.textSecondary
                        font.pixelSize: 21
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { color: "transparent" }
                    onClicked: accountDialog.close()
                }
            }

            Text {
                text: "Новый счёт"
                color: root.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            TextField {
                id: accountNameField
                Layout.fillWidth: true
                implicitHeight: 44
                maximumLength: 60
                placeholderText: financeController.selectedAsset === "fiat"
                                 ? "Например, Карта Альфа"
                                 : (financeController.selectedAsset === "crypto"
                                    ? "Например, MetaMask" : "Например, БКС")
                color: root.textPrimary
                leftPadding: 13
                rightPadding: 13
                background: Rectangle {
                    radius: 12
                    color: root.surfaceSoft
                    border.width: accountNameField.activeFocus ? 1.5 : 1
                    border.color: accountNameField.activeFocus
                                  ? root.accentColor : root.borderColor
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ComboBox {
                    id: accountTypeBox
                    Layout.fillWidth: true
                    implicitHeight: 44
                    model: accountDialog.accountTypes
                    textRole: "label"
                }

                ComboBox {
                    id: accountCurrencyBox
                    Layout.preferredWidth: 130
                    implicitHeight: 44
                    model: root.currencyData
                    textRole: "code"
                }
            }

            TextField {
                id: initialBalanceField
                Layout.fillWidth: true
                implicitHeight: 44
                placeholderText: "Начальный баланс, например 15000"
                color: root.textPrimary
                leftPadding: 13
                rightPadding: 13
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                validator: RegularExpressionValidator {
                    regularExpression: /^-?[0-9]+([\.,][0-9]{0,2})?$/
                }
                background: Rectangle {
                    radius: 12
                    color: root.surfaceSoft
                    border.width: initialBalanceField.activeFocus ? 1.5 : 1
                    border.color: initialBalanceField.activeFocus
                                  ? root.accentColor : root.borderColor
                }
                onAccepted: accountDialog.saveAccount()
            }

            Button {
                id: saveAccountButton
                Layout.fillWidth: true
                implicitHeight: 44
                enabled: accountNameField.text.trim().length > 0
                contentItem: Text {
                    text: "Добавить счёт в «" +
                          root.assetTitle(financeController.selectedAsset) + "»"
                    color: "white"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 12
                    color: saveAccountButton.enabled
                           ? root.accentColor : "#AEBBEB"
                }
                onClicked: accountDialog.saveAccount()
            }

            Text {
                id: accountError
                Layout.fillWidth: true
                color: root.expenseColor
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }

            Text {
                text: "Счета в активе"
                color: root.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 14
                color: root.surfaceSoft
                border.width: 1
                border.color: root.borderColor

                ListView {
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: financeController.accounts
                    delegate: Item {
                        id: accountRow
                        required property var modelData
                        width: ListView.view.width
                        height: 48

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            Text {
                                Layout.fillWidth: true
                                text: accountRow.modelData.name
                                color: root.textPrimary
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Text {
                                text: root.formatAmount(
                                    accountRow.modelData.initialBalanceMinor,
                                    accountRow.modelData.currency,
                                    false, "")
                                color: root.textSecondary
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: categoryDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(520, root.width - 52)
        height: Math.min(580, root.height - 40)
        anchors.centerIn: parent
        padding: 0

        property int typeIndex: 1
        property string editingId: ""
        readonly property string categoryType: typeIndex === 0 ? "income" : "expense"
        readonly property var visibleCategories: typeIndex === 0
            ? root.incomeCategories : root.expenseCategories

        function openForManagement() {
            typeIndex = 1
            categoryNameField.text = ""
            categoryError.text = ""
            editingId = ""
            open()
        }

        function beginEdit(category) {
            editingId = category.value
            categoryNameField.text = category.label
            categoryError.text = ""
            categoryNameField.forceActiveFocus()
            categoryNameField.selectAll()
        }

        function cancelEdit() {
            editingId = ""
            categoryNameField.text = ""
            categoryError.text = ""
        }

        function saveCurrentCategory() {
            if (categoryNameField.text.trim().length === 0)
                return

            const saved = editingId.length > 0
                ? financeController.renameCategory(editingId, categoryNameField.text)
                : financeController.addCategory(categoryNameField.text, categoryType)

            if (saved) {
                cancelEdit()
            } else {
                categoryError.text = "Категория уже существует или не может быть сохранена"
            }
        }

        onTypeIndexChanged: cancelEdit()

        Overlay.modal: Rectangle { color: "#740B1220" }

        background: Rectangle {
            radius: 24
            color: root.surfaceColor
            border.width: 1
            border.color: root.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Категории"
                        color: root.textPrimary
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "Создание категорий доходов и расходов"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    implicitWidth: 34
                    implicitHeight: 34
                    flat: true
                    contentItem: Text {
                        text: "×"
                        color: root.textSecondary
                        font.pixelSize: 21
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle { color: "transparent" }
                    onClicked: categoryDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 14
                color: "#F2F4F7"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4

                    Repeater {
                        model: ["Доходы", "Расходы"]
                        delegate: Button {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            flat: true
                            contentItem: Text {
                                text: modelData
                                color: root.textPrimary
                                font.pixelSize: 12
                                font.weight: categoryDialog.typeIndex === index
                                             ? Font.DemiBold : Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 11
                                color: categoryDialog.typeIndex === index
                                       ? root.surfaceColor : "transparent"
                                border.width: categoryDialog.typeIndex === index ? 1 : 0
                                border.color: root.borderColor
                            }
                            onClicked: categoryDialog.typeIndex = index
                        }
                    }
                }
            }

            Text {
                text: categoryDialog.editingId.length > 0
                      ? "Изменить категорию" : "Добавить категорию"
                color: root.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TextField {
                    id: categoryNameField
                    Layout.fillWidth: true
                    implicitHeight: 44
                    maximumLength: 60
                    placeholderText: "Название категории"
                    color: root.textPrimary
                    leftPadding: 13
                    rightPadding: 13
                    background: Rectangle {
                        radius: 12
                        color: root.surfaceSoft
                        border.width: categoryNameField.activeFocus ? 1.5 : 1
                        border.color: categoryNameField.activeFocus
                                      ? root.accentColor : root.borderColor
                    }
                    onAccepted: categoryDialog.saveCurrentCategory()
                }

                Button {
                    id: addCategoryButton
                    implicitWidth: 104
                    implicitHeight: 44
                    enabled: categoryNameField.text.trim().length > 0
                    contentItem: Text {
                        text: categoryDialog.editingId.length > 0
                              ? "Сохранить" : "Добавить"
                        color: "white"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 12
                        color: addCategoryButton.enabled
                               ? root.accentColor : "#AEBBEB"
                    }
                    onClicked: categoryDialog.saveCurrentCategory()
                }

                Button {
                    visible: categoryDialog.editingId.length > 0
                    implicitWidth: 80
                    implicitHeight: 44
                    contentItem: Text {
                        text: "Отмена"
                        color: root.textPrimary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 12
                        color: root.surfaceColor
                        border.width: 1
                        border.color: root.borderColor
                    }
                    onClicked: categoryDialog.cancelEdit()
                }
            }

            Text {
                id: categoryError
                Layout.fillWidth: true
                color: root.expenseColor
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 14
                color: root.surfaceSoft
                border.width: 1
                border.color: root.borderColor

                ListView {
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    model: categoryDialog.visibleCategories
                    delegate: Item {
                        id: categoryRow
                        required property var modelData
                        width: ListView.view.width
                        height: 48

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 6
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: categoryRow.modelData.label
                                color: root.textPrimary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            Button {
                                id: editCategoryButton
                                implicitWidth: 76
                                implicitHeight: 32
                                flat: true
                                contentItem: Text {
                                    text: "Изменить"
                                    color: root.accentColor
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 9
                                    color: editCategoryButton.hovered
                                           ? "#EEF2FF" : "transparent"
                                }
                                onClicked: categoryDialog.beginEdit(categoryRow.modelData)
                            }

                            Button {
                                id: removeCategoryButton
                                implicitWidth: 64
                                implicitHeight: 32
                                flat: true
                                contentItem: Text {
                                    text: "Удалить"
                                    color: root.expenseColor
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 9
                                    color: removeCategoryButton.hovered
                                           ? root.expenseSoft : "transparent"
                                }
                                onClicked: deleteCategoryDialog.openForCategory(
                                               categoryRow.modelData)
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteCategoryDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(420, root.width - 52)
        height: 220
        anchors.centerIn: parent
        padding: 0

        property string categoryId: ""
        property string categoryName: ""

        function openForCategory(category) {
            categoryId = category.value
            categoryName = category.label
            open()
        }

        background: Rectangle {
            radius: 20
            color: root.surfaceColor
            border.width: 1
            border.color: root.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16
            Text {
                Layout.fillWidth: true
                text: "Удалить категорию «" + deleteCategoryDialog.categoryName + "»?"
                color: root.textPrimary
                font.pixelSize: 16
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                text: "Старые транзакции сохранят название категории. Для новых операций она больше не будет доступна."
                color: root.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Отмена"
                    onClicked: deleteCategoryDialog.close()
                }
                Button {
                    text: "Удалить"
                    onClicked: {
                        if (financeController.deleteCategory(
                                deleteCategoryDialog.categoryId)) {
                            if (categoryDialog.editingId ===
                                    deleteCategoryDialog.categoryId)
                                categoryDialog.cancelEdit()
                            deleteCategoryDialog.close()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: transactionDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(610, root.width - 52)
        height: Math.min(650, root.height - 24)
        anchors.centerIn: parent
        padding: 0

        property int typeIndex: 1
        readonly property bool isIncome: typeIndex === 0

        function openForNewTransaction() {
            typeIndex = 1
            amountField.text = ""
            descriptionField.text = ""
            transactionCurrency.currentIndex = 0
            categoryPicker.currentIndex = 0
            open()
            amountField.forceActiveFocus()
        }

        function normalizedAmount() {
            const normalized = amountField.text.trim().replace(",", ".")
            const value = Number(normalized)
            if (!isFinite(value) || value <= 0)
                return 0
            return Math.round(value * 100)
        }

        onTypeIndexChanged: categoryPicker.currentIndex = 0

        Overlay.modal: Rectangle {
            color: "#740B1220"
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 5
                anchors.leftMargin: 5
                radius: 25
                color: "#1A0B1220"
            }

            Rectangle {
                anchors.fill: parent
                anchors.bottomMargin: 5
                anchors.rightMargin: 5
                radius: 25
                color: root.surfaceColor
                border.width: 1
                border.color: root.borderColor
            }
        }

        contentItem: Item {
            anchors.fill: parent

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 13

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: "Новая операция"
                            color: root.textPrimary
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }

                        Text {
                            text: transactionDialog.isIncome ? "Добавление дохода" : "Добавление расхода"
                            color: root.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        id: closeDialogButton
                        implicitWidth: 34
                        implicitHeight: 34
                        hoverEnabled: true
                        flat: true

                        contentItem: Text {
                            text: "×"
                            color: root.textSecondary
                            font.pixelSize: 21
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 11
                            color: closeDialogButton.hovered ? "#F2F4F7" : "transparent"
                        }

                        onClicked: transactionDialog.close()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 14
                    color: "#F2F4F7"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        Repeater {
                            model: ["Доход", "Расход"]

                            delegate: Button {
                                id: typeSegment
                                required property string modelData
                                required property int index

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                hoverEnabled: true
                                flat: true

                                contentItem: Text {
                                    text: typeSegment.modelData
                                    color: transactionDialog.typeIndex === typeSegment.index
                                           ? root.textPrimary
                                           : root.textSecondary
                                    font.pixelSize: 12
                                    font.weight: transactionDialog.typeIndex === typeSegment.index
                                                 ? Font.DemiBold
                                                 : Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    radius: 11
                                    color: transactionDialog.typeIndex === typeSegment.index ? root.surfaceColor : "transparent"
                                    border.width: transactionDialog.typeIndex === typeSegment.index ? 1 : 0
                                    border.color: root.borderColor
                                }

                                onClicked: transactionDialog.typeIndex = typeSegment.index
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    radius: 18
                    color: root.heroColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                text: "Сумма"
                                color: "#9CA8BC"
                                font.pixelSize: 10
                            }

                            TextField {
                                id: amountField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                leftPadding: 0
                                rightPadding: 0
                                topPadding: 0
                                bottomPadding: 0
                                placeholderText: "0,00"
                                placeholderTextColor: "#65728A"
                                color: "white"
                                font.pixelSize: 27
                                font.weight: Font.Bold
                                selectByMouse: true
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                validator: RegularExpressionValidator {
                                    regularExpression: /^[0-9]+([\.,][0-9]{0,2})?$/
                                }

                                background: Rectangle {
                                    color: "transparent"
                                }
                            }
                        }

                        BankComboBox {
                            id: transactionCurrency
                            Layout.preferredWidth: 132
                            compact: true
                            model: root.currencyData
                            textRole: "code"
                            secondaryRole: "symbol"
                            currentIndex: 0
                            surfaceColor: root.heroSoftColor
                            surfaceHoverColor: "#223150"
                            borderColor: "#31405B"
                            focusColor: "#7895FF"
                            textPrimary: "#FFFFFF"
                            textSecondary: "#B5C0D1"
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Категория"
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    CategoryPicker {
                        id: categoryPicker
                        Layout.fillWidth: true
                        model: transactionDialog.isIncome ? root.incomeCategories : root.expenseCategories
                        incomeMode: transactionDialog.isIncome
                        accentColor: root.accentColor
                        textPrimary: root.textPrimary
                        textSecondary: root.textSecondary
                        borderColor: root.borderColor
                        surfaceColor: root.surfaceColor
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Text {
                        text: "Комментарий"
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    TextField {
                        id: descriptionField
                        Layout.fillWidth: true
                        implicitHeight: 46
                        leftPadding: 14
                        rightPadding: 14
                        placeholderText: transactionDialog.isIncome
                                         ? "Например, зарплата за август"
                                         : "Например, супермаркет"
                        placeholderTextColor: "#A0A8B5"
                        color: root.textPrimary
                        font.pixelSize: 12
                        selectByMouse: true

                        background: Rectangle {
                            radius: 13
                            color: root.surfaceSoft
                            border.width: descriptionField.activeFocus ? 1.5 : 1
                            border.color: descriptionField.activeFocus ? root.accentColor : root.borderColor
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        id: cancelButton
                        Layout.preferredWidth: 120
                        implicitHeight: 46
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Отмена"
                            color: root.textPrimary
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 13
                            color: cancelButton.hovered ? "#F5F7F9" : root.surfaceColor
                            border.width: 1
                            border.color: root.borderColor
                        }

                        onClicked: transactionDialog.close()
                    }

                    Button {
                        id: saveButton
                        Layout.fillWidth: true
                        implicitHeight: 46
                        enabled: transactionDialog.normalizedAmount() > 0
                        hoverEnabled: true

                        contentItem: Text {
                            text: transactionDialog.isIncome ? "Добавить доход" : "Добавить расход"
                            color: "white"
                            opacity: saveButton.enabled ? 1 : 0.72
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 13
                            color: !saveButton.enabled
                                   ? "#AEBBEB"
                                   : (saveButton.down
                                      ? root.accentPressed
                                      : (saveButton.hovered ? "#2B56E3" : root.accentColor))
                        }

                        onClicked: {
                            const minorUnits = transactionDialog.normalizedAmount()
                            const currencyCode = root.currencyData[transactionCurrency.currentIndex].code
                            const categoryId = categoryPicker.currentValue

                            let saved = false
                            if (transactionDialog.isIncome) {
                                saved = financeController.addIncome(
                                    minorUnits,
                                    descriptionField.text.trim(),
                                    categoryId,
                                    currencyCode)
                            } else {
                                saved = financeController.addExpense(
                                    minorUnits,
                                    descriptionField.text.trim(),
                                    categoryId,
                                    currencyCode)
                            }

                            if (saved)
                                transactionDialog.close()
                        }
                    }
                }
            }
        }
    }
}
