#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>
#include <optional>

// One movement of money of an instrument, seen from the investor: money put into the instrument is negative,
// money taken out of it (sales, payouts, withdrawals) is positive.
struct ReturnFlow {
    qint64 atMillis = 0;
    qint64 minor = 0;
};

struct AnnualReturn {
    std::optional<double> percent; // annual rate in percent, when it can be computed
    QString reason;                // why it cannot be computed
    double days = 0;               // length of the history up to now
};

// Money-weighted annual return of one instrument (XIRR): its flows, and the value it has now.
// A history shorter than 30 days is not annualized: such a rate would only show noise.
AnnualReturn annualReturnOf(const QVector<ReturnFlow>& flows, qint64 nowMillis, qint64 valueNowMinor);

// Average annual return of the chosen instruments, weighted by the money each of them holds now.
// Items are maps: id, group ("investments" or "crypto"), title, valueMinor, annualPercent (null when unknown).
// merged == true gives one average over all of them; otherwise investments and crypto get their own.
QVariantMap averageReturnSummaryOf(const QVariantList& items, const QStringList& excludedIds, bool merged);
