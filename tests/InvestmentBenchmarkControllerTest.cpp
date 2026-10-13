#include "../interface/FinanceController.h"
#include "../services/InvestmentBenchmark.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QtTest>
#include <cmath>

class InvestmentBenchmarkControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);}
    void historyDownloadDoesNotRequirePortfolioObservations(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto today=QDate::currentDate();
        {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));}
        FinanceController c(nullptr,path);QVERIFY(!c.benchmarkComparison().value("canRefresh").toBool());
        c.selectBenchmarkIndex("index:IMOEX");auto comparison=c.benchmarkComparison();
        QVERIFY(comparison.value("canRefresh").toBool());QVERIFY(!comparison.value("available").toBool());
        QCOMPARE(comparison.value("historyTo").toString(),today.addDays(-1).toString(Qt::ISODate));
        QCOMPARE(comparison.value("historyFrom").toString(),today.addDays(-1).addYears(-1).toString(Qt::ISODate));
        c.refreshBenchmark();QVERIFY(c.benchmarkBusy());
        c.setPerformanceAccount("");QVERIFY(c.benchmarkBusy()); // A recalculation without snapshots must not cancel the request.
        c.selectBenchmarkIndex("");QVERIFY(!c.benchmarkBusy());
        c.setDateFilter(QDateTime(today.addDays(-20),QTime(0,0)),QDateTime(today.addDays(-5),QTime(23,59)));
        c.selectBenchmarkIndex("index:IMOEX");comparison=c.benchmarkComparison();QVERIFY(comparison.value("canRefresh").toBool());
        QCOMPARE(comparison.value("historyFrom").toString(),today.addDays(-20).toString(Qt::ISODate));
        QCOMPARE(comparison.value("historyTo").toString(),today.addDays(-5).toString(Qt::ISODate));
        c.setDateFilter(QDateTime(today,QTime(0,0)),QDateTime(today,QTime(23,59)));
        QVERIFY(c.benchmarkComparison().value("canRefresh").toBool());QVERIFY(c.benchmarkComparison().value("historyFallback").toBool());
        QCOMPARE(c.benchmarkComparison().value("historyTo").toString(),today.addDays(-1).toString(Qt::ISODate));
        c.refreshBenchmark();QVERIFY(c.benchmarkBusy());c.selectBenchmarkIndex("");QVERIFY(!c.benchmarkBusy());
    }
    void cachedComparisonAndScopeChanges(){
        QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto today=QDate::currentDate();const auto from=today.addDays(-3),to=today.addDays(-1);
        {
            FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false));QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10000}));
            for(const auto& name:QSqlDatabase::connectionNames()){auto db=QSqlDatabase::database(name);if(db.databaseName()==path){QSqlQuery q(db);q.prepare("UPDATE accounts SET created_at=? WHERE id='broker'");q.addBindValue(QDateTime(from.addDays(-1),QTime(0,0)).toMSecsSinceEpoch());QVERIFY(q.exec());}}
            QVariantList prices;const qint64 amounts[]{10000,11000,12100};
            for(int i=0;i<3;++i){DailyValuationBatch b;b.date=from.addDays(i);b.observedAtUtc=QDateTime(b.date,QTime(12,0)).toUTC();
                AssetDailyValuation v;v.kind=AssetDailyValuation::Investment;v.sourceId="cash:broker";v.accountId="broker";v.currency="RUB";v.valueMinor=amounts[i];v.valuedAtUtc=b.observedAtUtc;v.source="ledger";b.assets={v};QVERIFY(r.saveDailyValuations(b));
                prices.append(QVariantMap{{"price_date",b.date.toString(Qt::ISODate)},{"price_micros",amounts[i]*10000},{"currency","RUB"},{"issues",""}});
            }
            QVERIFY(r.saveBenchmarkHistory(investmentBenchmarkIndices().first().toMap(),prices,from.addDays(-14),to));
        }
        FinanceController c(nullptr,path);QVERIFY(!c.benchmarkBusy());const auto comparison=c.benchmarkComparison();
        QVERIFY2(comparison.value("available").toBool(),qPrintable(comparison.value("reason").toString()));QCOMPARE(comparison.value("to").toString(),to.toString(Qt::ISODate));
        QVERIFY(std::abs(comparison.value("portfolioPercent").toDouble()-21)<1e-8);QVERIFY(std::abs(comparison.value("differencePoints").toDouble())<1e-8);
        c.setPerformanceAccount("broker");QVERIFY(c.benchmarkComparison().value("available").toBool());
        c.setAppCurrency("USD");QVERIFY(!c.benchmarkComparison().value("available").toBool());c.setAppCurrency("RUB");QVERIFY(c.benchmarkComparison().value("available").toBool());
        const auto backup=dir.filePath("saved.db");QVERIFY(c.backupDatabase(QUrl::fromLocalFile(backup)).value("ok").toBool());
        c.selectBenchmarkIndex("");QVERIFY(c.benchmarkSelection().isEmpty());QVERIFY(c.restoreDatabase(QUrl::fromLocalFile(backup)).value("ok").toBool());
        QVERIFY(c.benchmarkComparison().value("available").toBool());QVERIFY(c.clearAllData().value("ok").toBool());QVERIFY(c.benchmarkSelection().isEmpty());QVERIFY(!c.benchmarkComparison().value("available").toBool());
        FinanceRepository r(path);QVERIFY(r.loadBenchmarkPrices("index:IMOEX").isEmpty());
    }
};
QTEST_GUILESS_MAIN(InvestmentBenchmarkControllerTest)
#include "InvestmentBenchmarkControllerTest.moc"
