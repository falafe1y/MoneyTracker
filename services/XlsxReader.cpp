#include "XlsxReader.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTime>
#include <QXmlStreamReader>

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
quint16 little16(const QByteArray& bytes, const qsizetype offset)
{
    if (offset < 0 || offset + 2 > bytes.size()) return 0;
    const auto* data = reinterpret_cast<const uchar*>(bytes.constData() + offset);
    return static_cast<quint16>(data[0] | (data[1] << 8));
}

quint32 little32(const QByteArray& bytes, const qsizetype offset)
{
    if (offset < 0 || offset + 4 > bytes.size()) return 0;
    const auto* data = reinterpret_cast<const uchar*>(bytes.constData() + offset);
    return static_cast<quint32>(data[0]) |
        (static_cast<quint32>(data[1]) << 8) |
        (static_cast<quint32>(data[2]) << 16) |
        (static_cast<quint32>(data[3]) << 24);
}

struct ZipEntry
{
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localOffset = 0;
};

QHash<QString, ZipEntry> zipEntries(const QByteArray& archive, QString& error)
{
    QHash<QString, ZipEntry> result;
    qsizetype endOffset = -1;
    const qsizetype start = std::max<qsizetype>(0, archive.size() - 65'557);
    for (qsizetype offset = archive.size() - 22; offset >= start; --offset) {
        if (little32(archive, offset) == 0x06054b50U) {
            endOffset = offset;
            break;
        }
    }
    if (endOffset < 0) {
        error = QStringLiteral("XLSX: ZIP directory was not found");
        return result;
    }
    const quint16 count = little16(archive, endOffset + 10);
    qsizetype offset = little32(archive, endOffset + 16);
    for (quint16 index = 0; index < count; ++index) {
        if (offset + 46 > archive.size() ||
            little32(archive, offset) != 0x02014b50U) {
            error = QStringLiteral("XLSX: invalid ZIP directory");
            return {};
        }
        const quint16 nameLength = little16(archive, offset + 28);
        const quint16 extraLength = little16(archive, offset + 30);
        const quint16 commentLength = little16(archive, offset + 32);
        if (offset + 46 + nameLength + extraLength + commentLength > archive.size()) {
            error = QStringLiteral("XLSX: truncated ZIP entry");
            return {};
        }
        ZipEntry entry;
        entry.method = little16(archive, offset + 10);
        entry.compressedSize = little32(archive, offset + 20);
        entry.uncompressedSize = little32(archive, offset + 24);
        entry.localOffset = little32(archive, offset + 42);
        entry.name = QString::fromUtf8(archive.constData() + offset + 46, nameLength);
        result.insert(entry.name, entry);
        offset += 46 + nameLength + extraLength + commentLength;
    }
    return result;
}

QByteArray inflateRaw(const QByteArray& compressed, const quint32 expectedSize,
                      QString& error)
{
    if (expectedSize > 128U * 1024U * 1024U) {
        error = QStringLiteral("XLSX: worksheet is too large");
        return {};
    }
    QByteArray output;
    output.resize(static_cast<qsizetype>(expectedSize));
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        error = QStringLiteral("XLSX: cannot initialize decompression");
        return {};
    }
    const int status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        error = QStringLiteral("XLSX: cannot decompress worksheet");
        return {};
    }
    output.resize(static_cast<qsizetype>(stream.total_out));
    return output;
}

QByteArray entryData(const QByteArray& archive, const ZipEntry& entry,
                     QString& error)
{
    const qsizetype offset = entry.localOffset;
    if (offset + 30 > archive.size() || little32(archive, offset) != 0x04034b50U) {
        error = QStringLiteral("XLSX: invalid ZIP entry");
        return {};
    }
    const quint16 nameLength = little16(archive, offset + 26);
    const quint16 extraLength = little16(archive, offset + 28);
    const qsizetype dataOffset = offset + 30 + nameLength + extraLength;
    if (dataOffset < 0 || dataOffset + entry.compressedSize > archive.size()) {
        error = QStringLiteral("XLSX: truncated ZIP data");
        return {};
    }
    const QByteArray compressed = archive.mid(dataOffset, entry.compressedSize);
    if (entry.method == 0) return compressed;
    if (entry.method == 8)
        return inflateRaw(compressed, entry.uncompressedSize, error);
    error = QStringLiteral("XLSX: unsupported ZIP compression");
    return {};
}

QStringList sharedStrings(const QByteArray& xml, QString& error)
{
    QStringList result;
    QXmlStreamReader reader(xml);
    QString current;
    bool inString = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("si")) {
            current.clear();
            inString = true;
        } else if (inString && reader.isStartElement() &&
                   reader.name() == QStringLiteral("t")) {
            current += reader.readElementText();
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("si")) {
            result.append(current);
            inString = false;
        }
    }
    if (reader.hasError()) error = QStringLiteral("XLSX: invalid shared strings XML");
    return result;
}

