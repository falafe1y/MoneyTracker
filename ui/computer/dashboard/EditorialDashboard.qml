import QtQuick
import QtQuick.Controls
import "DashboardData.js" as Data

Flickable {
    id: view
    objectName: "overviewScroll"
    required property var host
    required property var controller
    readonly property bool narrow: host.width <= 850
    readonly property bool small: host.width <= 1150 && !narrow
    readonly property bool medium: host.width <= 1500
    readonly property bool compact: host.height <= 1000 && !narrow
    readonly property int pad: narrow ? 20 : small ? 24 : medium ? 32 : 48
    readonly property int inset: small ? 16 : 24
    readonly property int sectionNumberWidth: small ? 24 : 32
    readonly property int topInset: compact ? 16 : 24
    readonly property int mainWidth: Math.max(0, width - pad * 2)
    readonly property int leftWidth: narrow ? mainWidth : Math.round(mainWidth * (small ? .58 : .59596) / 4) * 4
    readonly property string selectedId: controller.selectedAsset === "crypto" ? controller.selectedCryptoWalletId : controller.selectedAccountId
    property string asset: "all"
    property string category: ""
    property string sortField: "date"
    property int sortDirection: -1
    property int eventIndex: 0
    readonly property var accountRows: controller.selectedAsset === "crypto" ? controller.cryptoWallets : controller.accounts
    readonly property var sourceRows: Data.history(controller, host, asset, selectedId)
    readonly property var historyRows: Data.filterRows(sourceRows, host.searchText, category, sortField, sortDirection)
    readonly property var expenseGroups: Data.expenses(sourceRows)
    readonly property var expenseLegend: Data.legend(expenseGroups)
    readonly property real totalExpense: expenseGroups.reduce(function(sum, g) { return sum + g.amount; }, 0)
    readonly property var schedule: controller.scheduledTransactions
    readonly property var nextEvent: schedule.length ? schedule[Math.min(eventIndex, schedule.length - 1)] : null
    readonly property var capitalRows: controller.capitalHistory
    readonly property var chartColors: ["#031528", "#be7b56", "#b8ae99"]
    readonly property var columns: narrow ? [.22, .18, .32, 0, 0, .28] : [.10101, .09596, .14647, .12626, .37879, .15151]
    readonly property int firstVisibleRow: Math.max(0, Math.floor((contentY - historyBody.y - paper.y) / 56) - 3)
    readonly property int lastVisibleRow: Math.min(historyRows.length, Math.max(firstVisibleRow, Math.ceil((contentY + height - historyBody.y - paper.y) / 56) + 3))
    clip: true
    contentWidth: width
    contentHeight: paper.height + 72
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        let delta = 0;
        if (event.key === Qt.Key_PageDown) delta = height - 56;
        else if (event.key === Qt.Key_PageUp) delta = -height + 56;
        else if (event.key === Qt.Key_Down) delta = 56;
        else if (event.key === Qt.Key_Up) delta = -56;
        else if (event.key === Qt.Key_Home) delta = -contentHeight;
        else if (event.key === Qt.Key_End) delta = contentHeight;
        else return;
        cancelFlick(); contentY += delta; clampScroll(); event.accepted = true;
    }
    ScrollBar.vertical: ScrollBar {
        width: 6; policy: ScrollBar.AsNeeded
        contentItem: Rectangle { radius: 0; color: "#bbb5a8"; implicitWidth: 6; opacity: parent.active ? 1 : .5 }
        background: Item {}
    }
    onContentHeightChanged: clampScroll()
    onHeightChanged: clampScroll()
    onSelectedIdChanged: category = ""
    function clampScroll() { contentY = Math.max(0, Math.min(contentY, Math.max(0, contentHeight - height))); }
    function selectAsset(code) {
        asset = code;
        controller.selectedAccountId = "";
        controller.selectedCryptoWalletId = "";
        controller.selectedAsset = code === "all" ? "fiat" : code;
        category = "";
        accounts.positionViewAtBeginning();
    }
    function selectAccount(row) {
        if (controller.selectedAsset === "crypto") controller.selectedCryptoWalletId = selectedId === row.id ? "" : row.id;
        else controller.selectedAccountId = selectedId === row.id ? "" : row.id;
    }
    function clearFilters() { controller.selectedAccountId = ""; controller.selectedCryptoWalletId = ""; category = ""; }
    function showHistory() { contentY = Math.min(historyToolbar.y + paper.y, Math.max(0, contentHeight - height)); }
    function sortBy(field) { sortDirection = sortField === field ? -sortDirection : -1; sortField = field; }
    function colStart(index) { let value = 0; for (let i = 0; i < index; ++i) value += columns[i]; return Math.round(mainWidth * value / 4) * 4; }
    function colWidth(index) { return index === 5 ? mainWidth - colStart(index) : colStart(index + 1) - colStart(index); }
    function assetBalance(code) { return code === "all" ? controller.balanceMinorUnits : host.assetAmount(code); }
    function money(minor, code, sign, digits) { return Data.money(minor, code, sign, digits, controller.uiLanguage); }
    function scheduledAmount() {
        if (!nextEvent) return "";
        const converted = Data.convertMinor(nextEvent.amount, nextEvent.currency, controller.appCurrency, controller.currentCurrencyRates);
        return money((nextEvent.type === "income" ? 1 : -1) * (converted === null ? nextEvent.amount : converted), converted === null ? nextEvent.currency : controller.appCurrency, true, 0);
    }
    function scheduledMonth() {
        if (!nextEvent) return "";
        const months = controller.uiLanguage === "en" ? ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]
            : ["ЯНВ.", "ФЕВР.", "МАР.", "АПР.", "МАЙ", "ИЮН.", "ИЮЛ.", "АВГ.", "СЕНТ.", "ОКТ.", "НОЯБ.", "ДЕК."];
        return months[Data.dateValue(nextEvent.nextDate).getMonth()];
    }
    function caption(code) {
        if (code === "all") return qsTr("Суммарный капитал");
        if (code === "fiat") return controller.allAccounts.filter(function(a) { return a.asset === "fiat" && a.balanceMinor !== 0; }).length + qsTr(" активных счёта");
        if (code === "crypto") return assetBalance(code) ? qsTr("Суммарная оценка") : qsTr("Нет активов");
        return controller.balanceMinorUnits ? (100 * assetBalance(code) / controller.balanceMinorUnits).toFixed(1).replace(".", ",") + qsTr("% от капитала") : qsTr("Нет активов");
    }
    Component.onCompleted: { if (controller.selectedAsset !== "fiat" || selectedId) asset = controller.selectedAsset; }
    Connections {
        target: view.controller
        function onSelectedAssetChanged() {
            if (!(view.asset === "all" && view.controller.selectedAsset === "fiat")) view.asset = view.controller.selectedAsset;
        }
    }
    component Rule: Rectangle { height: 1; color: "#031528" }
    component Mono: EditorialText { font.family: "Cousine"; font.pixelSize: 12; color: "#59616a" }
    component Serif: EditorialText { font.family: "PT Serif"; font.pixelSize: view.small ? 32 : 36; font.letterSpacing: -.5 }
    component Heading: Item {
        property string number: ""
        property string title: ""
        property string action: ""
        property string note: ""
        signal activated()
        height: 32
        Mono { width: view.sectionNumberWidth; height: parent.height; text: parent.number; color: "#b84932" }
        EditorialText {
            x: view.sectionNumberWidth; height: parent.height
            width: Math.max(0, parent.width - x - (headAction.visible ? headAction.width + 8 : headNote.visible ? headNote.width + 8 : 0))
            text: parent.title; font.bold: true; font.pixelSize: view.small ? 13 : view.medium && !view.narrow ? 15 : 16
        }
        EditorialAction { id: headAction; visible: text !== ""; anchors.right: parent.right; text: parent.action; onClicked: parent.activated() }
        Mono { id: headNote; visible: text !== ""; anchors.right: parent.right; height: 32; text: parent.note; color: "#031528" }
    }

    Item {
        id: paper
        objectName: "editorialPaper"
        x: view.pad; y: 24; width: view.mainWidth
        height: pageFooter.y + pageFooter.height
        Item {
            id: masthead
            width: parent.width; height: view.narrow ? 240 : view.compact ? 128 : 160
            Item {
                id: titleBlock
                y: view.narrow ? 12 : parent.height - height - 8
                width: view.narrow ? parent.width : Math.max(0, filters.x - 16)
                height: (view.narrow ? 64 : view.compact ? 88 : view.medium ? 80 : 96) + (view.compact ? 24 : 32)
                Mono { y: 0; height: 24; width: Math.min(parent.width, 192); text: qsTr("ФИНАНСОВЫЙ ОБЗОР"); color: "#031528"; font.letterSpacing: .5 }
                Mono { x: 240; visible: parent.width >= 320; height: 24; text: "/ № 01"; color: "#b84932" }
                EditorialText {
                    y: view.compact ? 24 : 32; height: parent.height - y; width: parent.width
                    text: qsTr("Мои финансы."); font.family: "PT Serif"; font.italic: true
                    font.pixelSize: view.narrow ? 52 : view.compact ? Math.min(72, Math.max(40, parent.width / 5.5)) : view.medium ? 64 : 80
                    font.letterSpacing: -2
                }
            }
            Item {
                id: filters
                objectName: "editorialFilters"
                anchors.right: parent.right
                y: view.narrow ? 144 : parent.height - height - 8
                width: view.narrow ? parent.width : view.small ? 400 : view.medium ? 496 : 592
                height: 80
                readonly property int gap: view.small ? 12 : 16
                readonly property int planWidth: view.small ? 128 : 144
                readonly property int currencyWidth: view.small ? 96 : view.medium ? 128 : 160
                EditorialAction {
                    anchors.bottom: parent.bottom; width: filters.planWidth; height: 40
                    text: qsTr("ПЛАНОВЫЕ"); mono: false; textSize: view.small ? 14 : 16; outlined: true
                    onClicked: view.host.editorialSchedule(null)
                }
                Item {
                    x: filters.planWidth + filters.gap; height: 72; anchors.bottom: parent.bottom
                    width: filters.width - x - filters.currencyWidth - filters.gap
                    Mono { height: 24; text: qsTr("ПЕРИОД") }
                    EditorialAction {
                        objectName: "editorialPeriod"
                        y: 24; width: parent.width; height: 48
                        text: view.host.dateFilterLabel(); mono: false; textSize: 16; arrow: false; underline: true
                        onClicked: periodMenu.open()
                        Mono { anchors.right: parent.right; height: 48; text: "⌄"; color: "#031528" }
                        Menu {
                            id: periodMenu
                            y: parent.height; width: Math.max(parent.width, 240)
                            font.family: "Arimo"; font.pixelSize: 16
                            background: Rectangle { color: "#f3eddf"; border.color: "#031528" }
                            MenuItem { text: qsTr("Всё время"); onTriggered: view.controller.clearDateFilter() }
                            MenuItem { text: qsTr("Текущий месяц"); onTriggered: { const now = new Date(); view.controller.setDateFilter(new Date(now.getFullYear(), now.getMonth(), 1), now); } }
                            MenuItem { text: qsTr("Последние 7 дней"); onTriggered: { const end = new Date(); const start = new Date(); start.setDate(start.getDate() - 6); view.controller.setDateFilter(start, end); } }
                            MenuItem { text: qsTr("Последние 30 дней"); onTriggered: { const end = new Date(); const start = new Date(); start.setDate(start.getDate() - 29); view.controller.setDateFilter(start, end); } }
                            MenuItem { text: qsTr("Выбрать даты…"); onTriggered: view.host.editorialDateFilter() }
                        }
                    }
                }
                Item {
                    anchors.right: parent.right; anchors.bottom: parent.bottom
                    width: filters.currencyWidth; height: 72
                    Mono { height: 24; text: qsTr("ВАЛЮТА") }
                    ComboBox {
                        id: currency
                        objectName: "editorialCurrency"
                        y: 24; width: parent.width; height: 48; padding: 0; rightPadding: 20
                        model: ["RUB", "USD", "EUR", "CNY"]
                        currentIndex: Math.max(0, model.indexOf(view.controller.appCurrency))
                        font.family: "Arimo"; font.pixelSize: 16; font.bold: true
                        contentItem: EditorialText { text: currency.displayText; font: currency.font }
                        indicator: Mono { x: currency.width - 16; height: 48; text: "⌄"; color: "#031528" }
                        background: Item { Rule { y: parent.height - 1; width: parent.width } }
                        popup.background: Rectangle { color: "#f3eddf"; border.color: "#031528" }
                        onActivated: view.controller.appCurrency = currentText
                    }
                }
            }
        }
        Item {
            id: summary
            objectName: "editorialSummary"
            y: masthead.height; width: parent.width
            height: view.narrow ? 288 : view.compact ? 144 : 160
            Rule { width: parent.width }
            Rule { y: parent.height - 1; width: parent.width }
            Repeater {
                model: [ ["all", qsTr("ВСЕГО")], ["fiat", qsTr("ФИАТ")], ["crypto", qsTr("КРИПТА")], ["investment", qsTr("ИНВЕСТИЦИИ")] ]
                delegate: Button {
                    id: assetButton
                    required property int index
                    required property var modelData
                    readonly property int cellPad: index === 0 || (view.narrow && index === 2) ? 0 : view.small ? 16 : view.medium ? 20 : 24
                    x: view.narrow ? (index % 2) * summary.width / 2 : index === 0 ? 0 : summary.width * (3 + (index - 1) * 2) / 9
                    y: view.narrow ? Math.floor(index / 2) * 144 : 0
                    width: view.narrow ? summary.width / 2 : summary.width * (index === 0 ? 3 : 2) / 9
                    height: view.narrow ? 144 : summary.height
                    padding: 0; hoverEnabled: true
                    objectName: "editorialAsset_" + modelData[0]
                    Accessible.name: modelData[1] + " " + value.text
                    contentItem: Item {
                        Mono { x: assetButton.cellPad; y: view.compact ? 16 : 24; height: 24; width: parent.width - x - 16; text: "0" + (assetButton.index + 1) + " / " + assetButton.modelData[1]; font.letterSpacing: .5 }
                        Serif {
                            id: value
                            x: assetButton.cellPad; y: (view.compact ? 16 : 24) + 32; height: 56; width: parent.width - x - 12
                            text: view.money(view.assetBalance(assetButton.modelData[0]), view.controller.appCurrency, false, assetButton.modelData[0] === "crypto" && !view.assetBalance("crypto") ? 0 : 2)
                            font.pixelSize: assetButton.index === 0 ? (view.small ? 30 : view.medium ? 36 : 44) : view.small ? 26 : view.medium ? 30 : 36
                            font.letterSpacing: -.8
                            minimumPixelSize: 20; fontSizeMode: Text.Fit
                        }
                        Mono { x: assetButton.cellPad; y: value.y + 64; height: 24; width: parent.width - x - 12; text: view.caption(assetButton.modelData[0]) }
                        EditorialArrow { visible: assetButton.index > 0; x: parent.width - width - (view.small ? 12 : 24); y: view.compact ? 16 : 24; width: view.small ? 16 : 24; height: width }
                    }
                    background: Rectangle {
                        color: assetButton.hovered ? "#05031528" : "transparent"
                        border.width: assetButton.activeFocus ? 1 : 0; border.color: "#b84932"
                        Rectangle { visible: assetButton.index !== 0 && !(view.narrow && assetButton.index === 2); width: 1; height: parent.height; color: "#bbb5a8" }
                        Rule { visible: view.asset === assetButton.modelData[0]; y: parent.height - 2; height: 2; width: parent.width - 24 }
                        Rule { visible: view.narrow && assetButton.index < 2; y: parent.height - 1; width: parent.width; color: "#bbb5a8" }
                    }
                    onClicked: view.selectAsset(modelData[0])
                }
            }
        }
        Item {
            id: firstRow
            y: summary.y + summary.height; width: parent.width
            height: view.narrow ? 544 : view.compact ? 240 : 264
            Rule { y: parent.height - 1; width: parent.width }
            Item {
                id: accountsPanel
                width: view.leftWidth - (view.narrow ? 0 : view.inset); height: view.narrow ? 280 : parent.height
                Heading {
                    id: accountsHead
                    y: view.topInset; width: parent.width; number: "01."
                    title: qsTr("СЧЕТА / ") + (view.controller.selectedAsset === "crypto" ? qsTr("КРИПТА") : view.host.assetTitle(view.controller.selectedAsset).toUpperCase())
                    action: qsTr("+ ДОБАВИТЬ СЧЁТ")
                    onActivated: view.host.editorialAddAccount()
                }
                readonly property int numberWidth: view.small ? 20 : view.medium ? 24 : 32
                readonly property int amountWidth: view.small ? 136 : view.medium ? 176 : 208
                readonly property int arrowWidth: view.small ? 16 : 24
                readonly property int gap: view.small ? 8 : view.medium ? 12 : 16
                Item {
                    id: accountColumnHead
                    y: accountsHead.y + 40; width: parent.width - 8; height: 24
                    Mono { width: accountsPanel.numberWidth; height: 24; text: "№" }
                    Mono { x: accountsPanel.numberWidth + accountsPanel.gap; height: 24; width: parent.width - x - accountsPanel.amountWidth - accountsPanel.arrowWidth - accountsPanel.gap * 2; text: qsTr("НАЗВАНИЕ / ТИП") }
                    Mono { anchors.right: parent.right; anchors.rightMargin: accountsPanel.arrowWidth + accountsPanel.gap; width: accountsPanel.amountWidth; height: 24; text: qsTr("ОСТАТОК"); horizontalAlignment: Text.AlignRight }
                }
                ListView {
                    id: accounts
                    objectName: "editorialAccountsScroll"
                    y: accountColumnHead.y + accountColumnHead.height
                    width: parent.width; height: view.compact ? 152 : 168
                    clip: true; boundsBehavior: Flickable.StopAtBounds; reuseItems: true
                    model: view.accountRows
                    ScrollBar.vertical: ScrollBar {
                        width: 6; policy: ScrollBar.AsNeeded
                        contentItem: Rectangle { implicitWidth: 6; color: "#bbb5a8" }
                        background: Item {}
                    }
                    delegate: Button {
                        id: accountButton
                        required property var modelData
                        required property int index
                        objectName: "editorialAccountRow"
                        readonly property bool selected: view.selectedId === modelData.id
                        width: accounts.width - 8; height: 56; padding: 0; hoverEnabled: true
                        Accessible.name: modelData.name + " " + view.host.accountCompactAmount(modelData)
                        contentItem: Item {
                            Mono { width: accountsPanel.numberWidth; height: 56; text: String(accountButton.index + 1).padStart(2, "0") }
                            Item {
                                x: accountsPanel.numberWidth + accountsPanel.gap; height: 56
                                width: parent.width - x - accountsPanel.amountWidth - accountsPanel.arrowWidth - accountsPanel.gap * 2
                                EditorialText { y: 2; height: 24; width: parent.width; text: accountButton.modelData.name; font.bold: true; font.pixelSize: view.small ? 14 : 16 }
                                EditorialText { y: 30; height: 20; width: parent.width; text: accountButton.modelData.isExchange ? qsTr("Биржевой счёт") : accountButton.modelData.isCrypto ? (accountButton.modelData.symbol + " · " + accountButton.modelData.network) : view.host.accountTypeLabel(accountButton.modelData.type); font.pixelSize: view.small ? 12 : 14; color: "#59616a" }
                            }
                            Serif {
                                anchors.right: parent.right; anchors.rightMargin: accountsPanel.arrowWidth + accountsPanel.gap
                                width: accountsPanel.amountWidth; height: 56
                                text: view.host.accountCompactAmount(accountButton.modelData)
                                font.pixelSize: view.small ? 22 : view.medium ? 24 : 28
                                horizontalAlignment: Text.AlignRight; minimumPixelSize: 16; fontSizeMode: Text.Fit
                            }
                            EditorialArrow { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: accountsPanel.arrowWidth; height: width }
                        }
                        background: Rectangle {
                            color: accountButton.selected ? "#09031528" : accountButton.hovered ? "#06031528" : "transparent"
                            border.width: accountButton.activeFocus ? 1 : 0; border.color: "#b84932"
                            Rule { y: parent.height - 1; width: parent.width; color: "#bbb5a8" }
                            Rectangle { visible: accountButton.selected; y: 8; height: 40; width: 2; color: "#031528" }
                        }
                        onClicked: view.selectAccount(modelData)
                        TapHandler { acceptedButtons: Qt.RightButton; onTapped: (point) => view.host.openAccountContextMenu(accountButton.modelData, accountButton, point.position.x, point.position.y) }
                    }
                    Mono { visible: accounts.count === 0; anchors.fill: parent; horizontalAlignment: Text.AlignHCenter; text: qsTr("Счетов пока нет. Добавьте первый счёт.") }
                }
            }
            Item {
                id: horizon
                x: view.narrow ? 0 : view.leftWidth; y: view.narrow ? accountsPanel.height : 0
                width: parent.width - x; height: view.narrow ? 264 : parent.height
                Rule { visible: !view.narrow; width: 1; height: parent.height }
                Rule { visible: view.narrow; width: parent.width }
                Item {
                    x: view.narrow ? 0 : view.inset; y: view.topInset; width: parent.width - x; height: parent.height - y - view.topInset
                    Heading { id: horizonHead; width: parent.width; number: "02."; title: qsTr("НА ГОРИЗОНТЕ"); action: qsTr("ВСЕ СОБЫТИЯ"); onActivated: view.host.editorialSchedule(null) }
                    Button {
                        id: event
                        y: 32 + (view.compact ? 16 : 24); width: parent.width; height: 80; padding: 0
                        enabled: view.nextEvent !== null
                        contentItem: Item {
                            Item {
                                visible: view.nextEvent !== null; width: view.small ? 56 : 64; height: 80
                                Serif { height: 56; width: parent.width; text: view.nextEvent ? Data.dateValue(view.nextEvent.nextDate).getDate() : ""; font.pixelSize: 44 }
                                Mono { y: 56; height: 24; width: parent.width; text: view.scheduledMonth() }
                                Rule { anchors.right: parent.right; width: 1; height: parent.height; color: "#bbb5a8" }
                            }
                            Item {
                                visible: view.nextEvent !== null
                                x: (view.small ? 56 : 64) + (view.small ? 12 : 16); width: parent.width - x; height: 80
                                Serif { width: Math.max(0, parent.width - eventAmount.width - 16); height: 40; text: view.nextEvent ? view.nextEvent.name : ""; font.pixelSize: view.small ? 22 : view.medium ? 24 : 28 }
                                Serif { id: eventAmount; anchors.right: parent.right; height: 40; text: view.scheduledAmount(); font.pixelSize: view.small ? 22 : view.medium ? 24 : 28; color: view.nextEvent && view.nextEvent.type === "income" ? "#2e5947" : "#b84932" }
                                Mono { y: 48; width: parent.width; height: 24; text: view.nextEvent ? qsTr("ПЛАНОВЫЙ ") + view.host.transactionTypeLabel(view.nextEvent).toUpperCase() + " · " + Data.dateValue(view.nextEvent.nextDate).getFullYear() : "" }
                            }
                            Mono { visible: !view.nextEvent; anchors.fill: parent; text: qsTr("Плановых операций нет"); horizontalAlignment: Text.AlignHCenter }
                        }
                        background: Rectangle { color: event.hovered ? "#06031528" : "transparent"; border.width: event.activeFocus ? 1 : 0; border.color: "#b84932" }
                        onClicked: view.host.editorialSchedule(view.nextEvent)
                    }
                    Item {
                        anchors.bottom: parent.bottom; width: parent.width; height: 32
                        Rule { width: parent.width; color: "#bbb5a8" }
                        Mono { height: 32; width: parent.width - 120; text: view.nextEvent ? qsTr("БЛИЖАЙШАЯ ОПЕРАЦИЯ") : qsTr("ВСЁ ПОД КОНТРОЛЕМ") }
                        Row {
                            anchors.right: parent.right; height: 32; spacing: 8
                            EditorialAction { width: 24; text: "‹"; textSize: 20; arrow: false; enabled: view.schedule.length > 1; onClicked: view.eventIndex = (view.eventIndex - 1 + view.schedule.length) % view.schedule.length }
                            Mono { height: 32; text: String(view.schedule.length ? Math.min(view.eventIndex + 1, view.schedule.length) : 0).padStart(2, "0") + " / " + String(view.schedule.length).padStart(2, "0") }
                            EditorialAction { width: 24; text: "›"; textSize: 20; arrow: false; enabled: view.schedule.length > 1; onClicked: view.eventIndex = (view.eventIndex + 1) % view.schedule.length }
                        }
                    }
                }
            }
        }
        Item {
            id: analytics
            y: firstRow.y + firstRow.height; width: parent.width; height: view.narrow ? 528 : view.compact ? 224 : 264
            Rule { y: parent.height - 1; width: parent.width }
            Item {
                id: capital
                y: view.topInset; width: view.leftWidth - (view.narrow ? 0 : view.inset); height: (view.narrow ? 264 : analytics.height) - y * 2
                Heading { width: parent.width; number: "03."; title: qsTr("ИСТОРИЯ КАПИТАЛА"); note: view.capitalRows.length ? Data.shortDate(view.capitalRows[0].date) + " — " + Data.shortDate(view.capitalRows[view.capitalRows.length - 1].date) : "" }
                Serif { y: view.compact ? 40 : 48; width: parent.width - 160; height: 48; text: view.money(view.controller.balanceMinorUnits, view.controller.appCurrency, false, 0); minimumPixelSize: 24; fontSizeMode: Text.Fit }
                Mono { y: view.compact ? 40 : 48; anchors.right: parent.right; width: 152; height: 32; text: qsTr("СУММА ВСЕХ АКТИВОВ\nВ ") + view.controller.appCurrency; horizontalAlignment: Text.AlignRight }
                Canvas {
                    id: chart
                    objectName: "editorialCapitalChart"
                    y: (view.compact ? 40 : 48) + 48; width: parent.width; height: view.compact ? 104 : 120
                    property var rows: view.capitalRows
                    onRowsChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const c = getContext("2d"); c.reset();
                        if (!rows.length) return;
                        const scaleX = width / 920, scaleY = height / 120;
                        const values = rows.map(function(r) { return Number(r.totalMinor); });
                        let low = Math.min.apply(null, values), high = Math.max.apply(null, values);
                        const padding = Math.max((high - low) * .15, Math.abs(high) * .02, 1000); low -= padding; high += padding;
                        function x(i) { return (96 + (rows.length === 1 ? .5 : i / (rows.length - 1)) * 800) * scaleX; }
                        function y(value) { return (96 - (value - low) / (high - low) * 80) * scaleY; }
                        c.font = "12px Cousine"; c.fillStyle = "#59616a"; c.strokeStyle = "#bbb5a8"; c.lineWidth = 1;
                        [high, (low + high) / 2, low].forEach(function(value, i) {
                            const yp = (16 + i * 40) * scaleY;
                            c.beginPath(); c.moveTo(96 * scaleX, yp); c.lineTo(896 * scaleX, yp); c.stroke();
                            c.textAlign = "right"; c.fillText(view.host.capitalAxisMoney(value, view.controller.appCurrency), 80 * scaleX, yp + 4);
                        });
                        c.beginPath(); rows.forEach(function(r, i) { if (i === 0) c.moveTo(x(i), y(r.totalMinor)); else c.lineTo(x(i), y(r.totalMinor)); });
                        c.lineTo(x(rows.length - 1), 96 * scaleY); c.lineTo(x(0), 96 * scaleY); c.closePath(); c.fillStyle = "#0f031528"; c.fill();
                        c.beginPath(); rows.forEach(function(r, i) { if (i === 0) c.moveTo(x(i), y(r.totalMinor)); else c.lineTo(x(i), y(r.totalMinor)); });
                        c.strokeStyle = "#031528"; c.lineWidth = 2; c.stroke();
                        c.beginPath(); c.arc(x(rows.length - 1), y(rows[rows.length - 1].totalMinor), 4, 0, Math.PI * 2); c.fillStyle = "#f3eddf"; c.fill(); c.stroke();
                        c.fillStyle = "#59616a";
                        [0, Math.floor((rows.length - 1) / 2), rows.length - 1].forEach(function(index, i) { c.textAlign = ["left", "center", "right"][i]; c.fillText(Data.shortDate(rows[index].date), (96 + i * 400) * scaleX, height - 2); });
                    }
                }
                Mono { visible: !view.capitalRows.length; y: chart.y; width: parent.width; height: chart.height; text: qsTr("История капитала появится после добавления данных."); horizontalAlignment: Text.AlignHCenter }
            }
            Item {
                id: expense
                x: view.narrow ? 0 : view.leftWidth; y: view.narrow ? 264 : 0
                width: parent.width - x; height: view.narrow ? 264 : parent.height
                Rule { visible: !view.narrow; width: 1; height: parent.height }
                Rule { visible: view.narrow; width: parent.width }
                Item {
                    x: view.narrow ? 0 : view.inset; y: view.topInset; width: parent.width - x; height: parent.height - y * 2
                    Heading { width: parent.width; number: "04."; title: qsTr("АНАТОМИЯ РАСХОДОВ"); note: view.small ? "" : view.controller.dateFilterActive ? qsTr("ЗА ПЕРИОД") : qsTr("ЗА ВСЁ ВРЕМЯ") }
                    Serif { y: view.compact ? 40 : 48; width: parent.width - 144; height: 48; text: view.money(view.totalExpense, view.controller.appCurrency, false, 0); minimumPixelSize: 24; fontSizeMode: Text.Fit }
                    Mono { y: view.compact ? 40 : 48; anchors.right: parent.right; width: 136; height: 32; text: qsTr("ОБЩИЙ РАСХОД\nЗА ПЕРИОД"); horizontalAlignment: Text.AlignRight }
                    Item {
                        y: (view.compact ? 40 : 48) + 48; width: parent.width; height: view.compact ? 104 : 120
                        Canvas {
                            id: donut
                            width: view.compact || view.small ? 104 : 120; height: width
                            anchors.verticalCenter: parent.verticalCenter
                            property var groups: view.expenseLegend
                            onGroupsChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onPaint: {
                                const c = getContext("2d"); c.reset();
                                const radius = width / 2, thickness = width === 104 ? 16 : 20;
                                let start = -Math.PI / 2;
                                c.lineWidth = thickness;
                                if (!view.totalExpense) { c.strokeStyle = "#bbb5a8"; c.beginPath(); c.arc(radius, radius, radius - thickness / 2, 0, Math.PI * 2); c.stroke(); return; }
                                groups.forEach(function(g, i) { const end = start + g.amount / view.totalExpense * Math.PI * 2; c.beginPath(); c.strokeStyle = view.chartColors[i]; c.arc(radius, radius, radius - thickness / 2, start, end); c.stroke(); start = end; });
                            }
                            Serif { y: (parent.height - 52) / 2; height: 36; width: parent.width; text: String(view.expenseGroups.length).padStart(2, "0"); font.pixelSize: 28; horizontalAlignment: Text.AlignHCenter }
                            Mono { y: (parent.height - 52) / 2 + 36; height: 16; width: parent.width; text: qsTr("КАТЕГОРИИ"); horizontalAlignment: Text.AlignHCenter }
                        }
                        Column {
                            x: donut.width + (view.small ? 16 : 24); width: parent.width - x
                            anchors.verticalCenter: parent.verticalCenter
                            Repeater {
                                model: view.expenseLegend
                                delegate: Button {
                                    id: legendButton
                                    required property var modelData
                                    required property int index
                                    width: parent.width; height: view.compact ? 32 : 40; padding: 0; hoverEnabled: true
                                    contentItem: Item {
                                        Rectangle { y: (parent.height - 8) / 2; width: 8; height: 8; color: view.chartColors[legendButton.index] }
                                        EditorialText { x: 16; height: parent.height; width: Math.max(0, parent.width - x - legendMoney.width - 8); text: legendButton.modelData.name; font.pixelSize: view.small ? 12 : 14; color: legendButton.hovered ? "#b84932" : "#031528" }
                                        EditorialText { id: legendMoney; anchors.right: parent.right; height: parent.height; text: view.money(legendButton.modelData.amount, view.controller.appCurrency, false, 0); font.pixelSize: view.small ? 12 : 14 }
                                    }
                                    background: Item { Rule { y: parent.height - 1; width: parent.width; color: "#bbb5a8" } }
                                    onClicked: { view.category = modelData.other ? "" : modelData.name; view.showHistory(); }
                                }
                            }
                        }
                        Mono { visible: !view.expenseGroups.length; x: donut.width + 24; width: parent.width - x; height: parent.height; text: qsTr("За период расходов нет.") }
                    }
                }
            }
        }
        Item {
            id: historyToolbar
            y: analytics.y + analytics.height; width: parent.width; height: view.narrow ? 104 : 56
            Heading { x: 0; y: view.narrow ? 8 : 12; width: view.narrow ? parent.width : Math.max(0, search.x - 16); number: "05."; title: qsTr("РЕЕСТР ОПЕРАЦИЙ") }
            EditorialAction {
                id: addOperation
                objectName: "editorialAddOperation"
                anchors.right: parent.right; y: view.narrow ? 56 : 12
                width: view.small ? 160 : view.medium ? 176 : 192
                text: view.controller.selectedAsset === "crypto" ? qsTr("ОБНОВИТЬ АКТИВЫ") : qsTr("+ НОВАЯ ОПЕРАЦИЯ")
                underline: true
                onClicked: { if (view.controller.selectedAsset === "crypto") view.controller.refreshCryptoWallets(); else if (view.controller.selectedAsset === "investment") investmentMenu.open(); else view.host.openNewOperationForSelectedAsset(); }
                Menu {
                    id: investmentMenu
                    y: parent.height; width: 256
                    MenuItem { text: qsTr("Операция по счёту"); enabled: !!view.selectedId; onTriggered: view.host.openNewOperationForSelectedAsset() }
                    MenuItem { text: qsTr("Добавить позицию"); enabled: view.controller.investmentAccounts.length > 0; onTriggered: view.host.editorialAddPosition() }
                }
            }
            TextField {
                id: search
                objectName: "editorialSearch"
                x: view.narrow ? 0 : addOperation.x - width - 16; y: view.narrow ? 48 : 8
                width: view.narrow ? addOperation.x - 16 : view.small ? 184 : 224; height: 40
                padding: 0; font.family: "Arimo"; font.pixelSize: 14
                color: "#031528"; placeholderTextColor: "#59616a"; selectionColor: "#be7b56"
                placeholderText: qsTr("Поиск по операциям…")
                text: view.host.searchText
                onTextEdited: view.host.searchText = text
                background: Item { Rule { y: parent.height - 1; width: parent.width; color: search.activeFocus ? "#b84932" : "#031528" } }
            }
        }
        EditorialAction {
            id: filterChip
            visible: !!view.selectedId || !!view.category
            y: historyToolbar.y + historyToolbar.height; height: visible ? 32 : 0
            width: Math.min(implicitWidth, parent.width)
            text: (view.category || view.accountRows.filter(function(a) { return a.id === view.selectedId; }).map(function(a) { return a.name; }).join("")) + "  ×"
            ink: "#b84932"; arrow: false; underline: true
            onClicked: view.clearFilters()
        }
        Item {
            id: historyHead
            objectName: "editorialHistoryHead"
            y: filterChip.y + filterChip.height; width: parent.width; height: 40
            Rectangle { anchors.fill: parent; color: "#e9e0ce" }
            Rule { width: parent.width }
            Rule { y: parent.height - 1; width: parent.width }
            Repeater {
                model: [["date", qsTr("ДАТА")], ["type", qsTr("ТИП")], ["account", qsTr("СЧЁТ")], ["category", qsTr("КАТЕГОРИЯ")], ["description", qsTr("ОПИСАНИЕ")], ["amount", qsTr("СУММА")]]
                delegate: Button {
                    id: sortButton
                    required property var modelData
                    required property int index
                    x: view.colStart(index); width: view.colWidth(index); height: 40; visible: width > 0; padding: 0
                    Accessible.name: qsTr("Сортировать: ") + modelData[1]
                    contentItem: Mono { text: sortButton.modelData[1]; rightPadding: sortButton.index === 5 ? 0 : 12; horizontalAlignment: sortButton.index === 5 ? Text.AlignRight : Text.AlignLeft }
                    background: Rectangle { color: sortButton.hovered ? "#08031528" : "transparent"; border.width: sortButton.activeFocus ? 1 : 0; border.color: "#b84932" }
                    onClicked: view.sortBy(modelData[0])
                }
            }
        }
        Item {
            id: historyBody
            objectName: "editorialHistoryBody"
            y: historyHead.y + historyHead.height; width: parent.width; height: Math.max(1, view.historyRows.length) * 56
            // Virtualize against the OUTER viewport. The history has no Flickable of its own.
            Repeater {
                model: view.historyRows.slice(view.firstVisibleRow, view.lastVisibleRow)
                delegate: Button {
                    id: historyRow
                    required property int index
                    required property var modelData
                    objectName: "editorialHistoryRow"
                    y: (view.firstVisibleRow + index) * 56; width: historyBody.width; height: 56; padding: 0; hoverEnabled: true
                    Accessible.name: modelData.type + " " + modelData.description + " " + modelData.amount
                    contentItem: Item {
                        Repeater {
                            model: [Qt.formatDate(historyRow.modelData.date, "dd.MM.yyyy"), historyRow.modelData.type.toUpperCase(), historyRow.modelData.account, historyRow.modelData.category, historyRow.modelData.description, historyRow.modelData.amount]
                            delegate: EditorialText {
                                required property int index
                                required property string modelData
                                x: view.colStart(index); width: view.colWidth(index); height: 56; visible: width > 0
                                rightPadding: index === 5 ? 0 : 12
                                text: modelData; font.family: index === 1 ? "Cousine" : "Arimo"
                                font.pixelSize: index === 1 ? 12 : index === 5 ? (view.medium ? 14 : 16) : view.narrow ? 12 : view.medium ? 13 : 14
                                font.bold: index === 5
                                color: index === 5 ? (historyRow.modelData.signed > 0 ? "#2e5947" : "#b84932") : "#031528"
                                horizontalAlignment: index === 5 ? Text.AlignRight : Text.AlignLeft
                            }
                        }
                    }
                    background: Rectangle {
                        color: historyRow.hovered ? "#06031528" : "transparent"
                        border.width: historyRow.activeFocus ? 1 : 0; border.color: "#b84932"
                        Rule { y: parent.height - 1; width: parent.width; color: "#bbb5a8" }
                    }
                    onClicked: view.host.editorialEditOperation(modelData.source, historyRow, width / 2, height / 2)
                    TapHandler { acceptedButtons: Qt.RightButton; onTapped: (point) => view.host.openTransactionContextMenu(historyRow.modelData.source, historyRow, point.position.x, point.position.y) }
                }
            }
            Mono { visible: view.historyRows.length === 0; anchors.fill: parent; horizontalAlignment: Text.AlignHCenter; text: qsTr("Операций не найдено. Измените поиск или снимите фильтр.") }
        }
        Item {
            id: historyFooter
            y: historyBody.y + historyBody.height; width: parent.width; height: 64
            Rule { y: parent.height - 1; width: parent.width }
            Mono { height: parent.height; width: parent.width - 192; text: view.historyRows.length + " / " + qsTr("ОПЕРАЦИЙ · КОНЕЦ РЕЕСТРА") }
            EditorialAction { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: qsTr("СОХРАНИТЬ CSV"); onClicked: view.host.editorialExport() }
        }
        Item {
            id: pageFooter
            y: historyFooter.y + historyFooter.height + 24; width: parent.width; height: (view.narrow ? 80 : 32) + (view.host.csvStatus ? 48 : 0)
            Mono { height: 32; width: view.narrow ? parent.width - 112 : parent.width * .46; text: qsTr("VEXA © 2026 — ФИНАНСЫ БЕЗ ЛИШНЕГО ШУМА") }
            Mono { x: view.narrow ? 0 : parent.width * .48; y: view.narrow ? 40 : 0; height: 32; width: view.narrow ? parent.width : parent.width * .4; text: view.controller.selectedAsset === "crypto" ? view.controller.cryptoLastError : qsTr("НЕЗАВИСИМЫЙ УЧЁТ") }
            EditorialAction { anchors.right: parent.right; text: qsTr("НАВЕРХ ↑"); arrow: false; onClicked: view.contentY = 0 }
            Mono { y: view.narrow ? 80 : 40; width: parent.width; height: 32; visible: !!view.host.csvStatus; text: view.host.csvStatus; color: view.host.csvStatusOk ? "#2e5947" : "#b84932" }
        }
    }
}
