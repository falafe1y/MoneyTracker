#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QtTest>

class InvestmentPerformanceControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);}
    void manualAveragePriceFeedsSnapshotAndOpenProfit(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));}
        FinanceController c(nullptr,path);
        c.setSelectedAsset("investment");
        QVariantMap input{{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
            {"symbol","TEST"},{"name","Акция"},{"quantity","3"},{"averagePrice","100"},
            {"manualValue","360"},{"costBasis",""}};
        const auto saved=c.saveInvestmentPosition(input);QVERIFY2(saved.value("ok").toBool(),qPrintable(saved.value("error").toString()));
        const auto report=c.investmentPerformance();
        QCOMPARE(report.value("unrealizedMinor").toLongLong(),qint64(6000));
        QVERIFY(!report.value("unrealizedMinor").isNull());
        for(const auto& v:report.value("dataQuality").toMap().value("items").toList())
            QVERIFY(v.toMap().value("code")!="cost_unknown");
        FinanceRepository r(path);QVERIFY(r.loadInvestmentOperations().isEmpty());
        const auto id=r.loadInvestmentPositions().first().id();
        QCOMPARE(r.investmentCostBasis(id).toLongLong(),qint64(30000));
        const auto rows=r.loadDailyValuations(AssetDailyValuation::Investment);
        bool found=false;for(const auto& v:rows){const auto row=v.toMap();if(row.value("source_id")=="position:"+id){
            found=true;QCOMPARE(row.value("cost_basis_minor").toLongLong(),qint64(30000));}}
        QVERIFY(found);
    }
    void accountsFilterAndInflationSetting(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));
            QVERIFY(r.insertAccount({"deposit","Вклад",AssetType::Investment,AccountType::Deposit,Currency::RUB,20000}));
            QVERIFY(r.insertAccount({"fiat","Фиат",AssetType::Fiat,AccountType::Cash,Currency::RUB,30000}));
        }
        {
            FinanceController c(nullptr,path);QCOMPARE(c.performanceAccounts().size(),2);QCOMPARE(c.investmentPerformance().value("accountCount").toInt(),2);
            QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(30000));
            QVERIFY(c.investmentPerformance().value("periodPercent").isNull());
            QSignalSpy changes(&c,&FinanceController::performanceChanged);
            c.setPerformanceAccount("deposit");QCOMPARE(c.performanceAccountId(),QString("deposit"));QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(20000));
            c.setPerformanceAccount("fiat");QCOMPARE(c.performanceAccountId(),QString("deposit"));
            QVERIFY(c.setPerformanceInflation("6,25"));QCOMPARE(c.performanceInflation(),QString("6.2500"));
            QVERIFY(!c.setPerformanceInflation("-100"));QVERIFY(!c.setPerformanceInflation("1001"));QVERIFY(!c.setPerformanceInflation("NaN"));QVERIFY(!c.setPerformanceInflation("6.12345"));
            QVERIFY(changes.count()>=2);QCOMPARE(c.performanceInflation(),QString("6.2500"));
            c.setDateFilter(QDateTime(QDate(2020,1,1),QTime(0,0)),QDateTime(QDate(2020,1,2),QTime(0,0)));QVERIFY(c.investmentPerformance().value("closingMinor").isNull());
            c.clearDateFilter();QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(20000));
        }
        {
            FinanceController c(nullptr,path);QCOMPARE(c.performanceInflation(),QString("6.2500"));QVERIFY(c.setPerformanceInflation(""));QVERIFY(c.performanceInflation().isEmpty());
        }
        FinanceRepository r(path);QVERIFY(r.loadPerformanceInflation().isEmpty());
    }
    void restoreAndClearRefreshReport(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db"),saved=dir.filePath("saved.db");
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,12345}));
        }
        FinanceController c(nullptr,path);QVERIFY(c.setPerformanceInflation("4.5"));
        QVERIFY(c.backupDatabase(QUrl::fromLocalFile(saved)).value("ok").toBool());
        QVERIFY(c.setPerformanceInflation("9"));QVERIFY(c.restoreDatabase(QUrl::fromLocalFile(saved)).value("ok").toBool());
        QCOMPARE(c.performanceInflation(),QString("4.5000"));QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(12345));
        c.setPerformanceAccount("broker");QVERIFY(c.clearAllData().value("ok").toBool());
        QVERIFY(c.performanceAccountId().isEmpty());QVERIFY(c.performanceAccounts().isEmpty());QVERIFY(c.performanceInflation().isEmpty());QVERIFY(c.investmentPerformance().value("closingMinor").isNull());
    }
    void structureAndPaymentsFollowAccountAndDateSelection(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        const auto now=QDateTime::currentDateTimeUtc();
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));
            QVERIFY(r.insertAccount({"deposit","Вклад",AssetType::Investment,AccountType::Deposit,Currency::RUB,20000}));
            QVERIFY(r.insertTransaction({"interest","deposit","deposit_interest",Money(100,Currency::RUB),TransactionType::Income,now}));
        }
        FinanceController c(nullptr,path);
        QCOMPARE(c.investmentPerformance().value("allocation").toMap().value("capitalMinor").toLongLong(),qint64(30100));
        QCOMPARE(c.investmentPerformance().value("payments").toMap().value("netMinor").toLongLong(),qint64(100));
        c.setPerformanceAccount("broker");
        QCOMPARE(c.investmentPerformance().value("allocation").toMap().value("capitalMinor").toLongLong(),qint64(10000));
        QCOMPARE(c.investmentPerformance().value("payments").toMap().value("count").toInt(),0);
        c.setPerformanceAccount("deposit");
        QCOMPARE(c.investmentPerformance().value("payments").toMap().value("count").toInt(),1);
        const auto yesterday=now.toLocalTime().date().addDays(-1);
        c.setDateFilter(QDateTime(yesterday,QTime(0,0)),QDateTime(yesterday,QTime(23,59)));
        QVERIFY(!c.investmentPerformance().value("allocation").toMap().value("available").toBool());
        QCOMPARE(c.investmentPerformance().value("payments").toMap().value("count").toInt(),0);
        c.clearDateFilter();
        QCOMPARE(c.investmentPerformance().value("allocation").toMap().value("capitalMinor").toLongLong(),qint64(20100));
        QCOMPARE(c.investmentPerformance().value("payments").toMap().value("count").toInt(),1);
    }
};
QTEST_GUILESS_MAIN(InvestmentPerformanceControllerTest)
#include "InvestmentPerformanceControllerTest.moc"
