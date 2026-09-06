#pragma once

#include "CurrencyRateProvider.h"

class TestCurrencyRateProvider final : public CurrencyRateProvider
{
public:
    qint64 rateToUsd(Currency currency) const override
    {
        switch (currency) {
        case Currency::USD:
            return kCurrencyRateScale; // 1 USD

        case Currency::RUB:
            return 11'000;    // 0.011 USD

        case Currency::EUR:
            return 1'170'000; // 1.17 USD
        }

        return kCurrencyRateScale;
    }
};
