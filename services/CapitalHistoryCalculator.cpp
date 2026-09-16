#include "CapitalHistoryCalculator.h"

#include <algorithm>
#include <limits>

namespace
{
qint64 saturatedAdd(const qint64 left, const qint64 right)
{
    using Int128 = __int128_t;
    const Int128 sum = static_cast<Int128>(left) +
        static_cast<Int128>(right);
    if (sum > std::numeric_limits<qint64>::max()) {
        return std::numeric_limits<qint64>::max();
    }
    if (sum < std::numeric_limits<qint64>::min()) {
        return std::numeric_limits<qint64>::min();
    }
    return static_cast<qint64>(sum);
}

QDate periodEnd(
    const QDate& periodStart,
    const CapitalHistoryResolution resolution
    )
{
    if (resolution == CapitalHistoryResolution::Day) {
        return periodStart;
    }
    if (resolution == CapitalHistoryResolution::Month) {
        return QDate(
            periodStart.year(), periodStart.month(),
            periodStart.daysInMonth());
    }
    return QDate(periodStart.year(), 12, 31);
}

QDate nextPeriod(
    const QDate& periodStart,
    const CapitalHistoryResolution resolution
    )
{
    if (resolution == CapitalHistoryResolution::Day) {
        return periodStart.addDays(1);
    }
    if (resolution == CapitalHistoryResolution::Month) {
        return periodStart.addMonths(1);
    }
    return periodStart.addYears(1);
}
}

CapitalHistorySeries CapitalHistoryCalculator::calculate(
    const qint64 openingMinor,
    const QVector<CapitalHistoryEvent>& events,
    const QDate& currentDate,
    const QDate& from,
    const QDate& to
    )
{
    CapitalHistorySeries result;
    if (!currentDate.isValid() ||
        (from.isValid() != to.isValid()) ||
        (from.isValid() && from > to)) {
        return result;
    }

    QVector<CapitalHistoryEvent> sorted;
    sorted.reserve(events.size());
    for (const CapitalHistoryEvent& event : events) {
        if (event.date.isValid()) {
            sorted.append(event);
        }
    }
    std::sort(
        sorted.begin(), sorted.end(),
        [](const CapitalHistoryEvent& left,
           const CapitalHistoryEvent& right)
        {
            return left.date < right.date;
        });

    QDate effectiveFrom = from;
    QDate effectiveTo = to;
    if (!effectiveFrom.isValid()) {
        effectiveFrom = currentDate;
        effectiveTo = currentDate;
        if (!sorted.isEmpty()) {
            const QDate dayBeforeFirst = sorted.constFirst().date.addDays(-1);
            effectiveFrom = std::min(
                effectiveFrom,
                dayBeforeFirst.isValid()
                    ? dayBeforeFirst
                    : sorted.constFirst().date);
            effectiveTo = std::max(
                effectiveTo, sorted.constLast().date);
        }
    }

    const qint64 daySpan = qMax<qint64>(
        0, effectiveFrom.daysTo(effectiveTo));
    result.resolution = daySpan <= 62
        ? CapitalHistoryResolution::Day
        : daySpan <= 730
          ? CapitalHistoryResolution::Month
          : CapitalHistoryResolution::Year;

    qint64 runningTotal = openingMinor;
    qsizetype eventIndex = 0;
    while (eventIndex < sorted.size() &&
           sorted[eventIndex].date < effectiveFrom) {
        runningTotal = saturatedAdd(
            runningTotal, sorted[eventIndex].deltaMinor);
        ++eventIndex;
    }

    QDate period = effectiveFrom;
    if (result.resolution == CapitalHistoryResolution::Month) {
        period = QDate(effectiveFrom.year(), effectiveFrom.month(), 1);
    } else if (result.resolution == CapitalHistoryResolution::Year) {
        period = QDate(effectiveFrom.year(), 1, 1);
    }

    while (period.isValid() && period <= effectiveTo) {
        const QDate pointDate = std::min(
            periodEnd(period, result.resolution), effectiveTo);
        while (eventIndex < sorted.size() &&
               sorted[eventIndex].date <= pointDate) {
            runningTotal = saturatedAdd(
                runningTotal, sorted[eventIndex].deltaMinor);
            ++eventIndex;
        }
        result.points.append({pointDate, runningTotal});
        period = nextPeriod(period, result.resolution);
    }

    return result;
}
