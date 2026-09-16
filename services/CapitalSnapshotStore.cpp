#include "CapitalSnapshotStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
QJsonObject assetEntry(
    const QString& asset,
    const qint64 amountMinor,
    const QString& currency
    )
{
    return {
        {QStringLiteral("asset"), asset},
        {QStringLiteral("amountMinor"), QJsonValue(amountMinor)},
        {QStringLiteral("currency"), currency}
    };
}
}

QString CapitalSnapshotStore::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/capital_snapshots.json");
}

CapitalSnapshotStore::SaveResult CapitalSnapshotStore::saveIfNeeded(
    const QString& filePath,
    const QDate& currentDate,
    const CapitalSnapshot& snapshot
    )
{
    if (filePath.trimmed().isEmpty() || !currentDate.isValid()) {
        return {SaveStatus::Failed,
                QStringLiteral("Invalid capital snapshot path or date")};
    }

    QJsonObject snapshots;
    QFile input(filePath);
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly)) {
            return {SaveStatus::Failed, input.errorString()};
        }

        const QByteArray contents = input.readAll();
        if (input.error() != QFileDevice::NoError) {
            return {SaveStatus::Failed, input.errorString()};
        }
        input.close();

        if (!contents.trimmed().isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(
                contents, &parseError);
            if (parseError.error != QJsonParseError::NoError ||
                !document.isObject()) {
                return {
                    SaveStatus::Failed,
                    parseError.error == QJsonParseError::NoError
                        ? QStringLiteral("Capital snapshot root is not an object")
                        : parseError.errorString()
                };
            }
            snapshots = document.object();
        }
    }

    QDate lastSnapshotDate;
    for (auto iterator = snapshots.constBegin();
         iterator != snapshots.constEnd(); ++iterator) {
        const QDate date = QDate::fromString(iterator.key(), Qt::ISODate);
        if (date.isValid() &&
            (!lastSnapshotDate.isValid() || date > lastSnapshotDate)) {
            lastSnapshotDate = date;
        }
    }

    if (lastSnapshotDate.isValid() && lastSnapshotDate >= currentDate) {
        return {SaveStatus::AlreadyCurrent, {}};
    }

    QJsonArray values;
    values.append(assetEntry(
        QStringLiteral("fiat"), snapshot.fiatMinor, snapshot.currency));
    values.append(assetEntry(
        QStringLiteral("crypto"), snapshot.cryptoMinor, snapshot.currency));
    values.append(assetEntry(
        QStringLiteral("investment"), snapshot.investmentMinor,
        snapshot.currency));
    snapshots.insert(currentDate.toString(Qt::ISODate), values);

    const QFileInfo fileInfo(filePath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return {SaveStatus::Failed,
                QStringLiteral("Cannot create capital snapshot directory")};
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return {SaveStatus::Failed, output.errorString()};
    }
    const QByteArray contents = QJsonDocument(snapshots).toJson(
        QJsonDocument::Indented);
    if (output.write(contents) != contents.size()) {
        output.cancelWriting();
        return {SaveStatus::Failed, output.errorString()};
    }
    if (!output.commit()) {
        return {SaveStatus::Failed, output.errorString()};
    }

    return {SaveStatus::Saved, {}};
}
