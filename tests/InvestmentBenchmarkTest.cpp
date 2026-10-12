#include "../services/InvestmentBenchmark.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

class InvestmentBenchmarkTest : public QObject {
    Q_OBJECT
    static QVariantMap point(QString d,double p){return {{"date",d},{"percent",p}};}
    static QVariantMap price(QString d,QVariant p,QString c="RUB"){return {{"price_date",d},{"price_micros",p},{"currency",c},{"issues",""}};}
    static QByteArray page(const QJsonArray& rows,int start=0,int total=-1){
        if(total<0)total=rows.size();return QJsonDocument(QJsonObject{
            {"history",QJsonObject{{"columns",QJsonArray{"BOARDID","TRADEDATE","CLOSE","CURRENCYID","TRADINGSESSION"}},{"data",rows}}},
            {"history.cursor",QJsonObject{{"columns",QJsonArray{"INDEX","TOTAL","PAGESIZE"}},{"data",QJsonArray{QJsonArray{start,total,100}}}}}}).toJson();
    }
private slots:
    void samePeriodAndDifference(){
        const auto r=compareInvestmentBenchmark({point("2025-01-01",0),point("2025-01-02",15),point("2025-01-03",30)},
            {price("2025-01-01",100000000),price("2025-01-02",110000000),price("2025-01-03",121000000)},{},"RUB");
        QVERIFY(r.value("available").toBool());QVERIFY(std::abs(r.value("benchmarkPercent").toDouble()-21)<1e-9);QVERIFY(std::abs(r.value("differencePoints").toDouble()-9)<1e-9);
        QCOMPARE(r.value("points").toList().first().toMap().value("benchmark").toDouble(),0.0);
    }
    void weekendsUseEarlierPriceNeverFuture(){
        const auto r=compareInvestmentBenchmark({point("2025-01-03",0),point("2025-01-04",0),point("2025-01-05",0)},
            {price("2025-01-03",100000000),price("2025-01-06",200000000)},{},"RUB");
        QVERIFY(r.value("available").toBool());QCOMPARE(r.value("benchmarkPercent").toDouble(),0.0);QCOMPARE(r.value("points").toList().last().toMap().value("quoteDate").toString(),QString("2025-01-03"));
        const auto missing=compareInvestmentBenchmark({point("2025-01-01",0),point("2025-01-02",0)},{price("2025-01-03",100000000)},{},"RUB");QVERIFY(!missing.value("available").toBool());
    }
    void currencyConversionUsesRatesAtValuationDates(){
        const QVariantList rates{QVariantMap{{"currency","USD"},{"rate_date","2025-01-01"},{"source","CBR"},{"rub_per_unit_micros",100000000}},
            QVariantMap{{"currency","USD"},{"rate_date","2025-01-02"},{"source","CBR"},{"rub_per_unit_micros",110000000}},
            QVariantMap{{"currency","USD"},{"rate_date","2025-01-03"},{"source","CBR"},{"rub_per_unit_micros",200000000}}};
        const auto r=compareInvestmentBenchmark({point("2025-01-01",0),point("2025-01-02",0)},{price("2025-01-01",100000000),price("2025-01-02",100000000)},rates,"USD");
        QVERIFY(r.value("available").toBool());QVERIFY(std::abs(r.value("benchmarkPercent").toDouble()-(100.0/110-1)*100)<1e-9);
    }
    void unknownPricesAndRatesStayUnavailable(){
        const QVariantList portfolio{point("2025-01-01",0),point("2025-01-02",0)};
        QVERIFY(!compareInvestmentBenchmark(portfolio,{price("2025-01-01",100000000),price("2025-01-02",QVariant())},{},"RUB").value("available").toBool());
        QVERIFY(!compareInvestmentBenchmark(portfolio,{price("2025-01-01",100000000)},{},"USD").value("available").toBool());
        QVariantList rates{QVariantMap{{"currency","USD"},{"rate_date","2025-01-01"},{"source","estimated"},{"rub_per_unit_micros",100000000}}};
        QVERIFY(!compareInvestmentBenchmark(portfolio,{price("2025-01-01",100000000)},rates,"USD").value("available").toBool());
        QVERIFY(!compareInvestmentBenchmark({point("2025-01-01",0),point("2025-01-03",0)},{price("2025-01-01",100000000)},{},"RUB").value("available").toBool());
        const auto holiday=compareInvestmentBenchmark({point("2025-01-10",0),point("2025-01-11",0)},{price("2025-01-01",100000000)},{},"RUB");
        QVERIFY(holiday.value("available").toBool());QCOMPARE(holiday.value("maximumQuoteAgeDays").toInt(),10);
    }
    void indexPageAndIncompleteResponse(){
        auto index=investmentBenchmarkIndices().first().toMap();QVariantList rows,prices;QString error;int next;
        QVERIFY(parseBenchmarkPage(page(QJsonArray{QJsonArray{"SNDX","2025-01-01",100.123456,"RUB",3}}),index,rows,next,error));QCOMPARE(next,-1);
        QVERIFY(mergeBenchmarkRows(rows,prices,error));QCOMPARE(prices.first().toMap().value("price_micros").toLongLong(),100123456LL);
        rows.clear();QVERIFY(!parseBenchmarkPage("{}",index,rows,next,error));
        const QByteArray wrong=R"({"history":{"columns":["SECID","BOARDID","TRADEDATE","CLOSE","CURRENCYID"],"data":[["SBMX","SNDX","2025-01-01",100,"RUB"]]},"history.cursor":{"columns":["INDEX","TOTAL","PAGESIZE"],"data":[[0,1,100]]}})";
        QVERIFY(!parseBenchmarkPage(wrong,index,rows,next,error));
        QVERIFY(!parseBenchmarkPage(page({},0,200),index,rows,next,error));
    }
    void historicalFundBoardTransition(){
        const QVariantMap descriptor{{"kind","fund"},{"boards",QVariantList{
            QVariantMap{{"id","TQTF"},{"from","2018-09-17"},{"to","2026-06-19"},{"primary",false}},
            QVariantMap{{"id","TQBR"},{"from","2026-06-22"},{"to","2026-10-09"},{"primary",true}}}}};
        QVariantList rows,prices;int next;QString error;
        QVERIFY(parseBenchmarkPage(page({QJsonArray{"TQTF","2025-09-22",18.175,"SUR",3},QJsonArray{"TQBR","2025-09-22",99,"SUR",3},
            QJsonArray{"TQBR","2026-06-23",20,"SUR",3}}),descriptor,rows,next,error));
        QVERIFY(mergeBenchmarkRows(rows,prices,error));QCOMPARE(prices.size(),2);QCOMPARE(prices.first().toMap().value("price_micros").toLongLong(),18175000LL);
        QCOMPARE(prices.first().toMap().value("currency").toString(),QString("RUB"));QCOMPARE(prices.last().toMap().value("board").toString(),QString("TQBR"));
    }
    void nullClosingPriceIsNotReplacedByAnotherSession(){
        const auto descriptor=investmentBenchmarkIndices().first().toMap();QVariantList rows,prices;QString error;int next;
        QVERIFY(parseBenchmarkPage(page({QJsonArray{"SNDX","2025-01-01",100,"RUB",1},QJsonArray{"SNDX","2025-01-01",QJsonValue(),"RUB",3}}),descriptor,rows,next,error));
        QVERIFY(mergeBenchmarkRows(rows,prices,error));QVERIFY(prices.first().toMap().value("price_micros").isNull());
        rows.append(QVariantMap{{"price_date","2025-01-01"},{"price_micros",200000000},{"currency","RUB"},{"priority",103}});prices.clear();QVERIFY(!mergeBenchmarkRows(rows,prices,error));
    }
};
QTEST_GUILESS_MAIN(InvestmentBenchmarkTest)
#include "InvestmentBenchmarkTest.moc"
