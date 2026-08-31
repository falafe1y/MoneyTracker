#pragma once

#include "../core/Money.h"
#include "../core/Transaction.h"
#include "CurrencyConverter.h"

#include <QVector>

class BalanceCalculator
{
public:
    explicit BalanceCalculator(
        const CurrencyConverter& currencyConverter
        );

    Money calculate(
        const QVector<Transaction>& transactions,
        Currency targetCurrency
        ) const;

private:
    const CurrencyConverter& currencyConverter_;
};