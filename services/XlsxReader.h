#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class XlsxReader final
{
public:
    struct Result
    {
        QVector<QStringList> rows;
        QString error;
    };

    static Result readFirstSheet(const QString& filePath);
};
