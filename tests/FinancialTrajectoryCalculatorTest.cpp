#include "../services/FinancialTrajectoryCalculator.h"

#include <QtTest>

class FinancialTrajectoryCalculatorTest : public QObject
{
    Q_OBJECT
private slots:
    void addsMonthlyCashFlow();
    void appliesReturnInflationAndPurchase();
};

void FinancialTrajectoryCalculatorTest::addsMonthlyCashFlow()
{
    FinancialTrajectoryInput input;
    input.currentDate = QDate(2026, 9, 22);
    input.currentCapitalMinor = 1'000'000;
    input.averageIncomeMinor = 200'000;
    input.averageExpenseMinor = 150'000;
    input.settings.horizonMonths = 6;
    input.settings.annualReturnPercent = 0;
    input.settings.annualInflationPercent = 0;
    const auto points = FinancialTrajectoryCalculator::calculate(input);
    QCOMPARE(points.size(), 7);
    QCOMPARE(points.last().nominalMinor, 1'300'000);
    QCOMPARE(points.last().realMinor, 1'300'000);
    QCOMPARE(points.last().date, QDate(2027, 3, 22));
}

void FinancialTrajectoryCalculatorTest::appliesReturnInflationAndPurchase()
{
    FinancialTrajectoryInput input;
    input.currentDate = QDate(2026, 1, 31);
    input.currentCapitalMinor = 2'000'000;
    input.investmentCapitalMinor = 1'000'000;
    input.settings.horizonMonths = 12;
    input.settings.annualReturnPercent = 12;
    input.settings.annualInflationPercent = 10;
    input.settings.purchaseMinor = 500'000;
    input.settings.purchaseMonth = 3;
    const auto points = FinancialTrajectoryCalculator::calculate(input);
    QVERIFY(points.at(1).nominalMinor > input.currentCapitalMinor);
    QVERIFY(points.at(3).nominalMinor < points.at(2).nominalMinor);
    QVERIFY(points.last().realMinor < points.last().nominalMinor);
    QCOMPARE(points.last().date, QDate(2027, 1, 31));
}

QTEST_APPLESS_MAIN(FinancialTrajectoryCalculatorTest)
#include "FinancialTrajectoryCalculatorTest.moc"
