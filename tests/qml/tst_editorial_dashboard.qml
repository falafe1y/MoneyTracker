import QtQuick
import QtQuick.Controls
import QtTest
import "../../ui/computer/dashboard"
import "../../ui/computer/dashboard/DashboardData.js" as Data
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
    property var deletedRows: []
    property bool deleteSucceeds: true
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
    function modalDialogVisible() { return false; }
    function editorialDeleteHistory(selection) {
        deletedRows = selection.selectedRows.slice();
        if (!deleteSucceeds) return;
        const ids = deletedRows.map(function(row) { return row.id; });
        financeMock.transactions = financeMock.transactions.filter(function(row) { return ids.indexOf(row.id) < 0; });
        selection.clear();
    }
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
            scene.deletedRows = []; scene.deleteSucceeds = true;
            dashboard.visible = false; dashboard.visible = true;
            wait(30);
        }
        function test_reference_geometry() {
            const paper = findChild(dashboard, "editorialPaper");
            const head = findChild(dashboard, "editorialHistoryHead");
            const accounts = findChild(dashboard, "editorialAccountsScroll");
            compare(sidebar.width, 240);
            compare(paper.x, 48); compare(paper.width, 1584);
            compare(head.y + paper.y, 896);
            compare(accounts.height, 168);
            compare(accounts.y, 64);
            compare(findChild(dashboard, "editorialCapitalChart").height, 168);
            compare(findChild(dashboard, "editorialExpenseDonut").width, 168);
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
                const chart = findChild(dashboard, "editorialCapitalChart");
                const donut = findChild(dashboard, "editorialExpenseDonut");
                compare(chart.height, dashboard.compact ? 152 : 168);
                compare(donut.width, dashboard.compact ? 152 : 168);
                verify(donut.width + (dashboard.small ? 16 : 24) < donut.parent.width);
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
        function test_account_counts_include_zero_balances_and_update() {
            const original = financeMock.allAccounts;
            try {
                compare(dashboard.caption("fiat"), "12 счетов");
                financeMock.allAccounts = original.concat([{id:"i1",asset:"investment",balanceMinor:0}]);
                financeMock.cryptoWallets = [{id:"c1",asset:"crypto",balanceMinor:0},{id:"c2",asset:"crypto",balanceMinor:0}];
                compare(dashboard.caption("all"), "15 счетов");
                compare(dashboard.caption("investment"), "1 счёт");
                compare(dashboard.caption("crypto"), "2 счёта");
                dashboard.selectAsset("investment");
                compare(dashboard.caption("all"), "15 счетов");
                financeMock.allAccounts = financeMock.allAccounts.concat([{id:"c1",asset:"crypto",balanceMinor:0}]);
                compare(dashboard.caption("crypto"), "2 счёта");
                compare(dashboard.caption("all"), "15 счетов");
            } finally {
                financeMock.allAccounts = original; financeMock.cryptoWallets = [];
            }
        }
        function test_capital_scale_edge_cases() {
            const series = [[], [{totalMinor:0}], [{totalMinor:-100000}],
                [{totalMinor:100},{totalMinor:100}], [{totalMinor:-100000},{totalMinor:200000}],
                [{totalMinor:1e12},{totalMinor:2e12}]];
            series.forEach(function(rows) {
                const scale = Data.capitalScale(rows);
                compare(scale.ticks.length, 6);
                verify(scale.maximum > scale.minimum);
                verify(scale.ticks.every(function(value, i) { return isFinite(value) && (i === 0 || value < scale.ticks[i - 1]); }));
                rows.forEach(function(row) { verify(row.totalMinor > scale.minimum && row.totalMinor < scale.maximum); });
            });
            const scale = Data.capitalScale(financeMock.capitalHistory);
            const labels = scale.ticks.map(function(value) { return Data.capitalTickLabel(value, "ru"); });
            compare(new Set(labels).size, 6);
        }
        function test_navigation_and_actions() {
            mouseClick(findChild(sidebar, "editorialNav_categories")); compare(scene.page, "categories");
            mouseClick(findChild(sidebar, "editorialNav_settings")); compare(scene.page, "settings");
            mouseClick(findChild(sidebar, "editorialNav_overview")); compare(scene.page, "overview");
            mouseClick(findChild(dashboard, "editorialAddOperation")); compare(scene.addCalls, 1);
            const rows = findChildren(dashboard, "editorialHistoryRow");
            verify(rows.length > 0); mouseClick(rows[0]); compare(scene.editCalls, 0);
            compare(dashboard.selectedRows.length, 1);
        }
        function historyItems() {
            return findChildren(dashboard, "editorialHistoryRow").filter(function(row) {
                const point = row.mapToItem(dashboard, 0, 0);
                return point.y >= 0 && point.y + row.height <= dashboard.height;
            });
        }
        function test_history_multiple_selection_and_escape() {
            const rows = historyItems(); verify(rows.length >= 2);
            mouseClick(rows[0]); mouseClick(rows[1]);
            compare(dashboard.selectedRows.length, 2); compare(scene.editCalls, 0);
            verify(rows[0].selected); verify(rows[1].selected);
            compare(rows[0].background.border.width, 0);
            compare(rows[0].background.color, Qt.rgba(3/255,21/255,40/255,9/255));
            mouseClick(rows[0]); compare(dashboard.selectedRows.length, 1);
            keyClick(Qt.Key_Escape); compare(dashboard.selectedRows.length, 0);
        }
        function test_history_selection_outside_click_does_not_eat_action() {

            mouseClick(historyItems()[0]); compare(dashboard.selectedRows.length, 1);
            mouseClick(dashboard, 8, 100); compare(dashboard.selectedRows.length, 0);
            mouseClick(historyItems()[0]);
            mouseClick(findChild(sidebar, "editorialNav_categories"));
            compare(scene.page, "categories"); compare(dashboard.selectedRows.length, 0);
            scene.page = "overview";
            mouseClick(historyItems()[0]);
            mouseClick(findChild(dashboard, "editorialAccountRow"));
            compare(financeMock.selectedAccountId, "a0"); compare(dashboard.selectedRows.length, 0);
        }
        function test_history_selection_menu_and_select_all_filtered_rows() {
            const original = financeMock.transactions;
            try {
                scene.searchText = "Пятёрочка";
                const rows = historyItems(); verify(rows.length >= 2);
                mouseClick(rows[0]);
                mouseClick(rows[1], 40, 20, Qt.RightButton);
                const menu = findChild(dashboard, "editorialHistorySelectionMenu");
                tryCompare(menu, "visible", true); compare(scene.menuCalls, 0);
                mouseClick(findChild(dashboard, "editorialHistorySelectAll"));
                compare(dashboard.selectedRows.length, dashboard.historyRows.length);
                verify(dashboard.selectedRows.length > findChildren(dashboard, "editorialHistoryRow").length);
                mouseClick(historyItems()[0], 40, 20, Qt.RightButton);
                tryCompare(menu, "visible", true);
                mouseClick(findChild(dashboard, "editorialHistoryDeleteSelected"));
                verify(scene.deletedRows.length > 0);
                verify(scene.deletedRows.every(function(row) { return row.rawDescription.indexOf("Пятёрочка") >= 0; }));
                compare(financeMock.transactions.length, original.length - scene.deletedRows.length);
                compare(dashboard.selectedRows.length, 0);
            } finally { financeMock.transactions = original; }
        }
        function test_history_selection_survives_scroll_sort_and_failed_delete() {
            mouseClick(historyItems()[0]);
            const selectedId = dashboard.selectedRows[0].id;
            dashboard.contentY = 3000; wait(30);
            compare(dashboard.selectedRows[0].id, selectedId);
            mouseClick(historyItems()[0]); compare(dashboard.selectedRows.length, 2);
            dashboard.sortBy("amount"); compare(dashboard.selectedRows.length, 2);
            scene.deleteSucceeds = false;
            mouseClick(historyItems()[0], 40, 20, Qt.RightButton);
            mouseClick(findChild(dashboard, "editorialHistoryDeleteSelected"));
            compare(scene.deletedRows.length, 2); compare(dashboard.selectedRows.length, 2);
            scene.searchText = "несуществующая операция"; compare(dashboard.selectedRows.length, 0);
        }
        function test_history_single_row_context_without_selection() {
            mouseClick(historyItems()[0], 40, 20, Qt.RightButton);
            compare(scene.menuCalls, 1); compare(dashboard.selectedRows.length, 0);
        }
        function test_history_escape_and_outside_click_with_open_menu() {
            const menu = findChild(dashboard, "editorialHistorySelectionMenu");
            mouseClick(historyItems()[0]);
            mouseClick(historyItems()[0], 40, 20, Qt.RightButton);
            tryCompare(menu, "visible", true);
            keyClick(Qt.Key_Escape);
            tryCompare(menu, "visible", false); compare(dashboard.selectedRows.length, 0);
            mouseClick(historyItems()[0]);
            mouseClick(historyItems()[0], 40, 20, Qt.RightButton);
            tryCompare(menu, "visible", true);
            mouseClick(dashboard, 8, 100);
            tryCompare(menu, "visible", false); compare(dashboard.selectedRows.length, 0);
        }
        function test_history_wheel_while_selected() {
            mouseClick(historyItems()[0]);
            const point = historyItems()[0].mapToItem(scroller, 40, 20);
            compare(scroller.targetAt(point.x, point.y, null), dashboard);
            mouseWheel(dashboard, 400, 1000, 0, -120);
            tryVerify(function() { return dashboard.contentY > 0; });
            compare(dashboard.selectedRows.length, 1);
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
