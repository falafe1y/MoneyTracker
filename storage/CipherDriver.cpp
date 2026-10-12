#include "CipherDriver.h"
#include <QSqlResult>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QSqlIndex>
#include <QSqlQuery>
#include <QDateTime>
#include <sqlite3.h>
#include <openssl/crypto.h>
#include <limits>

namespace {
QSqlError error(sqlite3* db, QSqlError::ErrorType type)
{
    // Do not include SQL text: an ATTACH statement can contain key material.
    return QSqlError(QStringLiteral("Vexa SQLCipher"), QString::fromUtf8(sqlite3_errmsg(db)),
                     type, QString::number(sqlite3_extended_errcode(db)));
}
QMetaType declaredType(const char* declaration)
{
    const QByteArray type = QByteArray(declaration ? declaration : "").toUpper();
    if (type.contains("INT")) return QMetaType(QMetaType::LongLong);
    if (type.contains("REAL") || type.contains("FLOA") || type.contains("DOUB")) return QMetaType(QMetaType::Double);
    if (type.contains("BLOB")) return QMetaType(QMetaType::QByteArray);
    return QMetaType(QMetaType::QString);
}
class CipherResult final : public QSqlResult {
public:
    explicit CipherResult(const CipherDriver* driver) : QSqlResult(driver), owner_(driver) {}
    ~CipherResult() override { sqlite3_finalize(statement_); }
protected:
    bool prepare(const QString& query) override {
        sqlite3_finalize(statement_); statement_ = nullptr;
        rows_.clear(); current_.clear(); record_.clear(); done_ = false;
        setActive(false); setAt(QSql::BeforeFirstRow); setLastError({});
        auto* db = owner_->connection();
        if (!db) return false;
        const auto sql = query.toUtf8(); const char* tail = nullptr;
        if (sqlite3_prepare_v2(db, sql.constData(), sql.size(), &statement_, &tail) != SQLITE_OK) {
            setLastError(error(db, QSqlError::StatementError)); return false;
        }
        if (!statement_ || (tail && !QByteArray(tail).trimmed().isEmpty())) {
            setLastError(QSqlError("Vexa SQLCipher", "Ожидался один SQL-запрос", QSqlError::StatementError));
            return false;
        }
        return true;
    }
    bool reset(const QString& query) override { return prepare(query) && execute(false); }
    bool exec() override { return execute(true); }
    QVariant data(int column) override {
        if (at() < 0 || column < 0) return {};
        const auto& row = isForwardOnly() ? current_ : rows_.value(at());
        return column < row.size() ? row[column] : QVariant();
    }
    bool isNull(int column) override { return data(column).isNull(); }
    bool fetch(int row) override {
        if (!isActive() || !isSelect() || row < 0 || (isForwardOnly() && row < at())) return false;
        if (isForwardOnly()) {
            while (at() < row) if (!fetchNext()) return false;
            return at() == row;
        }
        while (rows_.size() <= row && !done_) if (!step()) break;
        if (row >= rows_.size()) { setAt(QSql::AfterLastRow); return false; }
        setAt(row); return true;
    }
    bool fetchNext() override {
        if (!isActive() || !isSelect() || at() == QSql::AfterLastRow) return false;
        const int next = at() == QSql::BeforeFirstRow ? 0 : at() + 1;
        if (!isForwardOnly()) return fetch(next);
        if (next == 0 && firstRow_) { firstRow_ = false; setAt(0); return true; }
        if (done_ || !step()) { setAt(QSql::AfterLastRow); return false; }
        setAt(next); return true;
    }
    bool fetchFirst() override { return fetch(0); }
    bool fetchLast() override {
        if (!isActive() || !isSelect()) return false;
        if (isForwardOnly()) {
            int last = at();
            while (fetchNext()) last = at();
            if (last < 0) return false;
            setAt(last); return true;
        }
        while (!done_) step();
        return !rows_.isEmpty() && fetch(rows_.size() - 1);
    }
    int size() override { return -1; }
    int numRowsAffected() override { return affected_; }
    QVariant lastInsertId() const override { return inserted_; }
    QSqlRecord record() const override { return record_; }
    void detachFromResultSet() override {
        if (statement_) sqlite3_reset(statement_);
        rows_.clear(); current_.clear(); done_ = true;
    }
private:
    bool execute(bool bind) {
        if (!statement_ || !owner_->connection()) return false;
        sqlite3_reset(statement_); sqlite3_clear_bindings(statement_);
        setLastError({}); setActive(false); setAt(QSql::BeforeFirstRow);
        rows_.clear(); current_.clear(); record_.clear(); done_ = false; firstRow_ = false;
        affected_ = 0; inserted_ = {};
        const int count = sqlite3_bind_parameter_count(statement_);
        if (bind && count != boundValues().size()) {
            // Named parameters are indexed by SQLite; repeated names use one slot.
            bool complete = true;
            for (int i = 1; i <= count; ++i) {
                const char* name = sqlite3_bind_parameter_name(statement_, i);
                bool found = false;
                for (int slot = 0; slot < boundValueCount(); ++slot)
                    if (name && boundValueName(slot) == QString::fromUtf8(name)) found = true;
                if (!found) complete = false;
            }
            if (!complete) { setLastError(QSqlError("Vexa SQLCipher", "Количество параметров не совпадает", QSqlError::StatementError)); return false; }
        }
        for (int i = 1; i <= count && bind; ++i) {
            const char* name = sqlite3_bind_parameter_name(statement_, i);
            const auto value = name && name[0] != '?' ? boundValue(QString::fromUtf8(name)) : boundValue(i - 1);
            int rc = SQLITE_OK;
            if (value.isNull()) rc = sqlite3_bind_null(statement_, i);
            else switch (value.metaType().id()) {
            case QMetaType::Bool: case QMetaType::Int: case QMetaType::UInt:
            case QMetaType::LongLong: case QMetaType::ULongLong:
                rc = sqlite3_bind_int64(statement_, i, value.toLongLong()); break;
            case QMetaType::Float: case QMetaType::Double:
                rc = sqlite3_bind_double(statement_, i, value.toDouble()); break;
            case QMetaType::QByteArray: {
                const auto bytes = value.toByteArray();
                rc = sqlite3_bind_blob64(statement_, i, bytes.constData(), bytes.size(), SQLITE_TRANSIENT); break;
            }
            default: {
                QString text;
                if (value.metaType().id() == QMetaType::QDateTime) text = value.toDateTime().toString(Qt::ISODateWithMs);
                else if (value.metaType().id() == QMetaType::QDate) text = value.toDate().toString(Qt::ISODate);
                else if (value.metaType().id() == QMetaType::QTime) text = value.toTime().toString(Qt::ISODateWithMs);
                else text = value.toString();
                const auto bytes = text.toUtf8();
                rc = sqlite3_bind_text64(statement_, i, bytes.constData(), bytes.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
            }}
            if (rc != SQLITE_OK) { setLastError(error(owner_->connection(), QSqlError::StatementError)); return false; }
        }
        for (int i = 0; i < sqlite3_column_count(statement_); ++i)
            record_.append(QSqlField(QString::fromUtf8(sqlite3_column_name(statement_, i)), declaredType(sqlite3_column_decltype(statement_, i))));
        setSelect(sqlite3_column_count(statement_) > 0);
        const bool row = step();
        if (lastError().isValid()) return false;
        firstRow_ = row;
        affected_ = sqlite3_changes(owner_->connection());
        inserted_ = QVariant::fromValue<qlonglong>(sqlite3_last_insert_rowid(owner_->connection()));
        setActive(true); return true;
    }
    bool step() {
        const int rc = sqlite3_step(statement_);
        if (rc != SQLITE_ROW) {
            done_ = true;
            if (rc != SQLITE_DONE) setLastError(error(owner_->connection(), QSqlError::StatementError));
            return false;
        }
        QVariantList row;
        for (int i = 0; i < sqlite3_column_count(statement_); ++i) {
            switch (sqlite3_column_type(statement_, i)) {
            case SQLITE_INTEGER: row.append(QVariant::fromValue<qlonglong>(sqlite3_column_int64(statement_, i))); break;
            case SQLITE_FLOAT: row.append(sqlite3_column_double(statement_, i)); break;
            case SQLITE_TEXT: row.append(QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement_, i)), sqlite3_column_bytes(statement_, i))); break;
            case SQLITE_BLOB: row.append(QByteArray(static_cast<const char*>(sqlite3_column_blob(statement_, i)), sqlite3_column_bytes(statement_, i))); break;
            default: row.append(QVariant()); break;
            }
        }
        if (isForwardOnly()) current_ = row; else rows_.append(row);
        return true;
    }
    const CipherDriver* owner_;
    sqlite3_stmt* statement_ = nullptr;
    QList<QVariantList> rows_;
    QVariantList current_;
    QSqlRecord record_;
    QVariant inserted_;
    int affected_ = 0;
    bool done_ = false, firstRow_ = false;
};
}

