#pragma once

#include "../core/Currency.h"

#include <QtGlobal>

// Nine decimal places keep user-entered RUB quotes precise enough that
// ordinary cent/kopeck conversions round exactly as expected.
inline constexpr qint64 kCurrencyRateScale = 1'000'000'000;

class CurrencyRateProvider
{
public:
    virtual ~CurrencyRateProvider() = default;

    virtual qint64 rateToUsd(Currency currency) const = 0;
};
