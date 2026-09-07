#pragma once

#include "../core/Transaction.h"

#include <QDate>
#include <QVector>

class TransactionDateFilter
{
public:
    static bool contains(
        const Transaction& transaction,
        const QDate& from,
        const QDate& to
        );

    static bool isOnOrBefore(
        const Transaction& transaction,
        const QDate& date
        );

    static QVector<Transaction> between(
        const QVector<Transaction>& transactions,
        const QDate& from,
        const QDate& to
        );

private:
    static QDate localDate(const Transaction& transaction);
};
