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
