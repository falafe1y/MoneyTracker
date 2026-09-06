#include "../services/CurrencyRateCache.h"
#include "../services/CurrencyRateProvider.h"
#include "../services/CbrCurrencyRateProvider.h"
#include "../services/CurrencyConverter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{

constexpr qsizetype currencyIndex(const Currency currency)
{
    return static_cast<qsizetype>(currency);
}

QByteArray completeResponse(
    const QByteArray& date = QByteArrayLiteral("06/09/2026"),
    const QByteArray& usdValue = QByteArrayLiteral("100,0000"),
    const QByteArray& eurValue = QByteArrayLiteral("120,0000")
    )
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<ValCurs Date=\"") + date + QByteArrayLiteral("\">"
        "<Valute ID=\"R01235\">"
        "<NumCode>840</NumCode><CharCode>USD</CharCode>"
        "<Nominal>1</Nominal><Value>") + usdValue + QByteArrayLiteral(
        "</Value><VunitRate>100,0000</VunitRate>"
        "</Valute>"
        "<Valute ID=\"R01239\">"
        "<NumCode>978</NumCode><CharCode>EUR</CharCode>"
        "<Nominal>1</Nominal><Value>") + eurValue + QByteArrayLiteral(
        "</Value><VunitRate>120,0000</VunitRate>"
        "</Valute>"
        "</ValCurs>");
}

void compareRates(
    const CurrencyRateSnapshot& actual,
    const CurrencyRateSnapshot& expected
    )
{
    QCOMPARE(
        actual.ratesToUsd[currencyIndex(Currency::RUB)],
        expected.ratesToUsd[currencyIndex(Currency::RUB)]);
    QCOMPARE(
        actual.ratesToUsd[currencyIndex(Currency::USD)],
        expected.ratesToUsd[currencyIndex(Currency::USD)]);
    QCOMPARE(
        actual.ratesToUsd[currencyIndex(Currency::EUR)],
        expected.ratesToUsd[currencyIndex(Currency::EUR)]);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

} // namespace

class CurrencyRateCacheTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesCompleteCbrResponse();
    void loadsLegacyRateScaleWithoutRewritingCache();
    void rejectsResponseWithoutEverySupportedCurrency();
    void rejectsStaleResponse();
    void completeResponseReplacesCache();
    void invalidResponseDoesNotReplaceCache();
    void olderResponseDoesNotReplaceNewerCache();
    void manualRatesAreUsedWhenAutomaticUpdatesAreDisabled();
    void invalidManualRatesAreRejected();
};

void CurrencyRateCacheTest::loadsLegacyRateScaleWithoutRewritingCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString cachePath = directory.filePath(QStringLiteral("rates.json"));
    const QByteArray legacy = QByteArrayLiteral(
        "{\"formatVersion\":1,\"source\":\"cbr.ru\","
        "\"sourceDate\":\"2026-09-06\","
        "\"fetchedAtUtc\":\"2026-09-06T03:00:00.000Z\","
        "\"ratesToUsd\":{\"RUB\":10000,\"USD\":1000000,\"EUR\":1200000}}");
    QFile file(cachePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(legacy), legacy.size());
    file.close();

    CurrencyRateSnapshot snapshot;
    QVERIFY(CurrencyRateCache(cachePath).load(snapshot));
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::RUB)],
        qint64(10'000'000));
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::USD)],
        kCurrencyRateScale);
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::EUR)],
        qint64(1'200'000'000));
    QCOMPARE(readFile(cachePath), legacy);
}

