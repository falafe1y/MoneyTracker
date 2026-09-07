#include "DateSliceCalculator.h"

#include "TransactionDateFilter.h"

namespace
{
constexpr int currencyIndex(const Currency currency)
{
    return static_cast<int>(currency);
}

bool isTransfer(const Transaction& transaction)
{
    return transaction.categoryId() == QStringLiteral("transfer-in") ||
           transaction.categoryId() == QStringLiteral("transfer-out");
}
}

DateSliceCalculator::Totals DateSliceCalculator::calculate(
    const QVector<Account>& accounts,
    const QVector<Transaction>& transactions,
    const QDate& from,
    const QDate& to
    )
{
    Totals result;
    if (!from.isValid() || !to.isValid() || from > to) {
        return result;
    }

    for (const Account& account : accounts) {
        result.balance[currencyIndex(account.currency())] +=
            account.initialBalanceMinor();
    }

    for (const Transaction& transaction : transactions) {
        const int index = currencyIndex(transaction.money().currency());
        const qint64 amount = transaction.money().minorUnits();

        if (TransactionDateFilter::isOnOrBefore(transaction, to)) {
            result.balance[index] +=
                transaction.type() == TransactionType::Income
                    ? amount
                    : -amount;
        }

        if (!isTransfer(transaction) && TransactionDateFilter::contains(
                transaction, from, to)) {
            auto& values = transaction.type() == TransactionType::Income
                ? result.income
                : result.expense;
            values[index] += amount;
        }
    }

    return result;
}
