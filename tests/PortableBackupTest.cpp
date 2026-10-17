#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include "storage/PortableBackup.h"
#include "storage/CipherDriver.h"
#include "storage/DatabaseProtection.h"
#include "persistence/FinanceRepository.h"

namespace {
class Keys : public DatabaseKeyStore {
public:
    QByteArray key;
    int writes = 0;
    QByteArray existing(QString&) override { return key; }
    bool save(const QByteArray& value, QString&) override { key = value; ++writes; return true; }
};
const QString Password = QString::fromUtf8("  пароль '日本語' 🔐  ");
const QString Notes = QString::fromUtf8("VEXA_PRIVATE_деньги_123456789");
QByteArray bytes(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
bool write(const QString& path, const QByteArray& data) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size(); }
bool seed(const QString& path) {
    FinanceRepository repository(path);
    return repository.isOpen() && repository.clearAllUserData() && repository.saveNotes(Notes)
        && repository.saveAppCurrency("CNY")
        && repository.insertAccount({"account", "Счёт", AssetType::Fiat, AccountType::Cash, Currency::RUB, 9007199254740993LL});
}
QString notes(const DatabaseAccess& access) {
    FinanceRepository repository(access.path, access.key); QString value;
    if (repository.isOpen()) repository.loadNotes(value); return value;
}
bool sql(const DatabaseAccess& access, const QString& statement) {
    const auto name = QUuid::createUuid().toString(); bool ok = false;
    {
        auto db = addCipherDatabase(name, access.key); db.setDatabaseName(access.path);
        if (db.open()) { QSqlQuery query(db); ok = query.exec(statement); }
    }
    QSqlDatabase::removeDatabase(name); return ok;
}
}
class PortableBackupTest : public QObject {
    Q_OBJECT
private slots:
    void roundTrip_data() {
        QTest::addColumn<bool>("encryptedSource"); QTest::addColumn<bool>("encryptedTarget"); QTest::addColumn<bool>("plaintextExport");
        for (bool source : {false,true}) for (bool target : {false,true}) for (bool plain : {false,true})
            QTest::newRow(qPrintable(QString("source-%1-target-%2-plain-%3").arg(source).arg(target).arg(plain))) << source << target << plain;
    }
    void roundTrip() {
        QFETCH(bool, encryptedSource); QFETCH(bool, encryptedTarget); QFETCH(bool, plaintextExport);
        QTemporaryDir dir; Keys sourceKeys, targetKeys;
        const auto sourcePath = dir.filePath("source.db"), targetPath = dir.filePath("target.db");
        QVERIFY(seed(sourcePath)); QVERIFY(seed(targetPath));
        auto source = DatabaseEncryption::convert(sourcePath, encryptedSource, sourceKeys); QVERIFY(source.ok);
        auto target = DatabaseEncryption::convert(targetPath, encryptedTarget, targetKeys); QVERIFY(target.ok);
        QVERIFY(sql(target.access, "UPDATE notes SET text='destination'"));
        const auto keyBefore = targetKeys.key;
        const auto output = dir.filePath(plaintextExport ? "export.sqlite3" : "export.vexabackup");
        const auto exported = PortableBackup::exportFile(source.access, output, dir.filePath("work"), Password, plaintextExport, "0.1");
        QVERIFY2(exported.ok, qPrintable(exported.error));
        QCOMPARE(bytes(output).contains(Notes.toUtf8()), plaintextExport);
        QCOMPARE(bytes(output).startsWith("VEXABACKUP"), !plaintextExport);
        // A portable file opens without consulting the old device key.
        sourceKeys.key.clear();
        auto imported = PortableBackup::prepareImport(output, dir.filePath("work"), Password, targetKeys, FinanceRepository::migrateBackup);
        QVERIFY2(imported.ok, qPrintable(imported.error));
        QVERIFY(imported.prepared->access.encrypted());
        QVERIFY(!bytes(imported.prepared->access.path).contains(Notes.toUtf8()));
        QCOMPARE(imported.prepared->summary.value("accountCount").toLongLong(), 1);
        QCOMPARE(imported.prepared->summary.value("appVersion").toString(), "0.1");
        QCOMPARE(notes(target.access), "destination");
        const auto restored = DatabaseEncryption::restore(targetPath, imported.prepared->access, targetKeys);
        QVERIFY2(restored.ok, qPrintable(restored.error));
        QCOMPARE(restored.access.encrypted(), encryptedTarget); QCOMPARE(targetKeys.key, keyBefore);
        QCOMPARE(notes(restored.access), Notes);
        {
            FinanceRepository repository(targetPath, targetKeys.key);
            QCOMPARE(repository.loadAppCurrency(), "CNY");
            QCOMPARE(repository.loadAccounts().first().initialBalanceMinor(), 9007199254740993LL);
        }
        QVERIFY(QFile::exists(restored.recoveryPath));
        QCOMPARE(notes({restored.recoveryPath, targetKeys.key}), "destination");
    }
    void passwordRulesAndBytes() {
        QVERIFY(!PortableBackup::validPassword(QString(11, 'a')));
        QVERIFY(PortableBackup::validPassword(QString(12, 'a')));
        QVERIFY(PortableBackup::validPassword(QString(1024, 'a')));
        QVERIFY(!PortableBackup::validPassword(QString(1025, 'a')));
        const auto emoji = QString::fromUtf8("🔐");
        QVERIFY(PortableBackup::validPassword(emoji.repeated(1024)));
        QVERIFY(!PortableBackup::validPassword(emoji.repeated(1025)));
        QTemporaryDir dir; Keys keys; const auto path = dir.filePath("source.db"); QVERIFY(seed(path));
        for (const auto& password : {Password, QString("x'") + QString(64, 'a') + "'"}) {
            const auto output = dir.filePath("copy.vexabackup");
            QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), password, false, "test").ok);
            auto restored = PortableBackup::prepareImport(output, dir.filePath("work"), password, keys, FinanceRepository::migrateBackup);
            QVERIFY2(restored.ok, qPrintable(restored.error));
            QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), password + "x", keys, FinanceRepository::migrateBackup).ok);
            if (password == Password) QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), password.trimmed(), keys, FinanceRepository::migrateBackup).ok);
        }
    }
    void damagedFilesAndUnknownVersions() {
        QTemporaryDir dir; Keys keys; const auto path = dir.filePath("source.db"), output = dir.filePath("copy.vexabackup"); QVERIFY(seed(path));
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        const auto good = bytes(output);
        QList<QByteArray> invalid{good.left(good.size()-1), good+"extra", good.left(16), QByteArray("short")};
        auto version = good; version[15] = 2; invalid.append(version);
        auto corrupt = good; corrupt[corrupt.size()-100] ^= 0x55; invalid.append(corrupt);
        for (const auto& data : invalid) {
            QVERIFY(write(output, data));
            QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
            QCOMPARE(notes({path}), Notes);
        }
        QCOMPARE(keys.writes, 0);
        QVERIFY(write(output, good));
        auto imported = PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup);
        QVERIFY(imported.ok);
        const auto staged = imported.prepared->access.path;
        QVERIFY(write(output, "replaced after preview"));
        QCOMPARE(notes(imported.prepared->access), Notes);
        imported.prepared.reset(); QVERIFY(!QFile::exists(staged));
    }
    void refusesUnknownSchemaAndInvalidRelationships() {
        QTemporaryDir dir; Keys keys; const auto path = dir.filePath("source.db"), output = dir.filePath("copy.vexabackup"); QVERIFY(seed(path));
        QVERIFY(sql({path}, "CREATE TABLE future_feature(id INTEGER PRIMARY KEY)"));
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
        QVERIFY(sql({path}, "DROP TABLE future_feature"));
        QVERIFY(sql({path}, "CREATE TRIGGER unexpected AFTER UPDATE ON notes BEGIN DELETE FROM accounts; END"));
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
        QVERIFY(sql({path}, "DROP TRIGGER unexpected"));
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = addCipherDatabase(connection); db.setDatabaseName(path); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec("PRAGMA foreign_keys=OFF"));
            QVERIFY(q.exec("INSERT INTO transactions(id,account_id,category_id,type,amount_minor,occurred_at,created_at,kind) VALUES('orphan','missing','missing-category',0,100,1,1,'income')"));
        }
        QSqlDatabase::removeDatabase(connection);
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
    }
    void sameColumnsWrittenDifferentlyAreAccepted() {
        // A database upgraded with ALTER TABLE keeps the text of its first CREATE TABLE: only the columns and objects count
        QTemporaryDir dir; Keys keys; const auto path = dir.filePath("source.db"), output = dir.filePath("copy.vexabackup"); QVERIFY(seed(path));
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = addCipherDatabase(connection); db.setDatabaseName(path); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec("PRAGMA writable_schema=ON"));
            QVERIFY(q.exec("UPDATE sqlite_master SET sql = replace(sql, ' (', '  (') WHERE type='table' AND name='accounts'"));
            QVERIFY(q.exec("PRAGMA writable_schema=OFF"));
        }
        QSqlDatabase::removeDatabase(connection);
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        QVERIFY(PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
        QVERIFY(sql({path}, "ALTER TABLE accounts ADD COLUMN extra TEXT"));
        QVERIFY(PortableBackup::exportFile({path}, output, dir.filePath("work"), Password, false, "test").ok);
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup).ok);
    }
    void legacySnapshotsAndWal() {
        QTemporaryDir dir; Keys keys; const auto path = dir.filePath("source.db"), output = dir.filePath("legacy.sqlite3"); QVERIFY(seed(path));
        auto source = DatabaseEncryption::convert(path, true, keys); QVERIFY(source.ok); QString error;
        QVERIFY(DatabaseEncryption::snapshot(source.access, output, error));
        QVERIFY(PortableBackup::prepareImport(output, dir.filePath("work"), {}, keys, FinanceRepository::migrateBackup).ok);
        Keys other;
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), {}, other, FinanceRepository::migrateBackup).ok);
        QVERIFY(write(output+"-wal", "not checkpointed"));
        QVERIFY(!PortableBackup::prepareImport(output, dir.filePath("work"), {}, keys, FinanceRepository::migrateBackup).ok);
        QCOMPARE(other.writes, 0);
    }
    void failedExportPreservesExistingFileAndLiveDatabase() {
        QTemporaryDir dir; const auto path = dir.filePath("source.db"), output = dir.filePath("copy.vexabackup"); QVERIFY(seed(path));
        QVERIFY(write(output, "old copy"));
        QVERIFY(!PortableBackup::exportFile({dir.filePath("missing.db")}, output, dir.filePath("work"), Password, false, "test").ok);
        QCOMPARE(bytes(output), QByteArray("old copy"));
        QVERIFY(!PortableBackup::exportFile({path}, path, dir.filePath("work"), Password, false, "test").ok);
        QCOMPARE(notes({path}), Notes);
    }
    void recoveryPreservesUnreadableOriginals() {
        QTemporaryDir dir; Keys keys;
        const auto source = dir.filePath("source.db"), target = dir.filePath("unreadable.db"), output = dir.filePath("copy.vexabackup");
        QVERIFY(seed(source));
        QVERIFY(PortableBackup::exportFile({source}, output, dir.filePath("work"), Password, false, "test").ok);
        auto prepared = PortableBackup::prepareImport(output, dir.filePath("work"), Password, keys, FinanceRepository::migrateBackup);
        QVERIFY(prepared.ok);
        for (const auto& suffix : {QString(), QString("-wal"), QString("-shm")}) QVERIFY(write(target + suffix, "original" + suffix.toUtf8()));
        const auto recovered = DatabaseEncryption::recover(target, prepared.prepared->access, keys);
        QVERIFY2(recovered.ok, qPrintable(recovered.error)); QVERIFY(recovered.access.encrypted());
        QCOMPARE(keys.writes, 1); QCOMPARE(notes(recovered.access), Notes);
        for (const auto& suffix : {QString(), QString("-wal"), QString("-shm")})
            QCOMPARE(bytes(recovered.recoveryPath + "/unreadable.db" + suffix), "original" + suffix.toUtf8());
        QVERIFY(!QFile::exists(target + "-wal"));
    }
    void previewIsBoundToUnlockedSession() {
        QTemporaryDir dir; const auto path = dir.filePath("live.db"), output = dir.filePath("copy.vexabackup"); QVERIFY(seed(path));
        DatabaseProtection manager(path); manager.setBackupSchemaValidator(FinanceRepository::migrateBackup);
        manager.setAdmitted(true); manager.prepare(); QVERIFY(QTest::qWaitFor([&] { return manager.ready(); }, 10000));
        QSignalSpy exported(&manager, &DatabaseProtection::backupExported);
        manager.exportBackup(QUrl::fromLocalFile(output), Password, false);
        QVERIFY(QTest::qWaitFor([&] { return !manager.busy(); }, 10000)); QCOMPARE(exported.count(), 1);
        manager.inspectBackup(QUrl::fromLocalFile(output), {}); QVERIFY(manager.backupPasswordRequired()); QVERIFY(!manager.hasBackupPreview());
        manager.inspectBackup(QUrl::fromLocalFile(output), Password); QVERIFY(QTest::qWaitFor([&] { return !manager.busy(); }, 10000));
        QVERIFY2(manager.hasBackupPreview(), qPrintable(manager.backupError()));
        manager.discardBackup(); QVERIFY(!manager.hasBackupPreview());
        QCOMPARE(QDir(dir.filePath(".vexa-portable")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
        manager.inspectBackup(QUrl::fromLocalFile(output), Password);
        manager.setAdmitted(false); manager.releaseSession(); manager.setAdmitted(true); manager.prepare();
        QVERIFY(QTest::qWaitFor([&] { return !manager.busy(); }, 10000)); QVERIFY(QTest::qWaitFor([&] { return manager.ready(); }, 10000)); QVERIFY(!manager.hasBackupPreview());
        QSignalSpy restore(&manager, &DatabaseProtection::restoreRequested);
        manager.restoreBackup(); QCOMPARE(restore.count(), 0);
        manager.inspectBackup(QUrl::fromLocalFile(output), Password); QVERIFY(QTest::qWaitFor([&] { return !manager.busy(); }, 10000)); QVERIFY(manager.hasBackupPreview());
        manager.restoreBackup(); QCOMPARE(restore.count(), 1); QVERIFY(manager.busy()); QVERIFY(!manager.ready());
        manager.restoreBackup(); QCOMPARE(restore.count(), 1);
        QCOMPARE(notes({path}), Notes); // Only the owner may install after runtime teardown.
    }
};
QTEST_GUILESS_MAIN(PortableBackupTest)
#include "PortableBackupTest.moc"
