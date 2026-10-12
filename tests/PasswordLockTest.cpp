#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include "security/PasswordLock.h"

class MemoryStore final : public PasswordStore {
public:
    QByteArray bytes;
    bool available = true, failWrite = false, corruptReadback = false;
    int reads = 0, writes = 0;
    bool read(QByteArray& value, QString& error) override {
        ++reads;
        if (!available || bytes.isEmpty()) { error = "unavailable"; return false; }
        value = corruptReadback ? QByteArray("corrupt") : bytes; return true;
    }
    bool write(const QByteArray& value, QString& error) override {
        ++writes;
        if (!available || failWrite) { error = "write failed"; return false; }
        bytes = value; return true;
    }
};
class PasswordLockTest : public QObject {
    Q_OBJECT
private slots:
    void passwordlessDoesNotNeedStore() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>(); store->available = false;
        PasswordLock lock(dir.path(), store);
        QVERIFY(lock.status().ok); QVERIFY(!lock.status().enabled);
        QVERIFY(lock.verify({}).ok); QCOMPARE(store->reads, 0);
    }
    void exactTextAndUnicodeBoundaries() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        PasswordLock lock(dir.path(), store);
        QVERIFY(!PasswordLock::validPassword({}));
        QVERIFY(PasswordLock::validPassword(" "));
        const QString emoji = QString::fromUtf8("🔒");
        QVERIFY(PasswordLock::validPassword(emoji.repeated(1024)));
        QVERIFY(!PasswordLock::validPassword(emoji.repeated(1025)));
        auto result = lock.change({}, " ", false);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!store->bytes.contains("recoveryCode"));
        QVERIFY(!store->bytes.contains(result.recoveryCode.toLatin1()));
        QVERIFY(store->bytes.contains("$argon2id$v=19$m=19456,t=2,p=1$"));
        QVERIFY(lock.verify(" ").ok);
        QVERIFY(!lock.verify("").ok);
        QVERIFY(!lock.verify("  ").ok);
        QVERIFY(lock.change(" ", emoji.repeated(1024), false).ok);
        QVERIFY(lock.verify(emoji.repeated(1024)).ok);
        QVERIFY(!lock.change(emoji.repeated(1024), emoji.repeated(1025), false).ok);
        QVERIFY(lock.verify(emoji.repeated(1024)).ok);
    }
    void recoveryRotationAndDisable() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        PasswordLock lock(dir.path(), store);
        auto first = lock.change({}, "1", false); QVERIFY(first.ok);
        QCOMPARE(first.recoveryCode.size(), 39);
        const auto firstBytes = store->bytes;
        auto normalized = first.recoveryCode.toLower().replace('-', ' ');
        QVERIFY(lock.verify(normalized).ok);
        QVERIFY(!lock.change("wrong", "2", false).ok);
        auto second = lock.change(normalized, "1", false); QVERIFY(second.ok);
        QVERIFY(first.recoveryCode != second.recoveryCode);
        const auto a = QJsonDocument::fromJson(firstBytes).object().value("verifier");
        const auto b = QJsonDocument::fromJson(store->bytes).object().value("verifier");
        QVERIFY(a != b); // Fresh salt even for the same password.
        QVERIFY(!lock.verify(first.recoveryCode).ok);
        QVERIFY(lock.verify(second.recoveryCode).ok);
        QVERIFY(lock.change(second.recoveryCode, {}, true).ok);
        QVERIFY(!lock.status().enabled);
        QVERIFY(QFile::exists(dir.filePath("access.lock")));
        store->available = false;
        QVERIFY(!lock.status().ok); // Disable never restores the never-protected exemption.
    }
    void cooldownSurvivesRestartAndCaps() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        qint64 now = 10000;
        PasswordLock lock(dir.path(), store, [&] { return now; });
        auto created = lock.change({}, "пароль", false); QVERIFY(created.ok);
        for (int i = 0; i < 4; ++i) {
            auto r = lock.verify("wrong"); QVERIFY(!r.ok); QCOMPARE(r.retryAfter, 0);
        }
        QCOMPARE(lock.verify("wrong").retryAfter, 30);
        PasswordLock restarted(dir.path(), store, [&] { return now; });
        QCOMPARE(restarted.verify("пароль").retryAfter, 30);
        QCOMPARE(restarted.verify(created.recoveryCode).retryAfter, 30);
        now += 30;
        for (const int delay : {60, 120, 240, 480, 900, 900}) {
            QCOMPARE(restarted.verify("wrong").retryAfter, delay); now += delay;
        }
        QVERIFY(restarted.verify("пароль").ok);
        QCOMPARE(restarted.status().retryAfter, 0);
        QCOMPARE(restarted.verify("wrong").retryAfter, 0);
    }
    void storageFailureNeverAdmits() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        PasswordLock lock(dir.path(), store);
        store->failWrite = true;
        QVERIFY(!lock.change({}, "secret", false).ok);
        QVERIFY(QFile::exists(dir.filePath("access.lock")));
        PasswordLock restarted(dir.path(), store);
        QVERIFY(!restarted.status().ok);
        QVERIFY(!restarted.verify("secret").ok);
    }
    void readbackAndCorruption() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        PasswordLock lock(dir.path(), store);
        store->corruptReadback = true;
        QVERIFY(!lock.change({}, "secret", false).ok);
        QVERIFY(!lock.status().ok);
        store->corruptReadback = false;
        QVERIFY(lock.verify("secret").ok); // The write had succeeded, only readback failed.
        store->bytes = "{}";
        QVERIFY(!lock.status().ok);
        QVERIFY(!lock.verify("secret").ok);
    }
    void failedAttemptMustPersist() {
        QTemporaryDir dir;
        auto store = std::make_shared<MemoryStore>();
        PasswordLock lock(dir.path(), store);
        QVERIFY(lock.change({}, "secret", false).ok);
        QVERIFY(!lock.verify("wrong").ok);
        store->failWrite = true;
        QVERIFY(!lock.verify("secret").ok); // Reset of failed attempts must persist too.
        store->failWrite = false;
        QVERIFY(lock.verify("secret").ok);
    }
};
QTEST_GUILESS_MAIN(PasswordLockTest)
#include "PasswordLockTest.moc"
