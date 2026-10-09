#include "../persistence/FinanceRepository.h"
#include "../services/InvestmentValuation.h"
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QtTest>

class InvestmentAccountingRepositoryTest : public QObject {
    Q_OBJECT
private slots:
    void openingTradeAndClearingAreAtomic();
    void failureRollsBackPositionAndCash();
    void migratesExistingPositionsWithoutLosingData();
    void backupRestoreKeepsTermsOperationsAndCurrentValues();
    void operationCurrencyAndZeroOptionQuotesAreProtected();
};
static InvestmentTerms futureTerms() {
    InvestmentTerms t;t.engine="futures";t.market="forts";t.pricing="future";t.currencyCode="RUB";
    t.priceStepMicros=1'000'000;t.stepValueMicros=10'000'000;return t;
}
static Account broker() {return {"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10'000'000};}
static InvestmentInstrument future() {return {"future","SiZ6","","Фьючерс",InvestmentInstrumentType::Future,Currency::RUB,"MOEX","RFUD",futureTerms()};}
static InvestmentPosition pos(qint64 qty=2'000'000) {
    InvestmentPositionSettings s;s.referencePriceMicros=100'000'000;s.referenceAtUtc=QDateTime::currentDateTimeUtc().addSecs(-3600);s.blockedMarginMinor=2'000'000;
    return {"position","broker","future",qty,100'000'000,{},{},s};
}
void InvestmentAccountingRepositoryTest::openingTradeAndClearingAreAtomic() {
    QTemporaryDir dir;FinanceRepository r(dir.filePath("data.db"));QVERIFY2(r.isOpen(),qPrintable(r.lastError()));QVERIFY(r.insertAccount(broker()));
    const auto p=pos();auto at=QDateTime::currentDateTimeUtc();InvestmentQuote q("future",110'000'000,at,futureTerms());
    InvestmentOperation op{"opening","broker","position","future","buy",-100,2'000'000,at.addSecs(-300),"Комиссия открытия"};
    QVERIFY2(r.saveInvestmentBundle(future(),p,q,false,op),qPrintable(r.lastError()));
    QCOMPARE(r.loadInvestmentOperations().size(),1);QCOMPARE(r.loadInvestmentPositions().at(0).settings().blockedMarginMinor,qint64(2'000'000));
    auto settings=r.loadInvestmentPositions().at(0).settings();const auto previous=r.loadInvestmentPositions().at(0);
    const auto value=valueInvestmentPosition(previous,&q,q.terms(),1'000'000);QCOMPARE(value.valueMinor,qint64(20'000));
    settings.referencePriceMicros=q.priceMicros();settings.referenceAtUtc=at;
    InvestmentPosition next(previous.id(),previous.accountId(),previous.instrumentId(),previous.quantityMicros(),previous.averagePriceMicros(),{},{},settings);
    InvestmentOperation clearing{"clearing","broker","position","future","margin",20'000,0,at,"Расчёт"};
    QVERIFY2(r.bookInvestmentOperation(clearing,previous,next,false),qPrintable(r.lastError()));
    QCOMPARE(valueInvestmentPosition(r.loadInvestmentPositions().at(0),&q,q.terms(),1'000'000).valueMinor,qint64(0));
    qint64 cash=broker().initialBalanceMinor();for(const auto& operation:r.loadInvestmentOperations())cash+=operation.cashDeltaMinor;
    QCOMPARE(cash,qint64(10'019'900));
}
void InvestmentAccountingRepositoryTest::failureRollsBackPositionAndCash() {
    QTemporaryDir dir;FinanceRepository r(dir.filePath("data.db"));QVERIFY(r.insertAccount(broker()));
    const auto p=pos();InvestmentQuote q("future",110'000'000,QDateTime::currentDateTimeUtc(),futureTerms());
    QVERIFY(r.saveInvestmentBundle(future(),p,q,false));const auto before=r.loadInvestmentPositions().at(0);
    InvestmentPosition next(before.id(),before.accountId(),before.instrumentId(),1'000'000,before.averagePriceMicros(),{},{},before.settings());
    InvestmentOperation bad{"bad","broker","position","future","unsupported",100,0,QDateTime::currentDateTimeUtc(),""};
    QVERIFY(!r.bookInvestmentOperation(bad,before,next,false));QCOMPARE(r.loadInvestmentPositions().at(0).quantityMicros(),before.quantityMicros());
    QVERIFY(r.loadInvestmentOperations().isEmpty());
    InvestmentOperation wrongRelation=bad;wrongRelation.kind="sell";wrongRelation.accountId="wrong";
    QVERIFY(!r.bookInvestmentOperation(wrongRelation,before,next,false));
    QCOMPARE(r.loadInvestmentPositions().at(0).quantityMicros(),before.quantityMicros());
}
void InvestmentAccountingRepositoryTest::migratesExistingPositionsWithoutLosingData() {
    QTemporaryDir dir;const auto path=dir.filePath("old.db");
    {FinanceRepository r(path);QVERIFY(r.insertAccount(broker()));
     InvestmentInstrument i("stock","SBER","","Сбер",InvestmentInstrumentType::Stock,Currency::RUB,"MOEX","TQBR");
     QVERIFY(r.insertInvestmentInstrument(i));QVERIFY(r.insertInvestmentPosition({"stock-position","broker","stock",5'000'000,120'000'000}));
     QVERIFY(r.saveInvestmentQuote({"stock",281'440'000,QDateTime::currentDateTimeUtc()}));}
    const auto connection=QStringLiteral("legacy-investment");
    {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(path);QVERIFY(db.open());QSqlQuery q(db);
     const QStringList sql{
        "PRAGMA foreign_keys=OFF",
        "CREATE TABLE legacy_instruments(id TEXT PRIMARY KEY,symbol TEXT NOT NULL,isin TEXT NOT NULL DEFAULT '',name TEXT NOT NULL,type INTEGER CHECK(type IN (0,1,2,3,4)),currency TEXT NOT NULL,market_code TEXT NOT NULL DEFAULT '',primary_board_id TEXT NOT NULL DEFAULT '',is_archived INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL)",
        "INSERT INTO legacy_instruments SELECT id,symbol,isin,name,type,currency,market_code,primary_board_id,is_archived,created_at FROM investment_instruments",
        "DROP TABLE investment_instruments", "ALTER TABLE legacy_instruments RENAME TO investment_instruments",
        "CREATE TABLE legacy_positions(id TEXT PRIMARY KEY,account_id TEXT REFERENCES accounts(id),instrument_id TEXT REFERENCES investment_instruments(id),quantity_micros INTEGER NOT NULL,average_price_micros INTEGER NOT NULL,is_archived INTEGER NOT NULL DEFAULT 0,created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL)",
        "INSERT INTO legacy_positions SELECT id,account_id,instrument_id,quantity_micros,average_price_micros,is_archived,created_at,updated_at FROM investment_positions",
        "DROP TABLE investment_positions", "ALTER TABLE legacy_positions RENAME TO investment_positions",
        "CREATE TABLE legacy_quotes(instrument_id TEXT PRIMARY KEY REFERENCES investment_instruments(id),price_micros INTEGER CHECK(price_micros>0),quoted_at INTEGER NOT NULL)",
        "INSERT INTO legacy_quotes SELECT instrument_id,price_micros,quoted_at FROM investment_quotes",
        "DROP TABLE investment_quotes", "ALTER TABLE legacy_quotes RENAME TO investment_quotes"};
     for(const auto& statement:sql)QVERIFY2(q.exec(statement),qPrintable(q.lastError().text()));q.finish();db.close();}
    QSqlDatabase::removeDatabase(connection);
    FinanceRepository migrated(path);QVERIFY2(migrated.isOpen(),qPrintable(migrated.lastError()));
    QCOMPARE(migrated.loadInvestmentPositions().at(0).quantityMicros(),qint64(5'000'000));
    QCOMPARE(migrated.loadInvestmentQuotes().at(0).priceMicros(),qint64(281'440'000));
    QVERIFY2(migrated.insertInvestmentInstrument(future()),qPrintable(migrated.lastError()));
    QVERIFY(migrated.insertInvestmentPosition(pos()));
}
void InvestmentAccountingRepositoryTest::backupRestoreKeepsTermsOperationsAndCurrentValues() {
    QTemporaryDir dir;const auto copy=dir.filePath("backup.db");
    {FinanceRepository source(dir.filePath("source.db"));QVERIFY(source.insertAccount(broker()));
     const auto p=pos();InvestmentQuote q("future",110'000'000,QDateTime::currentDateTimeUtc(),futureTerms());
     InvestmentOperation op{"opening","broker","position","future","buy",-100,2'000'000,QDateTime::currentDateTimeUtc(),"Открытие"};
     QVERIFY(source.saveInvestmentBundle(future(),p,q,false,op));QVERIFY(source.backupDatabase(copy));}
    FinanceRepository target(dir.filePath("target.db"));QVERIFY(target.insertAccount({"broker","Мой счёт",AssetType::Investment,AccountType::Brokerage,Currency::RUB,777}));
    QVERIFY2(target.restoreDatabase(copy),qPrintable(target.lastError()));
    QCOMPARE(target.loadInvestmentOperations().size(),1);QCOMPARE(target.loadInvestmentInstruments().at(0).terms().pricing,QStringLiteral("future"));
    QCOMPARE(target.loadInvestmentPositions().at(0).settings().blockedMarginMinor,qint64(2'000'000));
    const auto accounts=target.loadAccounts();
    const auto account=std::find_if(accounts.cbegin(),accounts.cend(),[](const auto& a){return a.id()==QStringLiteral("broker");});
    QVERIFY(account!=accounts.cend());QCOMPARE(account->initialBalanceMinor(),qint64(777));
    QVERIFY(target.restoreDatabase(copy));QCOMPARE(target.loadInvestmentOperations().size(),1);
    QVERIFY(target.clearAllUserData());QVERIFY(target.loadInvestmentOperations().isEmpty());
}

void InvestmentAccountingRepositoryTest::operationCurrencyAndZeroOptionQuotesAreProtected() {
    QTemporaryDir dir;FinanceRepository r(dir.filePath("data.db"));QVERIFY(r.insertAccount(broker()));
    InvestmentTerms t;t.engine="futures";t.market="options";t.pricing="premium_option";t.currencyCode="RUB";
    InvestmentOperation op{"opening","broker","option-position","option","buy",-100,1'000'000,QDateTime::currentDateTimeUtc(),"Открытие"};
    QVERIFY(r.saveInvestmentBundle({"option","OPT","","Опцион",InvestmentInstrumentType::Option,Currency::RUB,"MOEX","ROPD",t},
        {"option-position","broker","option",1'000'000,1'000'000},InvestmentQuote("option",0,QDateTime::currentDateTimeUtc(),t),false,op));
    QCOMPARE(r.loadInvestmentQuotes().at(0).priceMicros(),qint64(0));
    QVERIFY(!r.saveInvestmentQuote({"option",-1,QDateTime::currentDateTimeUtc(),t}));
    QVERIFY(!r.updateAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::USD,10'000'000}));
    QVERIFY(!r.updateAccount({"broker","Брокер",AssetType::Investment,AccountType::Deposit,Currency::RUB,10'000'000}));
    QVERIFY(r.updateAccount({"broker","Другое имя",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10'000'000}));
}

QTEST_GUILESS_MAIN(InvestmentAccountingRepositoryTest)
#include "InvestmentAccountingRepositoryTest.moc"