CipherDriver::CipherDriver(QByteArray key) : key_(std::move(key)) {}
CipherDriver::~CipherDriver() { close(); OPENSSL_cleanse(key_.data(), key_.size()); }
bool CipherDriver::hasFeature(DriverFeature feature) const {
    return feature == Transactions || feature == BLOB || feature == Unicode || feature == PreparedQueries
        || feature == NamedPlaceholders || feature == PositionalPlaceholders || feature == LastInsertId
        || feature == SimpleLocking || feature == FinishQuery;
}
bool CipherDriver::open(const QString& name, const QString&, const QString&, const QString&, int, const QString& options) {
    close();
    const int flags = options.contains("QSQLITE_OPEN_READONLY") ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(name.toUtf8().constData(), &connection_, flags | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        setLastError(error(connection_, QSqlError::ConnectionError)); close(); setOpenError(true); return false;
    }
    if (!key_.isEmpty()) {
        if (key_.size() != 32) { close(); setOpenError(true); return false; }
        QByteArray raw = "x'" + key_.toHex() + "'";
        const int rc = sqlite3_key(connection_, raw.constData(), raw.size());
        OPENSSL_cleanse(raw.data(), raw.size());
        if (rc != SQLITE_OK) { setLastError(error(connection_, QSqlError::ConnectionError)); close(); setOpenError(true); return false; }
    }
    sqlite3_busy_timeout(connection_, 30000);
    // Verify the key before callers can run migrations or financial queries.
    const int rc = sqlite3_exec(connection_, "SELECT count(*) FROM sqlite_master; PRAGMA temp_store=MEMORY; PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) { setLastError(error(connection_, QSqlError::ConnectionError)); close(); setOpenError(true); return false; }
    if (!(flags & SQLITE_OPEN_READONLY)
        && sqlite3_exec(connection_, "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        setLastError(error(connection_, QSqlError::ConnectionError)); close(); setOpenError(true); return false;
    }
    setOpenError(false); setOpen(true); return true;
}
void CipherDriver::close() {
    if (connection_) sqlite3_close_v2(connection_);
    connection_ = nullptr; setOpen(false);
}
QSqlResult* CipherDriver::createResult() const { return new CipherResult(this); }
bool CipherDriver::transactionCommand(const char* sql) {
    if (!connection_) return false;
    if (sqlite3_exec(connection_, sql, nullptr, nullptr, nullptr) == SQLITE_OK) return true;
    setLastError(error(connection_, QSqlError::TransactionError)); return false;
}
bool CipherDriver::beginTransaction() { return transactionCommand("BEGIN"); }
bool CipherDriver::commitTransaction() { return transactionCommand("COMMIT"); }
bool CipherDriver::rollbackTransaction() { return transactionCommand("ROLLBACK"); }
QString CipherDriver::escapeIdentifier(const QString& name, IdentifierType) const {
    QString quoted = name; quoted.replace('"', "\"\""); return '"' + quoted + '"';
}
QStringList CipherDriver::tables(QSql::TableType type) const {
    QStringList result;
    QSqlQuery query(createResult());
    if (query.exec("SELECT name,type FROM sqlite_master")) while (query.next()) {
        const auto kind = query.value(1).toString();
        if ((kind == "table" && (type & QSql::Tables)) || (kind == "view" && (type & QSql::Views))) result.append(query.value(0).toString());
    }
    return result;
}
QSqlRecord CipherDriver::record(const QString& name) const {
    QSqlRecord result; QSqlQuery query(createResult());
    if (query.exec("PRAGMA table_info(" + escapeIdentifier(name, TableName) + ")")) while (query.next()) {
        QSqlField field(query.value(1).toString(), declaredType(query.value(2).toString().toUtf8().constData()));
        field.setRequired(query.value(3).toBool()); field.setDefaultValue(query.value(4)); result.append(field);
    }
    return result;
}
QSqlIndex CipherDriver::primaryIndex(const QString& name) const {
    QSqlIndex result; QMap<int, QSqlField> fields; QSqlQuery query(createResult());
    if (query.exec("PRAGMA table_info(" + escapeIdentifier(name, TableName) + ")")) while (query.next())
        if (query.value(5).toInt()) fields.insert(query.value(5).toInt(), QSqlField(query.value(1).toString(), declaredType(query.value(2).toString().toUtf8().constData())));
    for (const auto& field : fields) result.append(field);
    return result;
}
QSqlDatabase addCipherDatabase(const QString& name, const QByteArray& key) {
    return QSqlDatabase::addDatabase(new CipherDriver(key), name);
}
