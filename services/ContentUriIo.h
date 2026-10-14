#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace ContentUriIo
{
bool isContentUri(const QUrl& url);
bool readAll(const QUrl& url, QByteArray& bytes, QString& error);
bool writeAll(const QUrl& url, const QByteArray& bytes, QString& error);
}
