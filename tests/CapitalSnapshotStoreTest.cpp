#include "../services/CapitalSnapshotStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class CapitalSnapshotStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void writesOnlyThreeAssetTotals();
    void doesNotOverwriteSnapshotForSameDate();
    void skipsMissedDaysAndAppendsCurrentDate();
    void doesNotReplaceInvalidJson();
    void loadsSnapshotsInDateOrder();
    void filtersAndGroupsHistoryByRange();
};

namespace
{
QJsonObject readSnapshots(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

CapitalSnapshotPoint point(
    const QDate& date,
    const qint64 fiatMinor
    )
{
    CapitalSnapshotPoint result;
    result.date = date;
    result.currency = QStringLiteral("RUB");
    result.fiatMinor = fiatMinor;
    return result;
}
}

void CapitalSnapshotStoreTest::writesOnlyThreeAssetTotals()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(
        QStringLiteral("nested/capital_snapshots.json"));
    const QDate date(2026, 9, 16);

    const auto result = CapitalSnapshotStore::saveIfNeeded(
        filePath, date, {QStringLiteral("RUB"), 12'345, 6'789, 4'200});

    QCOMPARE(result.status, CapitalSnapshotStore::SaveStatus::Saved);
    QVERIFY(result.error.isEmpty());

    const QJsonObject root = readSnapshots(filePath);
    QCOMPARE(root.size(), 1);
    const QJsonArray values = root.value(QStringLiteral("2026-09-16")).toArray();
    QCOMPARE(values.size(), 3);
    QCOMPARE(values.at(0).toObject().value(QStringLiteral("asset")).toString(),
             QStringLiteral("fiat"));
    QCOMPARE(values.at(0).toObject().value(
                 QStringLiteral("amountMinor")).toInteger(), qint64(12'345));
    QCOMPARE(values.at(0).toObject().value(
                 QStringLiteral("currency")).toString(), QStringLiteral("RUB"));
    QCOMPARE(values.at(1).toObject().value(QStringLiteral("asset")).toString(),
             QStringLiteral("crypto"));
    QCOMPARE(values.at(1).toObject().value(
                 QStringLiteral("amountMinor")).toInteger(), qint64(6'789));
    QCOMPARE(values.at(2).toObject().value(QStringLiteral("asset")).toString(),
             QStringLiteral("investment"));
    QCOMPARE(values.at(2).toObject().value(
                 QStringLiteral("amountMinor")).toInteger(), qint64(4'200));
}

void CapitalSnapshotStoreTest::doesNotOverwriteSnapshotForSameDate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(
        QStringLiteral("capital_snapshots.json"));
    const QDate date(2026, 9, 16);

    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, date, {QStringLiteral("RUB"), 100, 200, 300}).status,
             CapitalSnapshotStore::SaveStatus::Saved);
    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, date, {QStringLiteral("RUB"), 900, 800, 700}).status,
             CapitalSnapshotStore::SaveStatus::AlreadyCurrent);

    const QJsonArray values = readSnapshots(filePath)
        .value(QStringLiteral("2026-09-16")).toArray();
    QCOMPARE(values.at(0).toObject().value(
                 QStringLiteral("amountMinor")).toInteger(), qint64(100));
}

void CapitalSnapshotStoreTest::skipsMissedDaysAndAppendsCurrentDate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(
        QStringLiteral("capital_snapshots.json"));

    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, QDate(2026, 9, 13),
                 {QStringLiteral("RUB"), 1, 2, 3}).status,
             CapitalSnapshotStore::SaveStatus::Saved);
    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, QDate(2026, 9, 16),
                 {QStringLiteral("RUB"), 4, 5, 6}).status,
             CapitalSnapshotStore::SaveStatus::Saved);

    const QJsonObject root = readSnapshots(filePath);
    QCOMPARE(root.size(), 2);
    QVERIFY(root.contains(QStringLiteral("2026-09-13")));
    QVERIFY(!root.contains(QStringLiteral("2026-09-14")));
    QVERIFY(!root.contains(QStringLiteral("2026-09-15")));
    QVERIFY(root.contains(QStringLiteral("2026-09-16")));
}

