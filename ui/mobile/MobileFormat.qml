pragma Singleton
import QtQuick

QtObject {
    function locale() {
        return Qt.locale(financeController.uiLanguage === "en" ? "en_US" : "ru_RU");
    }

    function symbol(code) {
        return code === "USD" ? "$" : code === "EUR" ? "€" : code === "CNY" ? "¥" : "₽";
    }

    function money(minor, code, sign) {
        const roundedMinor = Math.round(Number(minor));
        const value = roundedMinor / 100;
        const decimals = Math.abs(roundedMinor) % 100 === 0 ? 0 : 2;
        const prefix = value < 0 ? "−" : (sign && value > 0 ? "+" : "");
        return prefix + Math.abs(value).toLocaleString(locale(), "f", decimals)
             + " " + symbol(code || financeController.appCurrency);
    }

    function transactionText(row) {
        if (row.type === "transfer")
            return money(row.displayAmount, financeController.appCurrency, false);
        return money(row.type === "income" ? row.displayAmount : -row.displayAmount,
                     financeController.appCurrency, true);
    }

    function amountText(minor) {
        const value = Math.round(Number(minor)) / 100;
        return Number.isInteger(value) ? String(value) : value.toFixed(2).replace(".", ",");
    }

    function minorFromText(text) {
        const value = Number(String(text).replace(",", "."));
        return Number.isFinite(value) && value > 0 ? Math.round(value * 100) : 0;
    }

    function compactValue(minor) {
        const value = Number(minor) / 100;
        const abs = Math.abs(value);
        if (abs >= 1000000)
            return String(Math.round(value / 100000) / 10).replace(".", ",") + " млн";
        if (abs >= 10000)
            return String(Math.round(value / 1000)) + " тыс";
        if (abs >= 1000)
            return String(Math.round(value / 100) / 10).replace(".", ",") + " тыс";
        return String(Math.round(value));
    }

    function numberFromText(text) {
        const value = Number(String(text).replace(",", "."));
        return Number.isFinite(value) ? value : 0;
    }

    function isoFromDate(date) {
        const month = date.getMonth() + 1;
        const day = date.getDate();
        return date.getFullYear() + "-" + (month < 10 ? "0" : "") + month + "-" + (day < 10 ? "0" : "") + day;
    }

    function isoDate(value) {
        const parts = String(value).split("-");
        return parts.length === 3 ? parts[2] + "." + parts[1] + "." + parts[0] : "";
    }

    function dateFromInput(text) {
        const parts = String(text).trim().split(".");
        if (parts.length !== 3)
            return null;
        const day = Number(parts[0]);
        const month = Number(parts[1]);
        const year = Number(parts[2]);
        const date = new Date(year, month - 1, day);
        return date.getFullYear() === year && date.getMonth() === month - 1 && date.getDate() === day
             ? date : null;
    }

    function monthLabel(key) {
        const parts = String(key).split("-");
        const date = parts.length >= 2 ? new Date(Number(parts[0]), Number(parts[1]) - 1, 1) : new Date();
        const name = locale().standaloneMonthName(date.getMonth());
        return name.charAt(0).toUpperCase() + name.slice(1) + " " + date.getFullYear();
    }
}
