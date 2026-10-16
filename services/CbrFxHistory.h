#pragma once

#include "../core/AnalyticsHistory.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>

// Official CBR dollar rates for the days from `from` to `to` inclusive.
QString cbrUsdHistoryUrl(QDate from, QDate to);

// Parses the CBR XML of cbrUsdHistoryUrl into USD rates (roubles per dollar in micros, source "CBR").
QVector<HistoricalFxRate> parseCbrUsdHistory(const QByteArray& xml, QDateTime observedAtUtc);
