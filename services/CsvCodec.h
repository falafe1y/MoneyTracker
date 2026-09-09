#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class CsvCodec
{
public:
    struct ReadResult
    {
        QVector<QStringList> rows;
        QString error;
    };

    static bool writeFile(
        const QString& filePath,
        const QVector<QStringList>& rows,
        QString& error
        );
    static ReadResult readFile(const QString& filePath);
};
