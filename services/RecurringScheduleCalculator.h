#pragma once

#include "../core/RecurringTransaction.h"

#include <QDate>
#include <QVector>

class RecurringScheduleCalculator
{
public:
    static bool occursOn(
        const RecurringTransaction& recurring,
        const QDate& date
        );

    static QVector<QDate> occurrences(
        const RecurringTransaction& recurring,
        const QDate& from,
        const QDate& through
        );

    static QDate nextOccurrence(
        const RecurringTransaction& recurring,
        const QDate& from
        );
};
