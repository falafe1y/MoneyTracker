#pragma once

#include "../core/Account.h"
#include "../core/Transaction.h"

#include <QDate>
#include <QVector>

#include <array>

class DateSliceCalculator
{
public:
    struct Totals
    {
        std::array<qint64, 3> balance{};
        std::array<qint64, 3> income{};
        std::array<qint64, 3> expense{};
    };

    static Totals calculate(
        const QVector<Account>& accounts,
        const QVector<Transaction>& transactions,
        const QDate& from,
        const QDate& to
        );
};
