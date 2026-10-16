#include "CryptoCostBasis.h"

#include <QDateTime>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
// The CBR publishes business days only: a trade on a weekend or holiday uses the last rate before it.
bool rateOn(const QMap<QDate, qint64>& rates, QDate day, qint64& rubPerUsdMicros)
{
    const auto after = rates.upperBound(day);
    if (after == rates.cbegin())
        return false;
    rubPerUsdMicros = std::prev(after).value();
    return rubPerUsdMicros > 0;
}
} // namespace

bool isStableCoin(const QString& coin)
{
    static const QSet<QString> stable{QStringLiteral("USD"), QStringLiteral("USDT"), QStringLiteral("USDC"),
                                      QStringLiteral("FDUSD"), QStringLiteral("DAI"), QStringLiteral("TUSD"),
                                      QStringLiteral("BUSD"), QStringLiteral("USDE")};
    return stable.contains(coin.toUpper());
}

QMap<QDate, qint64> rubPerUsdByDate(const QVariantList& rows)
{
    QMap<QDate, qint64> rates;
    QMap<QDate, QString> sources;
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        const QDate day = QDate::fromString(row.value(QStringLiteral("rate_date")).toString(), Qt::ISODate);
        const qint64 micros = row.value(QStringLiteral("rub_per_unit_micros")).toLongLong();
        const QString source = row.value(QStringLiteral("source")).toString();
        if (!day.isValid() || micros <= 0)
            continue;
        if (sources.value(day) == QStringLiteral("CBR") && source != QStringLiteral("CBR"))
            continue;
        rates.insert(day, micros);
        sources.insert(day, source);
    }
    return rates;
}

CoinReturn coinReturnOf(const QString& coin, double quantity, double priceUsd, qint64 usdRubNowMicros,
                        const QVariantList& history, const QMap<QDate, qint64>& usdRubByDate, qint64 nowMillis)
{
    CoinReturn result;
    if (priceUsd <= 0 || usdRubNowMicros <= 0) {
        result.annual = AnnualReturn{std::nullopt, QObject::tr("нет цены монеты или курса доллара"), 0};
        return result;
    }
    // Roubles = quantity × dollar price × CBR rate; kopecks are 1/100 of a rouble, micros are 1/1 000 000.
    result.valueMinor = std::llround(quantity * priceUsd * static_cast<double>(usdRubNowMicros) / 1e4);

    QVector<ReturnFlow> flows;
    double netQuantity = 0;
    QString problem;
    for (const QVariant& value : history) {
        const QVariantMap row = value.toMap();
        // "symbol" is the currency of the row. A trade gives two rows: the coin leg (base) and the dollar leg (quote).
        const QString pair = row.value(QStringLiteral("pair")).toString();
        // Derivatives settle in a coin too (inverse contracts use BTC as margin): only spot trades are counted
        if (pair.isEmpty() || row.value(QStringLiteral("category")).toString() != QStringLiteral("spot")
            || row.value(QStringLiteral("symbol")).toString() != coin || !pair.startsWith(coin))
            continue;
        const QString quote = pair.mid(coin.size());
        if (!isStableCoin(quote)) {
            problem = QObject::tr("есть сделки не за доллары");
            continue;
        }
        const QString side = row.value(QStringLiteral("tradeSide")).toString();
        bool priceOk = false, qtyOk = false, changeOk = false;
        const double tradePrice = row.value(QStringLiteral("tradePrice")).toString().toDouble(&priceOk);
        const double tradeQty = row.value(QStringLiteral("tradeQty")).toString().toDouble(&qtyOk);
        const double change = row.value(QStringLiteral("changeText")).toString().toDouble(&changeOk);
        if ((side != QStringLiteral("Buy") && side != QStringLiteral("Sell")) || !priceOk || !qtyOk || !changeOk
            || tradePrice <= 0 || tradeQty <= 0) {
            problem = QObject::tr("в истории есть сделка без цены или количества");
            continue;
        }
        const qint64 occurredAt = row.value(QStringLiteral("occurredAtMs")).toLongLong();
        const QDate day = QDateTime::fromMSecsSinceEpoch(occurredAt).date();
        qint64 rubPerUsd = 0;
        if (!rateOn(usdRubByDate, day, rubPerUsd)) {
            if (!result.firstMissingRateDate.isValid() || day < result.firstMissingRateDate)
                result.firstMissingRateDate = day;
            continue;
        }
        const qint64 rubMinor = std::llround(tradeQty * tradePrice * static_cast<double>(rubPerUsd) / 1e4);
        flows.append({occurredAt, side == QStringLiteral("Buy") ? -rubMinor : rubMinor});
        netQuantity += change;
    }

    if (!problem.isEmpty()) {
        result.annual = AnnualReturn{std::nullopt, problem, 0};
        return result;
    }
    if (result.firstMissingRateDate.isValid()) {
        result.annual = AnnualReturn{std::nullopt, QObject::tr("нет курса ЦБ на дату сделки"), 0};
        return result;
    }
    if (flows.isEmpty()) {
        result.annual = AnnualReturn{std::nullopt, QObject::tr("нет сделок на бирже"), 0};
        return result;
    }
    // Every coin on the exchange must come from trades. Coins that arrived by deposit or left by withdrawal
    // have no known purchase price here, so the rate for this coin would be wrong.
    if (std::abs(quantity - netQuantity) > std::max(1e-7, 1e-6 * std::abs(quantity))) {
        result.annual = AnnualReturn{std::nullopt, QObject::tr("часть монет пришла не сделками: ввод, вывод или покупка вне биржи"), 0};
        return result;
    }
    result.annual = annualReturnOf(flows, nowMillis, result.valueMinor);
    return result;
}
