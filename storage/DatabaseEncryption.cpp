#include "DatabaseEncryption.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QUuid>
#include <QDebug>
#include <sqlite3.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

DatabaseAccess::DatabaseAccess(QString filePath, QByteArray fileKey)
    : path(std::move(filePath)), key(std::move(fileKey)) {}
DatabaseAccess::~DatabaseAccess() { if (!key.isEmpty()) OPENSSL_cleanse(key.data(), key.size()); }
DatabaseAccess& DatabaseAccess::operator=(const DatabaseAccess& other) {
    if (this != &other) {
        if (!key.isEmpty()) OPENSSL_cleanse(key.data(), key.size());
        path = other.path; key = other.key;
    }
    return *this;
}
DatabaseAccess& DatabaseAccess::operator=(DatabaseAccess&& other) noexcept {
    if (this != &other) {
        if (!key.isEmpty()) OPENSSL_cleanse(key.data(), key.size());
        path = std::move(other.path); key = std::move(other.key);
    }
    return *this;
}

namespace {
constexpr qint64 Headroom = 16 * 1024 * 1024;
QString marker(const QString& path) { return path + ".encrypted"; }
QString sqlError(sqlite3* db) { return QString::fromUtf8(sqlite3_errmsg(db)); }
struct Connection {
    sqlite3* db = nullptr;
    ~Connection() { if (db) sqlite3_close_v2(db); }
    bool open(const DatabaseAccess& access, bool readOnly, bool create, QString& error) {
        const auto bytes = access.path.toUtf8();
        const int flags = readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);
        if (sqlite3_open_v2(bytes.constData(), &db, flags | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
            error = sqlError(db); return false;
        }
        if (!access.key.isEmpty()) {
            if (access.key.size() != 32) { error = QStringLiteral("Некорректный ключ базы."); return false; }
            QByteArray raw = "x'" + access.key.toHex() + "'";
            const int rc = sqlite3_key(db, raw.constData(), raw.size());
            OPENSSL_cleanse(raw.data(), raw.size());
            if (rc != SQLITE_OK) { error = sqlError(db); return false; }
        }
        sqlite3_busy_timeout(db, 30000);
        return exec("SELECT count(*) FROM sqlite_master; PRAGMA temp_store=MEMORY;", error);
    }
    bool exec(const char* sql, QString& error) {
        if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK) return true;
        error = sqlError(db); return false;
    }
};
class NativeKeyStore final : public DatabaseKeyStore {
public:
    QByteArray existing(QString& error) override {
#ifdef Q_OS_WIN
        PCREDENTIALW credential = nullptr;
        if (!CredReadW(L"Vexa/database-encryption/v1", CRED_TYPE_GENERIC, 0, &credential)) {
            if (GetLastError() != ERROR_NOT_FOUND) error = QStringLiteral("Не удалось прочитать ключ базы из Диспетчера учётных данных Windows.");
            return {};
        }
        QByteArray value(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);
        SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize); CredFree(credential);
        if (value.size() != 32) { error = QStringLiteral("Сохранённый ключ базы повреждён."); value.fill('\0'); return {}; }
        return value;
#else
        error = QStringLiteral("Хранилище ключа базы реализовано для Windows."); return {};
#endif
    }
    bool save(const QByteArray& key, QString& error) override {
#ifdef Q_OS_WIN
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<wchar_t*>(L"Vexa/database-encryption/v1");
        credential.UserName = const_cast<wchar_t*>(L"Vexa");
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        credential.CredentialBlobSize = DWORD(key.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(key.constData()));
        if (CredWriteW(&credential, 0)) return true;
#else
        Q_UNUSED(key);
#endif
        error = QStringLiteral("Не удалось сохранить ключ базы в системном хранилище."); return false;
    }
};
bool ensureKey(DatabaseKeyStore& store, QByteArray& key, QString& error) {
    key = store.existing(error);
    if (!error.isEmpty()) return false;
    if (!key.isEmpty()) return key.size() == 32;
    key.resize(32);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(key.data()), key.size()) != 1) {
        error = QStringLiteral("Не удалось создать случайный ключ базы."); return false;
    }
    if (!store.save(key, error)) return false;
    auto persisted = store.existing(error);
    const bool ok = error.isEmpty() && persisted.size() == key.size()
        && CRYPTO_memcmp(persisted.constData(), key.constData(), key.size()) == 0;
    OPENSSL_cleanse(persisted.data(), persisted.size());
    if (!ok && error.isEmpty()) error = QStringLiteral("Не удалось подтвердить сохранение ключа базы.");
    return ok;
}
void recordPolicy(const DatabaseAccess& access) {
    if (access.encrypted()) {
        QSaveFile file(marker(access.path));
        if (!file.open(QIODevice::WriteOnly) || file.write("VEXA_DB_1\n") != 10 || !file.commit())
            qWarning() << "Vexa: database encryption marker could not be saved";
    } else if (QFile::exists(marker(access.path)) && !QFile::remove(marker(access.path)))
        qWarning() << "Vexa: database encryption marker could not be removed";
}
bool spaceAvailable(const QString& directory, qint64 bytes, QString& error) {
    QStorageInfo disk(directory); disk.refresh();
    if (!disk.isValid() || !disk.isReady() || disk.bytesAvailable() < bytes + Headroom) {
        error = QStringLiteral("Недостаточно свободного места для преобразования базы и восстановления при ошибке."); return false;
    }
    return true;
}
bool syncFile(const QString& path, QString& error) {
#ifdef Q_OS_WIN
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { error = QStringLiteral("Не удалось открыть файл для надёжного сохранения."); return false; }
    const bool ok = FlushFileBuffers(h); CloseHandle(h);
#else
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDWR);
    const bool ok = fd >= 0 && ::fsync(fd) == 0;
    if (fd >= 0) ::close(fd);
