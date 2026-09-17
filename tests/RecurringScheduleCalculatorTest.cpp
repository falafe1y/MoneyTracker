#include "../services/RecurringScheduleCalculator.h"

#include <QtTest>

namespace
{
RecurringTransaction recurring(
    const RecurrenceType recurrenceType,
    const int weekday,
    const int dayOfMonth,
    const int weekOfMonth,
    const QDate& startsOn = QDate(2026, 1, 1)
    )
{
    return RecurringTransaction(
        QStringLiteral("schedule"),
        QStringLiteral("Operation"),
        QStringLiteral("account"),
        QStringLiteral("category"),
        TransactionType::Expense,
        10'000,
        recurrenceType,
        weekday,
        dayOfMonth,
        weekOfMonth,
        startsOn);
}
}

class RecurringScheduleCalculatorTest : public QObject
{
    Q_OBJECT

private slots:
    void calculatesWeeklyOccurrences();
    void usesLastDayForShortMonths();
    void calculatesOrdinalWeekday();
    void respectsStartDate();
};

void RecurringScheduleCalculatorTest::calculatesWeeklyOccurrences()
{
    const QVector<QDate> dates = RecurringScheduleCalculator::occurrences(
        recurring(RecurrenceType::Weekly, 4, 1, 1),
        QDate(2026, 9, 1),
        QDate(2026, 9, 17));
    QCOMPARE(dates, QVector<QDate>({
        QDate(2026, 9, 3),
        QDate(2026, 9, 10),
        QDate(2026, 9, 17)}));
}

void RecurringScheduleCalculatorTest::usesLastDayForShortMonths()
{
    const RecurringTransaction schedule = recurring(
        RecurrenceType::MonthlyDay, 1, 31, 1);
    QVERIFY(RecurringScheduleCalculator::occursOn(
        schedule, QDate(2027, 2, 28)));
    QVERIFY(RecurringScheduleCalculator::occursOn(
        schedule, QDate(2027, 4, 30)));
    QVERIFY(!RecurringScheduleCalculator::occursOn(
        schedule, QDate(2027, 4, 29)));
}

void RecurringScheduleCalculatorTest::calculatesOrdinalWeekday()
{
    const RecurringTransaction firstTuesday = recurring(
        RecurrenceType::MonthlyWeekday, 2, 1, 1);
    QVERIFY(RecurringScheduleCalculator::occursOn(
        firstTuesday, QDate(2026, 9, 1)));
    QVERIFY(!RecurringScheduleCalculator::occursOn(
        firstTuesday, QDate(2026, 9, 8)));

    const RecurringTransaction lastMonday = recurring(
        RecurrenceType::MonthlyWeekday, 1, 1, 5);
    QVERIFY(RecurringScheduleCalculator::occursOn(
        lastMonday, QDate(2026, 9, 28)));
    QVERIFY(!RecurringScheduleCalculator::occursOn(
        lastMonday, QDate(2026, 9, 21)));
}

void RecurringScheduleCalculatorTest::respectsStartDate()
{
    const RecurringTransaction schedule = recurring(
        RecurrenceType::Weekly, 4, 1, 1, QDate(2026, 9, 10));
    QVERIFY(!RecurringScheduleCalculator::occursOn(
        schedule, QDate(2026, 9, 3)));
    QCOMPARE(
        RecurringScheduleCalculator::nextOccurrence(
            schedule, QDate(2026, 9, 1)),
        QDate(2026, 9, 10));
}

QTEST_GUILESS_MAIN(RecurringScheduleCalculatorTest)

#include "RecurringScheduleCalculatorTest.moc"
