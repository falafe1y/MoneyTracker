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

QTEST_APPLESS_MAIN(CapitalSnapshotStoreTest)

#include "CapitalSnapshotStoreTest.moc"
