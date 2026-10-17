#include "ContentUriIo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

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

QString displayName(const QUrl& url)
{
    QJniEnvironment env;
    const QJniObject cursor = contentResolver().callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        uriOf(url).object(), jobject(nullptr), jobject(nullptr), jobject(nullptr), jobject(nullptr));
    if (env.checkAndClearExceptions() || !cursor.isValid()) {
        return {};
    }
    QString name;
    if (cursor.callMethod<jboolean>("moveToFirst", "()Z")) {
        const jint column = cursor.callMethod<jint>(
            "getColumnIndex", "(Ljava/lang/String;)I",
            QJniObject::fromString(QStringLiteral("_display_name")).object<jstring>());
        if (column >= 0) {
            name = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", column).toString();
        }
    }
    cursor.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    return name;
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

QString displayName(const QUrl&)
{
    return {};
}
#endif

bool copyFileTo(const QString& path, const QUrl& destination, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QCoreApplication::translate("ContentUriIo", "Не удалось прочитать файл");
        return false;
    }
    return writeAll(destination, file.readAll(), error);
}

std::unique_ptr<QTemporaryDir> temporaryDirectory(QString& error)
{
    // The cache belongs to the application alone, so it is a folder an Android app may write to
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cache.isEmpty() && QDir().mkpath(cache)) {
        auto directory = std::make_unique<QTemporaryDir>(cache + QStringLiteral("/picked-XXXXXX"));
        if (directory->isValid()) {
            return directory;
        }
    }
    error = QCoreApplication::translate("ContentUriIo", "Не удалось создать временную папку");
    return nullptr;
}

LocalCopy::LocalCopy(const QUrl& url)
{
    if (!isContentUri(url)) {
        path_ = url.toLocalFile();
        return;
    }
    directory_ = temporaryDirectory(error_);
    if (!directory_) {
        return;
    }
    QByteArray bytes;
    if (!readAll(url, bytes, error_)) {
        return;
    }
    const QString name = QFileInfo(displayName(url)).fileName();
    path_ = directory_->filePath(name.isEmpty() ? QStringLiteral("document.csv") : name);
    QFile file(path_);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        error_ = QCoreApplication::translate("ContentUriIo", "Не удалось записать файл");
        path_.clear();
    }
}
}
