#pragma once
#include "DatabaseEncryption.h"
#include <QTemporaryDir>
#include <QVariantMap>
#include <functional>

namespace PortableBackup {
constexpr qint64 MaximumBytes = qint64(2) * 1024 * 1024 * 1024;
using SchemaValidator = std::function<bool(const DatabaseAccess&, const QString&, QString&)>;
struct Prepared {
    std::shared_ptr<QTemporaryDir> workspace;
    DatabaseAccess access;
    QVariantMap summary;
};
struct Result {
    bool ok = false;
    QString error;
    std::shared_ptr<Prepared> prepared;
};
bool validPassword(const QString& password);
bool needsPassword(const QString& path, bool& required, QString& error);
Result exportFile(const DatabaseAccess& source, const QString& destination,
                  const QString& workspaceRoot, const QString& password,
                  bool plaintext, const QString& appVersion);
Result prepareImport(const QString& path, const QString& workspaceRoot,
                     const QString& password, DatabaseKeyStore& keys,
                     const SchemaValidator& validateSchema);
void cleanAbandoned(const QString& workspaceRoot);
}
