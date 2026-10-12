#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QUuid>
#include "storage/DatabaseEncryption.h"
#include "storage/CipherDriver.h"
#include "persistence/FinanceRepository.h"

class TestDatabaseKeys final : public DatabaseKeyStore {
public:
    QByteArray key;
    bool unavailable = false, corruptReadback = false;
    int writes = 0;
    QByteArray existing(QString& error) override {
        if (unavailable) { error = "unavailable"; return {}; }
        if (corruptReadback && !key.isEmpty()) return QByteArray(32, 'x');
        return key;
    }
    bool save(const QByteArray& value, QString& error) override {
        if (unavailable) { error = "unavailable"; return false; }
        key = value; ++writes; return true;
    }
};
namespace {
const QString secret = QString::fromUtf8("Vexa_private_секрет_7654321");
QByteArray bytes(const QString& path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
bool seed(const QString& path, const QByteArray& key = {}) {
    const auto name = QUuid::createUuid().toString();
    bool ok;
    {
        auto db = addCipherDatabase(name, key); db.setDatabaseName(path);
        ok = db.open();
        if (ok) {
            QSqlQuery q(db);
            ok = q.exec("CREATE TABLE sample(id INTEGER PRIMARY KEY, text TEXT, amount INTEGER, raw BLOB)")
                && q.prepare("INSERT INTO sample VALUES(?,?,?,?)");
            q.addBindValue(1); q.addBindValue(secret); q.addBindValue(qlonglong(9007199254740993LL)); q.addBindValue(QByteArray("a\0b", 3));
            ok = ok && q.exec();
        }
    }
    QSqlDatabase::removeDatabase(name); return ok;
}
bool contents(const DatabaseAccess& access) {
    const auto name = QUuid::createUuid().toString(); bool ok = false;
    {
        auto db = addCipherDatabase(name, access.key); db.setDatabaseName(access.path);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (db.open()) {
            QSqlQuery q(db);
            ok = q.exec("SELECT text, amount, raw FROM sample WHERE id=1") && q.next()
                && q.value(0).toString() == secret && q.value(1).toLongLong() == 9007199254740993LL
                && q.value(2).toByteArray() == QByteArray("a\0b", 3);
        }
    }
    QSqlDatabase::removeDatabase(name); return ok;
}
}
class DatabaseEncryptionTest : public QObject {
    Q_OBJECT
private slots:
    void freshDatabaseSurvivesReopen() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("new.db");
        QVERIFY(DatabaseEncryption::open(path, keys).ok);
        QVERIFY(DatabaseEncryption::open(path, keys).ok);
        QCOMPARE(keys.writes, 0);
        auto encrypted = DatabaseEncryption::convert(path, true, keys);
        QVERIFY2(encrypted.ok, qPrintable(encrypted.error));
        QVERIFY(QFile::remove(path));
        const auto recreated = DatabaseEncryption::open(path, keys);
        QVERIFY(recreated.ok); QVERIFY(recreated.access.encrypted());
        QVERIFY(DatabaseEncryption::open(path, keys).ok); QCOMPARE(keys.writes, 1);
    }
    void enableDisableAndReopen() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("кириллица.sqlite3");
        QVERIFY(seed(path)); QVERIFY(bytes(path).startsWith("SQLite format 3"));
        const auto enabled = DatabaseEncryption::convert(path, true, keys);
        QVERIFY2(enabled.ok, qPrintable(enabled.error)); QVERIFY(enabled.access.encrypted());
        QCOMPARE(keys.writes, 1); QCOMPARE(keys.key.size(), 32);
        QVERIFY(!bytes(path).startsWith("SQLite format 3")); QVERIFY(!bytes(path).contains(secret.toUtf8()));
        QVERIFY(QFile::exists(path + ".encrypted")); QVERIFY(enabled.recoveryPath.isEmpty());
        QVERIFY(contents(enabled.access)); QVERIFY(!contents({path, {}}));
        auto wrong = enabled.access; wrong.key = QByteArray(32, 'q'); QVERIFY(!contents(wrong));
        auto reopened = DatabaseEncryption::open(path, keys); QVERIFY(reopened.ok); QVERIFY(contents(reopened.access));
        const auto backup = dir.filePath("protected.sqlite3"); QString error;
        QVERIFY2(DatabaseEncryption::snapshot(enabled.access, backup, error), qPrintable(error));
        QVERIFY(!bytes(backup).contains(secret.toUtf8()));
        const auto disabled = DatabaseEncryption::convert(path, false, keys);
        QVERIFY2(disabled.ok, qPrintable(disabled.error)); QVERIFY(!disabled.access.encrypted());
        QVERIFY(bytes(path).startsWith("SQLite format 3")); QVERIFY(contents(disabled.access));
        QVERIFY(!QFile::exists(path + ".encrypted")); QCOMPARE(keys.writes, 1);
        QVERIFY(QFile::exists(disabled.recoveryPath));
        const auto old = DatabaseEncryption::openSnapshot(backup, keys); QVERIFY(old.ok); QVERIFY(contents(old.access));
        const auto reenabled = DatabaseEncryption::convert(path, true, keys);
        QVERIFY2(reenabled.ok, qPrintable(reenabled.error)); QCOMPARE(keys.writes, 1); QVERIFY(contents(reenabled.access));
    }
    void missingKeyDoesNotReplaceDatabase() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("data.db"); QVERIFY(seed(path));
        QVERIFY(DatabaseEncryption::convert(path, true, keys).ok);
        const auto original = bytes(path); keys.key.clear();
        auto opened = DatabaseEncryption::open(path, keys); QVERIFY(!opened.ok);
        QVERIFY(!DatabaseEncryption::convert(path, false, keys).ok);
        QCOMPARE(keys.writes, 1); QCOMPARE(bytes(path), original);
    }
    void keyWriteMustBeReadBack() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("data.db"); QVERIFY(seed(path));
        const auto original = bytes(path); keys.corruptReadback = true;
        QVERIFY(!DatabaseEncryption::convert(path, true, keys).ok);
        QCOMPARE(bytes(path), original); QVERIFY(!QFile::exists(path + ".encrypted"));
    }
    void unavailableStoreLeavesPlaintextUsable() {
        QTemporaryDir dir; TestDatabaseKeys keys; keys.unavailable = true;
        const auto path = dir.filePath("data.db"); QVERIFY(seed(path));
        const auto opened = DatabaseEncryption::open(path, keys); QVERIFY(opened.ok); QVERIFY(!opened.access.encrypted());
        QVERIFY(!DatabaseEncryption::convert(path, true, keys).ok); QVERIFY(contents(opened.access));
    }
    void tamperingFailsIntegrity() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("data.db"); QVERIFY(seed(path));
        auto encrypted = DatabaseEncryption::convert(path, true, keys); QVERIFY(encrypted.ok);
        QFile f(path); QVERIFY(f.open(QIODevice::ReadWrite)); QVERIFY(f.seek(f.size()-128));
        char byte; QCOMPARE(f.read(&byte, 1), 1); byte ^= 0x55;
        QVERIFY(f.seek(f.size()-128)); QCOMPARE(f.write(&byte, 1), 1); f.close();
        QString error; QVERIFY(!DatabaseEncryption::validate(encrypted.access, error));
    }
    void walIsEncryptedAndIncludedInConversion() {
        QTemporaryDir dir; TestDatabaseKeys keys;
        const auto path = dir.filePath("data.db"); QVERIFY(seed(path));
        auto encrypted = DatabaseEncryption::convert(path, true, keys); QVERIFY(encrypted.ok);
        const auto name = QUuid::createUuid().toString();
        {
            auto db = addCipherDatabase(name, keys.key); db.setDatabaseName(path); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec("PRAGMA journal_mode=WAL")); q.finish();
            QVERIFY(q.exec("PRAGMA wal_autocheckpoint=0"));
            QVERIFY(q.prepare("INSERT INTO sample(id,text) VALUES(2,?)")); q.addBindValue(secret); QVERIFY(q.exec());
            QVERIFY(QFileInfo(path+"-wal").size() > 0);
            QVERIFY(!bytes(path+"-wal").contains(secret.toUtf8()));
            QString error; QVERIFY(DatabaseEncryption::snapshot(encrypted.access, dir.filePath("with-wal.db"), error));
            q.prepare("SELECT count(*) FROM sample"); QVERIFY(q.exec()); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 2);
        }
        QSqlDatabase::removeDatabase(name);
        auto decrypted = DatabaseEncryption::convert(path, false, keys); QVERIFY2(decrypted.ok, qPrintable(decrypted.error));
        const auto check = QUuid::createUuid().toString();
        {
            auto db = addCipherDatabase(check); db.setDatabaseName(path); QVERIFY(db.open());
            QSqlQuery q(db); QVERIFY(q.exec("SELECT count(*) FROM sample")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 2);
        }
        QSqlDatabase::removeDatabase(check);
    }
    void restoreKeepsDestinationEncryption() {
        QTemporaryDir dir; auto keys = std::make_shared<TestDatabaseKeys>();
        const auto path = dir.filePath("finance.db"); const auto backup = dir.filePath("copy.sqlite3");
        { FinanceRepository repository(path, {}, keys); QVERIFY2(repository.isOpen(), qPrintable(repository.lastError())); QVERIFY(repository.saveNotes(secret)); }
        const auto plainBackup = dir.filePath("plain.sqlite3");
        { FinanceRepository repository(path, {}, keys); QVERIFY(repository.backupDatabase(plainBackup)); }
        auto encrypted = DatabaseEncryption::convert(path, true, *keys); QVERIFY2(encrypted.ok, qPrintable(encrypted.error));
        {
            FinanceRepository repository(path, keys->key, keys); QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
            QVERIFY(repository.saveNotes("before plain restore"));
            QVERIFY2(repository.restoreDatabase(plainBackup), qPrintable(repository.lastError()));
            QString plainNotes; QVERIFY(repository.loadNotes(plainNotes)); QCOMPARE(plainNotes, secret);
            QVERIFY2(repository.backupDatabase(backup), qPrintable(repository.lastError()));
            QVERIFY(repository.saveNotes("changed"));
            QVERIFY2(repository.restoreDatabase(backup), qPrintable(repository.lastError()));
            QString notes; QVERIFY(repository.loadNotes(notes)); QCOMPARE(notes, secret);
        }
        QVERIFY(!bytes(path).startsWith("SQLite format 3"));
        auto plaintext = DatabaseEncryption::convert(path, false, *keys); QVERIFY(plaintext.ok);
        {
            FinanceRepository repository(path, {}, keys); QVERIFY(repository.isOpen());
            QVERIFY(repository.saveNotes("changed again"));
            QVERIFY2(repository.restoreDatabase(backup), qPrintable(repository.lastError()));
            QString notes; QVERIFY(repository.loadNotes(notes)); QCOMPARE(notes, secret);
        }
        QVERIFY(bytes(path).startsWith("SQLite format 3"));
    }
};
QTEST_GUILESS_MAIN(DatabaseEncryptionTest)
#include "DatabaseEncryptionTest.moc"
