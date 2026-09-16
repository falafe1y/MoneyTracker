#include "../services/CapitalHistoryCalculator.h"

#include <QtTest>

class CapitalHistoryCalculatorTest : public QObject
{
    Q_OBJECT

private slots:
    void includesPastTodayAndFutureWithoutFilter();
    void appliesOperationsBeforeFilteredRangeToOpeningValue();
    void combinesOperationsAndFillsDaysWithoutChanges();
    void groupsLongRangesByMonthAndYear();
};

void CapitalHistoryCalculatorTest::includesPastTodayAndFutureWithoutFilter()
{
    const CapitalHistorySeries result = CapitalHistoryCalculator::calculate(
        10'000,
        {
            {QDate(2026, 9, 14), 2'000},
            {QDate(2026, 9, 20), -500}
        },
        QDate(2026, 9, 16));

    QCOMPARE(result.resolution, CapitalHistoryResolution::Day);
    QCOMPARE(result.points.size(), 8);
    QCOMPARE(result.points.constFirst().date, QDate(2026, 9, 13));
    QCOMPARE(result.points.constFirst().totalMinor, qint64(10'000));
    QCOMPARE(result.points.at(3).date, QDate(2026, 9, 16));
    QCOMPARE(result.points.at(3).totalMinor, qint64(12'000));
    QCOMPARE(result.points.constLast().date, QDate(2026, 9, 20));
    QCOMPARE(result.points.constLast().totalMinor, qint64(11'500));
}

void CapitalHistoryCalculatorTest::appliesOperationsBeforeFilteredRangeToOpeningValue()
{
    const CapitalHistorySeries result = CapitalHistoryCalculator::calculate(
        10'000,
        {
            {QDate(2026, 8, 1), 2'000},
            {QDate(2026, 9, 12), -700},
            {QDate(2026, 10, 1), 5'000}
        },
        QDate(2026, 9, 16),
        QDate(2026, 9, 10),
        QDate(2026, 9, 13));

    QCOMPARE(result.points.size(), 4);
    QCOMPARE(result.points.constFirst().totalMinor, qint64(12'000));
    QCOMPARE(result.points.constLast().totalMinor, qint64(11'300));
}

void CapitalHistoryCalculatorTest::combinesOperationsAndFillsDaysWithoutChanges()
{
    const CapitalHistorySeries result = CapitalHistoryCalculator::calculate(
        1'000,
        {
            {QDate(2026, 9, 15), 200},
            {QDate(2026, 9, 15), -50}
        },
        QDate(2026, 9, 16));

    QCOMPARE(result.points.size(), 3);
    QCOMPARE(result.points.at(0).totalMinor, qint64(1'000));
    QCOMPARE(result.points.at(1).totalMinor, qint64(1'150));
    QCOMPARE(result.points.at(2).totalMinor, qint64(1'150));
}

void CapitalHistoryCalculatorTest::groupsLongRangesByMonthAndYear()
{
    const CapitalHistorySeries monthly = CapitalHistoryCalculator::calculate(
        100,
        {
            {QDate(2026, 1, 10), 20},
            {QDate(2026, 2, 5), 30}
        },
        QDate(2026, 12, 31),
        QDate(2026, 1, 1),
        QDate(2026, 12, 31));
    QCOMPARE(monthly.resolution, CapitalHistoryResolution::Month);
    QCOMPARE(monthly.points.size(), 12);
    QCOMPARE(monthly.points.at(0).date, QDate(2026, 1, 31));
    QCOMPARE(monthly.points.at(0).totalMinor, qint64(120));
    QCOMPARE(monthly.points.at(1).totalMinor, qint64(150));

    const CapitalHistorySeries yearly = CapitalHistoryCalculator::calculate(
        100,
        {{QDate(2024, 6, 1), 25}, {QDate(2026, 7, 1), -10}},
        QDate(2026, 9, 16),
        QDate(2024, 1, 1),
        QDate(2026, 12, 31));
    QCOMPARE(yearly.resolution, CapitalHistoryResolution::Year);
    QCOMPARE(yearly.points.size(), 3);
    QCOMPARE(yearly.points.at(0).totalMinor, qint64(125));
    QCOMPARE(yearly.points.at(2).totalMinor, qint64(115));
}

QTEST_APPLESS_MAIN(CapitalHistoryCalculatorTest)

#include "CapitalHistoryCalculatorTest.moc"
