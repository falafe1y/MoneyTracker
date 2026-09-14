#include "../persistence/FinanceRepository.h"

#include <QTemporaryDir>
#include <QSqlQuery>
#include <QUuid>
#include <QtTest>

#include <algorithm>

class InvestmentRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void storesUpdatesAndArchivesInvestmentModel();
    void rejectsInvalidInvestmentRelationsAndValues();
    void archivesPositionsWithInvestmentAccount();
    void migratesMoexRoutingColumns();
    void migratesLegacyInvestmentAccountTypes();
};

void InvestmentRepositoryTest::storesUpdatesAndArchivesInvestmentModel()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("investment-model.sqlite3"));
    const QDateTime firstQuoteAt = QDateTime::fromMSecsSinceEpoch(
        1'788'300'000'123, Qt::UTC);
    const QDateTime updatedQuoteAt = firstQuoteAt.addSecs(300);

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

        const Account brokerage(
            QStringLiteral("brokerage"), QStringLiteral("Broker"),
            AssetType::Investment, AccountType::Brokerage, Currency::USD);
        QVERIFY2(repository.insertAccount(brokerage),
                 qPrintable(repository.lastError()));

        const InvestmentInstrument instrument(
            QStringLiteral("aapl"), QStringLiteral(" aapl "),
            QStringLiteral(" us0378331005 "), QStringLiteral(" Apple Inc. "),
            InvestmentInstrumentType::Stock, Currency::USD,
            QStringLiteral(" moex "), QStringLiteral(" tqbr "));
        QVERIFY2(repository.insertInvestmentInstrument(instrument),
                 qPrintable(repository.lastError()));

        const InvestmentPosition position(
            QStringLiteral("aapl-position"), brokerage.id(),
            QStringLiteral("aapl"), 1'250'000, 180'500'000);
        QVERIFY2(repository.insertInvestmentPosition(position),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.saveInvestmentQuote(InvestmentQuote(
                     QStringLiteral("aapl"), 201'125'000, firstQuoteAt)),
                 qPrintable(repository.lastError()));

        const InvestmentInstrument updatedInstrument(
            QStringLiteral("aapl"), QStringLiteral("AAPL"),
            QStringLiteral("US0378331005"), QStringLiteral("Apple"),
            InvestmentInstrumentType::Stock, Currency::USD,
            QStringLiteral("MOEX"), QStringLiteral("TQTF"));
        QVERIFY2(repository.updateInvestmentInstrument(updatedInstrument),
                 qPrintable(repository.lastError()));
        const InvestmentPosition updatedPosition(
            position.id(), brokerage.id(), QStringLiteral("aapl"),
            2'500'001, 190'750'125);
        QVERIFY2(repository.updateInvestmentPosition(updatedPosition),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.saveInvestmentQuote(InvestmentQuote(
                     QStringLiteral("aapl"), 205'000'001, updatedQuoteAt)),
                 qPrintable(repository.lastError()));
        QVERIFY(!repository.saveInvestmentQuote(InvestmentQuote(
            QStringLiteral("aapl"), 1, firstQuoteAt)));
        QVERIFY(!repository.updateInvestmentInstrument(InvestmentInstrument(
            QStringLiteral("aapl"), QStringLiteral("AAPL"),
            QStringLiteral("US0378331005"), QStringLiteral("Apple"),
            InvestmentInstrumentType::Stock, Currency::EUR)));
    }

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

        const QVector<InvestmentInstrument> instruments =
            repository.loadInvestmentInstruments();
        QCOMPARE(instruments.size(), 1);
        QCOMPARE(instruments.constFirst().id(), QStringLiteral("aapl"));
        QCOMPARE(instruments.constFirst().symbol(), QStringLiteral("AAPL"));
        QCOMPARE(instruments.constFirst().isin(), QStringLiteral("US0378331005"));
        QCOMPARE(instruments.constFirst().name(), QStringLiteral("Apple"));
        QCOMPARE(instruments.constFirst().type(),
                 InvestmentInstrumentType::Stock);
        QCOMPARE(instruments.constFirst().currency(), Currency::USD);
        QCOMPARE(instruments.constFirst().marketCode(), QStringLiteral("MOEX"));
        QCOMPARE(instruments.constFirst().primaryBoardId(), QStringLiteral("TQTF"));

        const QVector<InvestmentPosition> positions =
            repository.loadInvestmentPositions();
        QCOMPARE(positions.size(), 1);
        QCOMPARE(positions.constFirst().id(), QStringLiteral("aapl-position"));
        QCOMPARE(positions.constFirst().accountId(), QStringLiteral("brokerage"));
        QCOMPARE(positions.constFirst().instrumentId(), QStringLiteral("aapl"));
        QCOMPARE(positions.constFirst().quantityMicros(), qint64(2'500'001));
        QCOMPARE(positions.constFirst().averagePriceMicros(),
                 qint64(190'750'125));
        QVERIFY(positions.constFirst().createdAtUtc().isValid());
        QVERIFY(positions.constFirst().updatedAtUtc().isValid());

        const QVector<InvestmentQuote> quotes = repository.loadInvestmentQuotes();
        QCOMPARE(quotes.size(), 1);
        QCOMPARE(quotes.constFirst().instrumentId(), QStringLiteral("aapl"));
        QCOMPARE(quotes.constFirst().priceMicros(), qint64(205'000'001));
        QCOMPARE(quotes.constFirst().quotedAtUtc(), updatedQuoteAt);

        QVERIFY(!repository.archiveInvestmentInstrument(QStringLiteral("aapl")));
        QVERIFY2(repository.deleteInvestmentPosition(
                     QStringLiteral("aapl-position")),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.archiveInvestmentInstrument(QStringLiteral("aapl")),
                 qPrintable(repository.lastError()));
        QVERIFY(repository.loadInvestmentPositions().isEmpty());
        QVERIFY(repository.loadInvestmentInstruments().isEmpty());
        QVERIFY(repository.loadInvestmentQuotes().isEmpty());
    }
}

