#include "AppAccess.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QtConcurrent>

AppAccess::AppAccess(const QString& directory, QObject* parent)
    : AppAccess(std::make_shared<PasswordLock>(directory, nativePasswordStore()), parent) {}

AppAccess::AppAccess(std::shared_ptr<PasswordLock> manager, QObject* parent)
    : QObject(parent), lock_(std::move(manager))
{
    QCoreApplication::instance()->installEventFilter(this);
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (!locked_ && enabled_ && idle_.isValid() && idle_.elapsed() >= 5 * 60 * 1000) lock();
        if (retryAt_) { if (!retryAfter()) retryAt_ = 0; emit changed(); }
    });
    timer_.start();
}
AppAccess::~AppAccess() { worker_.waitForFinished(); }
int AppAccess::retryAfter() const
{
    return int(qMax<qint64>(0, retryAt_ - QDateTime::currentSecsSinceEpoch()));
}
void AppAccess::start() { if (!ready_ && !busy_) run(Operation::Status); }
void AppAccess::retry() { if (!busy_ && locked_ && recoveryCode_.isEmpty()) run(Operation::Status); }
void AppAccess::setLocked(bool locked)
{
    if (locked_ == locked) return;
    locked_ = locked;
    if (!locked_) idle_.start();
    emit changed();
    emit admissionChanged();
}
void AppAccess::lock()
{
    if (!enabled_) return;
    ++generation_;
    setLocked(true);
}
void AppAccess::systemLock() { lock(); }
void AppAccess::unlock(const QString& proof)
{
    if (busy_ || !locked_ || !ready_ || !recoveryCode_.isEmpty()) return;
    run(Operation::Unlock, proof);
}
void AppAccess::changePassword(const QString& proof, const QString& password, const QString& confirmation)
{
    if (busy_ || !ready_ || !storeAvailable_ || !recoveryCode_.isEmpty() || (locked_ && !enabled_)) return;
    if (password != confirmation) { error_ = tr("Пароли не совпадают."); emit changed(); return; }
    if (!PasswordLock::validPassword(password)) {
        error_ = tr("Пароль должен содержать от 1 до 1024 символов."); emit changed(); return;
    }
    run(Operation::PrepareChange, proof, password);
}
void AppAccess::disablePassword(const QString& proof)
{
    if (busy_ || !ready_ || !storeAvailable_ || locked_ || !enabled_) return;
    run(Operation::PrepareDisable, proof);
}
void AppAccess::acknowledgeRecovery()
{
    if (busy_ || recoveryCode_.isEmpty()) return;
    recoveryCode_.fill(QChar(0)); recoveryCode_.clear();
    error_.clear();
    emit changed();
}
void AppAccess::run(Operation operation, const QString& proof, const QString& password)
{
    busy_ = true;
    error_.clear();
    // Revoke access before any credential mutation, even if saving later fails.
    if (operation == Operation::Change || operation == Operation::Disable) setLocked(true);
    const auto generation = generation_;
    emit changed();
    disconnect(&worker_, nullptr, this, nullptr);
    connect(&worker_, &QFutureWatcher<AccessResult>::finished, this, [this, operation, generation, proof, password] {
        const auto result = worker_.result();
        busy_ = false;
        if (result.storeChecked) {
            storeChecked_ = true;
            storeAvailable_ = result.storeAvailable;
            storeError_ = result.storeError;
        }
        error_ = result.error;
        retryAt_ = result.retryAfter ? QDateTime::currentSecsSinceEpoch() + result.retryAfter : 0;
        if (result.ok && (operation == Operation::PrepareChange || operation == Operation::PrepareDisable)) {
            if (generation == generation_) {
                run(operation == Operation::PrepareChange ? Operation::Change : Operation::Disable, proof, password);
                return;
            }
            emit changed();
            return;
        }
        if (result.ok) {
            ready_ = true;
            enabled_ = result.enabled;
            recoveryCode_ = result.recoveryCode;
            if (recoveryCode_.isEmpty() && (!enabled_
                || (generation == generation_ && operation == Operation::Unlock))) setLocked(false);
        }
        emit changed();
    });
    const auto lock = lock_;
    worker_.setFuture(QtConcurrent::run([lock, operation, proof, password] {
        switch (operation) {
        case Operation::Status: return lock->status();
        case Operation::Unlock:
        case Operation::PrepareChange:
        case Operation::PrepareDisable: return lock->verify(proof);
        case Operation::Change: return lock->change(proof, password, false);
        case Operation::Disable: return lock->change(proof, {}, true);
        }
        return AccessResult{};
    }));
}
bool AppAccess::eventFilter(QObject* object, QEvent* event)
{
    if (!locked_) {
        // Expiry is checked before accepting input, including a late timer after sleep.
        if (enabled_ && idle_.isValid() && idle_.elapsed() >= 5 * 60 * 1000) {
            lock();
            if (event->isInputEvent()) return true;
        } else if (event->spontaneous()) {
            switch (event->type()) {
            case QEvent::KeyPress: case QEvent::MouseButtonPress: case QEvent::MouseMove:
            case QEvent::Wheel: case QEvent::TouchBegin: case QEvent::TouchUpdate:
            case QEvent::InputMethod:
                idle_.restart(); break;
            default: break;
            }
        }
    }
    return QObject::eventFilter(object, event);
}
