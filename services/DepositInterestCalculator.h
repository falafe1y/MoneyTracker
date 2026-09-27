#pragma once

#include "../core/DepositSettings.h"

#include <QDate>
#include <QVector>
#include <QtGlobal>

class DepositInterestCalculator
{
public:
    static qint64 interestMinor(
        qint64 balanceMinor,
        int annualRateBasisPoints,
        DepositPayoutFrequency frequency
        );

    static QDate firstPayoutAfter(
        const QDate& configuredOn,
        DepositPayoutFrequency frequency,
        int payoutDay
        );

    static QVector<QDate> occurrences(
        const DepositSettings& settings,
        const QDate& from,
        const QDate& through
        );

private:
    static QDate monthlyPayout(int year, int month, int payoutDay);
    static QDate nextMonthlyPayoutAfter(const QDate& date, int payoutDay);
};