void InvestmentRepositoryTest::rejectsInvalidInvestmentRelationsAndValues()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("investment-validation.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account fiat(
        QStringLiteral("fiat"), QStringLiteral("Fiat"), AssetType::Fiat,
        AccountType::DebitCard, Currency::USD);
    const Account brokerage(
        QStringLiteral("brokerage"), QStringLiteral("Brokerage"),
        AssetType::Investment, AccountType::Brokerage, Currency::USD);
    QVERIFY(repository.insertAccount(fiat));
    QVERIFY(repository.insertAccount(brokerage));

    const InvestmentInstrument apple(
        QStringLiteral("aapl"), QStringLiteral("AAPL"),
        QStringLiteral("US0378331005"), QStringLiteral("Apple"),
        InvestmentInstrumentType::Stock, Currency::USD);
    QVERIFY(repository.insertInvestmentInstrument(apple));
    QVERIFY(!repository.insertInvestmentInstrument(InvestmentInstrument(
        QStringLiteral("duplicate-isin"), QStringLiteral("APC"),
        QStringLiteral("US0378331005"), QStringLiteral("Apple Xetra"),
        InvestmentInstrumentType::Stock, Currency::EUR)));
    QVERIFY(!repository.insertInvestmentInstrument(InvestmentInstrument(
        QStringLiteral("blank"), QString(), QString(), QStringLiteral("Blank"),
        InvestmentInstrumentType::Other, Currency::USD)));

    QVERIFY(!repository.insertInvestmentPosition(InvestmentPosition(
        QStringLiteral("fiat-position"), fiat.id(), apple.id(),
        1'000'000, 100'000'000)));
    QVERIFY(!repository.insertInvestmentPosition(InvestmentPosition(
        QStringLiteral("missing-instrument"), brokerage.id(),
        QStringLiteral("missing"), 1'000'000, 100'000'000)));
    QVERIFY(!repository.insertInvestmentPosition(InvestmentPosition(
        QStringLiteral("zero-position"), brokerage.id(), apple.id(),
        0, 100'000'000)));

    const InvestmentPosition valid(
        QStringLiteral("valid-position"), brokerage.id(), apple.id(),
        1'000'000, 100'000'000);
    QVERIFY(repository.insertInvestmentPosition(valid));
    QVERIFY(!repository.insertInvestmentPosition(InvestmentPosition(
        QStringLiteral("duplicate-position"), brokerage.id(), apple.id(),
        2'000'000, 110'000'000)));
    QVERIFY(!repository.saveInvestmentQuote(InvestmentQuote(
        QStringLiteral("missing"), 100'000'000,
        QDateTime::currentDateTimeUtc())));
    QVERIFY(!repository.saveInvestmentQuote(InvestmentQuote(
        apple.id(), 0, QDateTime::currentDateTimeUtc())));
}

