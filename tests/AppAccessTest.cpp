#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "security/AppAccess.h"

namespace {
class TestStore final : public PasswordStore {
public:
    QByteArray data;
    bool accessible = true;
    bool available(QString& error) override {
        if (!accessible) error = QStringLiteral("Системное хранилище недоступно.");
        return accessible;
    }
    bool read(QByteArray& value, QString&) override { value = data; return !data.isEmpty(); }
    bool write(const QByteArray& value, QString&) override { data = value; return true; }
};
}
class AppAccessTest : public QObject {
    Q_OBJECT
private slots:
    void unavailableStoreDisablesSetupWithoutInventingProtection() {
        QTemporaryDir dir;auto store = std::make_shared<TestStore>();store->accessible = false;
        auto manager = std::make_shared<PasswordLock>(dir.path(),store);AppAccess access(manager);
        QVERIFY(!access.storeChecked());QVERIFY(!access.storeAvailable());access.start();
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); },5000));
        QVERIFY(access.storeChecked());QVERIFY(!access.storeAvailable());QVERIFY(!access.storeError().isEmpty());
        QVERIFY(access.ready());QVERIFY(!access.locked());QVERIFY(!access.enabled());
        access.changePassword({},"password","password");QVERIFY(!access.busy());QVERIFY(store->data.isEmpty());
        QVERIFY(!manager->change({},"password",false).ok);
        QVERIFY(!QFile::exists(dir.filePath("access.lock")));
    }
    void unavailableStoreDoesNotBypassExistingPassword() {
        QTemporaryDir dir;auto store = std::make_shared<TestStore>();
        auto manager = std::make_shared<PasswordLock>(dir.path(),store);QVERIFY(manager->change({},"secret",false).ok);
        store->accessible = false;AppAccess access(manager);access.start();
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); },5000));
        QVERIFY(access.storeChecked());QVERIFY(!access.storeAvailable());QVERIFY(access.locked());QVERIFY(!access.ready());
        access.unlock("secret");QVERIFY(!access.busy());QVERIFY(access.locked());
        store->accessible = true;access.retry();QVERIFY(QTest::qWaitFor([&] { return !access.busy(); },5000));
        QVERIFY(access.storeAvailable());QVERIFY(access.storeError().isEmpty());QVERIFY(access.ready());QVERIFY(access.locked());
        access.unlock("secret");QVERIFY(QTest::qWaitFor([&] { return !access.busy(); },5000));QVERIFY(!access.locked());
    }
    void nativeProbeDetectsMissingToolWithoutWritingCredentials() {
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        const auto original = qgetenv("PATH");qputenv("PATH", QByteArray());
        QString error;const bool available = nativePasswordStore()->available(error);
        qputenv("PATH",original);QVERIFY(!available);QVERIFY(error.contains("secret-tool"));
#endif
    }
    void startupAndCredentialLifecycle() {
        QTemporaryDir dir;
        auto store = std::make_shared<TestStore>();
        auto manager = std::make_shared<PasswordLock>(dir.path(), store);
        AppAccess access(manager);
        QVERIFY(access.locked());
        access.start();
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000));
        QVERIFY(access.ready()); QVERIFY(!access.locked()); QVERIFY(!access.enabled());
        access.systemLock(); QVERIFY(!access.locked());
        access.changePassword({}, "я", "я");
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000));
        QVERIFY(access.enabled()); QVERIFY(!access.recoveryCode().isEmpty());
        access.unlock("я"); QVERIFY(access.locked()); // Acknowledge recovery first.
        access.acknowledgeRecovery();
        access.unlock("wrong"); QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000)); QVERIFY(access.locked());
        access.unlock("я"); QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000)); QVERIFY(!access.locked());
        access.changePassword("wrong", "new", "new");
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000));
        QVERIFY(!access.locked());
        access.disablePassword("я");
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000)); QVERIFY(!access.enabled()); QVERIFY(!access.locked());
    }
    void systemLockInvalidatesPendingUnlock() {
        QTemporaryDir dir;
        auto manager = std::make_shared<PasswordLock>(dir.path(), std::make_shared<TestStore>());
        QVERIFY(manager->change({}, "secret", false).ok);
        AppAccess access(manager);
        access.start(); QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000));
        QVERIFY(access.locked());
        access.unlock("secret");
        access.systemLock();
        QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000)); QVERIFY(access.locked());
        access.unlock("secret"); QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000)); QVERIFY(!access.locked());
        access.systemLock(); QVERIFY(access.locked());
    }
    void confirmationAndBoundsDoNotChangeCredentials() {
        QTemporaryDir dir;
        auto manager = std::make_shared<PasswordLock>(dir.path(), std::make_shared<TestStore>());
        AppAccess access(manager);
        access.start(); QVERIFY(QTest::qWaitFor([&] { return !access.busy(); }, 5000));
        access.changePassword({}, "a", "b"); QVERIFY(!access.busy()); QVERIFY(!access.locked());
        access.changePassword({}, QString(1025, 'a'), QString(1025, 'a'));
        QVERIFY(!access.busy()); QVERIFY(!access.enabled());
        QVERIFY(!manager->status().enabled);
    }
};
QTEST_GUILESS_MAIN(AppAccessTest)
#include "AppAccessTest.moc"
