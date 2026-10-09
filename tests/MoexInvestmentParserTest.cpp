#include "../services/MoexInvestmentParser.h"

#include <QtTest>

class MoexInvestmentParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesStocksFundsAndFutures();
    void parsesLastPriceAndCurrency();
    void fallsBackToPreviousClose();
    void rejectsMalformedResponses();
    void searchesAllAssetClassesButNotIndicesOrSwaps();
    void parsesBondNominalAccrualAndCurrency();
    void routesAndValuesDerivativeSpecifications();
    void normalizesCurrencyQuotationUnits();
    void specialFuturesRequireExplicitValuation();
    void preservesQuotationSourceAndTime();
};

void MoexInvestmentParserTest::parsesStocksFundsAndFutures()
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
    QCOMPARE(instruments.size(), 3);
    QCOMPARE(instruments.at(0).id(), QStringLiteral("moex:SBER"));
    QCOMPARE(instruments.at(0).symbol(), QStringLiteral("SBER"));
    QCOMPARE(instruments.at(0).isin(), QStringLiteral("RU0009029540"));
    QCOMPARE(instruments.at(0).type(), InvestmentInstrumentType::Stock);
    QCOMPARE(instruments.at(0).primaryBoardId(), QStringLiteral("TQBR"));
    QCOMPARE(instruments.at(1).symbol(), QStringLiteral("LQDT"));
    QCOMPARE(instruments.at(1).type(), InvestmentInstrumentType::Fund);
    QCOMPARE(instruments.at(2).type(), InvestmentInstrumentType::Future);
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


void MoexInvestmentParserTest::searchesAllAssetClassesButNotIndicesOrSwaps()
{
    const auto payload=R"({"securities":{"columns":["secid","shortname","name","isin","type","group","primary_boardid","is_traded"],"data":[
        ["SBERP","Сбер-п","Акция","","preferred_share","stock_shares","TQBR",1],
        ["DR","Расписка","Расписка","","depositary_receipt","stock_dr","TQBR",1],
        ["BOND","Облигация","Облигация","","exchange_bond","stock_bonds","TQCB",1],
        ["SiZ6","Доллар","Фьючерс","","futures","futures_forts","RFUD",1],
        ["OPT","Опцион","Опцион","","option","futures_options","ROPD",1],
        ["GLDRUB_TOM","Золото","GLD/RUB_TOM","","gold_metal","currency_metal","CETS",1],
        ["CNYRUB_TOM","Юань","CNY/RUB_TOM","","currency","currency_selt","CETS",1],
        ["INDEX","Индекс","Индекс","","stock_index","stock_index","INPF",1],
        ["GLDRUBTODTOM","Своп","СВОП GLD/RUB","","gold_metal","currency_metal","CETS",1],
        ["OLD","Старая акция","Старая акция","","common_share","stock_shares","TQBR",0]
    ]}})";
    QVector<InvestmentMarketInstrument> instruments;QVERIFY(parseMoexInvestmentSearch(payload,instruments));QCOMPARE(instruments.size(),7);
    QCOMPARE(instruments.at(0).type(),InvestmentInstrumentType::PreferredStock);
    QCOMPARE(instruments.at(1).type(),InvestmentInstrumentType::DepositaryReceipt);
    QCOMPARE(instruments.at(2).terms().market,QStringLiteral("bonds"));
    QCOMPARE(instruments.at(3).symbol(),QStringLiteral("SiZ6"));
    QCOMPARE(instruments.at(5).type(),InvestmentInstrumentType::Metal);
    QCOMPARE(instruments.at(6).type(),InvestmentInstrumentType::Currency);
}
void MoexInvestmentParserTest::parsesBondNominalAccrualAndCurrency()
{
    InvestmentMarketInstrument i("bond","BOND","","Облигация",InvestmentInstrumentType::Bond,"TQCB",moexDefaultTerms(InvestmentInstrumentType::Bond));
    const auto payload=R"({"securities":{"columns":["SECID","BOARDID","PREVPRICE","CURRENCYID","FACEUNIT","FACEVALUE","FACEVALUEONSETTLEDATE","ACCRUEDINT"],"data":[["BOND","TQCB",95,"CNY","CNY",1000,500,20]]},"marketdata":{"columns":["SECID","BOARDID","LAST"],"data":[["BOND","TQCB",96]]}})";
    QVERIFY(parseMoexInvestmentQuote(payload,i));QCOMPARE(i.priceMicros(),qint64(96'000'000));
    QCOMPARE(i.terms().faceValueMicros,qint64(500'000'000));QCOMPARE(i.terms().accruedInterestMicros,qint64(20'000'000));
    QCOMPARE(i.currencyCode(),QStringLiteral("CNY"));QCOMPARE(i.terms().pricing,QStringLiteral("bond"));
    QVERIFY(!parseMoexInvestmentQuote(R"({"securities":{"columns":["SECID","BOARDID"],"data":[["BOND","OTHER"]]}})",i));
}
void MoexInvestmentParserTest::routesAndValuesDerivativeSpecifications()
{
    InvestmentMarketInstrument i("opt","OPT","","Опцион",InvestmentInstrumentType::Option,"ROPD",moexDefaultTerms(InvestmentInstrumentType::Option));
    const auto details=R"({"description":{"columns":["name","value"],"data":[["NAME","Премиальный опцион"],["SECID","Opt"],["FACEUNIT","RUB"]]},"boards":{"columns":["boardid","engine","market","is_primary"],"data":[["ROPD","futures","options",1]]}})";
    QVERIFY(parseMoexInstrumentDetails(details,i));QCOMPARE(i.symbol(),QStringLiteral("Opt"));QCOMPARE(i.terms().pricing,QStringLiteral("premium_option"));
    const auto payload=R"({"securities":{"columns":["SECID","BOARDID","MINSTEP","STEPPRICE","SETTLEPRICE_CLR","STRIKE","OPTIONTYPE"],"data":[["Opt","ROPD",0.01,1,12,100,"C"]]},"marketdata":{"columns":["SECID","BOARDID","LAST","SYSTIME"],"data":[["Opt","ROPD",0,"2026-10-09 10:00:00"]]}})";
    QVERIFY(parseMoexInvestmentQuote(payload,i));QCOMPARE(i.priceMicros(),qint64(12'000'000));QCOMPARE(i.terms().multiplierMicros,qint64(100'000'000));
    QCOMPARE(i.currencyCode(),QStringLiteral("RUB"));QCOMPARE(i.terms().strikeMicros,qint64(100'000'000));
}
void MoexInvestmentParserTest::normalizesCurrencyQuotationUnits()
{
    InvestmentMarketInstrument i("fx","FX","","Валюта",InvestmentInstrumentType::Currency,"CETS",moexDefaultTerms(InvestmentInstrumentType::Currency));
    const auto payload=R"({"securities":{"columns":["SECID","BOARDID","CURRENCYID","FACEUNIT","FACEVALUE","LOTSIZE","PREVPRICE"],"data":[["FX","CETS","SUR","JPY",100,100000,90]]},"marketdata":{"columns":["SECID","BOARDID","LAST"],"data":[["FX","CETS",91]]}})";
    QVERIFY(parseMoexInvestmentQuote(payload,i));QCOMPARE(i.terms().multiplierMicros,qint64(10000));QCOMPARE(i.terms().quantityUnit,QStringLiteral("JPY"));
}