void CurrencyRateCacheTest::parsesCompleteCbrResponse()
{
    CurrencyRateSnapshot snapshot;
    QString error;

    QVERIFY2(
        parseCbrCurrencyRates(
            completeResponse(),
            QDateTime::fromString(
                QStringLiteral("2026-09-06T03:00:00Z"), Qt::ISODate),
            snapshot,
            &error),
        qPrintable(error));

    QCOMPARE(snapshot.sourceDate, QDate(2026, 9, 6));
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::RUB)],
        qint64(10'000'000));
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::USD)],
        kCurrencyRateScale);
    QCOMPARE(
        snapshot.ratesToUsd[currencyIndex(Currency::EUR)],
        qint64(1'200'000'000));
}

void CurrencyRateCacheTest::rejectsResponseWithoutEverySupportedCurrency()
{
    const QByteArray response = QByteArrayLiteral(
        "<ValCurs Date=\"06.09.2026\">"
        "<Valute><CharCode>USD</CharCode><Nominal>1</Nominal>"
        "<Value>100,0000</Value></Valute>"
        "</ValCurs>");
    CurrencyRateSnapshot snapshot;

    QVERIFY(!parseCbrCurrencyRates(
        response,
        QDateTime::currentDateTimeUtc(),
        snapshot));
}

void CurrencyRateCacheTest::rejectsStaleResponse()
{
    CurrencyRateSnapshot snapshot;

    QVERIFY(!parseCbrCurrencyRates(
        completeResponse(QByteArrayLiteral("01.08.2026")),
        QDateTime::fromString(
            QStringLiteral("2026-09-06T03:00:00Z"), Qt::ISODate),
        snapshot));
}

void CurrencyRateCacheTest::completeResponseReplacesCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString cachePath =
        directory.filePath(QStringLiteral("rates.json"));
    const CurrencyRateCache cache(cachePath);
    CurrencyRateSnapshot original;
    original.sourceDate = QDate(2026, 9, 5);
    original.fetchedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-09-05T03:00:00Z"), Qt::ISODate);
    original.ratesToUsd = {11'111, kCurrencyRateScale, 1'222'222};
    QVERIFY(cache.replace(original));
    const QByteArray cacheBeforeUpdate = readFile(cachePath);
    QVERIFY(!cacheBeforeUpdate.isEmpty());

    const QDateTime fetchedAt = QDateTime::fromString(
        QStringLiteral("2026-09-06T15:00:00Z"), Qt::ISODate);
    CurrencyRateSnapshot updated;
    QString error;
    QVERIFY2(
        updateCurrencyRateCacheFromCbrResponse(
            cache,
            completeResponse(),
            fetchedAt,
            updated,
            &error),
        qPrintable(error));

    CurrencyRateSnapshot cached;
    QVERIFY(cache.load(cached));
    QCOMPARE(cached.sourceDate, QDate(2026, 9, 6));
    QCOMPARE(cached.fetchedAtUtc, fetchedAt);
    compareRates(cached, updated);
    QCOMPARE(
        cached.ratesToUsd[currencyIndex(Currency::RUB)],
        qint64(10'000'000));
    QCOMPARE(
        cached.ratesToUsd[currencyIndex(Currency::EUR)],
        qint64(1'200'000'000));
    QVERIFY(readFile(cachePath) != cacheBeforeUpdate);
}

void CurrencyRateCacheTest::invalidResponseDoesNotReplaceCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString cachePath =
        directory.filePath(QStringLiteral("rates.json"));
    const CurrencyRateCache cache(cachePath);
    CurrencyRateSnapshot original;
    original.sourceDate = QDate(2026, 9, 5);
    original.fetchedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-09-05T03:00:00Z"), Qt::ISODate);
    original.ratesToUsd = {11'111, kCurrencyRateScale, 1'222'222};
    QVERIFY(cache.replace(original));
    const QByteArray cacheBeforeFailure = readFile(cachePath);
    QVERIFY(!cacheBeforeFailure.isEmpty());

    CurrencyRateSnapshot updated;
    QVERIFY(!updateCurrencyRateCacheFromCbrResponse(
        cache,
        QByteArrayLiteral("{\"success\":false}"),
        QDateTime::currentDateTimeUtc(),
        updated));

    CurrencyRateSnapshot afterFailure;
    QVERIFY(cache.load(afterFailure));
    QCOMPARE(afterFailure.sourceDate, original.sourceDate);
    QCOMPARE(afterFailure.fetchedAtUtc, original.fetchedAtUtc);
    compareRates(afterFailure, original);
    QCOMPARE(readFile(cachePath), cacheBeforeFailure);
}

void CurrencyRateCacheTest::olderResponseDoesNotReplaceNewerCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString cachePath =
        directory.filePath(QStringLiteral("rates.json"));
    const CurrencyRateCache cache(cachePath);
    CurrencyRateSnapshot original;
    original.sourceDate = QDate(2026, 9, 6);
    original.fetchedAtUtc = QDateTime::fromString(
        QStringLiteral("2026-09-06T03:00:00Z"), Qt::ISODate);
    original.ratesToUsd = {11'111, kCurrencyRateScale, 1'222'222};
    QVERIFY(cache.replace(original));
    const QByteArray cacheBeforeFailure = readFile(cachePath);
    QVERIFY(!cacheBeforeFailure.isEmpty());

    CurrencyRateSnapshot updated;
    QVERIFY(!updateCurrencyRateCacheFromCbrResponse(
        cache,
        completeResponse(QByteArrayLiteral("05.09.2026")),
        QDateTime::fromString(
            QStringLiteral("2026-09-06T15:00:00Z"), Qt::ISODate),
        updated));

    CurrencyRateSnapshot afterFailure;
    QVERIFY(cache.load(afterFailure));
    QCOMPARE(afterFailure.sourceDate, original.sourceDate);
    QCOMPARE(afterFailure.fetchedAtUtc, original.fetchedAtUtc);
    compareRates(afterFailure, original);
    QCOMPARE(readFile(cachePath), cacheBeforeFailure);
}

void CurrencyRateCacheTest::manualRatesAreUsedWhenAutomaticUpdatesAreDisabled()
{
    CbrCurrencyRateProvider provider;
    provider.setAutomaticUpdatesEnabled(false);
    QVERIFY(provider.setManualRates(86.54, 100.0));

    const CurrencyConverter converter(provider);
    QCOMPARE(
        converter.convert(Money(100'00, Currency::USD), Currency::RUB)
            .minorUnits(),
        qint64(865'400));
    QCOMPARE(
        converter.convert(Money(100'00, Currency::EUR), Currency::RUB)
            .minorUnits(),
        qint64(1'000'000));
}

void CurrencyRateCacheTest::invalidManualRatesAreRejected()
{
    CbrCurrencyRateProvider provider;
    provider.setAutomaticUpdatesEnabled(false);
    QVERIFY(provider.setManualRates(86.54, 100.0));
    const qint64 previousRubRate = provider.rateToUsd(Currency::RUB);

    QVERIFY(!provider.setManualRates(0.0, 100.0));
    QVERIFY(!provider.setManualRates(86.54, -1.0));
    QCOMPARE(provider.rateToUsd(Currency::RUB), previousRubRate);
}

QTEST_MAIN(CurrencyRateCacheTest)

#include "CurrencyRateCacheTest.moc"
