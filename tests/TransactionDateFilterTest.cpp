#include "../services/TransactionDateFilter.h"
#include "../services/DateSliceCalculator.h"

#include <QTimeZone>
#include <QtTest>

class TransactionDateFilterTest : public QObject
{
    Q_OBJECT

private slots:
    void includesBothRangeBoundaries();
    void excludesTransactionsOutsideRange();
    void comparesByLocalCalendarDate();
    void rejectsInvalidRange();
    void calculatesTotalsForPeriodAndBalanceAtEndDate();
    void doesNotCountTransfersAsIncomeOrExpense();
};

namespace
{
Transaction makeTransaction(const QString& id, const QDateTime& occurredAt)
{
    return Transaction(
        id,
        QStringLiteral("account"),
        QStringLiteral("category"),
        Money(1'00, Currency::RUB),
        TransactionType::Expense,
        occurredAt
        );
}

QDateTime localDateTime(const QDate& date, const QTime& time)
{
    return QDateTime(date, time, QTimeZone::systemTimeZone());
}
}

void TransactionDateFilterTest::includesBothRangeBoundaries()
{
    const QVector<Transaction> transactions{
        makeTransaction(
            QStringLiteral("from"),
            localDateTime(QDate(2026, 9, 1), QTime(0, 0))
            ),
        makeTransaction(
            QStringLiteral("middle"),
            localDateTime(QDate(2026, 9, 15), QTime(12, 0))
            ),
        makeTransaction(
            QStringLiteral("to"),
            localDateTime(QDate(2026, 9, 30), QTime(23, 59, 59))
            )
    };

    const QVector<Transaction> result = TransactionDateFilter::between(
        transactions,
        QDate(2026, 9, 1),
        QDate(2026, 9, 30)
        );

    QCOMPARE(result.size(), 3);
    QCOMPARE(result.constFirst().id(), QStringLiteral("from"));
    QCOMPARE(result.constLast().id(), QStringLiteral("to"));
}

void TransactionDateFilterTest::excludesTransactionsOutsideRange()
{
    const QVector<Transaction> transactions{
        makeTransaction(
            QStringLiteral("before"),
            localDateTime(QDate(2026, 8, 31), QTime(23, 59))
            ),
        makeTransaction(
            QStringLiteral("inside"),
            localDateTime(QDate(2026, 9, 10), QTime(10, 0))
            ),
        makeTransaction(
            QStringLiteral("after"),
            localDateTime(QDate(2026, 10, 1), QTime(0, 0))
            )
    };

    const QVector<Transaction> result = TransactionDateFilter::between(
        transactions,
        QDate(2026, 9, 1),
        QDate(2026, 9, 30)
        );

    QCOMPARE(result.size(), 1);
    QCOMPARE(result.constFirst().id(), QStringLiteral("inside"));
    QVERIFY(TransactionDateFilter::isOnOrBefore(
        transactions[0], QDate(2026, 9, 30)));
    QVERIFY(!TransactionDateFilter::isOnOrBefore(
        transactions[2], QDate(2026, 9, 30)));
}

void TransactionDateFilterTest::comparesByLocalCalendarDate()
{
    const QDateTime local = localDateTime(
        QDate(2026, 9, 7),
        QTime(0, 30)
        );
    const Transaction transaction = makeTransaction(
        QStringLiteral("timezone"),
        local.toUTC()
        );

    QVERIFY(TransactionDateFilter::contains(
        transaction,
        QDate(2026, 9, 7),
        QDate(2026, 9, 7)
        ));
}

void TransactionDateFilterTest::rejectsInvalidRange()
{
    const Transaction transaction = makeTransaction(
        QStringLiteral("invalid"),
        localDateTime(QDate(2026, 9, 7), QTime(12, 0))
        );

    QVERIFY(!TransactionDateFilter::contains(
        transaction,
        QDate(2026, 9, 8),
        QDate(2026, 9, 7)
        ));
    QVERIFY(TransactionDateFilter::between(
        {transaction},
        QDate(),
        QDate(2026, 9, 7)
        ).isEmpty());
}

void TransactionDateFilterTest::calculatesTotalsForPeriodAndBalanceAtEndDate()
{
    const QVector<Account> accounts{
        Account(
            QStringLiteral("account"),
            QStringLiteral("Account"),
            AssetType::Fiat,
            AccountType::DebitCard,
            Currency::RUB,
            10'000
            )
    };
    const QVector<Transaction> transactions{
        Transaction(
            QStringLiteral("before"),
            QStringLiteral("account"),
            QStringLiteral("salary"),
            Money(5'000, Currency::RUB),
            TransactionType::Income,
            localDateTime(QDate(2026, 8, 31), QTime(12, 0))
            ),
        Transaction(
            QStringLiteral("inside"),
            QStringLiteral("account"),
            QStringLiteral("food"),
            Money(2'000, Currency::RUB),
            TransactionType::Expense,
            localDateTime(QDate(2026, 9, 15), QTime(12, 0))
            ),
        Transaction(
            QStringLiteral("after"),
            QStringLiteral("account"),
            QStringLiteral("food"),
            Money(1'000, Currency::RUB),
            TransactionType::Expense,
            localDateTime(QDate(2026, 10, 1), QTime(12, 0))
            )
    };

    const DateSliceCalculator::Totals totals = DateSliceCalculator::calculate(
        accounts,
        transactions,
        QDate(2026, 9, 1),
        QDate(2026, 9, 30)
        );

    const int rub = static_cast<int>(Currency::RUB);
    QCOMPARE(totals.balance[rub], qint64(13'000));
    QCOMPARE(totals.income[rub], qint64(0));
    QCOMPARE(totals.expense[rub], qint64(2'000));
}

void TransactionDateFilterTest::doesNotCountTransfersAsIncomeOrExpense()
{
    const QVector<Account> accounts{
        Account(
            QStringLiteral("source"),
            QStringLiteral("Source"),
            AssetType::Fiat,
            AccountType::DebitCard,
            Currency::RUB,
            10'000
            ),
        Account(
            QStringLiteral("target"),
            QStringLiteral("Target"),
            AssetType::Fiat,
            AccountType::DebitCard,
            Currency::RUB
            )
    };
    const QDateTime occurredAt = localDateTime(
        QDate(2026, 9, 15), QTime(12, 0));
    const QVector<Transaction> transactions{
        Transaction(
            QStringLiteral("transfer-out"),
            QStringLiteral("source"),
            QStringLiteral("transfer-out"),
            Money(2'000, Currency::RUB),
            TransactionType::Expense,
            occurredAt
            ),
        Transaction(
            QStringLiteral("transfer-in"),
            QStringLiteral("target"),
            QStringLiteral("transfer-in"),
            Money(2'000, Currency::RUB),
            TransactionType::Income,
            occurredAt
            )
    };

    const DateSliceCalculator::Totals totals = DateSliceCalculator::calculate(
        accounts,
        transactions,
        QDate(2026, 9, 1),
        QDate(2026, 9, 30)
        );

    const int rub = static_cast<int>(Currency::RUB);
    QCOMPARE(totals.balance[rub], qint64(10'000));
    QCOMPARE(totals.income[rub], qint64(0));
    QCOMPARE(totals.expense[rub], qint64(0));
}

QTEST_MAIN(TransactionDateFilterTest)

#include "TransactionDateFilterTest.moc"
