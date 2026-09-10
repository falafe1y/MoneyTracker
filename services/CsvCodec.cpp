#include "CsvCodec.h"

#include <QFile>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

namespace
{
QString encodeField(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QLatin1Char('"');
}
}

bool CsvCodec::writeFile(
    const QString& filePath,
    const QVector<QStringList>& rows,
    QString& error
    )
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }

    file.write("\xEF\xBB\xBF", 3);
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const QStringList& row : rows) {
        QStringList encoded;
        encoded.reserve(row.size());
        for (const QString& field : row) {
            encoded.append(encodeField(field));
        }
        stream << encoded.join(QLatin1Char(';')) << '\n';
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

CsvCodec::ReadResult CsvCodec::readFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, file.errorString()};
    }
    QByteArray bytes = file.readAll();
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        bytes.remove(0, 3);
    }
    const QString text = QString::fromUtf8(bytes);

    ReadResult result;
    QStringList row;
    QString field;
    bool quoted = false;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text[index];
        if (quoted) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < text.size() && text[index + 1] == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++index;
                } else {
                    quoted = false;
                }
            } else {
                field += character;
            }
        } else if (character == QLatin1Char('"')) {
            if (!field.isEmpty()) {
                return {{}, QStringLiteral("Unexpected quote in CSV field")};
            }
            quoted = true;
        } else if (character == QLatin1Char(';')) {
            row.append(field);
            field.clear();
        } else if (character == QLatin1Char('\n')) {
            row.append(field.endsWith(QLatin1Char('\r')) ? field.chopped(1) : field);
            field.clear();
            result.rows.append(row);
            row.clear();
        } else {
            field += character;
        }
    }
    if (quoted) {
        return {{}, QStringLiteral("Unclosed quoted CSV field")};
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row.append(field);
        result.rows.append(row);
    }
    return result;
}
