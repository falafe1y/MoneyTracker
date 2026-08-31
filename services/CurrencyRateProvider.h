#pragma once

#include "../core/Currency.h"

#include <QtGlobal>

class CurrencyRateProvider
{
public:
    virtual ~CurrencyRateProvider() = default;

    virtual qint64 rateToUsd(Currency currency) const = 0;
};