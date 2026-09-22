#include "FinancialGoalCalculator.h"
#include "CurrencyConverter.h"
#include <QSet>
#include <algorithm>
#include <limits>

FinancialGoalProgress FinancialGoalCalculator::calculate(
    const FinancialGoal& goal, const QVector<GoalAssetValue>& assets,
    const CurrencyRateProvider& rates, const QDate& today)
{
    FinancialGoalProgress result;
    CurrencyConverter converter(rates);
    __int128_t total = 0;
    QSet<QString> seen;
    for (const auto& asset : assets) {
        if (!goal.allSources && !goal.sourceIds.contains(asset.sourceId))
            continue;
        seen.insert(asset.sourceId);
        if (!asset.available || (asset.value.currency() != goal.currency &&
            (rates.rateToUsd(asset.value.currency()) <= 0 ||
             rates.rateToUsd(goal.currency) <= 0))) {
            result.complete = false;
            continue;
        }
        total += converter.convert(asset.value, goal.currency).minorUnits();
    }
    if (!goal.allSources) {
        if (goal.sourceIds.isEmpty()) result.complete = false;
        for (const auto& source : goal.sourceIds)
            if (!seen.contains(source)) result.complete = false;
    }
    const __int128_t maximum = std::numeric_limits<qint64>::max();
    const __int128_t minimum = std::numeric_limits<qint64>::min();
    if (total > maximum || total < minimum) result.complete = false;
    result.currentMinor = static_cast<qint64>(std::clamp(total, minimum, maximum));
    const __int128_t remaining = static_cast<__int128_t>(goal.targetMinor) - total;
    result.remainingMinor = static_cast<qint64>(std::clamp(remaining, __int128_t(0), maximum));
    if (goal.targetMinor <= 0) result.complete = false;
    result.ratio = goal.targetMinor > 0
        ? static_cast<double>(result.currentMinor) / goal.targetMinor : 0;
    result.achieved = result.complete && result.currentMinor >= goal.targetMinor;
    result.overdue = goal.deadline.isValid() && today > goal.deadline && !result.achieved;
    return result;
}
