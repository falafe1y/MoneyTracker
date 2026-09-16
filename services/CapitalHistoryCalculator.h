#pragma once

#include <QDate>
#include <QtGlobal>
#include <QVector>

struct CapitalHistoryEvent
{
    QDate date;
    qint64 deltaMinor = 0;
};

struct CapitalHistoryPoint
{
    QDate date;
    qint64 totalMinor = 0;
};

enum class CapitalHistoryResolution
{
    Day,
    Month,
    Year
};

struct CapitalHistorySeries
{
    QVector<CapitalHistoryPoint> points;
    CapitalHistoryResolution resolution = CapitalHistoryResolution::Day;
};

class CapitalHistoryCalculator final
{
public:
    static CapitalHistorySeries calculate(
        qint64 openingMinor,
        const QVector<CapitalHistoryEvent>& events,
        const QDate& currentDate,
        const QDate& from = {},
        const QDate& to = {}
        );
};