QSet<int> dateStyleIndexes(const QByteArray& xml)
{
    QSet<int> customDateFormats;
    QSet<int> styles;
    QXmlStreamReader reader(xml);
    bool inCellFormats = false;
    int styleIndex = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            if (reader.isEndElement() && reader.name() == QStringLiteral("cellXfs"))
                inCellFormats = false;
            continue;
        }
        if (reader.name() == QStringLiteral("numFmt")) {
            const int id = reader.attributes().value(QStringLiteral("numFmtId")).toInt();
            QString format = reader.attributes().value(QStringLiteral("formatCode")).toString().toLower();
            format.remove(QRegularExpression(QStringLiteral("\"[^\"]*\"|\\\\.")));
            if ((format.contains(QLatin1Char('y')) || format.contains(QLatin1Char('d'))) &&
                format.contains(QLatin1Char('m'))) customDateFormats.insert(id);
        } else if (reader.name() == QStringLiteral("cellXfs")) {
            inCellFormats = true;
            styleIndex = 0;
        } else if (inCellFormats && reader.name() == QStringLiteral("xf")) {
            const int id = reader.attributes().value(QStringLiteral("numFmtId")).toInt();
            if ((id >= 14 && id <= 22) || (id >= 45 && id <= 47) ||
                customDateFormats.contains(id)) styles.insert(styleIndex);
            ++styleIndex;
        }
    }
    return styles;
}

int columnIndex(const QString& reference)
{
    int result = 0;
    int count = 0;
    for (const QChar character : reference) {
        if (!character.isLetter()) break;
        result = result * 26 + character.toUpper().unicode() - QLatin1Char('A').unicode() + 1;
        ++count;
    }
    return count == 0 ? -1 : result - 1;
}

QString excelDate(const QString& numeric)
{
    bool ok = false;
    const double serial = numeric.toDouble(&ok);
    if (!ok || !std::isfinite(serial)) return numeric;
    const qint64 days = static_cast<qint64>(std::floor(serial));
    const QDate date = QDate(1899, 12, 30).addDays(days);
    if (!date.isValid()) return numeric;
    const int seconds = static_cast<int>(std::llround((serial - days) * 86'400.0));
    if (seconds <= 0 || seconds >= 86'400) return date.toString(QStringLiteral("dd.MM.yyyy"));
    return QDateTime(date, QTime(0, 0).addSecs(seconds)).toString(
        QStringLiteral("dd.MM.yyyy HH:mm:ss"));
}

QVector<QStringList> sheetRows(const QByteArray& xml,
                               const QStringList& strings,
                               const QSet<int>& dateStyles,
                               QString& error)
{
    QVector<QStringList> rows;
    QXmlStreamReader reader(xml);
    QStringList row;
    QString cellType;
    QString cellValue;
    int cellColumn = -1;
    int cellStyle = -1;
    bool inCell = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("row")) {
            row.clear();
        } else if (reader.isStartElement() && reader.name() == QStringLiteral("c")) {
            inCell = true;
            cellValue.clear();
            cellType = reader.attributes().value(QStringLiteral("t")).toString();
            cellColumn = columnIndex(reader.attributes().value(QStringLiteral("r")).toString());
            cellStyle = reader.attributes().value(QStringLiteral("s")).toInt();
        } else if (inCell && reader.isStartElement() &&
                   (reader.name() == QStringLiteral("v") ||
                    reader.name() == QStringLiteral("t"))) {
            cellValue += reader.readElementText();
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("c")) {
            QString value = cellValue;
            if (cellType == QStringLiteral("s")) {
                bool ok = false;
                const int index = value.toInt(&ok);
                value = ok && index >= 0 && index < strings.size() ? strings[index] : QString();
            } else if (dateStyles.contains(cellStyle)) {
                value = excelDate(value);
            }
            if (cellColumn < 0) cellColumn = row.size();
            while (row.size() <= cellColumn) row.append(QString());
            row[cellColumn] = value;
            inCell = false;
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("row")) {
            while (!row.isEmpty() && row.constLast().isEmpty()) row.removeLast();
            rows.append(row);
        }
    }
    if (reader.hasError()) error = QStringLiteral("XLSX: invalid worksheet XML");
    return rows;
}
}

XlsxReader::Result XlsxReader::readFirstSheet(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {{}, file.errorString()};
    if (file.size() > 128LL * 1024LL * 1024LL)
        return {{}, QStringLiteral("XLSX: file is too large")};
    const QByteArray archive = file.readAll();
    QString error;
    const auto entries = zipEntries(archive, error);
    if (!error.isEmpty()) return {{}, error};

    QStringList sheets;
    for (auto iterator = entries.cbegin(); iterator != entries.cend(); ++iterator) {
        if (iterator.key().startsWith(QStringLiteral("xl/worksheets/sheet")) &&
            iterator.key().endsWith(QStringLiteral(".xml"))) sheets.append(iterator.key());
    }
    std::sort(sheets.begin(), sheets.end());
    if (sheets.isEmpty()) return {{}, QStringLiteral("XLSX: worksheet was not found")};

    QStringList strings;
    if (entries.contains(QStringLiteral("xl/sharedStrings.xml"))) {
        strings = sharedStrings(entryData(archive,
            entries.value(QStringLiteral("xl/sharedStrings.xml")), error), error);
        if (!error.isEmpty()) return {{}, error};
    }
    QSet<int> dateStyles;
    if (entries.contains(QStringLiteral("xl/styles.xml"))) {
        dateStyles = dateStyleIndexes(entryData(
            archive, entries.value(QStringLiteral("xl/styles.xml")), error));
        if (!error.isEmpty()) return {{}, error};
    }
    const QByteArray sheet = entryData(archive, entries.value(sheets.first()), error);
    if (!error.isEmpty()) return {{}, error};
    return {sheetRows(sheet, strings, dateStyles, error), error};
}
