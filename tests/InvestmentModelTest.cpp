#include "../core/InvestmentInstrument.h"
#include "../core/InvestmentPosition.h"
#include "../core/InvestmentQuote.h"

#include <QtTest>

class InvestmentModelTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesInstrumentData();
    void preservesFractionalPositionPrecision();
    void preservesQuoteData();
};

void InvestmentModelTest::preservesInstrumentData()
{
    const InvestmentInstrument instrument(
        QStringLiteral("instrument-aapl"),
        QStringLiteral("AAPL"),
        QStringLiteral("US0378331005"),
        QStringLiteral("Apple Inc."),
        InvestmentInstrumentType::Stock,
        Currency::USD);

    QCOMPARE(instrument.id(), QStringLiteral("instrument-aapl"));
    QCOMPARE(instrument.symbol(), QStringLiteral("AAPL"));
    QCOMPARE(instrument.isin(), QStringLiteral("US0378331005"));
    QCOMPARE(instrument.name(), QStringLiteral("Apple Inc."));
    QCOMPARE(instrument.type(), InvestmentInstrumentType::Stock);
    QCOMPARE(instrument.currency(), Currency::USD);
}

void InvestmentModelTest::preservesFractionalPositionPrecision()
{
    const InvestmentPosition position(
        QStringLiteral("position-aapl"),
        QStringLiteral("brokerage-account"),
        QStringLiteral("instrument-aapl"),
        1'250'001,
        182'345'678);

    QCOMPARE(InvestmentPosition::Scale, qint64(1'000'000));
    QCOMPARE(position.quantityMicros(), qint64(1'250'001));
    QCOMPARE(position.averagePriceMicros(), qint64(182'345'678));
}

void InvestmentModelTest::preservesQuoteData()
{
    const QDateTime quotedAt = QDateTime::fromMSecsSinceEpoch(
        1'788'200'000'123, Qt::UTC);
    const InvestmentQuote quote(
        QStringLiteral("instrument-aapl"), 201'120'001, quotedAt);

    QCOMPARE(quote.instrumentId(), QStringLiteral("instrument-aapl"));
    QCOMPARE(quote.priceMicros(), qint64(201'120'001));
    QCOMPARE(quote.quotedAtUtc(), quotedAt);
}

QTEST_APPLESS_MAIN(InvestmentModelTest)

#include "InvestmentModelTest.moc"
