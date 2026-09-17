#include "RecurringScheduleCalculator.h"

#include <algorithm>

bool RecurringScheduleCalculator::occursOn(
    const RecurringTransaction& recurring,
    const QDate& date
    )
{
    if (!date.isValid() || !recurring.startsOn().isValid() ||
        date < recurring.startsOn()) {
        return false;
    }

    switch (recurring.recurrenceType()) {
    case RecurrenceType::Daily:
        return true;
    case RecurrenceType::Weekly:
        return date.dayOfWeek() == recurring.weekday();
    case RecurrenceType::MonthlyDay:
        return date.day() == std::min(
            recurring.dayOfMonth(), date.daysInMonth());
    case RecurrenceType::MonthlyWeekday: {
        if (date.dayOfWeek() != recurring.weekday()) {
            return false;
        }
        if (recurring.weekOfMonth() == 5) {
            return date.addDays(7).month() != date.month();
        }
        return ((date.day() - 1) / 7) + 1 == recurring.weekOfMonth();
    }
    }
    return false;
}

QVector<QDate> RecurringScheduleCalculator::occurrences(
    const RecurringTransaction& recurring,
    const QDate& from,
    const QDate& through
    )
{
    QVector<QDate> result;
    if (!from.isValid() || !through.isValid() || from > through) {
        return result;
    }

    QDate date = std::max(from, recurring.startsOn());
    while (date <= through) {
        if (occursOn(recurring, date)) {
            result.append(date);
        }
        date = date.addDays(1);
    }
    return result;
}

QDate RecurringScheduleCalculator::nextOccurrence(
    const RecurringTransaction& recurring,
    const QDate& from
    )
{
    if (!from.isValid() || !recurring.startsOn().isValid()) {
        return {};
    }
    QDate date = std::max(from, recurring.startsOn());
    const QDate limit = date.addYears(2);
    while (date <= limit) {
        if (occursOn(recurring, date)) {
            return date;
        }
        date = date.addDays(1);
    }
    return {};
}
