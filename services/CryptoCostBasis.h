#pragma once

#include "InvestmentReturn.h"

#include <QDate>
#include <QMap>
#include <QString>
#include <QVariantList>

// Stablecoins are money pegged to the dollar: their yield would only show the rouble moving, so they are not measured.
bool isStableCoin(const QString& coin);

// CBR dollar rates by day (roubles per dollar, in micros) from the stored history rows of the USD currency.
// An official CBR rate of a day wins over estimated or manual values for the same day.
QMap<QDate, qint64> rubPerUsdByDate(const QVariantList& rows);

struct CoinReturn {
    qint64 valueMinor = 0;        // what the coin is worth now, in kopecks
    AnnualReturn annual;          // annual rate in percent, or the reason it cannot be computed
    QDate firstMissingRateDate;   // earliest trade day without a CBR rate; invalid when every day has one
};

// The annual return of one coin held on an exchange (XIRR over its trades, in roubles).
// quantity: the coin in all sections of the exchange; priceUsd: its current price in dollars, 0 when unknown.
// history: the exchange's history rows (see BybitProvider::parseOperation); each trade is priced in dollars
// and converted to roubles at the CBR rate of its day.
CoinReturn coinReturnOf(const QString& coin, double quantity, double priceUsd, qint64 usdRubNowMicros,
                        const QVariantList& history, const QMap<QDate, qint64>& usdRubByDate, qint64 nowMillis);
