#pragma once
#include <optional>
#include <string>
#include <vector>

struct InvestmentReturnDay {
    std::string date;
    long double factor = 1;
    bool hasCapital = true;
};
struct InvestmentDrawdownPoint {
    std::string date;
    long double percent = 0; // Negative distance below the preceding high.
};
struct InvestmentRisk {
    bool available = false;
    int returnDays = 0;
    std::optional<long double> annualVolatilityPercent;
    std::optional<long double> bestDayPercent, worstDayPercent;
    long double maximumDrawdownPercent = 0, currentDrawdownPercent = 0;
    std::string bestDay, worstDay, peakDate, troughDate, recoveryDate;
    std::vector<InvestmentDrawdownPoint> drawdowns;
};

// Input must be a complete consecutive daily return chain, net of external flows.
// Calendar-day observations are annualized with sqrt(365.25), not a trading calendar.
InvestmentRisk calculateInvestmentRisk(const std::string& openingDate,
                                      const std::vector<InvestmentReturnDay>& days);
