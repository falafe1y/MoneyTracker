#pragma once

#include "../core/Currency.h"

#include <QtGlobal>

inline constexpr qint64 kCurrencyRateScale = 1'000'000;

class CurrencyRateProvider
{
public:
    virtual ~CurrencyRateProvider() = default;

    virtual qint64 rateToUsd(Currency currency) const = 0;
};
