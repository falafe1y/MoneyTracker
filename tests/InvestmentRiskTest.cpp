#include "../services/InvestmentRiskCalculator.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

static void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
static bool near(long double a, long double b) { return std::abs(a-b) < 1e-9L; }
int main() {
    auto r = calculateInvestmentRisk("2025-01-01", {{"2025-01-02",1.1L},{"2025-01-03",0.8L},{"2025-01-04",1.25L}});
    require(r.available && r.returnDays == 3,"complete daily chain");
    require(near(r.maximumDrawdownPercent,20) && near(r.currentDrawdownPercent,0),"compounded drawdown and recovery");
    require(r.peakDate=="2025-01-02" && r.troughDate=="2025-01-03" && r.recoveryDate=="2025-01-04","dates of worst drawdown");
    require(near(*r.bestDayPercent,25) && near(*r.worstDayPercent,-20),"daily extremes");
    require(r.bestDay=="2025-01-04" && r.worstDay=="2025-01-03","daily extreme dates");
    require(near(*r.annualVolatilityPercent,std::sqrt(525.0L*365.25L)),"sample standard deviation of simple daily percentage returns");
    require(r.drawdowns.size()==4 && near(r.drawdowns[2].percent,-20),"graph includes starting high");

    r=calculateInvestmentRisk("1",{{"2",1.1L},{"3",1.1L},{"4",1.1L}});
    require(near(*r.annualVolatilityPercent,0) && near(r.maximumDrawdownPercent,0),"constant daily return has no volatility or drawdown");
    require(r.peakDate.empty() && r.troughDate.empty() && r.recoveryDate.empty(),"zero drawdown does not invent event dates");
    r=calculateInvestmentRisk("1",{{"2",0.9L},{"3",0.9L}});
    require(near(r.maximumDrawdownPercent,19) && near(r.currentDrawdownPercent,19),"losses compound instead of adding");
    require(r.peakDate=="1" && r.troughDate=="3" && r.recoveryDate.empty(),"unrecovered loss");
    r=calculateInvestmentRisk("1",{{"2",1.1L},{"3",0.9L}});
    require(near(r.maximumDrawdownPercent,10),"drawdown measured from peak, not starting value");
    r=calculateInvestmentRisk("1",{{"2",0.9L}});
    require(r.available && !r.annualVolatilityPercent && near(*r.bestDayPercent,-10),"one return gives drawdown and extremes but no sample volatility");
    r=calculateInvestmentRisk("1",{{"2",1,false},{"3",1.1L},{"4",0.9L}});
    require(r.returnDays==2 && near(*r.annualVolatilityPercent,std::sqrt(200.0L*365.25L)),"idle zero-capital day excluded from volatility");
    require(r.drawdowns.size()==4,"idle days remain on chronological graph");
    require(!calculateInvestmentRisk("1",{{"2",1,false},{"3",1,false}}).available,"empty portfolio has unavailable risk");
    require(!calculateInvestmentRisk("1",{}).available,"empty chain is unavailable");
    require(!calculateInvestmentRisk("1",{{"2",-0.1L}}).available,"negative factor rejected");
    require(!calculateInvestmentRisk("1",{{"2",std::numeric_limits<long double>::infinity()}}).available,"infinite factor rejected");
    require(!calculateInvestmentRisk("1",{{"2",std::numeric_limits<long double>::quiet_NaN()}}).available,"NaN factor rejected");
    require(!calculateInvestmentRisk("1",{{"2",1.1L,false}}).available,"idle day cannot invent a return");
    r=calculateInvestmentRisk("1",{{"2",0},{"3",2}});
    require(near(r.maximumDrawdownPercent,100) && near(r.currentDrawdownPercent,100) && r.recoveryDate.empty(),"total loss cannot be undone by a later deposit");
    r=calculateInvestmentRisk("1",{{"2",0.9L},{"3",1.2L},{"4",0.8L}});
    require(near(r.maximumDrawdownPercent,20) && r.peakDate=="3" && r.troughDate=="4" && r.recoveryDate.empty(),"new deeper drawdown replaces prior recovery");
    r=calculateInvestmentRisk("1",{{"2",1.1L},{"3",1},{"4",0.8L},{"5",1.25L},{"6",0.9L}});
    require(r.peakDate=="3" && r.troughDate=="4" && r.recoveryDate=="5" && near(r.currentDrawdownPercent,10),"flat peak and recovered worst drawdown followed by smaller drawdown");
    std::cout << "Investment risk checks passed\n";
}
