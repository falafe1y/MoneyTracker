#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

// Only the access controller owns this store. It is not exposed to QML.
class PasswordStore {
public:
    virtual ~PasswordStore() = default;
    // Availability probing must not read secrets or create credentials.
    virtual bool available(QString& error) { Q_UNUSED(error); return true; }
    virtual bool read(QByteArray& value, QString& error) = 0;
    virtual bool write(const QByteArray& value, QString& error) = 0;
};
std::shared_ptr<PasswordStore> nativePasswordStore();
