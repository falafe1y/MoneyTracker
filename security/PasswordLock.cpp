#include "PasswordLock.h"
#include <argon2.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <limits>

namespace {
QString message(const char* text) { return QCoreApplication::translate("AppAccess", text); }
QByteArray randomBytes(int size)
{
    QByteArray bytes(size, '\0');
    for (int i = 0; i < size; i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4 && i + j < size; ++j) bytes[i + j] = char(value >> (j * 8));
    }
    return bytes;
}
QByteArray recoveryHash(const QString& code)
{
    return QCryptographicHash::hash(QByteArrayLiteral("vexa/access-recovery/v1/") + code.toUtf8(),
                                    QCryptographicHash::Sha256).toHex();
}
bool equalConstantTime(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size()) return false;
    volatile unsigned char different = 0;
    for (qsizetype i = 0; i < a.size(); ++i) different = different | (uchar(a[i]) ^ uchar(b[i]));
    return different == 0;
}
QJsonObject emptyRecord()
{
    return {{"version", 1}, {"verifier", QJsonValue::Null}, {"recoveryHash", QJsonValue::Null},
            {"failures", 0}, {"nextAttempt", 0}};
}
}
PasswordLock::PasswordLock(QString directory, std::shared_ptr<PasswordStore> store,
                           std::function<qint64()> clock)
    : marker_(QDir(directory).filePath("access.lock")), store_(std::move(store)),
      clock_(clock ? std::move(clock) : [] { return QDateTime::currentSecsSinceEpoch(); }) {}

