#pragma once
#include "DatabaseEncryption.h"
#include "PortableBackup.h"
#include <QObject>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

class DatabaseProtection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool encrypted READ encrypted NOTIFY changed)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
    Q_PROPERTY(QString backupError READ backupError NOTIFY backupChanged)
    Q_PROPERTY(bool backupPasswordRequired READ backupPasswordRequired NOTIFY backupChanged)
    Q_PROPERTY(bool hasBackupPreview READ hasBackupPreview NOTIFY backupChanged)
    Q_PROPERTY(QVariantMap backupSummary READ backupSummary NOTIFY backupChanged)
public:
    explicit DatabaseProtection(QString path, QObject* parent = nullptr);
    ~DatabaseProtection() override;
    bool ready() const { return ready_; }
    bool busy() const { return busy_; }
    bool encrypted() const { return access_.encrypted(); }
    bool supported() const;
    bool restartRequired() const { return restartRequired_; }
    QString error() const { return error_; }
    DatabaseAccess access() const { return access_; }
    QString backupError() const { return backupError_; }
    bool backupPasswordRequired() const { return passwordRequired_; }
    bool hasBackupPreview() const { return bool(preview_); }
    QVariantMap backupSummary() const { return preview_ ? preview_->summary : QVariantMap{}; }
    void setBackupSchemaValidator(PortableBackup::SchemaValidator validator) { schemaValidator_ = std::move(validator); }
    void setAdmitted(bool admitted);
    void releaseSession();
    void beginChange(bool encrypted); // Called only after all financial users close.
    void beginRestore();
    void storageFailed(const QString& error);
    Q_INVOKABLE void prepare();
    Q_INVOKABLE void requestChange(bool encrypted);
    Q_INVOKABLE void restart();
    Q_INVOKABLE void exportBackup(const QUrl& destination, const QString& password, bool plaintext);
    Q_INVOKABLE void inspectBackup(const QUrl& source, const QString& password);
    Q_INVOKABLE QString sourceName(const QUrl& source) const;
    Q_INVOKABLE void discardBackup();
    Q_INVOKABLE void restoreBackup();
signals:
    void changed();
    void available();
    void maintenanceRequested(bool encrypted);
    void restoreRequested();
    void backupChanged();
    void backupExported(const QString& path);
private:
    void launch(bool conversion, bool encrypted = false, std::shared_ptr<PortableBackup::Prepared> restore = {});
    void finishBackup(bool exporting, const QString& path, quint64 generation, const QUrl& destination);
    QString path_, error_;
    DatabaseAccess access_;
    std::shared_ptr<DatabaseKeyStore> store_;
    QFutureWatcher<DatabaseOutcome> worker_;
    QFutureWatcher<PortableBackup::Result> backupWorker_;
    PortableBackup::SchemaValidator schemaValidator_;
    std::shared_ptr<PortableBackup::Prepared> preview_, restoring_;
    QElapsedTimer previewAge_;
    std::unique_ptr<QTemporaryDir> backupDirectory_; // Holds a backup until it is copied to a picked document
    QString backupError_, workspaceRoot_;
    quint64 generation_ = 0;
    bool passwordRequired_ = false, recovery_ = false;
    bool admitted_ = false, ready_ = false, busy_ = false, restartRequired_ = false;
};
