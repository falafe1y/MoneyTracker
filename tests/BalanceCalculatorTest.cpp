#include "../core/Transaction.h"
#include "../services/BalanceCalculator.h"
#include "../services/CurrencyConverter.h"
#include "../services/TestCurrencyRateProvider.h"

#include <QtTest>

class BalanceCalculatorTest : public QObject
{
    Q_OBJECT

private slots:
    void calculatesSimpleBalance();
    void calculatesBalanceWithDifferentCurrencies();
    void calculatesNegativeBalance();
    void emptyTransactionsReturnZero();
};

void BalanceCalculatorTest::calculatesSimpleBalance()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);
    BalanceCalculator calculator(converter);

    const QVector<Transaction> transactions{
        Transaction(
            "1",
            "household",
            "salary",
            Money(100'000'00, Currency::RUB),
            TransactionType::Income,
            QDateTime::currentDateTime()
        ),

        Transaction(
            "2",
            "household",
            "food",
            Money(2'500'00, Currency::RUB),
            TransactionType::Expense,
            QDateTime::currentDateTime()
        ),

        Transaction(
            "3",
            "household",
            "games",
            Money(1'000'00, Currency::RUB),
            TransactionType::Expense,
            QDateTime::currentDateTime()
        )
    };

    const Money balance =
        calculator.calculate(
            transactions,
            Currency::RUB
        );

    QCOMPARE(balance.currency(), Currency::RUB);
    QCOMPARE(balance.minorUnits(), qint64(96'500'00));
}

void BalanceCalculatorTest::calculatesBalanceWithDifferentCurrencies()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);
    BalanceCalculator calculator(converter);

    const QVector<Transaction> transactions{
        Transaction(
            "1",
            "household",
            "salary",
            Money(100'000'00, Currency::RUB),
            TransactionType::Income,
            QDateTime::currentDateTime()
        ),

        Transaction(
            "2",
            "household",
            "salary",
            Money(100'00, Currency::USD),
            TransactionType::Income,
            QDateTime::currentDateTime()
        )
    };

    const Money balance =
        calculator.calculate(
            transactions,
            Currency::RUB
        );

    // 100000 RUB + 100 USD * 1'000'000 / 11'000
    QCOMPARE(balance.currency(), Currency::RUB);
    QCOMPARE(balance.minorUnits(), qint64(10'909'091));
}

void BalanceCalculatorTest::calculatesNegativeBalance()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);
    BalanceCalculator calculator(converter);

    const QVector<Transaction> transactions{
        Transaction(
            "1",
            "household",
            "food",
            Money(5'000'00, Currency::RUB),
            TransactionType::Expense,
            QDateTime::currentDateTime()
        )
    };

    const Money balance =
        calculator.calculate(
            transactions,
            Currency::RUB
        );

    QCOMPARE(balance.minorUnits(), qint64(-5'000'00));
}

void BalanceCalculatorTest::emptyTransactionsReturnZero()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);
    BalanceCalculator calculator(converter);

    const Money balance =
        calculator.calculate(
            {},
            Currency::USD
        );

    QCOMPARE(balance.currency(), Currency::USD);
    QCOMPARE(balance.minorUnits(), qint64(0));
}

QTEST_MAIN(BalanceCalculatorTest)

#include "BalanceCalculatorTest.moc"