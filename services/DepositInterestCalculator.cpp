#include "DepositInterestCalculator.h"

#include <algorithm>
#include <limits>

qint64 DepositInterestCalculator::interestMinor(
    const qint64 balanceMinor,
    const int annualRateBasisPoints,
    const DepositPayoutFrequency frequency
    )
{
    if (balanceMinor <= 0 || annualRateBasisPoints <= 0) {
        return 0;
    }

    using Int128 = __int128_t;
    const Int128 periods = frequency == DepositPayoutFrequency::Daily
        ? 365
        : 12;
    const Int128 divisor = 10'000 * periods;
    const Int128 product = static_cast<Int128>(balanceMinor) *
        static_cast<Int128>(annualRateBasisPoints);
    const Int128 rounded = (product + divisor / 2) / divisor;
    return rounded > std::numeric_limits<qint64>::max()
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(rounded);
}

QDate DepositInterestCalculator::monthlyPayout(
    const int year,
    const int month,
    const int payoutDay
    )
{
    const QDate monthStart(year, month, 1);
    if (!monthStart.isValid()) {
        return {};
    }
    return QDate(year, month, std::min(payoutDay, monthStart.daysInMonth()));
}

QDate DepositInterestCalculator::nextMonthlyPayoutAfter(
    const QDate& date,
    const int payoutDay
    )
{
    if (!date.isValid() || payoutDay < 1 || payoutDay > 31) {
        return {};
    }
    QDate candidate = monthlyPayout(date.year(), date.month(), payoutDay);
    if (candidate <= date) {
        const QDate nextMonth = QDate(date.year(), date.month(), 1).addMonths(1);
        candidate = monthlyPayout(
            nextMonth.year(), nextMonth.month(), payoutDay);
    }
    return candidate;
}

QDate DepositInterestCalculator::firstPayoutAfter(
    const QDate& configuredOn,
    const DepositPayoutFrequency frequency,
    const int payoutDay
    )
{
    if (!configuredOn.isValid()) {
        return {};
    }
    return frequency == DepositPayoutFrequency::Daily
        ? configuredOn.addDays(1)
        : nextMonthlyPayoutAfter(configuredOn, payoutDay);
}

QVector<QDate> DepositInterestCalculator::occurrences(
    const DepositSettings& settings,
    const QDate& from,
    const QDate& through
    )
{
    QVector<QDate> result;
    if (!from.isValid() || !through.isValid() || from > through ||
        !settings.startsOn().isValid()) {
        return result;
    }

    QDate current = std::max(from, settings.startsOn());
    if (settings.payoutFrequency() == DepositPayoutFrequency::Daily) {
        for (; current <= through; current = current.addDays(1)) {
            result.append(current);
        }
        return result;
    }

    const int payoutDay = settings.payoutDay();
    QDate candidate = monthlyPayout(
        current.year(), current.month(), payoutDay);
    if (candidate < current) {
        candidate = nextMonthlyPayoutAfter(current, payoutDay);
    }
    while (candidate.isValid() && candidate <= through) {
        if (candidate >= settings.startsOn()) {
            result.append(candidate);
        }
        candidate = nextMonthlyPayoutAfter(candidate, payoutDay);
    }
    return result;
}
