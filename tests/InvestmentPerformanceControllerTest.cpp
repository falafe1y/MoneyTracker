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
    void depositSettingsPersistAndComparisonFollowsSelectedScope(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto today=QDate::currentDate();
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.insertAccount({"a","Первый",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));
            QVERIFY(r.insertAccount({"b","Второй",AssetType::Investment,AccountType::Brokerage,Currency::RUB,20000}));
            for(int offset:{2,1}){
                DailyValuationBatch batch;batch.date=today.addDays(-offset);batch.observedAtUtc=QDateTime(batch.date,QTime(12,0)).toUTC();
                for(const auto& id:{QStringLiteral("a"),QStringLiteral("b")}){
                    AssetDailyValuation row;row.kind=AssetDailyValuation::Investment;row.sourceId="cash:"+id;row.accountId=id;row.currency="RUB";
                    row.valueMinor=id=="a"?(offset==2?10000:11000):20000;row.valuedAtUtc=batch.observedAtUtc;row.source="ledger";batch.assets.append(row);
                }
                QVERIFY(r.saveDailyValuations(batch));
            }
        }
        {
            FinanceController c(nullptr,path);
            QVERIFY(c.setPerformanceDeposit("36,5","monthly"));
            QCOMPARE(c.performanceDeposit().value("rate").toString(),QString("36.5000"));
            for(const auto& bad:{"-1","NaN","1001","1.23456","1e2"})QVERIFY(!c.setPerformanceDeposit(bad,"monthly"));
            QVERIFY(!c.setPerformanceDeposit("10","invalid"));
            c.setDateFilter(QDateTime(today.addDays(-1),QTime(0,0)),QDateTime(today.addDays(-1),QTime(23,59)));
            auto report=c.investmentPerformance().value("depositComparison").toMap();
            QVERIFY2(report.value("available").toBool(),qPrintable(report.value("reason").toString()));
            QCOMPARE(report.value("portfolioGainMinor").toLongLong(),qint64(1000));
            c.setPerformanceAccount("a");report=c.investmentPerformance().value("depositComparison").toMap();
            QCOMPARE(report.value("openingMinor").toLongLong(),qint64(10000));
            c.setPerformanceAccount("b");report=c.investmentPerformance().value("depositComparison").toMap();
            QCOMPARE(report.value("openingMinor").toLongLong(),qint64(20000));
            QCOMPARE(report.value("portfolioGainMinor").toLongLong(),qint64(0));
        }
        FinanceController c(nullptr,path);QCOMPARE(c.performanceDeposit().value("capitalization").toString(),QString("monthly"));
        QVERIFY(c.setPerformanceDeposit("0","daily"));QCOMPARE(c.performanceDeposit().value("rate").toString(),QString("0.0000"));
        QVERIFY(c.setPerformanceDeposit("","none"));QVERIFY(!c.investmentPerformance().value("depositComparison").toMap().value("configured").toBool());
    }
    void restoreAndClearRefreshReport(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db"),saved=dir.filePath("saved.db");
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,12345}));
        }
        FinanceController c(nullptr,path);QVERIFY(c.setPerformanceInflation("4.5"));QVERIFY(c.setPerformanceDeposit("16","daily"));
        QVERIFY(c.backupDatabase(QUrl::fromLocalFile(saved)).value("ok").toBool());
        QVERIFY(c.setPerformanceInflation("9"));QVERIFY(c.setPerformanceDeposit("10","none"));QVERIFY(c.restoreDatabase(QUrl::fromLocalFile(saved)).value("ok").toBool());
        QCOMPARE(c.performanceDeposit().value("rate").toString(),QString("16.0000"));QCOMPARE(c.performanceDeposit().value("capitalization").toString(),QString("daily"));
        QCOMPARE(c.performanceInflation(),QString("4.5000"));QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(12345));
        c.setPerformanceAccount("broker");QVERIFY(c.clearAllData().value("ok").toBool());
        QVERIFY(c.performanceAccountId().isEmpty());QVERIFY(c.performanceAccounts().isEmpty());QVERIFY(c.performanceInflation().isEmpty());QVERIFY(c.investmentPerformance().value("closingMinor").isNull());
        QVERIFY(c.performanceDeposit().isEmpty());
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
    void groupsKeepBrokersAndDepositsApart(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
         QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));
         QVERIFY(r.insertAccount({"deposit","Вклад",AssetType::Investment,AccountType::Deposit,Currency::RUB,20000}));
         QVERIFY(r.insertAccount({"fiat","Фиат",AssetType::Fiat,AccountType::Cash,Currency::RUB,30000}));}
        FinanceController c(nullptr,path);
        QCOMPARE(c.investmentPerformance().value("accountCount").toInt(),2);
        // Brokerage accounts and deposits are two portfolios; the fiat account is in neither
        c.setPerformanceAccount("group:brokerage");QCOMPARE(c.performanceAccountId(),QString("group:brokerage"));
        QCOMPARE(c.investmentPerformance().value("accountCount").toInt(),1);
        QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(10000));
        c.setPerformanceAccount("group:deposit");QCOMPARE(c.performanceAccountId(),QString("group:deposit"));
        QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(20000));
        // A group is chosen by its key only: an unknown group and a fiat account are refused
        c.setPerformanceAccount("group:fiat");QCOMPARE(c.performanceAccountId(),QString("group:deposit"));
        c.setPerformanceAccount("fiat");QCOMPARE(c.performanceAccountId(),QString("group:deposit"));
        c.setPerformanceAccount("");QCOMPARE(c.investmentPerformance().value("accountCount").toInt(),2);
    }
    void depositCommissionLowersDepositValue(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));
         QVERIFY(r.insertAccount({"deposit","Вклад",AssetType::Investment,AccountType::Deposit,Currency::RUB,20000}));}
        {FinanceController c(nullptr,path);c.setSelectedAsset("investment");
         QVERIFY(c.addExpense(100,QStringLiteral("Комиссия банка"),QStringLiteral("bank_fee"),QStringLiteral("RUB"),QStringLiteral("deposit"),QDateTime::currentDateTimeUtc()));}
        FinanceController c(nullptr,path);
        // The commission lowers the deposit value; the flow rule itself is checked on the calculator
        QCOMPARE(c.investmentPerformance().value("closingMinor").toLongLong(),qint64(19900));
    }
};
QTEST_GUILESS_MAIN(InvestmentPerformanceControllerTest)
#include "InvestmentPerformanceControllerTest.moc"