bool PasswordLock::validPassword(const QString& password)
{
    const auto length = password.toUcs4().size();
    return length >= 1 && length <= 1024;
}
bool PasswordLock::read(QJsonObject& record, QString& error)
{
    if (!QFile::exists(marker_)) { record = emptyRecord(); return true; }
    QByteArray bytes;
    if (!store_->read(bytes, error)) return false;
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    bytes.fill('\0');
    record = doc.object();
    const auto verifier = record.value("verifier");
    const auto recovery = record.value("recoveryHash");
    static const QRegularExpression hashPattern("^[0-9a-f]{64}$");
    static const QRegularExpression verifierPattern(
        "^\\$argon2id\\$v=19\\$m=19456,t=2,p=1\\$[A-Za-z0-9+/]{22}\\$[A-Za-z0-9+/]{43}$");
    const double failures = record.value("failures").toDouble(-1);
    const double next = record.value("nextAttempt").toDouble(-1);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || record.value("version").toInt() != 1
        || !record.contains("verifier") || !record.contains("recoveryHash")
        || !((verifier.isNull() && recovery.isNull())
             || (verifier.isString() && recovery.isString()
                 && verifierPattern.match(verifier.toString()).hasMatch()
                 && hashPattern.match(recovery.toString()).hasMatch()))
        || failures < 0 || failures > std::numeric_limits<quint32>::max() || failures != quint32(failures)
        || next < 0 || next > 9007199254740991.0 || next != qint64(next)) {
        error = message("Запись защиты отсутствует или повреждена. Доступ к данным закрыт.");
        return false;
    }
    return true;
}
bool PasswordLock::write(const QJsonObject& record, QString& error)
{
    QByteArray encoded = QJsonDocument(record).toJson(QJsonDocument::Compact);
    QByteArray saved;
    const bool ok = store_->write(encoded, error) && store_->read(saved, error)
        && equalConstantTime(encoded, saved);
    encoded.fill('\0'); saved.fill('\0');
    if (!ok && error.isEmpty()) error = message("Не удалось подтвердить сохранение защиты. Повторите попытку.");
    return ok;
}
AccessResult PasswordLock::status()
{
    AccessResult result;
    result.storeChecked = true;
    result.storeAvailable = store_->available(result.storeError);
    // Never bypass existing protection when credentials cannot be reached.
    if (!result.storeAvailable && QFile::exists(marker_)) {
        result.error = result.storeError;return result;
    }
    QJsonObject record;
    if (!read(record, result.error)) return result;
    result.ok = true;
    result.enabled = !record.value("verifier").isNull();
    result.retryAfter = std::max<qint64>(0, qint64(record.value("nextAttempt").toDouble()) - clock_());
    return result;
}
AccessResult PasswordLock::verifyRecord(QJsonObject& record, const QString& proof)
{
    AccessResult result;
    result.enabled = !record.value("verifier").isNull();
    if (!result.enabled) { result.ok = true; return result; }
    const auto now = clock_();
    result.retryAfter = std::max<qint64>(0, qint64(record.value("nextAttempt").toDouble()) - now);
    if (result.retryAfter > 0) { result.error = message("Слишком много попыток. Подождите."); return result; }
    if (proof.isEmpty()) { result.error = message("Введите пароль или код восстановления."); return result; }
    bool passwordMatches = false;
    // A recovery code also fits this bound; never hash unbounded renderer input.
    if (validPassword(proof)) {
        QByteArray utf8 = proof.toUtf8();
        const auto verifier = record.value("verifier").toString().toLatin1();
        passwordMatches = argon2id_verify(verifier.constData(), utf8.constData(), utf8.size()) == ARGON2_OK;
        utf8.fill('\0');
    }
    QString normalized;
    if (proof.size() <= 4096) {
        for (const auto ch : proof) if (!ch.isSpace() && ch != QLatin1Char('-')) normalized += ch;
        normalized = normalized.toUpper();
    }
    const bool recoveryMatches = normalized.size() == 32
        && equalConstantTime(recoveryHash(normalized), record.value("recoveryHash").toString().toLatin1());
    if (!passwordMatches && !recoveryMatches) {
        const auto failures = std::min<quint64>(quint64(record.value("failures").toDouble()) + 1,
                                               std::numeric_limits<quint32>::max());
        record["failures"] = double(failures);
        if (failures >= 5) {
            result.retryAfter = std::min<qint64>(30LL << std::min<quint64>(failures - 5, 5), 900);
            record["nextAttempt"] = double(now + result.retryAfter);
        }
        if (!write(record, result.error)) return result;
        result.error = result.retryAfter ? message("Слишком много попыток. Подождите.") : message("Неверный пароль или код восстановления.");
        return result;
    }
    if (record.value("failures").toDouble() != 0 || record.value("nextAttempt").toDouble() != 0) {
        record["failures"] = 0; record["nextAttempt"] = 0;
        if (!write(record, result.error)) return result;
    }
    result.ok = true;
    return result;
}
AccessResult PasswordLock::verify(const QString& proof)
{
    QJsonObject record;
    AccessResult result;
    if (!read(record, result.error)) return result;
    return verifyRecord(record, proof);
}
AccessResult PasswordLock::change(const QString& proof, const QString& password, bool disable)
{
    AccessResult result;
    if (!store_->available(result.error)) return result;
    if (!disable && !validPassword(password)) {
        result.error = message("Пароль должен содержать от 1 до 1024 символов."); return result;
    }
    QJsonObject current;
    if (!read(current, result.error)) return result;
    result = verifyRecord(current, proof);
    if (!result.ok) return result;
    result.ok = false;
    auto next = emptyRecord();
    QString recoveryCode;
    if (!disable) {
        auto utf8 = password.toUtf8();
        const auto salt = randomBytes(16);
        char encoded[128]{};
        const int status = argon2id_hash_encoded(2, 19 * 1024, 1, utf8.constData(), utf8.size(),
                                                salt.constData(), salt.size(), 32, encoded, sizeof encoded);
        utf8.fill('\0');
        if (status != ARGON2_OK) { result.error = message("Не удалось подготовить защиту паролем."); return result; }
        const auto code = QString::fromLatin1(randomBytes(16).toHex().toUpper());
        for (int i = 0; i < code.size(); i += 4) {
            if (i) recoveryCode += QLatin1Char('-');
            recoveryCode += code.mid(i, 4);
        }
        next["verifier"] = QString::fromLatin1(encoded);
        next["recoveryHash"] = QString::fromLatin1(recoveryHash(code));
    }
    // This marker is deliberately never removed, including when protection is disabled.
    // A failed credential write/read-back must not restore passwordless admission.
    if (!QFile::exists(marker_)) {
        QSaveFile marker(marker_);
        if (!marker.open(QIODevice::WriteOnly) || marker.write("VEXA_ACCESS_1\n") != 14 || !marker.commit()) {
            result.error = message("Не удалось сохранить состояние защиты."); return result;
        }
    }
    if (!write(next, result.error)) return result;
    result.ok = true;
    result.enabled = !disable;
    result.recoveryCode = recoveryCode;
    return result;
}
