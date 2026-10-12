#include "PasswordStore.h"
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString unavailable()
{
    return QCoreApplication::translate("AppAccess", "Системное хранилище учётных данных недоступно. Разблокируйте его и повторите попытку.");
}
class NativePasswordStore final : public PasswordStore {
public:
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
