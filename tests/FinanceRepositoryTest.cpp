#include "../persistence/FinanceRepository.h"

#include <QTemporaryDir>
#include <QtTest>

class FinanceRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesSelectedAccountAfterReopen();
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

QTEST_GUILESS_MAIN(FinanceRepositoryTest)

#include "FinanceRepositoryTest.moc"
