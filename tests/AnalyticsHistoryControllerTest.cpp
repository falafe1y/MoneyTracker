#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>

class AnalyticsHistoryControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);}
    void capturesAllAssetsEvenWithUnknownCrypto(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));QVERIFY(r.saveManualCurrencyRates(1,100,120,14));
         QVERIFY(r.insertAccount({"fiat","Доллары",AssetType::Fiat,AccountType::DebitCard,Currency::USD,12345}));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,100000}));
         InvestmentPositionSettings settings;settings.manualValuation=true;settings.manualValueMinor=30000;settings.valuedAtUtc=QDateTime::currentDateTimeUtc();
         QVERIFY(r.saveInvestmentBundle({"stock","TEST","","Акция",InvestmentInstrumentType::Stock,Currency::RUB},
            {"position","broker","stock",3'000'000,100'000'000,{},{},settings},{},false,{},30100));
         QVERIFY(r.insertCryptoWallet({"wallet","address","BITCOIN","BTC",8}));}
        {FinanceController c(nullptr,path);c.setAppCurrency("EUR");}
        FinanceRepository r(path);const auto fiat=r.loadDailyValuations(AssetDailyValuation::Fiat);
        bool found=false;for(const auto& row:fiat)if(row.toMap().value("source_id").toString()=="cash:fiat"){
            found=true;QCOMPARE(row.toMap().value("currency").toString(),QString("USD"));
            QCOMPARE(row.toMap().value("value_minor").toLongLong(),qint64(12345));
        }
        QVERIFY(found);
        const auto crypto=r.loadDailyValuations(AssetDailyValuation::Crypto);QCOMPARE(crypto.size(),1);
        QVERIFY(crypto.at(0).toMap().value("value_minor").isNull());
        const auto investments=r.loadDailyValuations(AssetDailyValuation::Investment);QCOMPARE(investments.size(),2);
        for(const auto& row:investments)if(row.toMap().value("source_id").toString()=="position:position"){
            QCOMPARE(row.toMap().value("value_minor").toLongLong(),qint64(30000));QCOMPARE(row.toMap().value("cost_basis_minor").toLongLong(),qint64(30100));
        }
    }
    void manualCostCanBeClearedWithoutChangingQuantity(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));QVERIFY(r.saveManualCurrencyRates(1,100,120,14));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,100000}));}
        FinanceController c(nullptr,path);c.setSelectedAsset("investment");
        auto result=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},{"name","Акция"},{"symbol","TEST"},
            {"quantity","1"},{"manualValuation",true},{"manualValue","110"},{"costBasis","100"}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
        const auto p=c.investmentPositions().at(0).toMap();QVERIFY(p.value("costBasisKnown").toBool());
        result=c.saveInvestmentPosition({{"id",p.value("id")},{"accountId","broker"},{"searchIndex",-1},{"quantity","1"},
            {"manualValuation",true},{"manualValue","110"},{"costBasis","90071992547409.93"}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
        QCOMPARE(c.investmentPositions().at(0).toMap().value("costBasisMinor").toLongLong(),9'007'199'254'740'993LL);

        result=c.saveInvestmentPosition({{"id",p.value("id")},{"accountId","broker"},{"searchIndex",-1},{"quantity","1"},
            {"manualValuation",true},{"manualValue","110"},{"costBasis",""}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
        QVERIFY(!c.investmentPositions().at(0).toMap().value("costBasisKnown").toBool());
    }
    void negativeFutureClearingPriceIsPreserved(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db"),at=QDateTime::currentDateTimeUtc().addSecs(-5).toString(Qt::ISODateWithMs);
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));QVERIFY(r.saveManualCurrencyRates(1,100,120,14));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,200000}));
         InvestmentTerms t;t.pricing="future";t.currencyCode="RUB";t.priceStepMicros=1'000'000;t.stepValueMicros=10'000'000;
         InvestmentPositionSettings settings;settings.referencePriceMicros=100'000'000;settings.referenceAtUtc=QDateTime::currentDateTimeUtc().addSecs(-3600);
         QVERIFY(r.saveInvestmentBundle({"future","FUT","","Фьючерс",InvestmentInstrumentType::Future,Currency::RUB,"","",t},
            {"p","broker","future",1'000'000,100'000'000,{},{},settings},InvestmentQuote("future",-10'000'000,QDateTime::fromString(at,Qt::ISODate),t),false));}
        {FinanceController c(nullptr,path);c.setSelectedAsset("investment");
         auto result=c.recordInvestmentOperation({{"positionId","p"},{"kind","margin"},{"date",at},{"cashAmount","-1100"},{"price","-10"}});
         QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));}
        FinanceRepository r(path);QCOMPARE(r.loadInvestmentOperations().at(0).tradePriceMicros.value_or(0),qint64(-10'000'000));
    }
    void chargesAreSavedOnceAndEmptyMeansUnknown(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));QVERIFY(r.saveManualCurrencyRates(1,100,120,14));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,100000}));}
        {FinanceController c(nullptr,path);c.setSelectedAsset("investment");
         auto result=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},{"name","Акция"},{"symbol","TEST"},
            {"quantity","3"},{"averagePrice","100"},{"manualValuation",true},{"manualValue","300"},
            {"purchase",true},{"cashAmount","-300"},{"fee","1"},{"tax","0"}});
         QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
         const auto p=c.investmentPositions().at(0).toMap();QCOMPARE(p.value("costBasisMinor").toLongLong(),qint64(30100));
         result=c.recordInvestmentOperation({{"positionId",p.value("id")},{"kind","sell"},{"date",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"quantity","1"},{"cashAmount","110"},{"price","110"},{"remainingValue","220"},{"fee","1"},{"tax",""}});
         QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));}
        FinanceRepository r(path);const auto ops=r.loadInvestmentOperations();QCOMPARE(ops.size(),4);
        for(const auto& op:ops){if(op.kind=="buy")QCOMPARE(op.cashDeltaMinor,qint64(-30'000));if(op.kind=="sell")QVERIFY(!op.feeMinor);if(op.kind=="fee")QCOMPARE(op.cashDeltaMinor,qint64(-100));}
        const auto history=r.loadPositionHistory(r.loadInvestmentPositions().at(0).id());
        QCOMPARE(history.last().toMap().value("realized_net_minor").toLongLong(),qint64(867));
        for(const auto& value:r.loadDailyValuations(AssetDailyValuation::Investment))if(value.toMap().value("source_id").toString()=="cash:broker")
            QCOMPARE(value.toMap().value("value_minor").toLongLong(),qint64(80800));
    }
};
QTEST_GUILESS_MAIN(AnalyticsHistoryControllerTest)
#include "AnalyticsHistoryControllerTest.moc"
