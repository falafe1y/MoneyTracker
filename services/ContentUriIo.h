#pragma once

#include <QByteArray>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

namespace ContentUriIo
{
bool isContentUri(const QUrl& url);
// The name a picker shows for a content:// document; empty when the document has none
QString displayName(const QUrl& url);
bool readAll(const QUrl& url, QByteArray& bytes, QString& error);
bool writeAll(const QUrl& url, const QByteArray& bytes, QString& error);
// Writes a local file over a document chosen in a save dialog
bool copyFileTo(const QString& path, const QUrl& destination, QString& error);
// A private folder in the cache; it and its files are removed with the returned object
std::unique_ptr<QTemporaryDir> temporaryDirectory(QString& error);

// A picked document as a local file. A content:// document has no path on Android, so its bytes are copied
// to a temporary file that keeps the document's name: readers choose the format by the suffix.
// The copy exists as long as this object does.
class LocalCopy {
public:
    explicit LocalCopy(const QUrl& url);

    QString path() const { return path_; }
    QString error() const { return error_; }

private:
    std::unique_ptr<QTemporaryDir> directory_;
    QString path_, error_;
};
}
