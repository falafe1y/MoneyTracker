#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

// Only the access controller owns this store. It is not exposed to QML.
class PasswordStore {
public:
    virtual ~PasswordStore() = default;
    virtual bool read(QByteArray& value, QString& error) = 0;
    virtual bool write(const QByteArray& value, QString& error) = 0;
};
std::shared_ptr<PasswordStore> nativePasswordStore();
