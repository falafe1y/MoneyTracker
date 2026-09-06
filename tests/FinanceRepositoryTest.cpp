#include "../persistence/FinanceRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class FinanceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesSelectedAccountAfterReopen();
    void storesUiLanguage();
    void storesCurrencyRateSettings();
    void storesCreditCardTerms();
    void migratesCreditLimitForExistingDatabase();
    void updatesAndDeletesTransaction();
    void storesTransferAtomicallyWithoutAffectingIncomeAndExpense();
    void replacesIncomeWithTransferAtomically();
    void deletesWholeTransferFromEitherComponent();
    void replacesTransferWithTransactionAtomically();
    void updatesAccountAndProtectsTransactionCurrency();
    void deletesAccountWithRelatedOperations();
};

void FinanceRepositoryTest::storesCurrencyRateSettings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("moneytracker-rate-settings-test.sqlite3"));

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
        QVERIFY(repository.loadAutomaticCurrencyRates());
        QVERIFY(repository.saveAutomaticCurrencyRates(false));
        QVERIFY(repository.saveManualCurrencyRates(86.54, 100.12));
    }

    FinanceRepository reopened(databasePath);
    QVERIFY2(reopened.isOpen(), qPrintable(reopened.lastError()));
    QVERIFY(!reopened.loadAutomaticCurrencyRates());
    QCOMPARE(reopened.loadManualUsdToRubRate(), 86.54);
    QCOMPARE(reopened.loadManualEurToRubRate(), 100.12);
}

