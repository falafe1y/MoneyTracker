#include "CurrencyConverter.h"

#include <limits>

namespace
{

using Int128 = __int128_t;

qint64 divideRounded(Int128 numerator, Int128 denominator)
{
    if (denominator <= 0) {
        return 0;
    }

    const Int128 half = denominator / 2;

    Int128 result;

    if (numerator >= 0) {
        result = (numerator + half) / denominator;
    } else {
        result = (numerator - half) / denominator;
    }

    constexpr Int128 minValue =
        std::numeric_limits<qint64>::min();

    constexpr Int128 maxValue =
        std::numeric_limits<qint64>::max();

    if (result < minValue) {
        return std::numeric_limits<qint64>::min();
    }

    if (result > maxValue) {
        return std::numeric_limits<qint64>::max();
    }

    return static_cast<qint64>(result);
}

} // namespace

CurrencyConverter::CurrencyConverter(
    const CurrencyRateProvider& rateProvider)
    : rateProvider_(rateProvider)
{
}

Money CurrencyConverter::convert(
    const Money& money,
    Currency targetCurrency) const
{
    if (money.currency() == targetCurrency) {
        return money;
    }

    const qint64 sourceRate =
        rateProvider_.rateToUsd(money.currency());

    const qint64 targetRate =
        rateProvider_.rateToUsd(targetCurrency);

    if (sourceRate <= 0 || targetRate <= 0) {
        return Money(0, targetCurrency);
    }

    const Int128 numerator =
        static_cast<Int128>(money.minorUnits()) * sourceRate;

    const qint64 targetMinorUnits =
        divideRounded(numerator, targetRate);

    return Money(targetMinorUnits, targetCurrency);
}