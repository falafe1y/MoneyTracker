#pragma once
#include "DatabaseEncryption.h"
#include <QObject>
#include <QFutureWatcher>

class DatabaseProtection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool encrypted READ encrypted NOTIFY changed)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
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
    void setAdmitted(bool admitted);
    void releaseSession();
    void beginChange(bool encrypted); // Called only after all financial users close.
    void storageFailed(const QString& error);
    Q_INVOKABLE void prepare();
    Q_INVOKABLE void requestChange(bool encrypted);
    Q_INVOKABLE void restart();
signals:
    void changed();
    void available();
    void maintenanceRequested(bool encrypted);
private:
    void launch(bool conversion, bool encrypted = false);
    QString path_, error_;
    DatabaseAccess access_;
    std::shared_ptr<DatabaseKeyStore> store_;
    QFutureWatcher<DatabaseOutcome> worker_;
    bool admitted_ = false, ready_ = false, busy_ = false, restartRequired_ = false;
};
