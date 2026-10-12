#include <QtTest>
#include <QTemporaryDir>
#include "security/AppAccess.h"

namespace {
class TestStore final : public PasswordStore {
public:
    QByteArray data;
    bool read(QByteArray& value, QString&) override { value = data; return !data.isEmpty(); }
    bool write(const QByteArray& value, QString&) override { data = value; return true; }
};
}
class AppAccessTest : public QObject {
    Q_OBJECT
private slots:
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
