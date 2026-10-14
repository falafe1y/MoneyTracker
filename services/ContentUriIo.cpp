#include "ContentUriIo.h"

#include <QCoreApplication>

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>
#endif

namespace ContentUriIo
{
bool isContentUri(const QUrl& url)
{
    return url.scheme().compare(QStringLiteral("content"), Qt::CaseInsensitive) == 0;
}

#if defined(Q_OS_ANDROID)
namespace
{
QJniObject contentResolver()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    return context.callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
}

QJniObject uriOf(const QUrl& url)
{
    return QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(url.toString()).object<jstring>());
}
}

bool readAll(const QUrl& url, QByteArray& bytes, QString& error)
{
    const QJniObject stream = contentResolver().callObjectMethod(
        "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;", uriOf(url).object());
    if (!stream.isValid()) {
        error = QCoreApplication::translate("ContentUriIo", "Не удалось открыть файл");
        return false;
    }

    QJniEnvironment env;
    jbyteArray buffer = env->NewByteArray(64 * 1024);
    bool ok = true;
    for (;;) {
        const jint count = stream.callMethod<jint>("read", "([B)I", buffer);
        if (env.checkAndClearExceptions()) {
            ok = false;
            break;
        }
        if (count < 0) {
            break;
        }
        QByteArray chunk(count, Qt::Uninitialized);
        env->GetByteArrayRegion(buffer, 0, count, reinterpret_cast<jbyte*>(chunk.data()));
        bytes.append(chunk);
    }
    stream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    env->DeleteLocalRef(buffer);
    if (!ok) {
        error = QCoreApplication::translate("ContentUriIo", "Не удалось прочитать файл");
    }
    return ok;
}

bool writeAll(const QUrl& url, const QByteArray& bytes, QString& error)
{
    const QJniObject stream = contentResolver().callObjectMethod(
        "openOutputStream", "(Landroid/net/Uri;Ljava/lang/String;)Ljava/io/OutputStream;",
        uriOf(url).object(), QJniObject::fromString(QStringLiteral("wt")).object<jstring>());
    if (!stream.isValid()) {
        error = QCoreApplication::translate("ContentUriIo", "Не удалось открыть файл для записи");
        return false;
    }

    QJniEnvironment env;
    const jsize size = static_cast<jsize>(bytes.size());
    jbyteArray buffer = env->NewByteArray(size);
    env->SetByteArrayRegion(buffer, 0, size, reinterpret_cast<const jbyte*>(bytes.constData()));
    stream.callMethod<void>("write", "([B)V", buffer);
    stream.callMethod<void>("flush", "()V");
    stream.callMethod<void>("close", "()V");
    const bool ok = !env.checkAndClearExceptions();
    env->DeleteLocalRef(buffer);
    if (!ok) {
        error = QCoreApplication::translate("ContentUriIo", "Не удалось записать файл");
    }
    return ok;
}
#else
bool readAll(const QUrl&, QByteArray&, QString& error)
{
    error = QCoreApplication::translate("ContentUriIo", "Этот адрес файла поддерживается только на Android");
    return false;
}

bool writeAll(const QUrl&, const QByteArray&, QString& error)
{
    error = QCoreApplication::translate("ContentUriIo", "Этот адрес файла поддерживается только на Android");
    return false;
}
#endif
}
