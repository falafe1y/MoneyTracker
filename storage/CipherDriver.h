#pragma once
#include <QSqlDriver>
#include <QSqlDatabase>
#include <QByteArray>
struct sqlite3;

// Public Qt SQL API only: no dependency on private Qt headers or driver plugins.
class CipherDriver final : public QSqlDriver {
public:
    explicit CipherDriver(QByteArray key = {});
    ~CipherDriver() override;
    bool hasFeature(DriverFeature feature) const override;
    bool open(const QString& name, const QString&, const QString&, const QString&, int,
              const QString& options) override;
    void close() override;
    QSqlResult* createResult() const override;
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;
    QStringList tables(QSql::TableType type) const override;
    QSqlRecord record(const QString& name) const override;
    QSqlIndex primaryIndex(const QString& name) const override;
    QString escapeIdentifier(const QString& name, IdentifierType type) const override;
    sqlite3* connection() const { return connection_; }
private:
    bool transactionCommand(const char* sql);
    sqlite3* connection_ = nullptr;
    QByteArray key_;
};

QSqlDatabase addCipherDatabase(const QString& connectionName, const QByteArray& key = {});