void FinanceRepositoryTest::storesCreditCardTerms()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("moneytracker-credit-card-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account creditCard(
        QStringLiteral("credit-card"),
        QStringLiteral("Credit card"),
        AssetType::Fiat,
        AccountType::CreditCard,
        Currency::RUB,
        -5'000,
        85'000);
    QVERIFY2(repository.insertAccount(creditCard),
             qPrintable(repository.lastError()));

    const QVector<Account> accounts = repository.loadAccounts();
    bool restored = false;
    for (const Account& account : accounts) {
        if (account.id() != creditCard.id()) {
            continue;
        }
        restored = true;
        QCOMPARE(account.type(), AccountType::CreditCard);
        QCOMPARE(account.initialBalanceMinor(), qint64(-5'000));
        QCOMPARE(account.creditLimitMinor(), qint64(85'000));
        QCOMPARE(account.debtMinor(-5'000), qint64(5'000));
        QCOMPARE(account.availableCreditMinor(-5'000), qint64(80'000));
        QCOMPARE(account.debtMinor(0), qint64(0));
        QCOMPARE(account.availableCreditMinor(0), qint64(85'000));
        QCOMPARE(account.availableCreditMinor(2'000), qint64(87'000));
    }
    QVERIFY(restored);

    const FinanceRepository::Summary summary = repository.loadSummary();
    QCOMPARE(summary.balance[static_cast<int>(Currency::RUB)], qint64(-5'000));
}

void FinanceRepositoryTest::migratesCreditLimitForExistingDatabase()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("moneytracker-credit-limit-migration.sqlite3"));
    const QString connectionName = QStringLiteral("credit-limit-migration-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY2(query.exec(QStringLiteral(
            "CREATE TABLE accounts ("
            "id TEXT PRIMARY KEY, name TEXT NOT NULL, "
            "asset_type INTEGER NOT NULL, account_type INTEGER NOT NULL, "
            "currency TEXT NOT NULL, initial_balance_minor INTEGER NOT NULL DEFAULT 0, "
            "is_archived INTEGER NOT NULL DEFAULT 0, created_at INTEGER NOT NULL)")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
            "INSERT INTO accounts(id, name, asset_type, account_type, currency, "
            "initial_balance_minor, is_archived, created_at) "
            "VALUES('legacy-credit-card', 'Legacy card', 0, 2, 'RUB', -5000, 0, 1)")),
            qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral(
            "INSERT INTO accounts(id, name, asset_type, account_type, currency, "
            "initial_balance_minor, is_archived, created_at) "
            "VALUES('legacy-credit-limit', 'Legacy limit', 0, 2, 'RUB', 85000, 0, 2)")),
            qPrintable(query.lastError().text()));
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    FinanceRepository repository(databasePath);
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
    const QVector<Account> accounts = repository.loadAccounts();
    bool restoredDebt = false;
    bool restoredLimit = false;
    for (const Account& account : accounts) {
        if (account.id() == QStringLiteral("legacy-credit-card")) {
            restoredDebt = true;
            QCOMPARE(account.initialBalanceMinor(), qint64(-5'000));
            QCOMPARE(account.creditLimitMinor(), qint64(0));
        } else if (account.id() == QStringLiteral("legacy-credit-limit")) {
            restoredLimit = true;
            QCOMPARE(account.initialBalanceMinor(), qint64(0));
            QCOMPARE(account.creditLimitMinor(), qint64(85'000));
        }
    }
    QVERIFY(restoredDebt);
    QVERIFY(restoredLimit);
}

void FinanceRepositoryTest::storesUiLanguage()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("moneytracker-language-test.sqlite3"));

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
        QCOMPARE(repository.loadUiLanguage(), QStringLiteral("ru"));
        QVERIFY2(repository.saveUiLanguage(QStringLiteral("en")),
                 qPrintable(repository.lastError()));
    }

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
        QCOMPARE(repository.loadUiLanguage(), QStringLiteral("en"));
    }
}

void FinanceRepositoryTest::preservesSelectedAccountAfterReopen()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("moneytracker-test.sqlite3")
    );
    const QString accountId = QStringLiteral("test-debit-card");
    const QString categoryId = QStringLiteral("test-groceries");
    const QString transactionId = QStringLiteral("test-transaction");
    const QDateTime occurredAt = QDateTime::fromMSecsSinceEpoch(
        1'788'000'000'000,
        Qt::UTC
    );

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

        const Account account(
            accountId,
            QStringLiteral("Test debit card"),
            AssetType::Fiat,
            AccountType::DebitCard,
            Currency::RUB
        );
        const Category category(
            categoryId,
            QStringLiteral("Test groceries"),
            CategoryType::Expense
        );
        const Transaction transaction(
            transactionId,
            accountId,
            categoryId,
            Money(12'345, Currency::RUB),
            TransactionType::Expense,
            occurredAt,
            QStringLiteral("Test purchase")
        );

        QVERIFY2(repository.insertAccount(account),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.insertCategory(category),
                 qPrintable(repository.lastError()));
        QVERIFY2(repository.insertTransaction(transaction),
                 qPrintable(repository.lastError()));
    }

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

        const QVector<Transaction> transactions = repository.loadTransactions();
        QCOMPARE(transactions.size(), 1);

        const Transaction& restored = transactions.constFirst();
        QCOMPARE(restored.id(), transactionId);
        QCOMPARE(restored.accountId(), accountId);
        QCOMPARE(restored.categoryId(), categoryId);
        QCOMPARE(restored.money().minorUnits(), qint64(12'345));
        QCOMPARE(restored.money().currency(), Currency::RUB);
        QCOMPARE(restored.type(), TransactionType::Expense);
        QCOMPARE(restored.date(), occurredAt);
        QCOMPARE(restored.description(), QStringLiteral("Test purchase"));
    }
}

void FinanceRepositoryTest::updatesAndDeletesTransaction()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FinanceRepository repository(
        temporaryDirectory.filePath(QStringLiteral("moneytracker-edit-test.sqlite3"))
    );
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account rubAccount(
        QStringLiteral("edit-rub-account"),
        QStringLiteral("RUB account"),
        AssetType::Fiat,
        AccountType::DebitCard,
        Currency::RUB
    );
    const Account usdAccount(
        QStringLiteral("edit-usd-account"),
        QStringLiteral("USD account"),
        AssetType::Fiat,
        AccountType::DebitCard,
        Currency::USD
    );
    const Category expenseCategory(
        QStringLiteral("edit-expense-category"),
        QStringLiteral("Expense"),
        CategoryType::Expense
    );
    const Category incomeCategory(
        QStringLiteral("edit-income-category"),
        QStringLiteral("Income"),
        CategoryType::Income
    );
    const QDateTime originalDate = QDateTime::fromMSecsSinceEpoch(
        1'788'000'000'000,
        Qt::UTC
    );
    const QDateTime updatedDate = originalDate.addDays(1);

    QVERIFY(repository.insertAccount(rubAccount));
    QVERIFY(repository.insertAccount(usdAccount));
    QVERIFY(repository.insertCategory(expenseCategory));
    QVERIFY(repository.insertCategory(incomeCategory));
    QVERIFY(repository.insertTransaction(Transaction(
        QStringLiteral("editable-transaction"),
        rubAccount.id(),
        expenseCategory.id(),
        Money(12'345, Currency::RUB),
        TransactionType::Expense,
        originalDate,
        QStringLiteral("Before edit")
    )));

    const Transaction updated(
        QStringLiteral("editable-transaction"),
        usdAccount.id(),
        incomeCategory.id(),
        Money(98'765, Currency::USD),
        TransactionType::Income,
        updatedDate,
        QStringLiteral("After edit")
    );
    QVERIFY2(repository.updateTransaction(updated),
             qPrintable(repository.lastError()));

    const QVector<Transaction> transactions = repository.loadTransactions();
    QCOMPARE(transactions.size(), 1);
    const Transaction& restored = transactions.constFirst();
    QCOMPARE(restored.id(), updated.id());
    QCOMPARE(restored.accountId(), usdAccount.id());
    QCOMPARE(restored.categoryId(), incomeCategory.id());
    QCOMPARE(restored.money().minorUnits(), qint64(98'765));
    QCOMPARE(restored.money().currency(), Currency::USD);
    QCOMPARE(restored.type(), TransactionType::Income);
    QCOMPARE(restored.date(), updatedDate);
    QCOMPARE(restored.description(), QStringLiteral("After edit"));

    const FinanceRepository::Summary updatedSummary = repository.loadSummary();
    QCOMPARE(updatedSummary.income[static_cast<int>(Currency::USD)], qint64(98'765));
    QCOMPARE(updatedSummary.expense[static_cast<int>(Currency::RUB)], qint64(0));
    QCOMPARE(updatedSummary.balance[static_cast<int>(Currency::USD)], qint64(98'765));

    QVERIFY2(repository.deleteTransaction(updated.id()),
             qPrintable(repository.lastError()));
    QVERIFY(repository.loadTransactions().isEmpty());

    const FinanceRepository::Summary deletedSummary = repository.loadSummary();
    QCOMPARE(deletedSummary.income[static_cast<int>(Currency::USD)], qint64(0));
    QCOMPARE(deletedSummary.balance[static_cast<int>(Currency::USD)], qint64(0));
}

void FinanceRepositoryTest::storesTransferAtomicallyWithoutAffectingIncomeAndExpense()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(
        temporaryDirectory.filePath(QStringLiteral("moneytracker-transfer-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account source(
        QStringLiteral("source-account"), QStringLiteral("Source"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 50'000);
    const Account target(
        QStringLiteral("target-account"), QStringLiteral("Target"),
        AssetType::Crypto, AccountType::CryptoWallet, Currency::USD, 0);
    QVERIFY(repository.insertAccount(source));
    QVERIFY(repository.insertAccount(target));

    const QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    const Transaction outgoing(
        QStringLiteral("transfer-id-out"), source.id(), QStringLiteral("transfer-out"),
        Money(10'000, Currency::RUB), TransactionType::Expense,
        occurredAt, QStringLiteral("Transfer test"));
    const Transaction incoming(
        QStringLiteral("transfer-id-in"), target.id(), QStringLiteral("transfer-in"),
        Money(125, Currency::USD), TransactionType::Income,
        occurredAt, QStringLiteral("Transfer test"));

    const Transaction invalidIncoming(
        QStringLiteral("invalid-transfer-in"), QStringLiteral("missing-account"),
        QStringLiteral("transfer-in"), Money(125, Currency::USD),
        TransactionType::Income, occurredAt, QStringLiteral("Invalid transfer"));
    QVERIFY(!repository.insertTransfer(outgoing, invalidIncoming));
    QVERIFY(repository.loadTransactions().isEmpty());

    QVERIFY2(repository.insertTransfer(outgoing, incoming),
             qPrintable(repository.lastError()));
    QCOMPARE(repository.loadTransactions().size(), 2);

    const FinanceRepository::Summary summary = repository.loadSummary();
    QCOMPARE(summary.balance[static_cast<int>(Currency::RUB)], qint64(40'000));
    QCOMPARE(summary.balance[static_cast<int>(Currency::USD)], qint64(125));
    QCOMPARE(summary.income[static_cast<int>(Currency::USD)], qint64(0));
    QCOMPARE(summary.expense[static_cast<int>(Currency::RUB)], qint64(0));
}

void FinanceRepositoryTest::replacesIncomeWithTransferAtomically()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("moneytracker-convert-to-transfer-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account source(
        QStringLiteral("convert-source"), QStringLiteral("Source"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 50'000);
    const Account target(
        QStringLiteral("convert-target"), QStringLiteral("Target"),
        AssetType::Crypto, AccountType::CryptoWallet, Currency::USD, 0);
    const Category incomeCategory(
        QStringLiteral("convert-income"), QStringLiteral("Income"),
        CategoryType::Income);
    QVERIFY(repository.insertAccount(source));
    QVERIFY(repository.insertAccount(target));
    QVERIFY(repository.insertCategory(incomeCategory));

    const QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    const Transaction income(
        QStringLiteral("income-to-convert"), source.id(), incomeCategory.id(),
        Money(1'000, Currency::RUB), TransactionType::Income,
        occurredAt, QStringLiteral("Income"));
    QVERIFY(repository.insertTransaction(income));

    const Transaction outgoing(
        QStringLiteral("converted-transfer-out"), source.id(),
        QStringLiteral("transfer-out"), Money(10'000, Currency::RUB),
        TransactionType::Expense, occurredAt, QStringLiteral("Converted"));
    const Transaction incoming(
        QStringLiteral("converted-transfer-in"), target.id(),
        QStringLiteral("transfer-in"), Money(125, Currency::USD),
        TransactionType::Income, occurredAt, QStringLiteral("Converted"));
    const Transaction invalidIncoming(
        QStringLiteral("converted-transfer-in"), QStringLiteral("missing-account"),
        QStringLiteral("transfer-in"), Money(125, Currency::USD),
        TransactionType::Income, occurredAt, QStringLiteral("Invalid"));

    QVERIFY(!repository.replaceTransactionWithTransfer(
        income.id(), outgoing, invalidIncoming));
    QVector<Transaction> transactions = repository.loadTransactions();
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.constFirst().id(), income.id());

    QVERIFY2(repository.replaceTransactionWithTransfer(
        income.id(), outgoing, incoming), qPrintable(repository.lastError()));
    transactions = repository.loadTransactions();
    QCOMPARE(transactions.size(), 2);

    QSet<QString> ids;
    for (const Transaction& transaction : transactions) {
        ids.insert(transaction.id());
    }
    QVERIFY(ids.contains(outgoing.id()));
    QVERIFY(ids.contains(incoming.id()));
    QVERIFY(!ids.contains(income.id()));

    const FinanceRepository::Summary summary = repository.loadSummary();
    QCOMPARE(summary.balance[static_cast<int>(Currency::RUB)], qint64(40'000));
    QCOMPARE(summary.balance[static_cast<int>(Currency::USD)], qint64(125));
    QCOMPARE(summary.income[static_cast<int>(Currency::RUB)], qint64(0));
    QCOMPARE(summary.expense[static_cast<int>(Currency::RUB)], qint64(0));

    const Transaction updatedOutgoing(
        outgoing.id(), source.id(), QStringLiteral("transfer-out"),
        Money(5'000, Currency::RUB), TransactionType::Expense,
        occurredAt, QStringLiteral("Updated transfer"));
    const Transaction updatedIncoming(
        incoming.id(), target.id(), QStringLiteral("transfer-in"),
        Money(63, Currency::USD), TransactionType::Income,
        occurredAt, QStringLiteral("Updated transfer"));
    QVERIFY2(repository.replaceTransactionWithTransfer(
        incoming.id(), updatedOutgoing, updatedIncoming),
        qPrintable(repository.lastError()));

    transactions = repository.loadTransactions();
    QCOMPARE(transactions.size(), 2);
    for (const Transaction& transaction : transactions) {
        QCOMPARE(transaction.description(), QStringLiteral("Updated transfer"));
        if (transaction.categoryId() == QStringLiteral("transfer-out")) {
            QCOMPARE(transaction.money().minorUnits(), qint64(5'000));
        } else {
            QCOMPARE(transaction.categoryId(), QStringLiteral("transfer-in"));
            QCOMPARE(transaction.money().minorUnits(), qint64(63));
        }
    }

    const FinanceRepository::Summary updatedSummary = repository.loadSummary();
    QCOMPARE(updatedSummary.balance[static_cast<int>(Currency::RUB)], qint64(45'000));
    QCOMPARE(updatedSummary.balance[static_cast<int>(Currency::USD)], qint64(63));
}

void FinanceRepositoryTest::deletesWholeTransferFromEitherComponent()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("moneytracker-delete-transfer-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account source(
        QStringLiteral("delete-source"), QStringLiteral("Source"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 50'000);
    const Account target(
        QStringLiteral("delete-target"), QStringLiteral("Target"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 0);
    QVERIFY(repository.insertAccount(source));
    QVERIFY(repository.insertAccount(target));

    const QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    const auto makeOutgoing = [&](const QString& prefix) {
        return Transaction(
            prefix + QStringLiteral("-out"), source.id(),
            QStringLiteral("transfer-out"), Money(10'000, Currency::RUB),
            TransactionType::Expense, occurredAt, QStringLiteral("Transfer"));
    };
    const auto makeIncoming = [&](const QString& prefix) {
        return Transaction(
            prefix + QStringLiteral("-in"), target.id(),
            QStringLiteral("transfer-in"), Money(10'000, Currency::RUB),
            TransactionType::Income, occurredAt, QStringLiteral("Transfer"));
    };

    Transaction outgoing = makeOutgoing(QStringLiteral("delete-by-incoming"));
    Transaction incoming = makeIncoming(QStringLiteral("delete-by-incoming"));
    QVERIFY(repository.insertTransfer(outgoing, incoming));
    QVERIFY2(repository.deleteTransaction(incoming.id()),
             qPrintable(repository.lastError()));
    QVERIFY(repository.loadTransactions().isEmpty());

    outgoing = makeOutgoing(QStringLiteral("delete-by-outgoing"));
    incoming = makeIncoming(QStringLiteral("delete-by-outgoing"));
    QVERIFY(repository.insertTransfer(outgoing, incoming));
    QVERIFY2(repository.deleteTransaction(outgoing.id()),
             qPrintable(repository.lastError()));
    QVERIFY(repository.loadTransactions().isEmpty());

    const FinanceRepository::Summary summary = repository.loadSummary();
    QCOMPARE(summary.balance[static_cast<int>(Currency::RUB)], qint64(50'000));
}

void FinanceRepositoryTest::replacesTransferWithTransactionAtomically()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("moneytracker-convert-from-transfer-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account source(
        QStringLiteral("replace-source"), QStringLiteral("Source"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 50'000);
    const Account target(
        QStringLiteral("replace-target"), QStringLiteral("Target"),
        AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 0);
    const Category expenseCategory(
        QStringLiteral("replacement-expense"), QStringLiteral("Expense"),
        CategoryType::Expense);
    QVERIFY(repository.insertAccount(source));
    QVERIFY(repository.insertAccount(target));
    QVERIFY(repository.insertCategory(expenseCategory));

    const QDateTime occurredAt = QDateTime::currentDateTimeUtc();
    const Transaction outgoing(
        QStringLiteral("replace-transfer-out"), source.id(),
        QStringLiteral("transfer-out"), Money(10'000, Currency::RUB),
        TransactionType::Expense, occurredAt, QStringLiteral("Transfer"));
    const Transaction incoming(
        QStringLiteral("replace-transfer-in"), target.id(),
        QStringLiteral("transfer-in"), Money(10'000, Currency::RUB),
        TransactionType::Income, occurredAt, QStringLiteral("Transfer"));
    QVERIFY(repository.insertTransfer(outgoing, incoming));

    const Transaction expense(
        QStringLiteral("replacement-operation"), target.id(), expenseCategory.id(),
        Money(2'500, Currency::RUB), TransactionType::Expense,
        occurredAt, QStringLiteral("Replacement"));
    QVERIFY2(repository.replaceTransaction(incoming.id(), expense),
             qPrintable(repository.lastError()));

    const QVector<Transaction> transactions = repository.loadTransactions();
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.constFirst().id(), expense.id());
    QCOMPARE(transactions.constFirst().categoryId(), expenseCategory.id());

    const FinanceRepository::Summary summary = repository.loadSummary();
    QCOMPARE(summary.balance[static_cast<int>(Currency::RUB)], qint64(47'500));
    QCOMPARE(summary.expense[static_cast<int>(Currency::RUB)], qint64(2'500));
}

void FinanceRepositoryTest::updatesAccountAndProtectsTransactionCurrency()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FinanceRepository repository(temporaryDirectory.filePath(
        QStringLiteral("moneytracker-update-account-test.sqlite3")));
    QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

    const Account original(
        QStringLiteral("editable-account"), QStringLiteral("Before"),
        AssetType::Fiat, AccountType::Cash, Currency::RUB, 10'000);
    QVERIFY(repository.insertAccount(original));

    const Account updated(
        original.id(), QStringLiteral("After"), AssetType::Fiat,
        AccountType::DebitCard, Currency::USD, 25'000);
    QVERIFY2(repository.updateAccount(updated), qPrintable(repository.lastError()));

    QVector<Account> accounts = repository.loadAccounts();
    bool foundUpdated = false;
    for (const Account& account : accounts) {
        if (account.id() != updated.id()) {
            continue;
        }
        foundUpdated = true;
        QCOMPARE(account.name(), updated.name());
        QCOMPARE(account.type(), updated.type());
        QCOMPARE(account.currency(), updated.currency());
        QCOMPARE(account.initialBalanceMinor(), qint64(25'000));
    }
    QVERIFY(foundUpdated);

    const Category category(
        QStringLiteral("account-update-expense"), QStringLiteral("Expense"),
        CategoryType::Expense);
    QVERIFY(repository.insertCategory(category));
    QVERIFY(repository.insertTransaction(Transaction(
        QStringLiteral("account-update-transaction"), updated.id(), category.id(),
        Money(1'000, Currency::USD), TransactionType::Expense,
        QDateTime::currentDateTimeUtc(), QStringLiteral("Expense"))));

    const Account invalidCurrencyChange(
        updated.id(), updated.name(), updated.assetType(), updated.type(),
        Currency::EUR, updated.initialBalanceMinor());
    QVERIFY(!repository.updateAccount(invalidCurrencyChange));

    accounts = repository.loadAccounts();
    for (const Account& account : accounts) {
        if (account.id() == updated.id()) {
            QCOMPARE(account.currency(), Currency::USD);
        }
    }
}

void FinanceRepositoryTest::deletesAccountWithRelatedOperations()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString databasePath = temporaryDirectory.filePath(
        QStringLiteral("moneytracker-delete-account-test.sqlite3"));

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));

        const Account source(
            QStringLiteral("account-to-delete"), QStringLiteral("Source"),
            AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 50'000);
        const Account target(
            QStringLiteral("account-to-keep"), QStringLiteral("Target"),
            AssetType::Fiat, AccountType::DebitCard, Currency::RUB, 0);
        const Category category(
            QStringLiteral("delete-account-expense"), QStringLiteral("Expense"),
            CategoryType::Expense);
        QVERIFY(repository.insertAccount(source));
        QVERIFY(repository.insertAccount(target));
        QVERIFY(repository.insertCategory(category));

        const QDateTime occurredAt = QDateTime::currentDateTimeUtc();
        QVERIFY(repository.insertTransaction(Transaction(
            QStringLiteral("delete-account-expense-row"), source.id(), category.id(),
            Money(1'000, Currency::RUB), TransactionType::Expense,
            occurredAt, QStringLiteral("Expense"))));
        QVERIFY(repository.insertTransfer(
            Transaction(
                QStringLiteral("delete-account-transfer-out"), source.id(),
                QStringLiteral("transfer-out"), Money(10'000, Currency::RUB),
                TransactionType::Expense, occurredAt, QStringLiteral("Transfer")),
            Transaction(
                QStringLiteral("delete-account-transfer-in"), target.id(),
                QStringLiteral("transfer-in"), Money(10'000, Currency::RUB),
                TransactionType::Income, occurredAt, QStringLiteral("Transfer"))));

        QVERIFY2(repository.deleteAccount(source.id()),
                 qPrintable(repository.lastError()));
        QVERIFY(repository.loadTransactions().isEmpty());

        const QVector<Account> accounts = repository.loadAccounts();
        bool foundDeleted = false;
        bool foundTarget = false;
        for (const Account& account : accounts) {
            foundDeleted = foundDeleted || account.id() == source.id();
            foundTarget = foundTarget || account.id() == target.id();
        }
        QVERIFY(!foundDeleted);
        QVERIFY(foundTarget);

        QVERIFY(repository.deleteAccount(QStringLiteral("household-eur")));
    }

    {
        FinanceRepository repository(databasePath);
        QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
        const QVector<Account> accounts = repository.loadAccounts();
        for (const Account& account : accounts) {
            QVERIFY(account.id() != QStringLiteral("account-to-delete"));
            QVERIFY(account.id() != QStringLiteral("household-eur"));
        }
    }
}

QTEST_GUILESS_MAIN(FinanceRepositoryTest)

#include "FinanceRepositoryTest.moc"