void InvestmentRepositoryTest::archivesPositionsWithInvestmentAccount()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("investment-account-archive.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account brokerage(
        QStringLiteral("brokerage"), QStringLiteral("Brokerage"),
        AssetType::Investment, AccountType::Brokerage, Currency::RUB);
    const InvestmentInstrument instrument(
        QStringLiteral("sber"), QStringLiteral("SBER"),
        QStringLiteral("RU0009029540"), QStringLiteral("Сбербанк"),
        InvestmentInstrumentType::Stock, Currency::RUB);
    QVERIFY(repository.insertAccount(brokerage));
    QVERIFY(repository.insertInvestmentInstrument(instrument));
    QVERIFY(repository.insertInvestmentPosition(InvestmentPosition(
        QStringLiteral("sber-position"), brokerage.id(), instrument.id(),
        10'000'000, 310'250'000)));

    QVERIFY2(repository.deleteAccount(brokerage.id()),
             qPrintable(repository.lastError()));
    QVERIFY(repository.loadInvestmentPositions().isEmpty());
    QVERIFY2(repository.archiveInvestmentInstrument(instrument.id()),
             qPrintable(repository.lastError()));
}

void InvestmentRepositoryTest::migratesMoexRoutingColumns()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("investment-routing-migration.sqlite3"));
    const QString connectionName = QUuid::createUuid().toString(
        QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE investment_instruments ("
            "id TEXT PRIMARY KEY, symbol TEXT NOT NULL, isin TEXT NOT NULL, "
            "name TEXT NOT NULL, type INTEGER NOT NULL, currency TEXT NOT NULL, "
            "is_archived INTEGER NOT NULL, created_at INTEGER NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO investment_instruments VALUES ("
            "'moex:SBER','SBER','RU0009029540','Сбербанк',0,'RUB',0,1)")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    FinanceRepository repository(databasePath);
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
    QVector<InvestmentInstrument> instruments =
        repository.loadInvestmentInstruments();
    QCOMPARE(instruments.size(), 1);
    QCOMPARE(instruments.constFirst().marketCode(), QString());
    QCOMPARE(instruments.constFirst().primaryBoardId(), QString());

    const InvestmentInstrument migrated(
        QStringLiteral("moex:SBER"), QStringLiteral("SBER"),
        QStringLiteral("RU0009029540"), QStringLiteral("Сбербанк"),
        InvestmentInstrumentType::Stock, Currency::RUB,
        QStringLiteral("MOEX"), QStringLiteral("TQBR"));
    QVERIFY2(repository.updateInvestmentInstrument(migrated),
             qPrintable(repository.lastError()));
    instruments = repository.loadInvestmentInstruments();
    QCOMPARE(instruments.constFirst().marketCode(), QStringLiteral("MOEX"));
    QCOMPARE(instruments.constFirst().primaryBoardId(), QStringLiteral("TQBR"));
}

void InvestmentRepositoryTest::migratesLegacyInvestmentAccountTypes()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("investment-account-type-migration.sqlite3"));
    const QString connectionName = QUuid::createUuid().toString(
        QUuid::WithoutBraces);
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE accounts ("
            "id TEXT PRIMARY KEY, name TEXT NOT NULL, "
            "asset_type INTEGER NOT NULL CHECK(asset_type IN (0,1,2)), "
            "account_type INTEGER NOT NULL CHECK(account_type IN (0,1,2,3,4)), "
            "currency TEXT NOT NULL CHECK(currency IN ('RUB','USD','EUR')), "
            "initial_balance_minor INTEGER NOT NULL DEFAULT 0, "
            "credit_limit_minor INTEGER NOT NULL DEFAULT 0 "
            "CHECK(credit_limit_minor >= 0), "
            "is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)), "
            "created_at INTEGER NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO accounts VALUES ("
            "'legacy','Старый счёт',0,4,'RUB',12500,0,0,1)")));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    FinanceRepository repository(databasePath);
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
    const QVector<Account> existingAccounts = repository.loadAccounts();
    QVERIFY(std::any_of(
        existingAccounts.cbegin(), existingAccounts.cend(),
        [](const Account& account) {
            return account.id() == QStringLiteral("legacy") &&
                account.initialBalanceMinor() == 12'500;
        }));

    const Account brokerage(
        QStringLiteral("brokerage"), QStringLiteral("Брокер"),
        AssetType::Investment, AccountType::Brokerage, Currency::RUB);
    QVERIFY2(repository.insertAccount(brokerage),
             qPrintable(repository.lastError()));
    const Account deposit(
        QStringLiteral("deposit"), QStringLiteral("Вклад"),
        AssetType::Investment, AccountType::Deposit, Currency::RUB);
    QVERIFY2(repository.insertAccount(deposit),
             qPrintable(repository.lastError()));
}

QTEST_GUILESS_MAIN(InvestmentRepositoryTest)

#include "InvestmentRepositoryTest.moc"
