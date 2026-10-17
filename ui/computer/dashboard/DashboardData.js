.pragma library

function money(minor, currency, signed, decimals, language) {
    const symbols = {RUB: "₽", USD: "$", EUR: "€", CNY: "¥"};
    const amount = Number(minor);
    const prefix = amount < 0 ? "−" : signed && amount > 0 ? "+" : "";
    return prefix + (Math.abs(amount) / 100).toLocaleString(Qt.locale(language === "en" ? "en_US" : "ru_RU"), "f", decimals)
        + " " + (symbols[currency] || currency);
}

function convertMinor(minor, from, to, rates) {
    if (from === to) return Number(minor);
    let source = 0, target = 0;
    (rates || []).forEach(function(r) { if (r.code === from) source = Number(r.rate); if (r.code === to) target = Number(r.rate); });
    return source > 0 && target > 0 ? Math.round(Number(minor) * source / target) : null;
}

function dateValue(value) {
    const date = value instanceof Date ? value : new Date(value);
    return isNaN(date.getTime()) ? new Date(0) : date;
}

function shortDate(value) {
    const date = dateValue(value);
    return String(date.getDate()).padStart(2, "0") + "." + String(date.getMonth() + 1).padStart(2, "0");
}

function inPeriod(row, controller) {
    if (!controller.dateFilterActive) return true;
    const date = dateValue(row.date);
    const from = new Date(controller.dateFilterFrom + "T00:00:00");
    const to = new Date(controller.dateFilterTo + "T23:59:59.999");
    return date >= from && date <= to;
}

// Keep the source row intact: the existing editors and context menus consume it.
function history(controller, host, asset, selectedId) {
    let result = [];
    const accountNames = {};
    const accountAssets = {};
    controller.allAccounts.forEach(function(a) { accountNames[a.id] = a.name; accountAssets[a.id] = a.asset; });
    controller.cryptoWallets.forEach(function(a) { accountNames[a.id] = a.name; });
    if (asset !== "crypto") {
        result = controller.transactions.filter(function(row) {
            return (asset === "all" || accountAssets[row.accountId] === asset)
                && (!selectedId || row.accountId === selectedId);
        });
        if (asset === "all" || asset === "investment") {
            const positions = controller.investmentPositions.map(function(p) { return host.investmentOperationRow(p); });
            result = result.concat(positions, controller.investmentOperations).filter(function(row) {
                return (!selectedId || row.accountId === selectedId) && inPeriod(row, controller);
            });
        }
    }
    if (asset === "all" || asset === "crypto") {
        result = result.concat(controller.cryptoTransactions.filter(function(row) {
            return !selectedId || row.walletId === selectedId;
        }).map(function(row) {
            return Object.assign({}, row, {
                date: row.occurredAt, type: row.direction === "in" ? "income" : "expense",
                accountId: row.walletId, categoryName: row.symbol || "Криптовалюта",
                rawDescription: row.counterparty || row.description || row.transactionId,
                cryptoRow: true
            });
        }).filter(function(row) { return inPeriod(row, controller); }));
    }
    return result.map(function(row) {
        const rawSigned = row.cryptoRow ? (row.direction === "in" ? 1 : -1) * Number(row.amountText || 0) : host.transactionSignedAmount(row);
        const displayAmount = row.cryptoRow ? null : row.displayAmount !== undefined ? Number(row.displayAmount)
            : convertMinor(row.amount, row.currency, controller.appCurrency, controller.currentCurrencyRates);
        const converted = displayAmount !== null;
        const signed = converted ? (rawSigned < 0 ? -1 : 1) * displayAmount : rawSigned;
        return { source: row, date: dateValue(row.date), type: host.transactionTypeLabel(row),
            account: row.accountName || accountNames[row.accountId] || "—",
            category: row.categoryName || "—", description: host.transactionSummary(row),
            signed: signed, amount: row.cryptoRow
                ? (signed > 0 ? "+" : "−") + (row.amountText || "0") + " " + (row.symbol || "")
                : money(signed, converted ? controller.appCurrency : row.currency, true, 2, controller.uiLanguage) };
    });
}

function filterRows(rows, query, category, sortField, direction) {
    const q = query.trim().toLowerCase();
    const filtered = rows.filter(function(row) {
        return (!category || row.category === category)
            && (!q || [row.type, row.account, row.category, row.description, row.amount,
                shortDate(row.date), row.date.getFullYear()].join(" ").toLowerCase().indexOf(q) >= 0);
    });
    return filtered.sort(function(a, b) {
        let left = a[sortField], right = b[sortField];
        if (sortField === "date") return (a.date.getTime() - b.date.getTime()) * direction;
        if (sortField === "amount") return (a.signed - b.signed) * direction;
        return String(left).localeCompare(String(right)) * direction;
    });
}

function expenses(rows) {
    const sums = {};
    rows.forEach(function(row) {
        if (row.source.type !== "expense" || row.source.cryptoRow) return;
        sums[row.category] = (sums[row.category] || 0) + Math.abs(row.signed);
    });
    return Object.keys(sums).map(function(key) { return {name: key, amount: sums[key]}; })
        .sort(function(a, b) { return b.amount - a.amount; });
}

function legend(groups) {
    const result = groups.slice(0, 2);
    if (groups.length > 2)
        result.push({name: "Остальное", amount: groups.slice(2).reduce(function(sum, g) { return sum + g.amount; }, 0), other: true});
    return result;
}