void MoexInvestmentParserTest::specialFuturesRequireExplicitValuation()
{
    const auto payload=R"({"description":{"columns":["name","value"],"data":[["SECID","BRZ6"],["NAME","Фьючерс на нефть"],["FACEUNIT","USD"]]},"boards":{"columns":["boardid","engine","market","is_primary"],"data":[["RFUD","futures","forts",1]]}})";
    InvestmentMarketInstrument i("moex:BRZ6","BRZ6","","Нефть",InvestmentInstrumentType::Future,"RFUD",moexDefaultTerms(InvestmentInstrumentType::Future));
    QVERIFY(parseMoexInstrumentDetails(payload,i));
    QCOMPARE(i.terms().pricing,QStringLiteral("manual"));
    QByteArray perpetual(payload);perpetual.replace("USD","RUB");perpetual.replace("Фьючерс на нефть","Автопролонгируемый фьючерс");
    QVERIFY(parseMoexInstrumentDetails(perpetual,i));
    QVERIFY(i.terms().perpetual);QCOMPARE(i.terms().pricing,QStringLiteral("manual"));
}
void MoexInvestmentParserTest::preservesQuotationSourceAndTime()
{
    const QByteArray payload=R"({"securities":{"columns":["SECID","BOARDID","CURRENCYID","PREVPRICE","PREVDATE"],"data":[["SBER","TQBR","SUR",120,"2026-10-08"]]},"marketdata":{"columns":["SECID","BOARDID","LAST","TIME","SYSTIME","TRADEDATE"],"data":[["SBER","TQBR",121,"12:00:00","2026-10-09 12:15:00","2026-10-09"]]}})";
    InvestmentMarketInstrument i("moex:SBER","SBER","","Сбер",InvestmentInstrumentType::Stock,"TQBR",moexDefaultTerms(InvestmentInstrumentType::Stock));
    QVERIFY(parseMoexInvestmentQuote(payload,i));QCOMPARE(i.terms().quoteSource,QStringLiteral("last"));
    QCOMPARE(i.quotedAtUtc(),QDateTime::fromString("2026-10-09T09:00:00Z",Qt::ISODate));
    auto previous=payload;previous.replace("121,", "null,");
    QVERIFY(parseMoexInvestmentQuote(previous,i));QCOMPARE(i.terms().quoteSource,QStringLiteral("prevprice"));
    QCOMPARE(i.quotedAtUtc(),QDateTime::fromString("2026-10-08T20:59:59Z",Qt::ISODate));
}

QTEST_APPLESS_MAIN(MoexInvestmentParserTest)

#include "MoexInvestmentParserTest.moc"
