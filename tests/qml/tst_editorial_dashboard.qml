import QtQuick
import QtQuick.Controls
import QtTest
import "../../ui/computer/dashboard"
import "../../ui/computer/common"

Item {
    id: scene
    width: 1920; height: 1080
    property string page: "overview"
    property string searchText: ""
    property string csvStatus: ""
    property bool csvStatusOk: true
    property int addCalls: 0
    property int editCalls: 0
    property int menuCalls: 0
    property var lastAccount: null
    Rectangle { anchors.fill: parent; color: "#f3eddf" }
    function money(value, currency, sign) { return (sign && value > 0 ? "+" : "") + Number(value / 100).toLocaleString(Qt.locale("ru_RU"), "f", 2) + " ₽"; }
    function assetAmount(code) { return code === "fiat" ? 5245554 : code === "investment" ? 21300063 : 0; }
    function assetTitle(code) { return code === "fiat" ? "Фиат" : code === "crypto" ? "Крипта" : "Инвестиции"; }
    function accountCompactAmount(row) { return money(row.balanceMinor, row.currency, false); }
    function accountTypeLabel(type) { return type === "cash" ? "Наличные деньги" : "Банковская карта"; }
    function transactionSignedAmount(row) { return row.type === "income" ? Number(row.amount) : -Number(row.amount); }
    function transactionTypeLabel(row) { return row.type === "income" ? "Доход" : row.type === "expense" ? "Расход" : "Перевод"; }
    function transactionSummary(row) { return row.rawDescription || row.description || "—"; }
    function investmentOperationRow(row) { return row; }
    function capitalAxisMoney(value, code) { return Math.round(value / 100000) + "K"; }
    function dateFilterLabel() { return "Всё время"; }
    function editorialAddAccount() { ++addCalls; }
    function editorialSchedule(row) { ++addCalls; }
    function editorialDateFilter() { ++addCalls; }
    function editorialExport() { ++addCalls; }
    function editorialAddPosition() { ++addCalls; }
    function editorialEditOperation(row, source, x, y) { ++editCalls; }
    function openNewOperationForSelectedAsset() { ++addCalls; }
    function openAccountContextMenu(row, source, x, y) { ++menuCalls; }
    function openTransactionContextMenu(row, source, x, y) { ++menuCalls; }

    QtObject {
        id: financeMock
        property string selectedAsset: "fiat"
        property string selectedAccountId: ""
        property string selectedCryptoWalletId: ""
        property string appCurrency: "RUB"
        property string uiLanguage: "ru"
        property var currentCurrencyRates: [{code:"RUB",rate:1},{code:"USD",rate:100},{code:"EUR",rate:120},{code:"CNY",rate:14}]
        property real balanceMinorUnits: 26545617
        property var allAccounts: Array.from({length: 12}, function(_, i) { return {id: "a" + i, name: ["Альфа | На каждый день", "Альфа | Грядущие траты", "Наличные"][i % 3] + (i > 2 ? " " + i : ""), type: i === 2 ? "cash" : "debit_card", asset: "fiat", currency: "RUB", balanceMinor: i === 0 ? 1645554 : i === 1 ? 3000000 : i === 2 ? 600000 : 0}; })
        property var accounts: selectedAsset === "fiat" ? allAccounts : []
        property var cryptoWallets: []
        property var transactions: Array.from({length: 200}, function(_, i) { return {id: "t" + i, accountId: "a" + (i % 3), categoryName: ["Квартира", "Продукты", "Кафе"][i % 3], amount: (i + 1) * 10000, displayAmount: (i + 1) * 10000, currency: "RUB", type: i % 7 === 0 ? "income" : "expense", rawDescription: i % 2 ? "Пятёрочка · продукты" : "Оплата проекта", date: new Date(2026, 9, 16 - Math.floor(i / 3), 12)}; })
        property var cryptoTransactions: []
        property var investmentPositions: []
        property var investmentOperations: []
        property var investmentAccounts: []
        property var capitalHistory: [{date:"2026-10-15",totalMinor:24700000},{date:"2026-10-16",totalMinor:26600000},{date:"2026-10-17",totalMinor:26545617}]
        property var scheduledTransactions: [{id:"s1",name:"Хата",accountId:"a0",type:"expense",amount:2800000,currency:"RUB",nextDate:"2026-11-16"}]
        property bool dateFilterActive: false
        property string dateFilterFrom: ""
        property string dateFilterTo: ""
        property string cryptoLastError: ""
        function clearDateFilter() { dateFilterActive = false; }
        function setDateFilter(from, to) { dateFilterActive = true; }
        function refreshCryptoWallets() {}
    }
    QtObject {
        id: input
        property var window: null
        property bool active: false
        signal middlePressed(point position)
        signal pointerMoved(point position)
        signal cancelled()
    }
    AutoScrollController { id: scroller; parent: scene; input: input }
    EditorialSidebar {
        id: sidebar
        host: scene; height: scene.height
        width: scene.width <= 850 ? 72 : scene.width <= 1150 ? 184 : scene.width <= 1500 ? 208 : 240
    }
    EditorialDashboard {
        id: dashboard
        host: scene; controller: financeMock
        x: sidebar.width; width: scene.width - sidebar.width; height: scene.height
    }

    TestCase {
        name: "EditorialDashboard"
        when: windowShown
        function findChildren(item, name) {
            let result = [];
            if (item.objectName === name) result.push(item);
            for (let i = 0; i < item.children.length; ++i) result = result.concat(findChildren(item.children[i], name));
            return result;
        }
        function init() {
            scene.width = 1920; scene.height = 1080; scene.page = "overview"; scene.searchText = "";
            dashboard.selectAsset("all"); dashboard.contentY = 0;
            dashboard.sortField = "date"; dashboard.sortDirection = -1;
            scene.addCalls = 0; scene.editCalls = 0; scene.menuCalls = 0;
            wait(30);
        }
        function test_reference_geometry() {
            const paper = findChild(dashboard, "editorialPaper");
            const head = findChild(dashboard, "editorialHistoryHead");
            const accounts = findChild(dashboard, "editorialAccountsScroll");
            compare(sidebar.width, 240);
            compare(paper.x, 48); compare(paper.width, 1584);
            compare(head.y + paper.y, 928);
            compare(accounts.height, 168);
            verify(accounts.contentHeight > accounts.height);
            verify(dashboard.contentHeight > dashboard.height);
            compare(findChildren(dashboard, "editorialHistoryRow").length < 16, true);
        }
        function test_desktop_sizes() {
            const widths = [1920, 1600, 1440, 1280, 1024, 800];
            const heights = [1080, 960, 900, 800, 768, 600];
            for (let i = 0; i < widths.length; ++i) {
                scene.width = widths[i]; scene.height = heights[i]; wait(20);
                compare(dashboard.contentWidth, dashboard.width);
                verify(dashboard.mainWidth > 0);
                const filters = findChild(dashboard, "editorialFilters");
                verify(filters.x >= 0);
                verify(filters.x + filters.width <= dashboard.mainWidth + 1);
                verify(findChild(dashboard, "editorialAccountsScroll").height > 0);
                if (typeof uiCaptureDirectory !== "undefined" && uiCaptureDirectory !== "") {
                    wait(100);
                    grabImage(scene).save(uiCaptureDirectory + "/dashboard-" + widths[i] + "x" + heights[i] + ".png");
                }
            }
        }
        function test_scroll_independence() {
            const accounts = findChild(dashboard, "editorialAccountsScroll");
            const sidebarY = sidebar.y;
            accounts.contentY = 224;
            compare(dashboard.contentY, 0);
            const accountOffset = accounts.contentY;
            dashboard.contentY = 700;
            compare(accounts.contentY, accountOffset);
            compare(sidebar.y, sidebarY);
            verify(findChild(dashboard, "editorialPaper").mapToItem(scene, 0, 0).y < 0);
            dashboard.contentY = dashboard.contentHeight - dashboard.height;
            wait(30);
            verify(findChildren(dashboard, "editorialHistoryRow").length < 30);
        }
        function test_wheel_targets_nested_list_and_document() {
            const accounts = findChild(dashboard, "editorialAccountsScroll");
            accounts.contentY = 0;
            const point = accounts.mapToItem(scroller, 100, 40);
            compare(scroller.targetAt(point.x, point.y, null), accounts);
            mouseWheel(accounts, 100, 40, 0, -120);
            tryVerify(function() { return accounts.contentY > 0; });
            compare(dashboard.contentY, 0);
            accounts.contentY = accounts.contentHeight - accounts.height;
            mouseWheel(accounts, 100, 40, 0, -120);
            compare(dashboard.contentY, 0);
            mouseWheel(dashboard, dashboard.width - 40, 700, 0, -120);
            tryVerify(function() { return dashboard.contentY > 0; });
            compare(sidebar.y, 0);
        }
        function test_assets_accounts_search_sort() {
            dashboard.selectAsset("fiat");
            compare(financeMock.selectedAsset, "fiat");
            dashboard.selectAccount(financeMock.allAccounts[1]);
            compare(financeMock.selectedAccountId, "a1");
            verify(dashboard.historyRows.every(function(r) { return r.source.accountId === "a1"; }));
            dashboard.selectAccount(financeMock.allAccounts[1]); compare(financeMock.selectedAccountId, "");
            scene.searchText = "несуществующая операция"; compare(dashboard.historyRows.length, 0);
            scene.searchText = "Пятёрочка"; verify(dashboard.historyRows.length > 0);
            verify(dashboard.historyRows.every(function(r) { return r.description.indexOf("Пятёрочка") >= 0; }));
            scene.searchText = ""; dashboard.sortBy("amount");
            verify(dashboard.historyRows[0].signed >= dashboard.historyRows[1].signed);
            dashboard.selectAsset("crypto"); compare(financeMock.selectedAsset, "crypto"); compare(dashboard.historyRows.length, 0);
            dashboard.selectAsset("investment"); compare(financeMock.selectedAsset, "investment");
            dashboard.selectAsset("all"); compare(financeMock.selectedAsset, "fiat"); verify(dashboard.historyRows.length > 0);
        }
        function test_navigation_and_actions() {
            mouseClick(findChild(sidebar, "editorialNav_categories")); compare(scene.page, "categories");
            mouseClick(findChild(sidebar, "editorialNav_settings")); compare(scene.page, "settings");
            mouseClick(findChild(sidebar, "editorialNav_overview")); compare(scene.page, "overview");
            mouseClick(findChild(dashboard, "editorialAddOperation")); compare(scene.addCalls, 1);
            const rows = findChildren(dashboard, "editorialHistoryRow");
            verify(rows.length > 0); mouseClick(rows[0]); compare(scene.editCalls, 1);
        }
        function test_account_click_and_context_menu() {
            const rows = findChildren(dashboard, "editorialAccountRow");
            verify(rows.length > 0);
            mouseClick(rows[0]); compare(financeMock.selectedAccountId, "a0");
            mouseClick(rows[0]); compare(financeMock.selectedAccountId, "");
            mouseClick(rows[0], 80, 20, Qt.RightButton); compare(scene.menuCalls, 1);
        }
    }
}
