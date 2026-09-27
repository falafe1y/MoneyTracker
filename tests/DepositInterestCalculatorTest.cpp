#include "../services/DepositInterestCalculator.h"

#include <QtTest>

class DepositInterestCalculatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void calculatesMonthlyCompoundInterest();
    void calculatesDailyInterest();
    void clampsMonthlyPayoutToMonthEnd();
};

void DepositInterestCalculatorTest::calculatesMonthlyCompoundInterest()
{
    const qint64 first = DepositInterestCalculator::interestMinor(
        21'000'000, 1'300, DepositPayoutFrequency::Monthly);
    QCOMPARE(first, 227'500);

    const qint64 second = DepositInterestCalculator::interestMinor(
        21'000'000 + first, 1'300, DepositPayoutFrequency::Monthly);
    QCOMPARE(second, 229'965);
}

void DepositInterestCalculatorTest::calculatesDailyInterest()
{
    QCOMPARE(
        DepositInterestCalculator::interestMinor(
            21'000'000, 1'300, DepositPayoutFrequency::Daily),
        7'479);
}

void DepositInterestCalculatorTest::clampsMonthlyPayoutToMonthEnd()
{
    const DepositSettings settings(
        QStringLiteral("deposit"),
        1'300,
        DepositPayoutFrequency::Monthly,
        31,
        QDate(2026, 1, 31),
        QDate(2026, 1, 30));

    QCOMPARE(
        DepositInterestCalculator::occurrences(
            settings, QDate(2026, 1, 31), QDate(2026, 4, 30)),
        QVector<QDate>({
            QDate(2026, 1, 31),
            QDate(2026, 2, 28),
            QDate(2026, 3, 31),
            QDate(2026, 4, 30)}));
}

QTEST_MAIN(DepositInterestCalculatorTest)
#include "DepositInterestCalculatorTest.moc"
