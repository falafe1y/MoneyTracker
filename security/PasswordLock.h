#pragma once
#include "PasswordStore.h"
#include <QJsonObject>
#include <functional>

struct AccessResult {
    bool ok = false;
    bool enabled = true; // Failure never establishes a passwordless session.
    QString error;
    QString recoveryCode;
    qint64 retryAfter = 0;
};

// Called serially on a worker thread. The application owns an installation lock.
class PasswordLock {
public:
    PasswordLock(QString directory, std::shared_ptr<PasswordStore> store,
                 std::function<qint64()> clock = {});
    AccessResult status();
    AccessResult verify(const QString& proof);
    AccessResult change(const QString& proof, const QString& password, bool disable);
    static bool validPassword(const QString& password);
private:
    bool read(QJsonObject& record, QString& error);
    bool write(const QJsonObject& record, QString& error);
    AccessResult verifyRecord(QJsonObject& record, const QString& proof);
    QString marker_;
    std::shared_ptr<PasswordStore> store_;
    std::function<qint64()> clock_;
};
