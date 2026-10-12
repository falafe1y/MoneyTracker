#include "PasswordStore.h"
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#endif

namespace {
QString unavailable()
{
    return QCoreApplication::translate("AppAccess", "Системное хранилище учётных данных недоступно. Разблокируйте его и повторите попытку.");
}
class NativePasswordStore final : public PasswordStore {
public:
    bool available(QString& error) override
    {
#ifdef Q_OS_WIN
        // Enumerate metadata for a deliberately absent name; do not retrieve secrets.
        DWORD count = 0;
        PCREDENTIALW* entries = nullptr;
        if (CredEnumerateW(L"Vexa/availability-check/*", 0, &count, &entries)) {
            CredFree(entries);
            return true;
        }
        if (GetLastError() == ERROR_NOT_FOUND) return true;
        error = unavailable();
        return false;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        if (QStandardPaths::findExecutable("secret-tool").isEmpty()) {
            error = QCoreApplication::translate("AppAccess", "Системное хранилище недоступно: не найден secret-tool.");
            return false;
        }
        const auto bus = QDBusConnection::sessionBus();
        auto alias = QDBusMessage::createMethodCall("org.freedesktop.secrets", "/org/freedesktop/secrets", "org.freedesktop.Secret.Service", "ReadAlias");
        alias << QStringLiteral("default");
        const auto reply = bus.call(alias, QDBus::Block, 1500);
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) {
            error = unavailable();return false;
        }
        const auto path = qvariant_cast<QDBusObjectPath>(reply.arguments().first()).path();
        if (path.isEmpty() || path == "/") {
            error = QCoreApplication::translate("AppAccess", "Системное хранилище недоступно: хранилище по умолчанию не настроено.");
            return false;
        }
        auto locked = QDBusMessage::createMethodCall("org.freedesktop.secrets", path, "org.freedesktop.DBus.Properties", "Get");
        locked << QStringLiteral("org.freedesktop.Secret.Collection") << QStringLiteral("Locked");
        const auto state = bus.call(locked, QDBus::Block, 1500);
        if (state.type() != QDBusMessage::ReplyMessage || state.arguments().size() != 1) {
            error = unavailable();return false;
        }
        const auto property = qvariant_cast<QDBusVariant>(state.arguments().first()).variant();
        if (property.metaType().id() != QMetaType::Bool) { error = unavailable();return false; }
        if (property.toBool()) {
            error = QCoreApplication::translate("AppAccess", "Системное хранилище заблокировано. Разблокируйте его и перезапустите Vexa.");
            return false;
        }
        return true;
#else
        error = unavailable();return false;
#endif
    }
    bool read(QByteArray& value, QString& error) override
    {
#ifdef Q_OS_WIN
        PCREDENTIALW credential = nullptr;
        if (!CredReadW(L"Vexa/application-access/v1", CRED_TYPE_GENERIC, 0, &credential)) {
            error = unavailable();
            return false;
        }
        value = QByteArray(reinterpret_cast<const char*>(credential->CredentialBlob),
                           credential->CredentialBlobSize);
        SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
        CredFree(credential);
        return true;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        return run({"lookup", "application", "Vexa", "purpose", "application-access-v1"}, {}, value, error);
#else
        Q_UNUSED(value);
        error = unavailable();
        return false;
#endif
    }
    bool write(const QByteArray& value, QString& error) override
    {
#ifdef Q_OS_WIN
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<wchar_t*>(L"Vexa/application-access/v1");
        credential.UserName = const_cast<wchar_t*>(L"Vexa");
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        credential.CredentialBlobSize = static_cast<DWORD>(value.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.constData()));
        if (CredWriteW(&credential, 0)) return true;
        error = unavailable();
        return false;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        QByteArray output;
        return run({"store", "--label=Vexa", "application", "Vexa", "purpose", "application-access-v1"}, value, output, error);
#else
        Q_UNUSED(value);
        error = unavailable();
        return false;
#endif
    }
private:
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    static bool run(const QStringList& arguments, const QByteArray& input,
                    QByteArray& output, QString& error)
    {
        const auto executable = QStandardPaths::findExecutable("secret-tool");
        if (executable.isEmpty()) {
            error = QCoreApplication::translate("AppAccess", "Для защиты паролем нужны secret-tool и системное хранилище Secret Service.");
            return false;
        }
        QProcess process;
        process.start(executable, arguments);
        if (!process.waitForStarted(5000)) { error = unavailable(); return false; }
        if (!input.isEmpty() && process.write(input) != input.size()) {
            process.kill(); process.waitForFinished(); error = unavailable(); return false;
        }
        process.closeWriteChannel();
        if (!process.waitForFinished(60000)) {
            process.kill(); process.waitForFinished(); error = unavailable(); return false;
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            error = unavailable(); return false;
        }
        output = process.readAllStandardOutput().trimmed();
        return true;
    }
#endif
};
}
std::shared_ptr<PasswordStore> nativePasswordStore()
{
    return std::make_shared<NativePasswordStore>();
}
