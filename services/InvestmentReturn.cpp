#include "InvestmentReturn.h"

#include <cmath>

namespace {
constexpr double kDayMillis = 86400000.0;
constexpr double kYearMillis = 365.25 * kDayMillis;
constexpr double kMinimumDays = 30.0;

QVariantMap groupSummary(const QVariantList& items, const QStringList& excludedIds, const QString& key,
                         const QString& title, const QString& group)
{
    double weighted = 0;
    double weights = 0;
    qint64 total = 0;
    int counted = 0;
    int shown = 0;
    QVariantList slices;
    for (const QVariant& value : items) {
        const QVariantMap item = value.toMap();
        if (!group.isEmpty() && item.value(QStringLiteral("group")).toString() != group)
            continue;
        if (excludedIds.contains(item.value(QStringLiteral("id")).toString()))
            continue;
        ++shown;
        const qint64 amount = item.value(QStringLiteral("valueMinor")).toLongLong();
        if (amount > 0) {
            total += amount;
            slices.append(QVariantMap{{"id", item.value(QStringLiteral("id"))},
                                      {"title", item.value(QStringLiteral("title"))},
                                      {"valueMinor", amount}});
        }
        const QVariant rate = item.value(QStringLiteral("annualPercent"));
        if (amount > 0 && !rate.isNull()) {
            weighted += double(amount) * rate.toDouble();
            weights += double(amount);
            ++counted;
        }
    }
    for (QVariant& slice : slices) {
        QVariantMap map = slice.toMap();
        map.insert(QStringLiteral("share"), double(map.value(QStringLiteral("valueMinor")).toLongLong()) / double(total));
        slice = map;
    }
    return QVariantMap{{"key", key},
                       {"title", title},
                       {"annualPercent", weights > 0 ? QVariant(weighted / weights) : QVariant()},
                       {"valueMinor", total},
                       {"counted", counted},
                       {"shown", shown},
                       {"slices", slices}};
}
} // namespace

AnnualReturn annualReturnOf(const QVector<ReturnFlow>& flows, qint64 nowMillis, qint64 valueNowMinor)
{
    AnnualReturn result;
    if (flows.isEmpty()) {
        result.reason = QStringLiteral("нет вложений в истории");
        return result;
    }
    qint64 start = nowMillis;
    bool invested = false;
    bool paidOut = false;
    for (const ReturnFlow& flow : flows) {
        start = qMin(start, flow.atMillis);
        invested = invested || flow.minor < 0;
        paidOut = paidOut || flow.minor > 0;
    }
    result.days = (nowMillis - start) / kDayMillis;
    if (!invested) {
        result.reason = QStringLiteral("нет вложений в истории");
        return result;
    }
    if (result.days < kMinimumDays) {
        result.reason = QStringLiteral("мало времени (меньше 30 дней)");
        return result;
    }
    if (valueNowMinor <= 0 && !paidOut) {
        result.percent = -100.0; // the whole money is lost
        return result;
    }
    // Every flow and the value now, each at its moment in years from the start
    struct Cash {
        double years;
        double amount;
    };
    QVector<Cash> cash;
    cash.reserve(flows.size() + 1);
    for (const ReturnFlow& flow : flows)
        cash.append({(flow.atMillis - start) / kYearMillis, double(flow.minor)});
    cash.append({result.days * kDayMillis / kYearMillis, double(valueNowMinor)});
    const auto npv = [&cash](double rate) {
        double sum = 0;
        for (const Cash& item : cash)
            sum += item.amount / std::pow(1.0 + rate, item.years);
        return sum;
    };
    double low = -0.9999;
    double high = 100.0;
    double lowValue = npv(low);
    const double highValue = npv(high);
    if (!std::isfinite(lowValue) || !std::isfinite(highValue) || lowValue * highValue > 0) {
        result.reason = QStringLiteral("не удалось рассчитать");
        return result;
    }
    for (int i = 0; i < 400 && high - low > 1e-12; ++i) {
        const double middle = (low + high) / 2;
        const double value = npv(middle);
        if (!std::isfinite(value))
            break;
        if ((value > 0) == (lowValue > 0)) {
            low = middle;
            lowValue = value;
        } else {
            high = middle;
        }
    }
    result.percent = (low + high) / 2 * 100;
    return result;
}

QVariantMap averageReturnSummaryOf(const QVariantList& items, const QStringList& excludedIds, bool merged)
{
    QVariantList groups;
    if (merged) {
        groups.append(groupSummary(items, excludedIds, QStringLiteral("all"),
                                   QStringLiteral("Инвестиции и криптовалюта"), QString()));
    } else {
        groups.append(groupSummary(items, excludedIds, QStringLiteral("investments"),
                                   QStringLiteral("Инвестиции"), QStringLiteral("investments")));
        groups.append(groupSummary(items, excludedIds, QStringLiteral("crypto"),
                                   QStringLiteral("Криптовалюта"), QStringLiteral("crypto")));
    }
    return QVariantMap{{"merged", merged}, {"excluded", excludedIds}, {"groups", groups}};
}
