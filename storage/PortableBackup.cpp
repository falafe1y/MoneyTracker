#include "PortableBackup.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <sqlite3.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace {
const QByteArray Header = QByteArray("VEXABACKUP", 10) + QByteArray(5, '\0') + char(1);
const char* Invalid = "Копия повреждена, несовместима с этой версией Vexa или указан неверный пароль.";
struct Secret {
    QByteArray bytes;
    ~Secret() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
};
struct Connection {
    sqlite3* db = nullptr;
    ~Connection() { if (db) sqlite3_close_v2(db); }
    bool exec(const char* sql) { return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }
    bool open(const QString& path, const QByteArray& key) {
        if (sqlite3_open_v2(path.toUtf8().constData(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) return false;
        if (!key.isEmpty() && sqlite3_key(db, key.constData(), key.size()) != SQLITE_OK) return false;
        return exec("PRAGMA cipher_compatibility=4; PRAGMA kdf_iter=256000; PRAGMA trusted_schema=OFF; PRAGMA temp_store=MEMORY;");
    }
    bool open(const DatabaseAccess& access) {
        Secret key{access.encrypted() ? "x'" + access.key.toHex() + "'" : QByteArray()};
        return open(access.path, key.bytes);
    }
};
struct Statement {
    sqlite3_stmt* value = nullptr;
    ~Statement() { sqlite3_finalize(value); }
    bool prepare(sqlite3* db, const char* sql) { return sqlite3_prepare_v2(db, sql, -1, &value, nullptr) == SQLITE_OK; }
    bool text(int index, const QByteArray& bytes) { return sqlite3_bind_text(value, index, bytes.constData(), bytes.size(), SQLITE_TRANSIENT) == SQLITE_OK; }
};
Secret passwordKey(const QString& password) { return {QByteArray("vexa-portable-v1:") + password.toUtf8()}; }
std::shared_ptr<QTemporaryDir> workspace(const QString& root) {
    if (!QDir().mkpath(root)) return {};
    auto result = std::make_shared<QTemporaryDir>(root + "/portable-XXXXXX");
    return result->isValid() ? result : nullptr;
}
bool randomAccess(const QString& path, DatabaseAccess& access) {
    access.path = path; access.key.resize(32);
    return RAND_bytes(reinterpret_cast<unsigned char*>(access.key.data()), 32) == 1;
}
bool checkRows(Connection& c, const char* sql, bool expectsOk) {
    Statement s; if (!s.prepare(c.db, sql)) return false;
    int count = 0, rc;
    while ((rc = sqlite3_step(s.value)) == SQLITE_ROW) {
        ++count;
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(s.value, 0));
        if (!expectsOk || !text || QByteArray(text) != "ok") return false;
    }
    return rc == SQLITE_DONE && (!expectsOk || count == 1);
}
bool scalar(Connection& c, const char* sql, qint64& value) {
    Statement s;
    if (!s.prepare(c.db, sql) || sqlite3_step(s.value) != SQLITE_ROW) return false;
    value = sqlite3_column_int64(s.value, 0); return true;
}
bool integrity(Connection& c, bool encrypted, qint64 length) {
    qint64 pages = 0, size = 0;
    return checkRows(c, "PRAGMA integrity_check", true)
        && (!encrypted || checkRows(c, "PRAGMA cipher_integrity_check", false))
        && scalar(c, "PRAGMA page_count", pages) && scalar(c, "PRAGMA page_size", size)
        && pages > 0 && size > 0 && pages <= PortableBackup::MaximumBytes / size && pages * size == length;
}
bool copyWithKey(Connection& source, const QString& target, const QByteArray& key) {
    QFile file(target); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return false; file.close();
    {
        Statement attach;
        if (!attach.prepare(source.db, "ATTACH DATABASE ? AS portable KEY ?")
            || !attach.text(1, target.toUtf8()) || !attach.text(2, key)
            || sqlite3_step(attach.value) != SQLITE_DONE) return false;
    }
    const bool ok = source.exec("PRAGMA portable.cipher_compatibility=4; PRAGMA portable.kdf_iter=256000; SELECT sqlcipher_export('portable');");
    const bool detached = source.exec("DETACH DATABASE portable;");
    return ok && detached;
}
bool stream(QIODevice& input, QIODevice& output, qint64 expected) {
    qint64 total = 0;
    while (total < expected) {
        auto block = input.read(qMin(qint64(1024 * 1024), expected - total));
        if (block.isEmpty() || output.write(block) != block.size()) return false;
        total += block.size();
    }
    return input.read(1).isEmpty() && input.atEnd();
}
bool samePath(const QString& a, const QString& b) {
    QFileInfo left(a), right(b);
    if (right.exists() && left.canonicalFilePath() == right.canonicalFilePath()) return true;
    return QDir::cleanPath(left.absoluteFilePath()).compare(QDir::cleanPath(right.absoluteFilePath()),
#ifdef Q_OS_WIN
        Qt::CaseInsensitive
#else
        Qt::CaseSensitive
#endif
    ) == 0;
}
bool summary(Connection& c, bool protectedFile, QVariantMap& data) {
    qint64 accounts = 0, transactions = 0, metadata = 0;
    for (const auto* table : {"accounts", "transactions", "categories", "settings"}) {
        Statement s;
        if (!s.prepare(c.db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?")
            || !s.text(1, table) || sqlite3_step(s.value) != SQLITE_ROW) return false;
    }
    if (!scalar(c, "SELECT count(*) FROM accounts", accounts)
        || !scalar(c, "SELECT count(*) FROM transactions", transactions)
        || !scalar(c, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='vexa_portable_backup'", metadata)) return false;
    data = {{"accountCount", accounts}, {"activityCount", transactions}};
    if (metadata) {
        Statement s;
        if (!s.prepare(c.db, "SELECT profile,created_at,app_version FROM vexa_portable_backup WHERE id=1")
            || sqlite3_step(s.value) != SQLITE_ROW || sqlite3_column_int(s.value, 0) != 1) return false;
        for (int i = 1; i <= 2; ++i) {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(s.value, i));
            if (!text) return false;
            data.insert(i == 1 ? "createdAt" : "appVersion", QString::fromUtf8(text));
        }
    } else if (protectedFile) return false;
    return true;
}
}

namespace PortableBackup {
bool validPassword(const QString& password) {
    const auto count = password.toUcs4().size();
    return count >= 12 && count <= 1024 && password.toUtf8().size() <= 4096;
}
bool needsPassword(const QString& path, bool& required, QString& error) {
    required = false;
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() < 16 || info.size() > MaximumBytes) {
        error = QStringLiteral("Выберите файл резервной копии размером от 16 байт до 2 ГиБ."); return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
    const auto header = file.read(16);
    if (header.startsWith("VEXABACKUP")) {
        if (header != Header) { error = QStringLiteral("Версия файла резервной копии не поддерживается."); return false; }
        required = true;
    }
    return true;
}
Result exportFile(const DatabaseAccess& source, const QString& destination, const QString& root,
                  const QString& password, bool plaintext, const QString& version) {
    Result result;
    if (!plaintext && !validPassword(password)) {
        result.error = QStringLiteral("Пароль копии должен содержать от 12 до 1024 символов."); return result;
    }
    if (destination.isEmpty() || samePath(source.path, destination)
        || samePath(destination, source.path + "-wal") || samePath(destination, source.path + "-shm")
        || samePath(destination, source.path + "-journal") || samePath(destination, source.path + ".encrypted")) {
        result.error = QStringLiteral("Нельзя сохранить копию поверх рабочей базы или её служебных файлов."); return result;
    }
    auto work = workspace(root);
    if (!work) { result.error = QStringLiteral("Не удалось создать временный каталог копии."); return result; }
    result.error = QStringLiteral("Не удалось создать или проверить резервную копию.");
    DatabaseAccess clean;
    if (!randomAccess(work->filePath("clean.db"), clean)
        || !DatabaseEncryption::copy(source, clean, result.error)) return result;
    Connection c;
    if (!c.open(clean) || !integrity(c, true, QFileInfo(clean.path).size())
        || !c.exec("DROP TABLE IF EXISTS vexa_portable_backup; CREATE TABLE vexa_portable_backup(id INTEGER PRIMARY KEY CHECK(id=1), profile INTEGER NOT NULL, created_at TEXT NOT NULL, app_version TEXT NOT NULL);")) return result;
    {
        Statement metadata;
        if (!metadata.prepare(c.db, "INSERT INTO vexa_portable_backup VALUES(1,1,?,?)")
            || !metadata.text(1, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8())
            || !metadata.text(2, version.toUtf8()) || sqlite3_step(metadata.value) != SQLITE_DONE) return result;
    }
    const auto payload = work->filePath("payload.db");
    Secret key{plaintext ? QByteArray("") : passwordKey(password).bytes};
    if (!copyWithKey(c, payload, key.bytes)) return result;
    const qint64 length = QFileInfo(payload).size();
    if (length > MaximumBytes - (plaintext ? 0 : Header.size())) {
        result.error = QStringLiteral("Размер копии превышает 2 ГиБ."); return result;
    }
    {
        Connection verified;
        if (!verified.open(payload, key.bytes) || !integrity(verified, !plaintext, length)) return result;
    }
    QFile input(payload); QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)
        || (!plaintext && output.write(Header) != Header.size()) || !stream(input, output, length)
        || !output.commit()) { result.error = QStringLiteral("Не удалось сохранить файл копии."); return result; }
    result.ok = true; result.error.clear(); return result;
}
Result prepareImport(const QString& path, const QString& root, const QString& password,
                     DatabaseKeyStore& keys, const SchemaValidator& validateSchema) {
    Result result; bool protectedFile = false;
    if (!needsPassword(path, protectedFile, result.error)) return result;
    if (protectedFile && !validPassword(password)) {
        result.error = QStringLiteral("Пароль копии должен содержать от 12 до 1024 символов."); return result;
    }
    if (!protectedFile && QFileInfo(path + "-wal").size() > 0) {
        result.error = QStringLiteral("Копия содержит отдельный журнал WAL. Создайте самостоятельный файл копии."); return result;
    }
    auto work = workspace(root);
    if (!work) { result.error = QStringLiteral("Не удалось подготовить каталог восстановления."); return result; }
    result.error = QString::fromUtf8(Invalid);
    const auto staged = work->filePath("input.db");
    QFile input(path), output(staged);
    if (!input.open(QIODevice::ReadOnly)) return result;
    const qint64 length = input.size() - (protectedFile ? Header.size() : 0);
    if (length < 0 || length > MaximumBytes || (protectedFile && input.read(16) != Header)
        || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly) || !stream(input, output, length)
        || !output.flush()) return result;
    output.close(); input.close();
    auto prepared = std::make_shared<Prepared>(); prepared->workspace = work;
    DatabaseAccess clean;
    if (!randomAccess(work->filePath("clean.db"), clean)) return result;
    {
        Connection c; bool encrypted = protectedFile;
        if (protectedFile) {
            auto key = passwordKey(password);
            if (!c.open(staged, key.bytes)) return result;
        } else {
            auto legacy = DatabaseEncryption::openSnapshot(staged, keys);
            if (!legacy.ok) { result.error = legacy.error; return result; }
            encrypted = legacy.access.encrypted();
            if (!c.open(legacy.access)) return result;
        }
        if (!integrity(c, encrypted, length) || !summary(c, protectedFile, prepared->summary)) return result;
        Secret raw{"x'" + clean.key.toHex() + "'"};
        if (!copyWithKey(c, clean.path, raw.bytes)) return result;
    }
    // The original input and installation key are never changed by preview.
    if (!validateSchema || !validateSchema(clean, work->path(), result.error)) return result;
    {
        Connection c;
        if (!c.open(clean) || !checkRows(c, "PRAGMA foreign_key_check", false)
            || !c.exec("DROP TABLE IF EXISTS vexa_portable_backup;")) return result;
    }
    if (!randomAccess(work->filePath("restore.db"), prepared->access)
        || !DatabaseEncryption::copy(clean, prepared->access, result.error)
        || !DatabaseEncryption::validate(prepared->access, result.error)) return result;
    result.prepared = prepared; result.ok = true; result.error.clear(); return result;
}
void cleanAbandoned(const QString& root) {
    QDir directory(root);
    for (const auto& name : directory.entryList({"portable-*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!QFileInfo(directory.filePath(name)).isSymLink()) QDir(directory.filePath(name)).removeRecursively();
    }
}
}
