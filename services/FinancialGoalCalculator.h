#pragma once

#include "../core/FinancialGoal.h"
#include "../core/Money.h"
#include "CurrencyRateProvider.h"
#include <QVector>

struct GoalAssetValue
{
    QString sourceId;
    Money value;
    bool available = true;
};

struct FinancialGoalProgress
{
    qint64 currentMinor = 0;
    qint64 remainingMinor = 0;
    double ratio = 0;
    bool complete = true;
    bool achieved = false;
    bool overdue = false;
};

class FinancialGoalCalculator
{
public:
    static FinancialGoalProgress calculate(
        const FinancialGoal& goal, const QVector<GoalAssetValue>& assets,
        const CurrencyRateProvider& rates, const QDate& today);
};
