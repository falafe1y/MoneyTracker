#include "../services/CryptoCostBasis.h"
#include <QtTest>

namespace {
constexpr qint64 kDay = 86400000LL;
const qint64 kBuyDay = QDateTime(QDate(2025, 10, 16), QTime(12, 0), QTimeZone::UTC).toMSecsSinceEpoch();
const QDate kBuyDate = QDateTime::fromMSecsSinceEpoch(kBuyDay).date();
constexpr qint64 kRateMicros = 90'000'000; // 90 roubles per dollar

// Rows of one trade as BybitProvider::parseOperation stores them: the coin leg and the dollar leg
QVariantMap tradeRow(const QString& currency, const QString& pair, const QString& side, const QString& price,
                     const QString& qty, const QString& change, qint64 at)
{
    return {{"symbol", currency}, {"pair", pair}, {"category", "spot"}, {"tradeSide", side}, {"tradePrice", price},
            {"tradeQty", qty}, {"changeText", change}, {"occurredAtMs", at}};
}

QMap<QDate, qint64> rates()
{
    return {{kBuyDate, kRateMicros}};
}
} // namespace

class CryptoCostBasisTest : public QObject {
    Q_OBJECT
private slots:
    void boughtCoinIsWorthItsDollarPriceAtTodayRate()
    {
        // Bought 1 BTC for 50 000 USDT, worth 60 000 USDT now: both at 90 roubles per dollar
        const QVariantList history{tradeRow("BTC", "BTCUSDT", "Buy", "50000", "1", "1", kBuyDay),
                                   tradeRow("USDT", "BTCUSDT", "Buy", "50000", "1", "-50000", kBuyDay)};
        const CoinReturn result = coinReturnOf("BTC", 1.0, 60000.0, kRateMicros, history, rates(), kBuyDay + 365 * kDay);
        QCOMPARE(result.valueMinor, qint64(540'000'000)); // 5 400 000 ₽ in kopecks
        QVERIFY(result.annual.percent.has_value());
        QVERIFY(qAbs(*result.annual.percent - 20.0) < 0.1);
    }

    void dollarLegOfTheSameTradeIsNotCountedAsTheCoin()
    {
        const QVariantList history{tradeRow("USDT", "BTCUSDT", "Buy", "50000", "1", "-50000", kBuyDay)};
        const CoinReturn result = coinReturnOf("BTC", 1.0, 60000.0, kRateMicros, history, rates(), kBuyDay + 365 * kDay);
        QVERIFY(!result.annual.percent.has_value());
        QVERIFY(result.annual.reason.contains(QStringLiteral("нет сделок")));
    }

    void coinsNotBoughtInTradesAreNotMeasured()
    {
        // Holds 2 BTC, but the trades explain only 1: the other coin came by deposit
        const QVariantList history{tradeRow("BTC", "BTCUSDT", "Buy", "50000", "1", "1", kBuyDay),
                                   tradeRow("USDT", "BTCUSDT", "Buy", "50000", "1", "-50000", kBuyDay)};
        const CoinReturn result = coinReturnOf("BTC", 2.0, 60000.0, kRateMicros, history, rates(), kBuyDay + 365 * kDay);
        QVERIFY(!result.annual.percent.has_value());
        QVERIFY(result.annual.reason.contains(QStringLiteral("не сделками")));
    }

    void tradeWithoutCbrRateWaitsForIt()
    {
        const QVariantList history{tradeRow("BTC", "BTCUSDT", "Buy", "50000", "1", "1", kBuyDay),
                                   tradeRow("USDT", "BTCUSDT", "Buy", "50000", "1", "-50000", kBuyDay)};
        const CoinReturn result = coinReturnOf("BTC", 1.0, 60000.0, kRateMicros, history, {}, kBuyDay + 365 * kDay);
        QVERIFY(!result.annual.percent.has_value());
        QCOMPARE(result.firstMissingRateDate, kBuyDate);
        QVERIFY(result.annual.reason.contains(QStringLiteral("курса ЦБ")));
    }

    void tradeAgainstNonDollarCoinIsNotMeasured()
    {
        const QVariantList history{tradeRow("ETH", "ETHBTC", "Buy", "0.05", "1", "1", kBuyDay)};
        const CoinReturn result = coinReturnOf("ETH", 1.0, 3000.0, kRateMicros, history, rates(), kBuyDay + 365 * kDay);
        QVERIFY(!result.annual.percent.has_value());
        QVERIFY(result.annual.reason.contains(QStringLiteral("не за доллары")));
    }

    void coinWithoutPriceHasNoValue()
    {
        const CoinReturn result = coinReturnOf("BTC", 1.0, 0.0, kRateMicros, {}, rates(), kBuyDay);
        QCOMPARE(result.valueMinor, qint64(0));
        QVERIFY(result.annual.reason.contains(QStringLiteral("нет цены")));
    }

    void derivativesAreNotSpotTrades()
    {
        // An inverse BTCUSD contract settles in BTC: its rows must not be taken for purchases of the coin
        QVariantMap contract = tradeRow("BTC", "BTCUSD", "Buy", "50000", "1", "0.0001", kBuyDay);
        contract.insert("category", "inverse");
        const CoinReturn result = coinReturnOf("BTC", 1.0, 60000.0, kRateMicros, {contract}, rates(), kBuyDay + 365 * kDay);
        QVERIFY(!result.annual.percent.has_value());
        QVERIFY(result.annual.reason.contains(QStringLiteral("нет сделок")));
    }

    void stableCoinsAreRecognised()
    {
        QVERIFY(isStableCoin(QStringLiteral("USDT")));
        QVERIFY(isStableCoin(QStringLiteral("usdc")));
        QVERIFY(!isStableCoin(QStringLiteral("BTC")));
    }

    void officialRateOfTheDayWinsOverEstimates()
    {
        const QVariantList rows{
            QVariantMap{{"currency", "USD"}, {"rate_date", "2025-10-16"}, {"source", "manual"}, {"rub_per_unit_micros", 100'000'000}},
            QVariantMap{{"currency", "USD"}, {"rate_date", "2025-10-16"}, {"source", "CBR"}, {"rub_per_unit_micros", 90'000'000}},
            QVariantMap{{"currency", "USD"}, {"rate_date", "2025-10-16"}, {"source", "estimated"}, {"rub_per_unit_micros", 95'000'000}}};
        const auto byDate = rubPerUsdByDate(rows);
        QCOMPARE(byDate.value(kBuyDate), qint64(90'000'000));
    }
};

QTEST_GUILESS_MAIN(CryptoCostBasisTest)
#include "CryptoCostBasisTest.moc"
