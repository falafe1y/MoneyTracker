#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>

class InvestmentControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void clearingKeepsCapitalAndSurvivesRestart();
    void bondCouponAndTradesPreserveCapital();
    void shortOptionIsAnObligation();
    void foreignCurrencyAndMissingRateAreExplicit();
    void manuallyAddsEveryAssetClass();
};
static qint64 balance(FinanceController& c) {
    c.setSelectedAsset("investment");for(const auto& item:c.accounts())if(item.toMap().value("id").toString()=="broker")return item.toMap().value("balanceMinor").toLongLong();
    return -1;
}
static bool seed(FinanceRepository& r) {
    return r.isOpen() && r.saveAutomaticCurrencyRates(false) && r.saveManualCurrencyRates(1,100,120) &&
        r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10'000'000});
}
void InvestmentControllerTest::clearingKeepsCapitalAndSurvivesRestart() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="futures";t.market="forts";t.pricing="future";t.currencyCode="RUB";t.priceStepMicros=1'000'000;t.stepValueMicros=10'000'000;
     InvestmentPositionSettings s;s.referencePriceMicros=100'000'000;s.referenceAtUtc=at.addSecs(-3600);s.blockedMarginMinor=2'000'000;
     QVERIFY(r.saveInvestmentBundle({"future","SiZ6","","Фьючерс",InvestmentInstrumentType::Future,Currency::RUB,"MOEX","RFUD",t},
       {"position","broker","future",2'000'000,100'000'000,{},{},s},InvestmentQuote("future",110'000'000,at,t),false));}
    {FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'020'000));
     const auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","margin"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","200"},{"price","110"}});
     QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'020'000));
     QVERIFY(!c.investmentValuationIncomplete());QCOMPARE(c.investmentOperations().size(),1);}
    FinanceController restored(nullptr,path);QCOMPARE(balance(restored),qint64(10'020'000));
    QCOMPARE(restored.investmentPositions().at(0).toMap().value("marketValueMinor").toLongLong(),qint64(0));
}
void InvestmentControllerTest::bondCouponAndTradesPreserveCapital() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="stock";t.market="bonds";t.pricing="bond";t.currencyCode="RUB";t.faceValueMicros=1'000'000'000;t.accruedInterestMicros=20'000'000;
     QVERIFY(r.saveInvestmentBundle({"bond","BOND","","Облигация",InvestmentInstrumentType::Bond,Currency::RUB,"MOEX","TQCB",t},
       {"position","broker","bond",10'000'000,95'000'000},InvestmentQuote("bond",95'000'000,at,t),false));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'970'000));
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","coupon"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","200"},{"accruedAfter","0"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","buy"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","-4750"},{"quantity","5"},{"price","95"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","4750"},{"quantity","5"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
}
void InvestmentControllerTest::shortOptionIsAnObligation() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="futures";t.market="options";t.pricing="premium_option";t.currencyCode="RUB";t.multiplierMicros=100'000'000;
     InvestmentPositionSettings s;s.direction=-1;
     InvestmentOperation opening{"opening","broker","position","option","buy",100'000,1'000'000,at,"Продажа опциона"};
     QVERIFY(r.saveInvestmentBundle({"option","OPT","","Опцион",InvestmentInstrumentType::Option,Currency::RUB,"MOEX","ROPD",t},
       {"position","broker","option",1'000'000,10'000'000,{},{},s},InvestmentQuote("option",10'000'000,at,t),false,opening));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'000'000));
    const auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","-1000"},{"quantity","1"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'000'000));QVERIFY(c.investmentPositions().isEmpty());
}
void InvestmentControllerTest::foreignCurrencyAndMissingRateAreExplicit() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="stock";t.market="shares";t.currencyCode="USD";
     QVERIFY(r.saveInvestmentBundle({"foreign","USDSTOCK","","Акция",InvestmentInstrumentType::Stock,Currency::USD,"MOEX","TQTF",t},
       {"foreign-position","broker","foreign",10'000'000,1'000'000},InvestmentQuote("foreign",1'000'000,QDateTime::currentDateTimeUtc(),t),false));
     t.currencyCode="XYZ";QVERIFY(r.saveInvestmentBundle({"unknown","UNKNOWN","","Акция",InvestmentInstrumentType::Stock,Currency::RUB,"MOEX","TQTF",t},
       {"unknown-position","broker","unknown",1'000'000,1'000'000},InvestmentQuote("unknown",1'000'000,QDateTime::currentDateTimeUtc(),t),false));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'100'000));QVERIFY(c.investmentValuationIncomplete());
    const auto result=c.saveInvestmentPosition({{"id","unknown-position"},{"accountId","broker"},{"searchIndex",-1},{"quantity","1"},
        {"manualValuation",true},{"manualValue","500"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'150'000));QVERIFY(!c.investmentValuationIncomplete());
}
void InvestmentControllerTest::manuallyAddsEveryAssetClass() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");{FinanceRepository r(path);QVERIFY(seed(r));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    for(int type=0;type<=10;++type){const auto result=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},
        {"instrumentType",type},{"name",QStringLiteral("Инструмент %1").arg(type)},{"symbol",QStringLiteral("ASSET%1").arg(type)},
        {"quantity","1"},{"manualValuation",true},{"manualValue","100"}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));}
    QCOMPARE(c.investmentPositions().size(),11);QCOMPARE(balance(c),qint64(10'110'000));
}
QTEST_GUILESS_MAIN(InvestmentControllerTest)
#include "InvestmentControllerTest.moc"
