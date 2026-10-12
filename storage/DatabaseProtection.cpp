#include "DatabaseProtection.h"
#include <QCoreApplication>
#include <QProcess>
#include <QtConcurrent>

DatabaseProtection::DatabaseProtection(QString path, QObject* parent)
    : QObject(parent), path_(std::move(path)), store_(nativeDatabaseKeyStore()) {}
DatabaseProtection::~DatabaseProtection() { worker_.waitForFinished(); }
bool DatabaseProtection::supported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
void DatabaseProtection::setAdmitted(bool admitted) { admitted_ = admitted; }
void DatabaseProtection::releaseSession() {
    ready_ = false; access_ = {};
    emit changed();
}
void DatabaseProtection::storageFailed(const QString& error) {
    ready_ = false; error_ = error; emit changed();
}
void DatabaseProtection::prepare() {
    if (!admitted_ || busy_ || ready_ || restartRequired_) return;
    launch(false);
}
void DatabaseProtection::requestChange(bool encrypted) {
    if (!admitted_ || !ready_ || busy_ || !supported() || encrypted == access_.encrypted()) return;
    // Withdraw readiness immediately, before yielding to the UI event loop.
    ready_ = false; busy_ = true; error_.clear(); emit changed();
    emit maintenanceRequested(encrypted);
}
void DatabaseProtection::beginChange(bool encrypted) {
    // The request was admitted before teardown. A later app lock cannot cancel file maintenance.
    launch(true, encrypted);
}
void DatabaseProtection::launch(bool conversion, bool encrypted) {
    busy_ = true; error_.clear(); emit changed();
    disconnect(&worker_, nullptr, this, nullptr);
    connect(&worker_, &QFutureWatcher<DatabaseOutcome>::finished, this, [this, conversion] {
        const auto result = worker_.result();
        busy_ = false;
        if (!result.ok) {
            ready_ = false;
            error_ = result.error;
            if (!result.recoveryPath.isEmpty()) error_ += tr(" Копия для восстановления: %1").arg(result.recoveryPath);
        } else if (conversion) {
            access_ = result.access;
            restartRequired_ = true;
            emit changed();
            restart();
            return;
        } else if (admitted_) {
            access_ = result.access; ready_ = true;
        }
        emit changed();
        if (ready_ && admitted_) emit available();
    });
    const auto path = path_; const auto store = store_;
    worker_.setFuture(QtConcurrent::run([path, store, conversion, encrypted] {
        if (conversion) return DatabaseEncryption::convert(path, encrypted, *store);
        DatabaseEncryption::cleanAbandoned(path);
        return DatabaseEncryption::open(path, *store);
    }));
}
void DatabaseProtection::restart() {
    if (!restartRequired_ || busy_) return;
    // Quit first so the next process can acquire the installation ownership lock.
    QCoreApplication::exit(42);
}
