#include "../services/InvestmentPerformanceCalculator.h"
#include <QtTest>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>
#include <cmath>

class InvestmentPerformanceTest : public QObject {
    Q_OBJECT
    static QDateTime time(int day){return QDateTime(QDate(2025,1,day),QTime(12,0),QTimeZone::UTC);}
    static QVariantMap row(int d,const QString& source,qint64 amount,QVariant cost={},QString account="a",QString currency="RUB"){
        return {{"snapshot_date",time(d).date().toString(Qt::ISODate)},{"observed_at",time(d).toMSecsSinceEpoch()},
                {"source_id",source},{"account_id",account},{"currency",currency},{"value_minor",amount},{"cost_basis_minor",cost},
                {"details_json","{}"},{"issues",""}};
    }
    static InvestmentPerformanceInput input(){
        InvestmentPerformanceInput i;i.accounts={{"a","Счёт","RUB",time(1).addDays(-10).toMSecsSinceEpoch(),0}};
        i.now=time(3);i.from=QDate(2025,1,2);i.to=QDate(2025,1,2);return i;
    }
    static InvestmentOperation op(QString kind,qint64 cash,QString id="op"){
        InvestmentOperation o;o.id=id;o.accountId="a";o.kind=kind;o.cashDeltaMinor=cash;o.occurredAtUtc=time(2).addSecs(-3600);o.feeMinor=0;o.taxMinor=0;return o;
    }
    static QVariantMap realized(QString id,qint64 profit){return {{"sequence",1},{"position_id","p"},{"account_id","a"},{"operation_id",id},{"recorded_at",time(2).addSecs(-3600).toMSecsSinceEpoch()},{"quantity_micros",500000},{"realized_net_minor",profit},{"state_json","{}"}};}
    static qint64 part(const QVariantMap& report,QString code){for(const auto& r:report.value("breakdown").toList())if(r.toMap().value("code")==code)return r.toMap().value("amountMinor").toLongLong();return -1234567;}
    static void reconciles(const QVariantMap& r){qint64 sum=0;for(const auto& p:r.value("breakdown").toList())sum+=p.toMap().value("amountMinor").toLongLong();QCOMPARE(sum,r.value("gainMinor").toLongLong());}
private slots:
    void cashFlowIsNotProfit(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",16500)};
        i.transactions.append({"tx","a","top-up",Money(5000,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)});
        const auto r=InvestmentPerformanceCalculator::calculate(i);
        QCOMPARE(r.value("gainMinor").toLongLong(),qint64(1500));QVERIFY(std::abs(r.value("periodPercent").toDouble()-10)<1e-9);
        QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(5000));reconciles(r);
    }
    void withdrawalIsNotLoss(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",6000)};
        i.transactions.append({"tx","a","withdraw",Money(5000,Currency::RUB),TransactionType::Expense,time(2).addSecs(-3600)});
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(1000));QVERIFY(std::abs(r.value("periodPercent").toDouble()-10)<1e-9);
    }
    void saleAndOpenProfitAreSeparate(){
        auto i=input();i.valuations={row(1,"cash:a",0),row(1,"position:p",10000,8000),row(2,"cash:a",5800),row(2,"position:p",6000,4000)};
        auto sale=op("sell",5800);sale.feeMinor=200;i.operations={sale};i.positionHistory={realized("op",1800)};
        const auto r=InvestmentPerformanceCalculator::calculate(i);
        QCOMPARE(r.value("realizedMinor").toLongLong(),qint64(1800));QCOMPARE(r.value("unrealizedMinor").toLongLong(),qint64(2000));QCOMPARE(r.value("unrealizedChangeMinor").toLongLong(),qint64(0));
        QCOMPARE(part(r,"positions"),qint64(2000));QCOMPARE(part(r,"fees"),qint64(-200));QCOMPARE(part(r,"unexplained"),qint64(0));reconciles(r);
    }
    void purchaseFeesAreCountedOnce(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",4900),row(2,"position:p",5000,5100)};
        auto buy=op("buy",-5100);buy.feeMinor=100;i.operations={buy};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(-100));QCOMPARE(part(r,"positions"),qint64(0));QCOMPARE(part(r,"fees"),qint64(-100));QCOMPARE(part(r,"unexplained"),qint64(0));reconciles(r);
    }
    void payoutGrossAndCharges(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10500)};auto payout=op("dividend",500);payout.feeMinor=10;payout.taxMinor=90;i.operations={payout};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(part(r,"income"),qint64(600));QCOMPARE(part(r,"fees"),qint64(-10));QCOMPARE(part(r,"tax"),qint64(-90));QCOMPARE(part(r,"unexplained"),qint64(0));reconciles(r);
    }
    void depositInterestIsIncome(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10100)};
        i.transactions.append({"interest","a","deposit_interest",Money(100,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)});
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(part(r,"income"),qint64(100));QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(0));reconciles(r);
    }
    void internalTransfersAndAccountSelection(){
        auto i=input();i.accounts.append({"b","Другой","RUB",time(1).addDays(-10).toMSecsSinceEpoch(),0});
        i.valuations={row(1,"cash:a",10000),row(1,"cash:b",10000,{},"b"),row(2,"cash:a",5000),row(2,"cash:b",15000,{},"b")};
        i.transactions={{"transfer-out","a","transfer-out",Money(5000,Currency::RUB),TransactionType::Expense,time(2).addSecs(-3600)},
                        {"transfer-in","b","transfer-in",Money(5000,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)}};
        auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(0));QCOMPARE(r.value("gainMinor").toLongLong(),qint64(0));
        i.accountId="a";r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(-5000));QCOMPARE(r.value("gainMinor").toLongLong(),qint64(0));
    }
    void missingBasisDoesNotHideReturn(){
        auto i=input();i.valuations={row(1,"cash:a",0),row(1,"position:p",10000),row(2,"cash:a",0),row(2,"position:p",11000)};
        auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(!r.value("periodPercent").isNull());QVERIFY(r.value("unrealizedMinor").isNull());QCOMPARE(r.value("gainMinor").toLongLong(),qint64(1000));reconciles(r);
        i.operations={op("sell",500)};r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("realizedMinor").isNull());
    }
    void missingDayStopsReturnButKeepsMoneyResult(){
        auto i=input();i.to=QDate(2025,1,3);i.valuations={row(1,"cash:a",10000),row(3,"cash:a",11000)};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("periodPercent").isNull());QCOMPARE(r.value("gainMinor").toLongLong(),qint64(1000));
    }
    void changedHistoryAndMissingCashAreUnavailable(){
        auto i=input();auto end=row(2,"cash:a",11000);end["issues"]="history_changed";i.valuations={row(1,"cash:a",10000),end};
        auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("gainMinor").isNull());QVERIFY(r.value("periodPercent").isNull());
        i.valuations={row(1,"cash:a",10000),row(2,"position:p",11000,10000)};r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("gainMinor").isNull());
    }
    void currencyRevaluationIsExplained(){
        auto i=input();i.accounts[0].currency="USD";i.valuations={row(1,"cash:a",10000,{},"a","USD"),row(2,"cash:a",10000,{},"a","USD")};
        i.rates={QVariantMap{{"currency","USD"},{"rate_date","2025-01-01"},{"source","manual"},{"rub_per_unit_micros",100000000}},QVariantMap{{"currency","USD"},{"rate_date","2025-01-02"},{"source","manual"},{"rub_per_unit_micros",110000000}}};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(100000));QCOMPARE(part(r,"fx"),qint64(100000));QCOMPARE(part(r,"unexplained"),qint64(0));reconciles(r);
        i.currency="USD";i.rates.clear();QCOMPARE(InvestmentPerformanceCalculator::calculate(i).value("gainMinor").toLongLong(),qint64(0));
    }
    void inflationRequiresExplicitAssumption(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10100)};
        auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("realPercent").isNull());
        i.annualInflationPercent=10;r=InvestmentPerformanceCalculator::calculate(i);
        const double expected=(1.01/std::pow(1.1,1/365.25)-1)*100;QVERIFY(std::abs(r.value("realPercent").toDouble()-expected)<1e-9);
    }
    void oldUnknownFlowDoesNotPoisonNewPeriod(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10000)};
        i.transactions.append({"broken-in","a","transfer-in",Money(100,Currency::RUB),TransactionType::Income,time(1).addDays(-5)});
        auto old= row(1,"position:old",200);old["snapshot_date"]="2024-12-01";old["issues"]="history_changed";i.valuations.append(old);
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(!r.value("periodPercent").isNull());QCOMPARE(r.value("gainMinor").toLongLong(),qint64(0));QVERIFY(r.value("warnings").toStringList().isEmpty());
    }
    void unknownFlowInPeriodStopsResult(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",11000)};
        i.transactions.append({"broken-in","a","transfer-in",Money(1000,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)});
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("gainMinor").isNull());QVERIFY(r.value("periodPercent").isNull());
    }
    void totalOverflowIsUnavailable(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",std::numeric_limits<qint64>::max()),row(2,"position:p",10000,10000)};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("gainMinor").isNull());QVERIFY(r.value("periodPercent").isNull());
    }
    void emptyDaysBeforeFundingAreNeutral(){
        auto i=input();i.from={};i.to=QDate(2025,1,3);i.valuations={row(1,"cash:a",0),row(2,"cash:a",0),row(3,"cash:a",11000)};
        i.transactions.append({"deposit","a","top-up",Money(10000,Currency::RUB),TransactionType::Income,time(3).addSecs(-3600)});
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(std::abs(r.value("periodPercent").toDouble()-10)<1e-9);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(1000));
        i.transactions.clear();i.valuations[2]=row(3,"cash:a",0);QVERIFY(InvestmentPerformanceCalculator::calculate(i).value("periodPercent").isNull());
    }
    void importedManualPositionIsExternalValue(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10000),row(2,"position:p",5000,5000)};
        QJsonObject settings{{"manual",true},{"value","5000"},{"valuedAt",time(2).addSecs(-3600).toString(Qt::ISODateWithMs)}};
        const QString state=QString::fromUtf8(QJsonDocument(QJsonObject{{"settings",settings},{"quantityMicros","1000000"}}).toJson(QJsonDocument::Compact));
        i.positionHistory={QVariantMap{{"sequence",1},{"position_id","p"},{"account_id","a"},{"currency","RUB"},{"event","opening"},{"recorded_at",time(2).addSecs(-3600).toMSecsSinceEpoch()},{"quantity_micros",1000000},{"state_json",state}}};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(0));QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(5000));QCOMPARE(r.value("periodPercent").toDouble(),0.0);reconciles(r);
    }
    void futureClearingDoesNotCountProfitTwice(){
        auto i=input();auto first=row(1,"position:p",2000);auto last=row(2,"position:p",0);
        first["details_json"]="{\"terms\":{\"pricing\":\"future\"}}";last["details_json"]=first["details_json"];
        i.valuations={row(1,"cash:a",10000),first,row(2,"cash:a",12000),last};
        i.operations={op("margin",2000)};auto h=realized("op",2000);h["state_json"]="{\"terms\":{\"pricing\":\"future\"},\"type\":9}";i.positionHistory={h};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QCOMPARE(r.value("gainMinor").toLongLong(),qint64(0));QCOMPARE(r.value("realizedMinor").toLongLong(),qint64(2000));QCOMPARE(r.value("unrealizedChangeMinor").toLongLong(),qint64(-2000));QCOMPARE(part(r,"positions"),qint64(0));reconciles(r);
    }
    void moreThanOneYearKeepsPeriodReturn(){
        auto i=input();i.from={};i.to={};i.now=time(1).addDays(730);
        for(int d=0;d<=730;++d){auto p=row(1,"cash:a",qint64(10000*(1.0+0.21*d/730.0)));p["snapshot_date"]=time(1).addDays(d).date().toString(Qt::ISODate);p["observed_at"]=time(1).addDays(d).toMSecsSinceEpoch();i.valuations.append(p);}
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(std::abs(r.value("periodPercent").toDouble()-21)<1e-8);
        QVERIFY(std::abs(r.value("annualPercent").toDouble()-10)<0.02);QVERIFY(r.value("periodPercent")!=r.value("annualPercent"));
    }
    void correctionAcrossAccountBoundaryIsNotMarketProfit(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10000),row(2,"position:p",5000)};
        auto before=realized("",0);before.remove("operation_id");before["sequence"]=1;before["account_id"]="outside";before["event"]="opening";before["instrument_id"]="stock";before["recorded_at"]=time(1).addDays(-2).toMSecsSinceEpoch();
        auto after=before;after["sequence"]=2;after["event"]="correction";after["account_id"]="a";after["recorded_at"]=time(2).addSecs(-3600).toMSecsSinceEpoch();i.positionHistory={before,after};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("gainMinor").isNull());QVERIFY(r.value("periodPercent").isNull());
    }
    void riskUsesFlowAdjustedDailyReturns(){
        auto i=input();i.from={};i.to={};i.now=time(3);
        i.valuations={row(1,"cash:a",10000),row(2,"cash:a",16500),row(3,"cash:a",3200)};
        i.transactions={{"deposit","a","top-up",Money(5000,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)},
                        {"withdrawal","a","withdraw",Money(10000,Currency::RUB),TransactionType::Expense,time(3).addSecs(-3600)}};
        const auto r=InvestmentPerformanceCalculator::calculate(i);const auto risk=r.value("risk").toMap();
        QVERIFY(risk.value("available").toBool());QCOMPARE(risk.value("returnDays").toInt(),2);
        QVERIFY(std::abs(risk.value("maximumDrawdownPercent").toDouble()-20)<1e-8);
        QVERIFY(std::abs(risk.value("bestDayPercent").toDouble()-10)<1e-8);
        QVERIFY(std::abs(risk.value("worstDayPercent").toDouble()+20)<1e-8);
        QVERIFY(std::abs(risk.value("annualVolatilityPercent").toDouble()-std::sqrt(450.0*365.25))<1e-8);
        QCOMPARE(risk.value("drawdowns").toList().size(),3);
    }
    void riskIsUnavailableWhenHistoryIsBroken(){
        auto i=input();i.to=QDate(2025,1,3);i.valuations={row(1,"cash:a",10000),row(3,"cash:a",11000)};
        auto risk=InvestmentPerformanceCalculator::calculate(i).value("risk").toMap();
        QVERIFY(!risk.value("available").toBool());QVERIFY(risk.value("drawdowns").toList().isEmpty());QVERIFY(risk.value("maximumDrawdownPercent").isNull());
        i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10000),row(3,"cash:a",11000)};
        i.transactions.append({"broken-in","a","transfer-in",Money(1000,Currency::RUB),TransactionType::Income,time(2).addSecs(-3600)});
        risk=InvestmentPerformanceCalculator::calculate(i).value("risk").toMap();QVERIFY(!risk.value("available").toBool());QVERIFY(risk.value("drawdowns").toList().isEmpty());
    }
    void oneDailyReturnDoesNotInventVolatility(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",11000)};
        const auto risk=InvestmentPerformanceCalculator::calculate(i).value("risk").toMap();
        QVERIFY(risk.value("available").toBool());QVERIFY(risk.value("annualVolatilityPercent").isNull());QCOMPARE(risk.value("maximumDrawdownPercent").toDouble(),0.0);
    }
    void firstDayIsNotAnInventedZeroReturn(){
        auto i=input();i.from={};i.to={};i.now=time(1);i.valuations={row(1,"cash:a",10000)};
        const auto r=InvestmentPerformanceCalculator::calculate(i);QVERIFY(r.value("periodPercent").isNull());QVERIFY(r.value("gainMinor").isNull());
    }
};
QTEST_GUILESS_MAIN(InvestmentPerformanceTest)
#include "InvestmentPerformanceTest.moc"
