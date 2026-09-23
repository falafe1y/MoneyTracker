#include "FinancialTrajectoryCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
qint64 roundedMinor(const long double value)
{
    if (value >= std::numeric_limits<qint64>::max())
        return std::numeric_limits<qint64>::max();
    if (value <= std::numeric_limits<qint64>::min())
        return std::numeric_limits<qint64>::min();
    return static_cast<qint64>(std::llround(value));
}
}
QVector<FinancialTrajectoryPoint> FinancialTrajectoryCalculator::calculate(
    const FinancialTrajectoryInput& input)
{
    const auto& settings = input.settings;
    const int horizon = std::clamp(settings.horizonMonths, 1, 600);
    const long double monthlyReturn =
        std::pow(1.0L + std::max(-99.0, settings.annualReturnPercent) / 100.0L,
                 1.0L / 12.0L) - 1.0L;
    const long double monthlyInflation =
        std::pow(1.0L + std::max(-99.0, settings.annualInflationPercent) / 100.0L,
                 1.0L / 12.0L);
    const long double income = input.averageIncomeMinor *
        (1.0L + settings.incomeChangePercent / 100.0L);
    long double expense = input.averageExpenseMinor *
        (1.0L + settings.expenseChangePercent / 100.0L);
    long double capital = input.currentCapitalMinor;
    long double invested = std::max<qint64>(0, input.investmentCapitalMinor);

    QVector<FinancialTrajectoryPoint> result;
    result.reserve(horizon + 1);
    result.append({input.currentDate, input.currentCapitalMinor,
                   input.currentCapitalMinor});
    long double accumulatedInflation = 1.0L;
    for (int month = 1; month <= horizon; ++month) {
        const long double investmentGain = invested * monthlyReturn;
        invested += investmentGain;
        capital += income - expense + investmentGain;
        if (settings.purchaseMinor > 0 && settings.purchaseMonth == month)
            capital -= settings.purchaseMinor;
        accumulatedInflation *= monthlyInflation;
        result.append({input.currentDate.addMonths(month), roundedMinor(capital),
                       roundedMinor(capital / accumulatedInflation)});
        expense *= monthlyInflation;
    }
    return result;
}
