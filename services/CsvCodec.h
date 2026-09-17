#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class CsvCodec
{
public:
    enum class Encoding
    {
        Auto,
        Utf8,
        Windows1251,
        Utf16Le,
        Utf16Be
    };

    struct ReadOptions
    {
        QChar delimiter;
        Encoding encoding = Encoding::Auto;
    };

    struct ReadResult
    {
        QVector<QStringList> rows;
        QString error;
        QChar delimiter;
        QString encoding;
    };

    static bool writeFile(
        const QString& filePath,
        const QVector<QStringList>& rows,
        QString& error
        );
    static ReadResult readFile(const QString& filePath);
    static ReadResult readFile(
        const QString& filePath,
        const ReadOptions& options
        );
};
