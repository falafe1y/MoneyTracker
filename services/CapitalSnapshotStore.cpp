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

#include <algorithm>
#include <utility>

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

CapitalSnapshotStore::LoadResult CapitalSnapshotStore::load(
    const QString& filePath
    )
{
    QFile input(filePath);
    if (!input.exists()) {
        return {};
    }
    if (!input.open(QIODevice::ReadOnly)) {
        return {{}, input.errorString()};
    }

    const QByteArray contents = input.readAll();
    if (input.error() != QFileDevice::NoError) {
        return {{}, input.errorString()};
    }
    if (contents.trimmed().isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        contents, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return {
            {},
            parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Capital snapshot root is not an object")
                : parseError.errorString()
        };
    }

    QVector<CapitalSnapshotPoint> snapshots;
    const QJsonObject root = document.object();
    snapshots.reserve(root.size());
    for (auto iterator = root.constBegin(); iterator != root.constEnd();
         ++iterator) {
        CapitalSnapshotPoint point;
        point.date = QDate::fromString(iterator.key(), Qt::ISODate);
        if (!point.date.isValid() || !iterator.value().isArray()) {
            continue;
        }

        bool hasFiat = false;
        bool hasCrypto = false;
        bool hasInvestment = false;
        bool currencyIsConsistent = true;
        const QJsonArray values = iterator.value().toArray();
        for (const QJsonValue& value : values) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            const QJsonValue amount = object.value(
                QStringLiteral("amountMinor"));
            if (!amount.isDouble()) {
                continue;
            }

            const QString currency = object.value(
                QStringLiteral("currency")).toString().trimmed().toUpper();
            if (!currency.isEmpty()) {
                if (point.currency.isEmpty()) {
                    point.currency = currency;
                } else if (point.currency != currency) {
                    currencyIsConsistent = false;
                }
            }

            const QString asset = object.value(
                QStringLiteral("asset")).toString();
            if (asset == QStringLiteral("fiat")) {
                point.fiatMinor = amount.toInteger();
                hasFiat = true;
            } else if (asset == QStringLiteral("crypto")) {
                point.cryptoMinor = amount.toInteger();
                hasCrypto = true;
            } else if (asset == QStringLiteral("investment")) {
                point.investmentMinor = amount.toInteger();
                hasInvestment = true;
            }
        }

        if (!currencyIsConsistent || !hasFiat || !hasCrypto ||
            !hasInvestment) {
            continue;
        }
        if (point.currency.isEmpty()) {
            point.currency = QStringLiteral("RUB");
        }
        snapshots.append(point);
    }

    std::sort(
        snapshots.begin(), snapshots.end(),
        [](const CapitalSnapshotPoint& left,
           const CapitalSnapshotPoint& right)
        {
            return left.date < right.date;
        });
    return {snapshots, {}};
}

CapitalHistorySeries CapitalSnapshotStore::seriesForRange(
    const QVector<CapitalSnapshotPoint>& snapshots,
    const QDate& from,
    const QDate& to
    )
{
    CapitalHistorySeries result;
    if ((from.isValid() && to.isValid() && from > to) ||
        snapshots.isEmpty()) {
        return result;
    }

    QVector<CapitalSnapshotPoint> sorted = snapshots;
    std::sort(
        sorted.begin(), sorted.end(),
        [](const CapitalSnapshotPoint& left,
           const CapitalSnapshotPoint& right)
        {
            return left.date < right.date;
        });

    const QDate effectiveFrom = from.isValid()
        ? from
        : sorted.constFirst().date;
    const QDate effectiveTo = to.isValid()
        ? to
        : sorted.constLast().date;
    const qint64 daySpan = qMax<qint64>(
        0, effectiveFrom.daysTo(effectiveTo));
    if (daySpan <= 62) {
        result.resolution = CapitalHistoryResolution::Day;
    } else if (daySpan <= 730) {
        result.resolution = CapitalHistoryResolution::Month;
    } else {
        result.resolution = CapitalHistoryResolution::Year;
    }

    const auto samePeriod = [&result](
        const QDate& left,
        const QDate& right
        )
    {
        if (result.resolution == CapitalHistoryResolution::Day) {
            return left == right;
        }
        if (result.resolution == CapitalHistoryResolution::Month) {
            return left.year() == right.year() &&
                   left.month() == right.month();
        }
        return left.year() == right.year();
    };

    for (const CapitalSnapshotPoint& point : std::as_const(sorted)) {
        if ((from.isValid() && point.date < from) ||
            (to.isValid() && point.date > to)) {
            continue;
        }
        if (!result.points.isEmpty() &&
            samePeriod(result.points.constLast().date, point.date)) {
            result.points.last() = point;
        } else {
            result.points.append(point);
        }
    }
    return result;
}
