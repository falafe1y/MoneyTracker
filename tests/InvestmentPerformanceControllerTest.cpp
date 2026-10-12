#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QtTest>

class InvestmentPerformanceControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);}
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
};
QTEST_GUILESS_MAIN(InvestmentPerformanceControllerTest)
#include "InvestmentPerformanceControllerTest.moc"