#endif
    if (!ok) error = QStringLiteral("Не удалось подтвердить запись файла на диск.");
    return ok;
}
bool publish(const QString& candidate, const QString& target, QString& error) {
    if (!syncFile(candidate, error)) return false;
#ifdef Q_OS_WIN
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(candidate.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
#else
    if (::rename(QFile::encodeName(candidate).constData(), QFile::encodeName(target).constData()) == 0) {
        const int fd = ::open(QFile::encodeName(QFileInfo(target).absolutePath()).constData(), O_RDONLY | O_DIRECTORY);
        const bool ok = fd >= 0 && ::fsync(fd) == 0;
        if (fd >= 0) ::close(fd);
        if (ok) return true;
    }
#endif
    error = QStringLiteral("Не удалось заменить файл базы. Закройте программы, использующие этот файл."); return false;
}
bool sameFile(const QString& a, const QString& b) {
    const QFileInfo left(a), right(b);
    if (right.exists() && left.canonicalFilePath() == right.canonicalFilePath()) return true;
    return QDir::cleanPath(left.absoluteFilePath()).compare(QDir::cleanPath(right.absoluteFilePath()),
#ifdef Q_OS_WIN
            Qt::CaseInsensitive
#else
            Qt::CaseSensitive
#endif
        ) == 0;
}
bool copyDatabase(const DatabaseAccess& source, const DatabaseAccess& target, QString& error) {
    if (QFile::exists(target.path)) { error = QStringLiteral("Временный файл уже существует."); return false; }
    Connection connection;
    if (!connection.open(source, false, false, error)) return false;
    // The source is opened without CREATE to protect missing live files. ATTACH
    // inherits those flags, so create only the new private target explicitly.
    QFile emptyTarget(target.path);
    if (!emptyTarget.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        error = emptyTarget.errorString(); return false;
    }
    emptyTarget.close();
    // Bound KEY text uses SQLCipher's raw key notation, never password derivation.
    sqlite3_stmt* attach = nullptr;
    if (sqlite3_prepare_v2(connection.db, "ATTACH DATABASE ? AS vexa_copy KEY ?", -1, &attach, nullptr) != SQLITE_OK) {
        error = sqlError(connection.db); return false;
    }
    const auto path = target.path.toUtf8();
    QByteArray raw = target.encrypted() ? "x'" + target.key.toHex() + "'" : QByteArray("");
    sqlite3_bind_text(attach, 1, path.constData(), path.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(attach, 2, raw.constData(), raw.size(), SQLITE_TRANSIENT);
    OPENSSL_cleanse(raw.data(), raw.size());
    const int rc = sqlite3_step(attach); sqlite3_finalize(attach);
    if (rc != SQLITE_DONE) { error = sqlError(connection.db); return false; }
    const bool copied = connection.exec("PRAGMA vexa_copy.synchronous=FULL; SELECT sqlcipher_export('vexa_copy');", error);
    QString detachError;
    const bool detached = connection.exec("DETACH DATABASE vexa_copy;", detachError);
    if (!detached && error.isEmpty()) error = detachError;
    return copied && detached;
}
bool integrityRows(sqlite3* db, const char* sql, bool expectsOk, QString& error) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK) { error = sqlError(db); return false; }
    int rc; int rows = 0; bool valid = true;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        ++rows;
        const auto value = QByteArray(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
        if (!expectsOk || value != "ok") valid = false;
    }
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE || !valid || (expectsOk && rows != 1)) {
        error = QStringLiteral("Проверка целостности базы не пройдена."); return false;
    }
    return true;
}
}
std::shared_ptr<DatabaseKeyStore> nativeDatabaseKeyStore() { return std::make_shared<NativeKeyStore>(); }

