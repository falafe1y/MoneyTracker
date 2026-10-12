#include "InvestmentRiskCalculator.h"
#include <algorithm>
#include <cmath>

InvestmentRisk calculateInvestmentRisk(const std::string& openingDate,
                                      const std::vector<InvestmentReturnDay>& days)
{
    InvestmentRisk result;
    if (openingDate.empty() || days.empty()) return result;
    long double index = 1, peak = 1, worstPeak = 1;
    long double mean = 0, deviations = 0;
    std::string peakDate = openingDate;
    result.drawdowns.push_back({openingDate, 0});
    for (const auto& day : days) {
        if (day.date.empty() || !std::isfinite(day.factor) || day.factor < 0
            || (!day.hasCapital && day.factor != 1)) return {};
        index *= day.factor;
        if (!std::isfinite(index)) return {};
        if (day.hasCapital) {
            const long double daily = (day.factor - 1) * 100;
            if (!std::isfinite(daily)) return {};
            ++result.returnDays;
            // Welford's algorithm avoids loss of precision for nearly constant returns.
            const long double difference = daily - mean;
            mean += difference / result.returnDays;
            deviations += difference * (daily - mean);
            if (!result.bestDayPercent || daily > *result.bestDayPercent) {
                result.bestDayPercent = daily; result.bestDay = day.date;
            }
            if (!result.worstDayPercent || daily < *result.worstDayPercent) {
                result.worstDayPercent = daily; result.worstDay = day.date;
            }
        }
        // Permit only floating-point roundoff when recognising a recovered peak.
        const auto atPeak = [](long double current, long double high) {
            return current >= high || high - current <= 1e-12L * high;
        };
        if (result.maximumDrawdownPercent > 0 && result.recoveryDate.empty()
            && atPeak(index, worstPeak)) result.recoveryDate = day.date;
        if (atPeak(index, peak)) {
            peak = std::max(peak, index);
            peakDate = day.date;
        }
        long double depth = std::clamp((1 - index / peak) * 100, 0.0L, 100.0L);
        if (depth < 1e-10L) depth = 0;
        if (depth > result.maximumDrawdownPercent + 1e-10L) {
            result.maximumDrawdownPercent = depth;
            result.peakDate = peakDate;
            result.troughDate = day.date;
            result.recoveryDate.clear();
            worstPeak = peak;
        }
        result.currentDrawdownPercent = depth;
        result.drawdowns.push_back({day.date, -depth});
    }
    if (!result.returnDays) return {};
    result.available = true;
    if (result.returnDays >= 2) {
        const long double annual = std::sqrt(std::max(0.0L, deviations)
                                            / (result.returnDays - 1) * 365.25L);
        if (std::isfinite(annual)) result.annualVolatilityPercent = annual;
    }
    return result;
}
