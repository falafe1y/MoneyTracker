pragma Singleton
import QtQuick

QtObject {
    readonly property int chartPriceLevels: 7

    function dateFromIso(value) {
        if (!value)
            return null;
        const parts = String(value).split("-");
        if (parts.length !== 3)
            return null;
        return new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]), 12, 0, 0, 0);
    }

    function capitalDateLabel(row) {
        const date = dateFromIso(row.date);
        return date ? Qt.formatDate(date, "dd.MM.yyyy") : "";
    }

    function historyResolutionLabel(rows) {
        if (rows.length === 0)
            return "";
        if (rows[0].resolution === "year")
            return qsTr("По годам");
        if (rows[0].resolution === "month")
            return qsTr("По месяцам");
        return qsTr("По дням");
    }

    function niceChartStep(range, targetIntervals) {
        if (!Number.isFinite(range) || range <= 0)
            return 1;
        const raw = range / Math.max(1, targetIntervals);
        const magnitude = Math.pow(10, Math.floor(Math.log(raw) / Math.LN10));
        const normalized = raw / magnitude;
        const factor = normalized <= 1 ? 1
                     : normalized <= 2 ? 2
                     : normalized <= 2.5 ? 2.5
                     : normalized <= 3 ? 3
                     : normalized <= 4 ? 4
                     : normalized <= 5 ? 5
                     : normalized <= 7.5 ? 7.5 : 10;
        return factor * magnitude;
    }

    function niceChartAxis(minimum, maximum) {
        let low = Number(minimum);
        let high = Number(maximum);
        const intervals = Math.max(1, chartPriceLevels - 1);
        if (!Number.isFinite(low) || !Number.isFinite(high))
            return { minimum: 0, maximum: intervals, step: 1, intervals: intervals };
        if (low > high) {
            const swap = low;
            low = high;
            high = swap;
        }
        let span = high - low;
        if (span === 0)
            span = Math.max(100, Math.abs(high) * 0.1);
        const padding = span * 0.04;
        let step = niceChartStep(span + padding * 2, intervals);
        let axisMinimum;
        while (true) {
            const lowerStart = Math.floor((low - padding) / step + 1e-9) * step;
            const upperStart = Math.ceil((high + padding) / step - 1e-9) * step
                             - intervals * step;
            axisMinimum = Math.max(lowerStart, upperStart);
            if (axisMinimum <= low + step * 1e-9)
                break;
            step = niceChartStep(step * (1 + 1e-9), 1);
        }
        return {
            minimum: axisMinimum,
            maximum: axisMinimum + intervals * step,
            step: step,
            intervals: intervals
        };
    }

    function capitalAxisMoney(minor, code) {
        const amount = Math.abs(Number(minor)) < 0.5 ? 0 : Number(minor) / 100;
        const absolute = Math.abs(amount);
        let divisor = 1;
        let suffix = "";
        const westernStyle = code !== "RUB";
        if (absolute >= 1000000000) {
            divisor = 1000000000;
            suffix = westernStyle ? "B" : qsTr("млрд");
        } else if (absolute >= 1000000) {
            divisor = 1000000;
            suffix = westernStyle ? "M" : qsTr("млн");
        } else if (absolute >= 1000) {
            divisor = 1000;
            suffix = westernStyle ? "K" : qsTr("тыс.");
        }
        if (westernStyle) {
            const scaledWestern = amount / divisor;
            const roundedWestern = Math.round(scaledWestern);
            const wholeWestern = Math.abs(scaledWestern - roundedWestern) < 0.000001;
            const westernDigits = wholeWestern || Math.abs(scaledWestern) >= 100 ? 0
                                : Math.abs(scaledWestern) >= 10 ? 1 : 2;
            const westernNumber = Math.abs(scaledWestern).toFixed(westernDigits)
                .replace(/\.0+$/, "").replace(/(\.\d*[1-9])0+$/, "$1");
            return (scaledWestern < 0 ? "−" : "") + MobileFormat.symbol(code) + westernNumber + suffix;
        }
        if (divisor === 1)
            return MobileFormat.money(Math.round(Number(minor)), code, false);
        const scaled = amount / divisor;
        const roundedScaled = Math.round(scaled);
        const whole = Math.abs(scaled - roundedScaled) < 0.000001;
        const digits = whole || Math.abs(scaled) >= 100 ? 0
                     : Math.abs(scaled) >= 10 ? 1 : 2;
        return scaled.toLocaleString(MobileFormat.locale(), "f", digits)
            + " " + suffix + " " + MobileFormat.symbol(code);
    }

    function expenseMonthLabel(month) {
        return [
            qsTr("Янв."), qsTr("Фев."), qsTr("Мар."), qsTr("Апр."),
            qsTr("Май"), qsTr("Июн."), qsTr("Июл."), qsTr("Авг."),
            qsTr("Сен."), qsTr("Окт."), qsTr("Ноя."), qsTr("Дек.")
        ][month];
    }

    function averageCompletedMonthExpense(rows) {
        let total = 0, count = 0;
        for (let i = 0; i < rows.length; ++i) {
            if (!rows[i].tracked || !rows[i].complete)
                continue;
            total += Number(rows[i].totalMinor);
            ++count;
        }
        if (count === 0) {
            for (let j = 0; j < rows.length; ++j) {
                if (!rows[j].tracked)
                    continue;
                total += Number(rows[j].totalMinor);
                ++count;
            }
        }
        return count > 0 ? Math.round(total / count) : 0;
    }

    function expenseRowsForYear(year) {
        const source = financeController.expenseHistoryByMonthRub;
        const byMonth = {};
        for (let i = 0; i < source.length; ++i) {
            const date = dateFromIso(source[i].date);
            if (date && date.getFullYear() === year)
                byMonth[date.getMonth()] = source[i];
        }
        const result = [];
        for (let month = 0; month < 12; ++month) {
            const row = byMonth[month];
            result.push({
                date: year + "-" + (month < 9 ? "0" : "") + (month + 1) + "-01",
                monthIndex: month,
                totalMinor: row ? Number(row.totalMinor) : 0,
                complete: row ? Boolean(row.complete) : false,
                tracked: Boolean(row)
            });
        }
        return result;
    }

    function expenseHistoryFirstYear() {
        const rows = financeController.expenseHistoryByMonthRub;
        if (rows.length === 0)
            return new Date().getFullYear();
        const date = dateFromIso(rows[0].date);
        return date ? date.getFullYear() : new Date().getFullYear();
    }

    function expenseHistoryLastYear() {
        const rows = financeController.expenseHistoryByMonthRub;
        if (rows.length === 0)
            return new Date().getFullYear();
        const date = dateFromIso(rows[rows.length - 1].date);
        return date ? Math.max(new Date().getFullYear(), date.getFullYear()) : new Date().getFullYear();
    }
}