namespace DatabaseEncryption {
bool copy(const DatabaseAccess& source, const DatabaseAccess& target, QString& error) {
    return copyDatabase(source, target, error);
}
DatabaseOutcome recover(const QString& path, const DatabaseAccess& source, DatabaseKeyStore& store) {
    DatabaseOutcome result;
    result.access.path = QFileInfo(path).absoluteFilePath();
    if (!validate(source, result.error)) return result;
    QFile original(path);
    bool plaintext = false;
    if (original.exists()) {
        if (!original.open(QIODevice::ReadOnly)) { result.error = original.errorString(); return result; }
        plaintext = original.read(16) == QByteArray("SQLite format 3\0", 16); original.close();
    }
    const bool encrypted = QFile::exists(marker(path)) || (QFile::exists(path) && !plaintext);
    // Only explicit recovery may provision a key for an unreadable old database.
    if (encrypted && !ensureKey(store, result.access.key, result.error)) return result;
    const auto directory = QFileInfo(path).absolutePath();
    QStringList originals;
    qint64 originalSize = 0;
    for (const auto& suffix : {QString(), QString("-wal"), QString("-shm"), QString("-journal")}) {
        QFileInfo info(path + suffix);
        if (info.isSymLink() || (info.exists() && !info.isFile())) {
            result.error = QStringLiteral("Восстановление требует обычные файлы базы и журналов."); return result;
        }
        if (info.exists()) { originals.append(info.absoluteFilePath()); originalSize += info.size(); }
    }
    if (!spaceAvailable(directory, originalSize + QFileInfo(source.path).size(), result.error)) return result;
    QTemporaryDir work(directory + "/." + QFileInfo(path).fileName() + ".maintenance-XXXXXX");
    if (!work.isValid()) { result.error = QStringLiteral("Не удалось подготовить восстановление."); return result; }
    DatabaseAccess candidate{work.filePath("recovered.db"), result.access.key};
    if (!copyDatabase(source, candidate, result.error) || !validate(candidate, result.error) || !syncFile(candidate.path, result.error)) return result;
    const auto archive = directory + "/recovery-original-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkdir(archive)) { result.error = QStringLiteral("Не удалось сохранить исходные файлы."); return result; }
    result.recoveryPath = archive;
    for (const auto& file : originals) {
        const auto saved = archive + "/" + QFileInfo(file).fileName();
        const auto size = QFileInfo(file).size();
        if (!QFile::copy(file, saved) || QFileInfo(saved).size() != size || !syncFile(saved, result.error)) {
            result.error = QStringLiteral("Не удалось сохранить исходные файлы перед восстановлением."); return result;
        }
    }
#ifndef Q_OS_WIN
    for (const auto& dir : {archive, directory}) {
        const int fd = ::open(QFile::encodeName(dir).constData(), O_RDONLY | O_DIRECTORY);
        const bool synced = fd >= 0 && ::fsync(fd) == 0;
        if (fd >= 0) ::close(fd);
        if (!synced) { result.error = QStringLiteral("Не удалось подтвердить сохранение исходных файлов."); return result; }
    }
#endif
    // All original bytes are durable before changing any active filename.
    for (const auto& file : originals) {
        if (file != result.access.path && !QFile::remove(file)) {
            result.error = QStringLiteral("Не удалось закрыть исходные файлы журнала."); return result;
        }
    }
    if (!publish(candidate.path, path, result.error) || !validate(result.access, result.error)) return result;
    recordPolicy(result.access); result.ok = true; return result;
}
DatabaseOutcome restore(const QString& path, const DatabaseAccess& source, DatabaseKeyStore& store) {
    auto result = open(path, store);
    if (!result.ok) return result;
    result.ok = false;
    const auto current = result.access;
    if (!validate(source, result.error)) return result;
    const auto directory = QFileInfo(path).absolutePath();
    if (!spaceAvailable(directory, QFileInfo(path).size() * 2 + QFileInfo(source.path).size() * 2, result.error)) return result;
    {
        Connection c;
        if (!c.open(current, false, false, result.error)) return result;
        int total = 0, done = 0;
        if (sqlite3_wal_checkpoint_v2(c.db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, &total, &done) != SQLITE_OK
            || (total >= 0 && total != done)
            || !c.exec("PRAGMA locking_mode=EXCLUSIVE; BEGIN EXCLUSIVE; COMMIT;", result.error)) {
            result.error = QStringLiteral("База занята. Восстановление отменено."); return result;
        }
    }
    QTemporaryDir work(directory + "/." + QFileInfo(path).fileName() + ".maintenance-XXXXXX");
    if (!work.isValid()) { result.error = QStringLiteral("Не удалось подготовить восстановление."); return result; }
    DatabaseAccess candidate{work.filePath("candidate.db"), current.key};
    if (!copyDatabase(source, candidate, result.error) || !validate(candidate, result.error)) return result;
    const auto recovery = directory + "/backups/before-restore-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite3";
    if (!snapshot(current, recovery, result.error)) return result;
    result.recoveryPath = recovery;
    if (QFileInfo(path + "-wal").size() > 0) {
        result.error = QStringLiteral("В журнале базы появились новые записи. Восстановление отменено."); return result;
    }
    for (const auto& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")}) {
        if (QFile::exists(path + suffix) && !QFile::remove(path + suffix)) {
            result.error = QStringLiteral("Не удалось закрыть файлы журнала базы."); return result;
        }
    }
    if (!publish(candidate.path, path, result.error)) return result;
    if (!validate(current, result.error)) {
        DatabaseAccess backup{recovery, current.key}, rollback{work.filePath("rollback.db"), current.key};
        QString rollbackError;
        if (!copyDatabase(backup, rollback, rollbackError) || !validate(rollback, rollbackError)
            || !publish(rollback.path, path, rollbackError) || !validate(current, rollbackError))
            result.error += QStringLiteral(" Откат не подтверждён. Сохранена копия: %1").arg(recovery);
        return result;
    }
    recordPolicy(current); result.ok = true; return result;
}
bool validate(const DatabaseAccess& access, QString& error) {
    Connection c;
    if (!c.open(access, true, false, error)) return false;
    if (access.encrypted() && !integrityRows(c.db, "PRAGMA cipher_integrity_check", false, error)) return false;
    return integrityRows(c.db, "PRAGMA integrity_check", true, error)
        && integrityRows(c.db, "PRAGMA foreign_key_check", false, error);
}
DatabaseOutcome openSnapshot(const QString& path, DatabaseKeyStore& store) {
    DatabaseOutcome result; result.access.path = QFileInfo(path).absoluteFilePath();
    if (!QFileInfo(path).isFile() || QFileInfo(path).size() < 100) {
        result.error = QStringLiteral("Файл базы отсутствует или повреждён."); return result;
    }
    QString plainError;
    Connection plain;
    if (plain.open(result.access, true, false, plainError)) { result.ok = true; return result; }
    result.access.key = store.existing(result.error);
    if (!result.error.isEmpty()) return result;
    if (result.access.key.size() != 32) {
        result.error = QStringLiteral("Ключ зашифрованной базы отсутствует в Диспетчере учётных данных Windows."); return result;
    }
    Connection encrypted;
    if (!encrypted.open(result.access, true, false, result.error)) {
        result.error = QStringLiteral("Не удалось открыть базу: неверный ключ либо файл повреждён."); return result;
    }
    result.ok = true; return result;
}
DatabaseOutcome open(const QString& path, DatabaseKeyStore& store) {
    DatabaseOutcome result; result.access.path = QFileInfo(path).absoluteFilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { result.error = QStringLiteral("Не удалось открыть каталог базы."); return result; }
    if (QFile::exists(path)) result = openSnapshot(path, store);
    else {
        if (QFile::exists(marker(path)) && !ensureKey(store, result.access.key, result.error)) return result;
        Connection c;
        if (!c.open(result.access, false, true, result.error)
            || !c.exec("PRAGMA user_version=0;", result.error)) return result;
        result.ok = true;
    }
    if (result.ok) recordPolicy(result.access);
    return result;
}
void cleanAbandoned(const QString& path) {
    // Only disposable operation directories; published recovery snapshots are retained.
    QDir parent(QFileInfo(path).absolutePath());
    const auto prefix = "." + QFileInfo(path).fileName() + ".maintenance-";
    for (const auto& name : parent.entryList({prefix + "*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
        const QFileInfo entry(parent.filePath(name));
        if (!entry.isSymLink()) QDir(entry.absoluteFilePath()).removeRecursively();
    }
}
bool snapshot(const DatabaseAccess& source, const QString& target, QString& error) {
    if (sameFile(source.path, target)) { error = QStringLiteral("Нельзя сохранить копию поверх рабочей базы."); return false; }
    const auto directory = QFileInfo(target).absolutePath();
    if (!QDir().mkpath(directory)) { error = QStringLiteral("Не удалось создать папку копии."); return false; }
    if (!spaceAvailable(directory, QFileInfo(source.path).size(), error)) return false;
    QTemporaryDir temporary(directory + "/.vexa-snapshot-XXXXXX");
    if (!temporary.isValid()) { error = QStringLiteral("Не удалось подготовить копию."); return false; }
    DatabaseAccess candidate{temporary.filePath("copy.db"), source.key};
    if (!copyDatabase(source, candidate, error) || !validate(candidate, error)) return false;
    return publish(candidate.path, target, error);
}
DatabaseOutcome convert(const QString& path, bool encrypted, DatabaseKeyStore& store) {
    auto result = open(path, store);
    if (!result.ok) return result;
    if (result.access.encrypted() == encrypted) return result;
    result.ok = false;
    const auto current = result.access;
    const auto directory = QFileInfo(path).absolutePath();
    if (!spaceAvailable(directory, QFileInfo(path).size() * 3, result.error)) return result;
    // Checkpoint and discard only confirmed-empty sidecars before replacing the main file.
    {
        Connection c;
        if (!c.open(current, false, false, result.error)) return result;
        int total = 0, done = 0;
        if (sqlite3_wal_checkpoint_v2(c.db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, &total, &done) != SQLITE_OK
            || (total >= 0 && total != done)) {
            result.error = QStringLiteral("База занята другой программой. Преобразование отменено."); return result;
        }
        if (!c.exec("PRAGMA locking_mode=EXCLUSIVE; BEGIN EXCLUSIVE; COMMIT;", result.error)) return result;
    }
    cleanAbandoned(path);
    QTemporaryDir temporary(directory + "/." + QFileInfo(path).fileName() + ".maintenance-XXXXXX");
    if (!temporary.isValid()) { result.error = QStringLiteral("Не удалось подготовить преобразование."); return result; }
    DatabaseAccess candidate{temporary.filePath("candidate.db"), {}};
    if (encrypted && !ensureKey(store, candidate.key, result.error)) return result;
    if (!copyDatabase(current, candidate, result.error) || !validate(candidate, result.error)) return result;
    const auto backupDirectory = directory + "/backups";
    const auto recovery = backupDirectory + "/before-encryption-change-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".sqlite3";
    const auto& backupSource = encrypted ? candidate : current;
    if (!snapshot(backupSource, recovery, result.error)) return result;
    result.recoveryPath = recovery;
    if (!spaceAvailable(directory, QFileInfo(recovery).size(), result.error)) return result;
    // No live connection remains. Nonempty WAL is never discarded.
    if (QFileInfo(path + "-wal").size() > 0) {
        result.error = QStringLiteral("В журнале базы появились новые записи. Преобразование отменено."); return result;
    }
    for (const auto& suffix : {QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")}) {
        if (QFile::exists(path + suffix) && !QFile::remove(path + suffix)) {
            result.error = QStringLiteral("Не удалось закрыть файлы журнала базы."); return result;
        }
    }
    if (!publish(candidate.path, path, result.error)) return result;
    DatabaseAccess installed{current.path, candidate.key};
    if (!validate(installed, result.error)) {
        DatabaseAccess backup{recovery, backupSource.key};
        DatabaseAccess rollback{temporary.filePath("rollback.db"), current.key};
        QString rollbackError;
        if (!copyDatabase(backup, rollback, rollbackError) || !validate(rollback, rollbackError)
            || !publish(rollback.path, path, rollbackError) || !validate(current, rollbackError)) {
            result.error += QStringLiteral(" Откат не подтверждён. Копия для восстановления: %1").arg(recovery);
            return result;
        }
        recordPolicy(current);
        if (encrypted) { QFile::remove(recovery); result.recoveryPath.clear(); }
        return result;
    }
    result.access = installed; result.ok = true; recordPolicy(installed);
    if (encrypted && QFile::remove(recovery)) result.recoveryPath.clear();
    return result;
}
}
