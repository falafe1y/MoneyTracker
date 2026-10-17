#include "DatabaseProtection.h"
#include "../services/ContentUriIo.h"
#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QtConcurrent>

DatabaseProtection::DatabaseProtection(QString path, QObject* parent)
    : QObject(parent), path_(std::move(path)), store_(nativeDatabaseKeyStore()) {
    workspaceRoot_ = QFileInfo(path_).absolutePath() + "/.vexa-portable";
    // The application owns the installation lock before constructing this object.
    PortableBackup::cleanAbandoned(workspaceRoot_);
}
DatabaseProtection::~DatabaseProtection() { worker_.waitForFinished(); backupWorker_.waitForFinished(); }
bool DatabaseProtection::supported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
void DatabaseProtection::setAdmitted(bool admitted) {
    admitted_ = admitted;
    if (!admitted) { ++generation_; preview_.reset(); passwordRequired_ = false; backupError_.clear(); emit backupChanged(); }
}
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
void DatabaseProtection::beginRestore() {
    if (!restoring_) return;
    auto candidate = std::move(restoring_);
    launch(true, false, std::move(candidate));
}
void DatabaseProtection::launch(bool conversion, bool encrypted, std::shared_ptr<PortableBackup::Prepared> restore) {
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
    const bool recovery = recovery_;
    worker_.setFuture(QtConcurrent::run([path, store, conversion, encrypted, restore, recovery] {
        if (restore) return recovery ? DatabaseEncryption::recover(path, restore->access, *store)
                                    : DatabaseEncryption::restore(path, restore->access, *store);
        if (conversion) return DatabaseEncryption::convert(path, encrypted, *store);
        DatabaseEncryption::cleanAbandoned(path);
        return DatabaseEncryption::open(path, *store);
    }));
}
void DatabaseProtection::finishBackup(bool exporting, const QString& path, quint64 generation, const QUrl& destination) {
    auto result = backupWorker_.result();
    disconnect(&backupWorker_, nullptr, this, nullptr);
    backupWorker_.setFuture(QFuture<PortableBackup::Result>());
    busy_ = false;
    const bool current = generation == generation_ && admitted_;
    // A picked document receives the backup only while the session that started the export is still open
    if (exporting && result.ok && current && ContentUriIo::isContentUri(destination)
        && !ContentUriIo::copyFileTo(path, destination, result.error)) {
        result.ok = false;
    }
    backupDirectory_.reset();
    if (current) {
        backupError_ = result.error;
        if (result.ok && !exporting) { preview_ = result.prepared; previewAge_.start(); passwordRequired_ = false; }
        emit backupChanged();
        if (result.ok && exporting) {
            const QString name = ContentUriIo::isContentUri(destination) ? ContentUriIo::displayName(destination) : path;
            emit backupExported(name.isEmpty() ? destination.toString() : name);
        }
    }
    emit changed();
    if (generation != generation_ && admitted_ && !ready_) prepare();
}
void DatabaseProtection::exportBackup(const QUrl& destination, const QString& password, bool plaintext) {
    if (!admitted_ || !ready_ || busy_) return;
    backupError_.clear();
    if (!plaintext && !PortableBackup::validPassword(password)) {
        backupError_ = tr("Пароль копии должен содержать от 12 до 1024 символов."); emit backupChanged(); return;
    }
    QString path = destination.toLocalFile();
    if (ContentUriIo::isContentUri(destination)) {
        // A picked document has no path on Android: the backup is written to a temporary folder and copied over later
        QString error;
        backupDirectory_ = ContentUriIo::temporaryDirectory(error);
        if (!backupDirectory_) { backupError_ = error; emit backupChanged(); return; }
        path = backupDirectory_->filePath(QStringLiteral("backup"));
    }
    if (path.isEmpty()) { backupError_ = tr("Выберите место сохранения копии."); emit backupChanged(); return; }
    if (QFileInfo(path).suffix().isEmpty()) path += plaintext ? ".sqlite3" : ".vexabackup";
    busy_ = true; emit changed(); emit backupChanged();
    const auto generation = generation_; const auto source = access_; const auto root = workspaceRoot_;
    const auto version = QCoreApplication::applicationVersion();
    disconnect(&backupWorker_, nullptr, this, nullptr);
    connect(&backupWorker_, &QFutureWatcher<PortableBackup::Result>::finished, this,
            [this, path, generation, destination] { finishBackup(true, path, generation, destination); });
    backupWorker_.setFuture(QtConcurrent::run([source, path, root, password, plaintext, version] {
        return PortableBackup::exportFile(source, path, root, password, plaintext, version);
    }));
}
void DatabaseProtection::inspectBackup(const QUrl& source, const QString& password) {
    if (!admitted_ || busy_ || restartRequired_ || (!ready_ && error_.isEmpty())) return;
    preview_.reset(); backupError_.clear(); passwordRequired_ = false;
    // A picked copy is read through a temporary file, which exists until the import has been prepared
    const auto copy = std::make_shared<ContentUriIo::LocalCopy>(source);
    if (!copy->error().isEmpty()) { backupError_ = copy->error(); emit backupChanged(); return; }
    const auto path = copy->path();
    if (QFileInfo(path).canonicalFilePath() == QFileInfo(path_).canonicalFilePath()) {
        backupError_ = tr("Выберите копию, а не рабочую базу."); emit backupChanged(); return;
    }
    if (!PortableBackup::needsPassword(path, passwordRequired_, backupError_)
        || (passwordRequired_ && password.isEmpty())) { emit backupChanged(); return; }
    busy_ = true; emit changed(); emit backupChanged();
    const auto generation = generation_; const auto root = workspaceRoot_; const auto keys = store_; const auto validator = schemaValidator_;
    disconnect(&backupWorker_, nullptr, this, nullptr);
    connect(&backupWorker_, &QFutureWatcher<PortableBackup::Result>::finished, this,
            [this, generation] { finishBackup(false, {}, generation, {}); });
    backupWorker_.setFuture(QtConcurrent::run([copy, root, password, keys, validator] {
        return PortableBackup::prepareImport(copy->path(), root, password, *keys, validator);
    }));
}
QString DatabaseProtection::sourceName(const QUrl& source) const {
    // A picked document on Android has an address made of provider ids: its name comes from the provider
    return ContentUriIo::isContentUri(source) ? ContentUriIo::displayName(source) : QFileInfo(source.toLocalFile()).fileName();
}
void DatabaseProtection::discardBackup() {
    if (busy_) return;
    preview_.reset(); passwordRequired_ = false; backupError_.clear(); emit backupChanged();
}
void DatabaseProtection::restoreBackup() {
    if (!admitted_ || busy_ || restartRequired_ || !preview_ || (!ready_ && error_.isEmpty())) return;
    if (!previewAge_.isValid() || previewAge_.elapsed() >= 10 * 60 * 1000) {
        preview_.reset(); backupError_ = tr("Проверка копии устарела. Выберите файл и проверьте его заново."); emit backupChanged(); return;
    }
    restoring_ = std::move(preview_);
    recovery_ = !ready_;
    ready_ = false; busy_ = true; error_.clear(); emit changed(); emit backupChanged();
    emit restoreRequested();
}
void DatabaseProtection::restart() {
    if (!restartRequired_ || busy_) return;
    // Quit first so the next process can acquire the installation ownership lock.
    QCoreApplication::exit(42);
}
