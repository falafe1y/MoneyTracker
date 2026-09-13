#include "../services/MoexInvestmentParser.h"

#include <QtTest>

class MoexInvestmentParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesStocksAndFundsAndSkipsOtherSecurities();
    void parsesLastPriceAndCurrency();
    void fallsBackToPreviousClose();
    void rejectsMalformedResponses();
};

void MoexInvestmentParserTest::parsesStocksAndFundsAndSkipsOtherSecurities()
{
    const QByteArray payload = R"json({
        "securities": {
            "columns": ["secid", "shortname", "name", "isin", "type", "group", "primary_boardid"],
            "data": [
                ["SBER", "Сбербанк", "Сбербанк России", "RU0009029540", "common_share", "stock_shares", "TQBR"],
                ["LQDT", "Ликвидность", "БПИФ Ликвидность", "RU000A108WX3", "exchange_ppif", "stock_ppif", "TQTF"],
                ["SiZ6", "USD/RUB", "Фьючерс", "", "futures", "futures_forts", "RFUD"]
            ]
        }
    })json";

    QVector<InvestmentMarketInstrument> instruments;
    QString error;
    QVERIFY2(parseMoexInvestmentSearch(payload, instruments, &error),
             qPrintable(error));
    QCOMPARE(instruments.size(), 2);
    QCOMPARE(instruments.at(0).id(), QStringLiteral("moex:SBER"));
    QCOMPARE(instruments.at(0).symbol(), QStringLiteral("SBER"));
    QCOMPARE(instruments.at(0).isin(), QStringLiteral("RU0009029540"));
    QCOMPARE(instruments.at(0).type(), InvestmentInstrumentType::Stock);
    QCOMPARE(instruments.at(0).primaryBoardId(), QStringLiteral("TQBR"));
    QCOMPARE(instruments.at(1).symbol(), QStringLiteral("LQDT"));
    QCOMPARE(instruments.at(1).type(), InvestmentInstrumentType::Etf);
}

void MoexInvestmentParserTest::parsesLastPriceAndCurrency()
{
    const QByteArray payload = R"json({
        "securities": {
            "columns": ["SECID", "BOARDID", "PREVPRICE", "CURRENCYID"],
            "data": [["SBER", "TQBR", 310.15, "SUR"]]
        },
        "marketdata": {
            "columns": ["SECID", "BOARDID", "LAST", "MARKETPRICE", "LCURRENTPRICE", "LEGALCLOSEPRICE"],
            "data": [["SBER", "TQBR", 312.345678, 311.9, null, null]]
        }
    })json";

    qint64 priceMicros = 0;
    QString currency;
    QString error;
    QVERIFY2(parseMoexInvestmentQuote(
                 payload, QStringLiteral("TQBR"),
                 priceMicros, currency, &error),
             qPrintable(error));
    QCOMPARE(priceMicros, qint64(312'345'678));
    QCOMPARE(currency, QStringLiteral("RUB"));
}

void MoexInvestmentParserTest::fallsBackToPreviousClose()
{
    const QByteArray payload = R"json({
        "securities": {
            "columns": ["SECID", "BOARDID", "PREVPRICE", "CURRENCYID"],
            "data": [["FXUS", "TQTF", 91.75, "USD"]]
        },
        "marketdata": {
            "columns": ["SECID", "BOARDID", "LAST", "MARKETPRICE", "LCURRENTPRICE", "LEGALCLOSEPRICE"],
            "data": [["FXUS", "TQTF", null, null, null, null]]
        }
    })json";

    qint64 priceMicros = 0;
    QString currency;
    QVERIFY(parseMoexInvestmentQuote(
        payload, QStringLiteral("TQTF"), priceMicros, currency));
    QCOMPARE(priceMicros, qint64(91'750'000));
    QCOMPARE(currency, QStringLiteral("USD"));
}

void MoexInvestmentParserTest::rejectsMalformedResponses()
{
    QVector<InvestmentMarketInstrument> instruments;
    QVERIFY(!parseMoexInvestmentSearch("not-json", instruments));

    qint64 priceMicros = 0;
    QString currency;
    QVERIFY(!parseMoexInvestmentQuote(
        R"json({"securities":{"columns":[],"data":[]}})json",
        QStringLiteral("TQBR"), priceMicros, currency));
}

QTEST_APPLESS_MAIN(MoexInvestmentParserTest)

#include "MoexInvestmentParserTest.moc"
