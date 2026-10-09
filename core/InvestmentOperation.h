#pragma once
#include <QDateTime>
#include <QString>
#include <QtGlobal>

struct InvestmentOperation
{
    QString id;
    QString accountId;
    QString positionId;
    QString instrumentId;
    QString kind; // buy/sell/coupon/dividend/amortization/fee/margin/expiry
    qint64 cashDeltaMinor = 0; // Actual cash movement, includes commissions.
    qint64 quantityDeltaMicros = 0;
    QDateTime occurredAtUtc;
    QString description;
};
