#pragma once

#include "Currency.h"

#include <QtGlobal>

class Money
{
public:
    constexpr Money() = default;

    constexpr Money(
        qint64 minorUnits,
        Currency currency
        )
        : minorUnits_(minorUnits)
        , currency_(currency)
    {
    }

    constexpr qint64 minorUnits() const noexcept
    {
        return minorUnits_;
    }

    constexpr Currency currency() const noexcept
    {
        return currency_;
    }

    constexpr bool isZero() const noexcept
    {
        return minorUnits_ == 0;
    }

    constexpr bool isPositive() const noexcept
    {
        return minorUnits_ > 0;
    }

    constexpr bool isNegative() const noexcept
    {
        return minorUnits_ < 0;
    }

private:
    qint64 minorUnits_ = 0;
    Currency currency_ = Currency::RUB;
};