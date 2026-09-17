#include "CsvCodec.h"

#include <QFile>
#include <QHash>
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

QString decodeWindows1251(const QByteArray& bytes)
{
    static constexpr char16_t extended[] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
    };

    QString result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        if (byte < 0x80) {
            result.append(QChar(byte));
        } else if (byte >= 0xC0) {
            result.append(QChar(0x0410 + byte - 0xC0));
        } else {
            const char16_t codePoint = extended[byte - 0x80];
            result.append(codePoint == 0 ? QChar(0xFFFD)
                                         : QChar(codePoint));
        }
    }
    return result;
}

QString decodeBytes(
    QByteArray bytes,
    const CsvCodec::Encoding requested,
    QString& encoding,
    QString& error
    )
{
    CsvCodec::Encoding actual = requested;
    if (actual == CsvCodec::Encoding::Auto) {
        if (bytes.startsWith("\xEF\xBB\xBF")) {
            actual = CsvCodec::Encoding::Utf8;
        } else if (bytes.startsWith("\xFF\xFE")) {
            actual = CsvCodec::Encoding::Utf16Le;
        } else if (bytes.startsWith("\xFE\xFF")) {
            actual = CsvCodec::Encoding::Utf16Be;
        } else {
            QStringDecoder decoder(QStringConverter::Utf8);
            decoder.decode(bytes);
            actual = decoder.hasError()
                ? CsvCodec::Encoding::Windows1251
                : CsvCodec::Encoding::Utf8;
        }
    }

    if (actual == CsvCodec::Encoding::Utf8) {
        if (bytes.startsWith("\xEF\xBB\xBF")) {
            bytes.remove(0, 3);
        }
        QStringDecoder decoder(QStringConverter::Utf8);
        const QString text = decoder.decode(bytes);
        if (decoder.hasError()) {
            error = QStringLiteral("Invalid UTF-8 CSV file");
            return {};
        }
        encoding = QStringLiteral("UTF-8");
        return text;
    }
    if (actual == CsvCodec::Encoding::Utf16Le ||
        actual == CsvCodec::Encoding::Utf16Be) {
        if (bytes.startsWith("\xFF\xFE") || bytes.startsWith("\xFE\xFF")) {
            bytes.remove(0, 2);
        }
        const auto qtEncoding = actual == CsvCodec::Encoding::Utf16Le
            ? QStringConverter::Utf16LE
            : QStringConverter::Utf16BE;
        QStringDecoder decoder(qtEncoding);
        const QString text = decoder.decode(bytes);
        if (decoder.hasError()) {
            error = QStringLiteral("Invalid UTF-16 CSV file");
            return {};
        }
        encoding = actual == CsvCodec::Encoding::Utf16Le
            ? QStringLiteral("UTF-16LE")
            : QStringLiteral("UTF-16BE");
        return text;
    }

    encoding = QStringLiteral("Windows-1251");
    return decodeWindows1251(bytes);
}

CsvCodec::ReadResult parseText(const QString& text, const QChar delimiter)
{
    CsvCodec::ReadResult result;
    result.delimiter = delimiter;
    QStringList row;
    QString field;
    bool quoted = false;
    bool quoteClosed = false;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text[index];
        if (quoted) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < text.size() &&
                    text[index + 1] == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++index;
                } else {
                    quoted = false;
                    quoteClosed = true;
                }
            } else {
                field += character;
            }
        } else if (character == QLatin1Char('"')) {
            if (!field.isEmpty() || quoteClosed) {
                result.error = QStringLiteral("Unexpected quote in CSV field");
                return result;
            }
            quoted = true;
        } else if (character == delimiter) {
            row.append(field);
            field.clear();
            quoteClosed = false;
        } else if (character == QLatin1Char('\n')) {
            if (field.endsWith(QLatin1Char('\r'))) {
                field.chop(1);
            }
            row.append(field);
            field.clear();
            quoteClosed = false;
            result.rows.append(row);
            row.clear();
        } else if (quoteClosed && !character.isSpace()) {
            result.error = QStringLiteral("Unexpected text after quoted CSV field");
            return result;
        } else if (!quoteClosed) {
            field += character;
        }
    }
    if (quoted) {
        result.error = QStringLiteral("Unclosed quoted CSV field");
        return result;
    }
    if (!field.isEmpty() || !row.isEmpty() || quoteClosed) {
        row.append(field);
        result.rows.append(row);
    }
    return result;
}

QChar detectDelimiter(const QString& text)
{
    const QVector<QChar> candidates{
        QLatin1Char(';'), QLatin1Char(','), QLatin1Char('\t')};
    QChar best = QLatin1Char(';');
    qint64 bestScore = -1;
    for (const QChar candidate : candidates) {
        const CsvCodec::ReadResult parsed = parseText(text, candidate);
        if (!parsed.error.isEmpty()) {
            continue;
        }

        QHash<qsizetype, int> frequencies;
        int inspected = 0;
        for (const QStringList& row : parsed.rows) {
            if (row.size() <= 1) {
                continue;
            }
            ++frequencies[row.size()];
            if (++inspected >= 30) {
                break;
            }
        }
        for (auto iterator = frequencies.cbegin();
             iterator != frequencies.cend(); ++iterator) {
            const qint64 score = static_cast<qint64>(iterator.key()) *
                iterator.value() * iterator.value();
            if (score > bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
    }
    return best;
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

CsvCodec::ReadResult CsvCodec::readFile(
    const QString& filePath
    )
{
    return readFile(filePath, ReadOptions{});
}

CsvCodec::ReadResult CsvCodec::readFile(
    const QString& filePath,
    const ReadOptions& options
    )
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, file.errorString(), {}, {}};
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return {{}, file.errorString(), {}, {}};
    }

    QString encoding;
    QString error;
    const QString text = decodeBytes(bytes, options.encoding, encoding, error);
    if (!error.isEmpty()) {
        return {{}, error, {}, encoding};
    }
    const QChar delimiter = options.delimiter.isNull()
        ? detectDelimiter(text)
        : options.delimiter;
    ReadResult result = parseText(text, delimiter);
    result.encoding = encoding;
    return result;
}
