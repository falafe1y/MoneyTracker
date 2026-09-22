#pragma once

#include "Currency.h"
#include <QDate>
#include <QStringList>

// A live target, not a ledger of contributions. Achievement is never persisted.
struct FinancialGoal
{
    QString id;
    QString name;
    Currency currency = Currency::RUB;
    qint64 targetMinor = 0;
    QDate deadline; // Invalid date means no deadline.
    bool allSources = true;
    QStringList sourceIds; // account:<id> or wallet:<id>
};
