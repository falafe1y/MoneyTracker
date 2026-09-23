#pragma once

#include "../core/FinancialTrajectory.h"

class FinancialTrajectoryCalculator
{
public:
    static QVector<FinancialTrajectoryPoint> calculate(
        const FinancialTrajectoryInput& input);
};
