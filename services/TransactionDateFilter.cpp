#include "TransactionDateFilter.h"

QDate TransactionDateFilter::localDate(const Transaction& transaction)
{
    return transaction.date().toLocalTime().date();
}

bool TransactionDateFilter::contains(
    const Transaction& transaction,
    const QDate& from,
    const QDate& to
    )
{
    if (!from.isValid() || !to.isValid() || from > to) {
        return false;
    }

    const QDate date = localDate(transaction);
    return date >= from && date <= to;
}

bool TransactionDateFilter::isOnOrBefore(
    const Transaction& transaction,
    const QDate& date
    )
{
    return date.isValid() && localDate(transaction) <= date;
}

QVector<Transaction> TransactionDateFilter::between(
    const QVector<Transaction>& transactions,
    const QDate& from,
    const QDate& to
    )
{
    QVector<Transaction> result;
    if (!from.isValid() || !to.isValid() || from > to) {
        return result;
    }

    result.reserve(transactions.size());
    for (const Transaction& transaction : transactions) {
        if (contains(transaction, from, to)) {
            result.append(transaction);
        }
    }
    return result;
}
