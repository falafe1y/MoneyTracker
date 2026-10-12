#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

class DatabaseKeyStore {
public:
    virtual ~DatabaseKeyStore() = default;
    // Empty key + empty error means no key exists; lookup never creates a key.
    virtual QByteArray existing(QString& error) = 0;
    virtual bool save(const QByteArray& key, QString& error) = 0;
};
std::shared_ptr<DatabaseKeyStore> nativeDatabaseKeyStore();

struct DatabaseAccess {
    DatabaseAccess() = default;
    DatabaseAccess(QString path, QByteArray key = {});
    DatabaseAccess(const DatabaseAccess&) = default;
    DatabaseAccess(DatabaseAccess&&) noexcept = default;
    DatabaseAccess& operator=(const DatabaseAccess& other);
    DatabaseAccess& operator=(DatabaseAccess&& other) noexcept;
    ~DatabaseAccess();
    QString path;
    QByteArray key;
    bool encrypted() const { return !key.isEmpty(); }
};
struct DatabaseOutcome {
    bool ok = false;
    DatabaseAccess access;
    QString error;
    QString recoveryPath;
};

namespace DatabaseEncryption {
// Does not generate a replacement key for an existing unreadable database.
DatabaseOutcome open(const QString& path, DatabaseKeyStore& store);
DatabaseOutcome convert(const QString& path, bool encrypted, DatabaseKeyStore& store);
bool validate(const DatabaseAccess& access, QString& error);
bool snapshot(const DatabaseAccess& source, const QString& target, QString& error);
// New private target only. Used to re-encrypt portable staging files.
bool copy(const DatabaseAccess& source, const DatabaseAccess& target, QString& error);
// Call only after closing the financial runtime; retains destination protection.
DatabaseOutcome restore(const QString& path, const DatabaseAccess& source, DatabaseKeyStore& store);
// Explicit recovery from a validated portable file when the old database cannot open.
DatabaseOutcome recover(const QString& path, const DatabaseAccess& source, DatabaseKeyStore& store);
// May use a retained key, even if the live database is no longer encrypted.
DatabaseOutcome openSnapshot(const QString& path, DatabaseKeyStore& store);
void cleanAbandoned(const QString& path);
}
