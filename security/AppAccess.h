#pragma once
#include "PasswordLock.h"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>

class AppAccess final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString recoveryCode READ recoveryCode NOTIFY changed)
    Q_PROPERTY(int retryAfter READ retryAfter NOTIFY changed)
public:
    explicit AppAccess(const QString& directory, QObject* parent = nullptr);
    explicit AppAccess(std::shared_ptr<PasswordLock> lock, QObject* parent = nullptr);
    ~AppAccess() override;
    bool locked() const { return locked_; }
    bool enabled() const { return enabled_; }
    bool busy() const { return busy_; }
    bool ready() const { return ready_; }
    QString error() const { return error_; }
    QString recoveryCode() const { return recoveryCode_; }
    int retryAfter() const;
    void start();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void unlock(const QString& proof);
    Q_INVOKABLE void changePassword(const QString& proof, const QString& password, const QString& confirmation);
    Q_INVOKABLE void disablePassword(const QString& proof);
    Q_INVOKABLE void acknowledgeRecovery();
    Q_INVOKABLE void lock();
    void systemLock();
signals:
    void changed();
    void admissionChanged();
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    enum class Operation { Status, Unlock, PrepareChange, PrepareDisable, Change, Disable };
    void run(Operation operation, const QString& proof = {}, const QString& password = {});
    void setLocked(bool locked);
    std::shared_ptr<PasswordLock> lock_;
    QFutureWatcher<AccessResult> worker_;
    QTimer timer_;
    QElapsedTimer idle_;
    qint64 retryAt_ = 0;
    quint64 generation_ = 0;
    bool locked_ = true, enabled_ = true, busy_ = false, ready_ = false;
    QString error_, recoveryCode_;
};