void CapitalSnapshotStoreTest::doesNotReplaceInvalidJson()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(
        QStringLiteral("capital_snapshots.json"));
    const QByteArray invalidContents("{broken-json");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(invalidContents), invalidContents.size());
    file.close();

    const auto result = CapitalSnapshotStore::saveIfNeeded(
        filePath, QDate(2026, 9, 16),
        {QStringLiteral("RUB"), 1, 2, 3});

    QCOMPARE(result.status, CapitalSnapshotStore::SaveStatus::Failed);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), invalidContents);
}

void CapitalSnapshotStoreTest::loadsSnapshotsInDateOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(
        QStringLiteral("capital_snapshots.json"));

    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, QDate(2026, 9, 13),
                 {QStringLiteral("RUB"), 100, 20, 3}).status,
             CapitalSnapshotStore::SaveStatus::Saved);
    QCOMPARE(CapitalSnapshotStore::saveIfNeeded(
                 filePath, QDate(2026, 9, 16),
                 {QStringLiteral("RUB"), 400, 50, 6}).status,
             CapitalSnapshotStore::SaveStatus::Saved);

    const CapitalSnapshotStore::LoadResult result =
        CapitalSnapshotStore::load(filePath);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.snapshots.size(), 2);
    QCOMPARE(result.snapshots.at(0).date, QDate(2026, 9, 13));
    QCOMPARE(result.snapshots.at(0).fiatMinor, qint64(100));
    QCOMPARE(result.snapshots.at(0).cryptoMinor, qint64(20));
    QCOMPARE(result.snapshots.at(0).investmentMinor, qint64(3));
    QCOMPARE(result.snapshots.at(1).date, QDate(2026, 9, 16));
    QCOMPARE(result.snapshots.at(1).currency, QStringLiteral("RUB"));
}

void CapitalSnapshotStoreTest::filtersAndGroupsHistoryByRange()
{
    const QVector<CapitalSnapshotPoint> snapshots{
        point(QDate(2024, 1, 3), 100),
        point(QDate(2024, 1, 28), 200),
        point(QDate(2024, 2, 8), 300),
        point(QDate(2024, 2, 26), 400),
        point(QDate(2025, 3, 4), 500),
        point(QDate(2025, 12, 29), 600),
        point(QDate(2026, 5, 7), 700)
    };

    const CapitalHistorySeries daily = CapitalSnapshotStore::seriesForRange(
        snapshots, QDate(2024, 1, 1), QDate(2024, 2, 10));
    QCOMPARE(daily.resolution, CapitalHistoryResolution::Day);
    QCOMPARE(daily.points.size(), 3);
    QCOMPARE(daily.points.constLast().date, QDate(2024, 2, 8));

    const CapitalHistorySeries monthly = CapitalSnapshotStore::seriesForRange(
        snapshots, QDate(2024, 1, 1), QDate(2024, 12, 31));
    QCOMPARE(monthly.resolution, CapitalHistoryResolution::Month);
    QCOMPARE(monthly.points.size(), 2);
    QCOMPARE(monthly.points.at(0).date, QDate(2024, 1, 28));
    QCOMPARE(monthly.points.at(0).fiatMinor, qint64(200));
    QCOMPARE(monthly.points.at(1).date, QDate(2024, 2, 26));

    const CapitalHistorySeries yearly = CapitalSnapshotStore::seriesForRange(
        snapshots, QDate(2024, 1, 1), QDate(2026, 12, 31));
    QCOMPARE(yearly.resolution, CapitalHistoryResolution::Year);
    QCOMPARE(yearly.points.size(), 3);
    QCOMPARE(yearly.points.at(0).date, QDate(2024, 2, 26));
    QCOMPARE(yearly.points.at(1).date, QDate(2025, 12, 29));
    QCOMPARE(yearly.points.at(2).date, QDate(2026, 5, 7));
}

QTEST_APPLESS_MAIN(CapitalSnapshotStoreTest)

#include "CapitalSnapshotStoreTest.moc"
