#include "../services/InvestmentPerformanceCalculator.h"
#include <QtTest>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>

class InvestmentStructureIncomeTest : public QObject {
    Q_OBJECT
    static QDateTime time(int d){return QDateTime(QDate(2025,1,d),QTime(12,0),QTimeZone::UTC);}
    static InvestmentPerformanceInput input(){
        InvestmentPerformanceInput i;i.accounts={{"a","Брокер","RUB",time(1).addDays(-10).toMSecsSinceEpoch(),0}};
        i.from=QDate(2025,1,2);i.to=QDate(2025,1,2);i.now=time(2);
        i.instruments={QVariantMap{{"id","stock"},{"symbol","ABC"},{"type",0}},QVariantMap{{"id","bond"},{"symbol","BOND"},{"type",2}}};return i;
    }
    static QVariantMap row(int d,QString source,QVariant amount,QString instrument="",QString account="a",QString currency="RUB"){
        return {{"snapshot_date",time(d).date().toString(Qt::ISODate)},{"observed_at",time(d).toMSecsSinceEpoch()},
            {"source_id",source},{"account_id",account},{"instrument_id",instrument},{"currency",currency},
            {"value_minor",amount},{"cost_basis_minor",amount},{"details_json","{}"},{"issues",""}};
    }
    static InvestmentOperation op(QString kind,qint64 cash,QString instrument="stock",QString id="op"){
        InvestmentOperation o;o.id=id;o.accountId="a";o.positionId="p";o.instrumentId=instrument;o.kind=kind;
        o.cashDeltaMinor=cash;o.occurredAtUtc=time(2).addSecs(-3600);o.feeMinor=0;o.taxMinor=0;return o;
    }
    static QVariantMap find(const QVariantList& list,QString id){for(const auto& item:list)if(item.toMap().value("id")==id)return item.toMap();return {};}
    static QVariantMap calculate(const InvestmentPerformanceInput& i){return InvestmentPerformanceCalculator::calculate(i);}
    static QVariantMap contribution(const QVariantMap& report,QString id){return find(report.value("contributions").toMap().value("items").toList(),id);}
    static void reconciles(const QVariantMap& report){const auto c=report.value("contributions").toMap();QVERIFY(c.value("available").toBool());
        qint64 sum=c.value("fxMinor").toLongLong()+c.value("unexplainedMinor").toLongLong();
        for(const auto& item:c.value("items").toList())sum+=item.toMap().value("amountMinor").toLongLong();
        QCOMPARE(sum,report.value("gainMinor").toLongLong());}
private slots:
    void allocationAndGrouping(){
        auto i=input();i.accounts.append({"b","Вклад","RUB",0,0});
        i.valuations={row(2,"cash:a",1000),row(2,"cash:b",2000,"","b"),row(2,"position:p",7000,"stock")};
        const auto a=calculate(i).value("allocation").toMap();QVERIFY(a.value("pieAvailable").toBool());
        QCOMPARE(a.value("capitalMinor").toLongLong(),qint64(10000));
        const auto stock=find(a.value("items").toList(),"instrument:stock");QCOMPARE(stock.value("label").toString(),QString("ABC"));QCOMPARE(stock.value("percent").toDouble(),70.0);
        QCOMPARE(find(a.value("accounts").toList(),"a").value("amountMinor").toLongLong(),qint64(8000));
        QCOMPARE(find(a.value("accounts").toList(),"b").value("label").toString(),QString("Вклад"));
        QCOMPARE(find(a.value("types").toList(),"Деньги").value("amountMinor").toLongLong(),qint64(3000));
        i.accountId="b";QCOMPARE(calculate(i).value("allocation").toMap().value("capitalMinor").toLongLong(),qint64(2000));
    }
    void partialSaleAndPurchaseFeesAreNetOnce(){
        auto i=input();i.valuations={row(1,"cash:a",0),row(1,"position:p",10000,"stock"),row(2,"cash:a",5800),row(2,"position:p",6000,"stock")};
        auto sale=op("sell",5800);sale.feeMinor=200;i.operations={sale};auto r=calculate(i);
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(1800));reconciles(r);
        i.valuations={row(1,"cash:a",10000),row(2,"cash:a",4900),row(2,"position:p",5000,"stock")};
        auto buy=op("buy",-5100);buy.feeMinor=100;i.operations={buy};r=calculate(i);
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(-100));reconciles(r);
    }
    void closedPositionRecoversHistoricalIdentity(){
        auto i=input();i.instruments.clear();i.valuations={row(1,"cash:a",0),row(1,"position:p",10000,"stock"),row(2,"cash:a",12000)};
        auto sale=op("sell",12000);sale.positionId.clear();sale.instrumentId.clear();i.operations={sale};
        i.positionHistory={QVariantMap{{"sequence",1},{"position_id","p"},{"instrument_id","stock"},{"account_id","a"},{"operation_id","op"},
            {"recorded_at",sale.occurredAtUtc.toMSecsSinceEpoch()},{"quantity_micros",0},{"realized_net_minor",2000},
            {"state_json",QString::fromUtf8(QJsonDocument(QJsonObject{{"symbol","OLD"},{"type",0}}).toJson())}}};
        auto r=calculate(i);const auto c=contribution(r,"instrument:stock");QCOMPARE(c.value("amountMinor").toLongLong(),qint64(2000));
        // The opening row predates the identity record; the sale supplies its archived name.
        QCOMPARE(c.value("label").toString(),QString("OLD"));reconciles(r);
    }
    void negativeAndUnknownHoldingsDoNotInventPie(){
        auto i=input();i.valuations={row(2,"cash:a",10000),row(2,"position:p",-1000,"stock")};
        auto a=calculate(i).value("allocation").toMap();QVERIFY(a.value("available").toBool());QVERIFY(!a.value("pieAvailable").toBool());
        QVERIFY(find(a.value("items").toList(),"instrument:stock").value("percent").isNull());
        i.valuations[1]=row(2,"position:p",{},"stock");a=calculate(i).value("allocation").toMap();
        QVERIFY(!a.value("available").toBool());QVERIFY(a.value("capitalMinor").isNull());
        i.valuations={row(2,"cash:a",0)};QVERIFY(!calculate(i).value("allocation").toMap().value("pieAvailable").toBool());
    }
    void marginedResultIsSeparateFromInvestedCapital(){
        auto i=input();auto first=row(1,"position:p",500,"stock"),last=row(2,"position:p",200,"stock");
        first["details_json"]=last["details_json"]="{\"terms\":{\"pricing\":\"future\"}}";
        i.valuations={row(1,"cash:a",10000),first,row(2,"cash:a",10400),last};i.operations={op("margin",400)};
        const auto r=calculate(i),a=r.value("allocation").toMap();QCOMPARE(a.value("capitalMinor").toLongLong(),qint64(10400));
        QCOMPARE(a.value("adjustmentMinor").toLongLong(),qint64(200));QCOMPARE(a.value("items").toList().size(),1);
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(100));reconciles(r);
    }
    void importedPositionIsNotAContribution(){
        auto i=input();const auto imported=time(2).addSecs(-3600);
        i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10000),row(2,"position:p",5500,"stock")};
        QJsonObject settings{{"manual",true},{"value","5000"},{"valuedAt",imported.toString(Qt::ISODateWithMs)}};
        i.positionHistory={QVariantMap{{"sequence",1},{"position_id","p"},{"instrument_id","stock"},{"account_id","a"},{"currency","RUB"},
            {"event","opening"},{"recorded_at",imported.toMSecsSinceEpoch()},{"quantity_micros",1000000},
            {"state_json",QString::fromUtf8(QJsonDocument(QJsonObject{{"symbol","ABC"},{"settings",settings}}).toJson())}}};
        const auto r=calculate(i);QCOMPARE(r.value("externalFlowMinor").toLongLong(),qint64(5000));
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(500));reconciles(r);
    }
    void payoutGrossNetUnknownChargesAndDepositInterest(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",10500)};
        auto dividend=op("dividend",500);dividend.feeMinor=10;dividend.taxMinor=90;i.operations={dividend};
        auto r=calculate(i);auto p=r.value("payments").toMap();QCOMPARE(p.value("grossMinor").toLongLong(),qint64(600));
        QCOMPARE(p.value("netMinor").toLongLong(),qint64(500));QCOMPARE(p.value("feesMinor").toLongLong(),qint64(10));
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(500));reconciles(r);
        i.operations[0].taxMinor.reset();p=calculate(i).value("payments").toMap();QVERIFY(p.value("grossMinor").isNull());
        QVERIFY(p.value("taxMinor").isNull());QCOMPARE(p.value("netMinor").toLongLong(),qint64(500));
        i.transactions={{"interest","a","deposit_interest",Money(100,Currency::RUB),TransactionType::Income,time(2).addSecs(-1)}};
        p=calculate(i).value("payments").toMap();QCOMPARE(p.value("netMinor").toLongLong(),qint64(600));QCOMPARE(p.value("count").toInt(),2);
        QVERIFY(p.value("feesMinor").isNull());
    }
    void payoutsUsePaymentDateRatesAndScope(){
        auto i=input();i.accounts[0].currency="USD";i.accounts.append({"b","Другой","RUB",0,0});
        i.rates={QVariantMap{{"currency","USD"},{"rate_date","2025-01-01"},{"source","cbr"},{"observed_at",1},{"rub_per_unit_micros",90000000}},
            QVariantMap{{"currency","USD"},{"rate_date","2025-01-02"},{"source","cbr"},{"observed_at",2},{"rub_per_unit_micros",100000000}}};
        i.valuations={row(1,"cash:a",10000,"","a","USD"),row(1,"cash:b",0,"","b"),row(2,"cash:a",10100,"","a","USD"),row(2,"cash:b",0,"","b")};
        i.operations={op("coupon",100)};auto r=calculate(i);auto p=r.value("payments").toMap();QCOMPARE(p.value("netMinor").toLongLong(),qint64(10000));
        QCOMPARE(contribution(r,"instrument:stock").value("amountMinor").toLongLong(),qint64(9000));reconciles(r);
        i.accountId="b";QCOMPARE(calculate(i).value("payments").toMap().value("count").toInt(),0);
        i.accountId="a";i.rates.clear();p=calculate(i).value("payments").toMap();QVERIFY(p.value("netMinor").isNull());
        QCOMPARE(p.value("events").toList()[0].toMap().value("nativeNetMinor").toLongLong(),qint64(100));
    }
    void monthlyGroupingAndDateFilterDoNotRequireSnapshots(){
        auto i=input();i.valuations.clear();i.now=QDateTime(QDate(2025,2,3),QTime(12,0),QTimeZone::UTC);i.to=QDate(2025,2,2);
        auto a=op("dividend",500,"stock","one"),b=op("coupon",300,"bond","two"),outside=op("coupon",999,"bond","old");
        b.occurredAtUtc=QDateTime(QDate(2025,2,2),QTime(11,0),QTimeZone::UTC);outside.occurredAtUtc=time(1);i.operations={b,outside,a};
        const auto p=calculate(i).value("payments").toMap();QCOMPARE(p.value("count").toInt(),2);QCOMPARE(p.value("months").toList().size(),2);
        QCOMPARE(find(p.value("months").toList(),"2025-01").value("netMinor").toLongLong(),qint64(500));
        QCOMPARE(find(p.value("assets").toList(),"instrument:bond").value("netMinor").toLongLong(),qint64(300));
        QCOMPARE(p.value("events").toList()[0].toMap().value("id").toString(),QString("two"));
    }
    void dataGapAndResidualRemainVisible(){
        auto i=input();i.valuations={row(1,"cash:a",10000),row(2,"cash:a",11000)};auto r=calculate(i);
        QCOMPARE(r.value("contributions").toMap().value("unexplainedMinor").toLongLong(),qint64(1000));reconciles(r);
        auto invalid=row(1,"cash:a",10000);invalid["issues"]="history_changed";i.valuations[0]=invalid;
        QVERIFY(!calculate(i).value("contributions").toMap().value("available").toBool());
        i.valuations={row(2,"cash:a",10000)};QVERIFY(!calculate(i).value("contributions").toMap().value("available").toBool());
    }
    void aggregationOverflowIsUnknown(){
        auto i=input();i.valuations.clear();i.operations={op("dividend",std::numeric_limits<qint64>::max(),"stock","one"),op("coupon",1,"stock","two")};
        const auto p=calculate(i).value("payments").toMap();QVERIFY(p.value("netMinor").isNull());QVERIFY(p.value("grossMinor").isNull());
    }
};
QTEST_GUILESS_MAIN(InvestmentStructureIncomeTest)
#include "InvestmentStructureIncomeTest.moc"
