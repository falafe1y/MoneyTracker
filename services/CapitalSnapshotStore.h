#pragma once

#include <QDate>
#include <QString>
#include <QtGlobal>
#include <QVector>

struct CapitalSnapshot
{
    QString currency;
    qint64 fiatMinor = 0;
    qint64 cryptoMinor = 0;
    qint64 investmentMinor = 0;
};

struct CapitalSnapshotPoint : CapitalSnapshot
{
    QDate date;
};

enum class CapitalHistoryResolution
{
    Day,
    Month,
    Year
};

struct CapitalHistorySeries
{
    QVector<CapitalSnapshotPoint> points;
    CapitalHistoryResolution resolution = CapitalHistoryResolution::Day;
};

class CapitalSnapshotStore final
{
public:
    enum class SaveStatus
    {
        Saved,
        AlreadyCurrent,
        Failed
    };

    struct SaveResult
    {
        SaveStatus status = SaveStatus::Failed;
        QString error;
    };

    struct LoadResult
    {
        QVector<CapitalSnapshotPoint> snapshots;
        QString error;
    };

    static QString defaultFilePath();

    static SaveResult saveIfNeeded(
        const QString& filePath,
        const QDate& currentDate,
        const CapitalSnapshot& snapshot
        );

    static LoadResult load(const QString& filePath);

    static CapitalHistorySeries seriesForRange(
        const QVector<CapitalSnapshotPoint>& snapshots,
        const QDate& from = {},
        const QDate& to = {}
        );
};
