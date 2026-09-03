#include "../persistence/FinanceRepository.h"

#include <QTemporaryDir>
#include <QtTest>

class FinanceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesSelectedAccountAfterReopen();
    void updatesAndDeletesTransaction();
};

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

QTEST_GUILESS_MAIN(FinanceRepositoryTest)

#include "FinanceRepositoryTest.moc"
