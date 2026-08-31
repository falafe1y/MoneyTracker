#include "BalanceCalculator.h"

#include <limits>

namespace
{
qint64 addSafely(qint64 lhs, qint64 rhs)
{
    if (rhs > 0 &&
        lhs > std::numeric_limits<qint64>::max() - rhs) {
        return std::numeric_limits<qint64>::max();
    }

    if (rhs < 0 &&
        lhs < std::numeric_limits<qint64>::min() - rhs) {
        return std::numeric_limits<qint64>::min();
    }

    return lhs + rhs;
}

qint64 subtractSafely(qint64 lhs, qint64 rhs)
{
    if (rhs < 0 &&
        lhs > std::numeric_limits<qint64>::max() + rhs) {
        return std::numeric_limits<qint64>::max();
    }

    if (rhs > 0 &&
        lhs < std::numeric_limits<qint64>::min() + rhs) {
        return std::numeric_limits<qint64>::min();
    }

    return lhs - rhs;
}
}

BalanceCalculator::BalanceCalculator(
    const CurrencyConverter& currencyConverter
    )
    : currencyConverter_(currencyConverter)
{
}

Money BalanceCalculator::calculate(
    const QVector<Transaction>& transactions,
    Currency targetCurrency
    ) const
{
    qint64 balance = 0;

    for (const Transaction& transaction : transactions) {
        const Money converted =
            currencyConverter_.convert(
                transaction.money(),
                targetCurrency
                );

        if (transaction.type() == TransactionType::Income) {
            balance = addSafely(
                balance,
                converted.minorUnits()
                );
        } else {
            balance = subtractSafely(
                balance,
                converted.minorUnits()
                );
        }
    }

    return Money(balance, targetCurrency);
}