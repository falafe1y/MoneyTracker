#pragma once

#include "Currency.h"

#include <QDate>
#include <QtGlobal>
#include <QVector>

struct CapitalSnapshot
{
    QDate date;
    Currency currency = Currency::RUB;
    qint64 totalMinor = 0;
};

struct FinancialTrajectorySettings
{
    int analysisMonths = 6;
    int horizonMonths = 12;
    double annualReturnPercent = 5.0;
    double annualInflationPercent = 6.0;
    double incomeChangePercent = 0.0;
    double expenseChangePercent = 0.0;
    qint64 purchaseMinor = 0;
    int purchaseMonth = 0;
};

struct FinancialTrajectoryInput
{
    qint64 currentCapitalMinor = 0;
    qint64 investmentCapitalMinor = 0;
    qint64 averageIncomeMinor = 0;
    qint64 averageExpenseMinor = 0;
    FinancialTrajectorySettings settings;
    QDate currentDate;
};

struct FinancialTrajectoryPoint
{
    QDate date;
    qint64 nominalMinor = 0;
    qint64 realMinor = 0;
};
