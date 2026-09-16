#pragma once

#include <QDate>
#include <QString>
#include <QtGlobal>

struct CapitalSnapshot
{
    QString currency;
    qint64 fiatMinor = 0;
    qint64 cryptoMinor = 0;
    qint64 investmentMinor = 0;
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

    static QString defaultFilePath();

    static SaveResult saveIfNeeded(
        const QString& filePath,
        const QDate& currentDate,
        const CapitalSnapshot& snapshot
        );
};
