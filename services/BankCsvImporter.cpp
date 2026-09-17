#include "BankCsvImporter.h"

#include <QCryptographicHash>
#include <QLocale>
#include <QTime>

#include <cmath>
#include <limits>

namespace
{
QString fieldAt(const QStringList& row, const int index)
{
    return index >= 0 && index < row.size() ? row[index].trimmed() : QString();
}

bool rowIsEmpty(const QStringList& row)
{
    for (const QString& field : row) {
        if (!field.trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

QDateTime dateAtNoon(const QDate& date)
{
    return date.isValid()
        ? QDateTime(date, QTime(12, 0), Qt::LocalTime)
        : QDateTime();
}

QString normalizedFingerprint(const QStringList& row)
{
    QStringList normalized;
    normalized.reserve(row.size());
    for (const QString& field : row) {
        normalized.append(field.simplified());
    }
    return QString::fromLatin1(QCryptographicHash::hash(
        normalized.join(QChar(0x001F)).toUtf8(),
        QCryptographicHash::Sha256).toHex());
}

qint64 positiveMagnitude(const qint64 value)
{
    using Int128 = __int128_t;
    const Int128 magnitude = value < 0
        ? -static_cast<Int128>(value)
        : static_cast<Int128>(value);
    return magnitude > std::numeric_limits<qint64>::max()
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(magnitude);
}
}

QJsonObject BankCsvProfile::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("accountId"), accountId},
        {QStringLiteral("incomeCategoryId"), incomeCategoryId},
        {QStringLiteral("expenseCategoryId"), expenseCategoryId},
        {QStringLiteral("encoding"), encoding},
        {QStringLiteral("delimiter"), delimiter},
        {QStringLiteral("dateFormat"), dateFormat},
        {QStringLiteral("amountMode"), amountMode},
        {QStringLiteral("headerRow"), headerRow},
        {QStringLiteral("dateColumn"), dateColumn},
        {QStringLiteral("amountColumn"), amountColumn},
        {QStringLiteral("incomeColumn"), incomeColumn},
        {QStringLiteral("expenseColumn"), expenseColumn},
        {QStringLiteral("descriptionColumn"), descriptionColumn},
        {QStringLiteral("idColumn"), idColumn},
        {QStringLiteral("categoryColumn"), categoryColumn},
        {QStringLiteral("positiveMeansIncome"), positiveMeansIncome}
    };
}

BankCsvProfile BankCsvProfile::fromJson(const QJsonObject& object)
{
    BankCsvProfile profile;
    profile.id = object.value(QStringLiteral("id")).toString();
    profile.name = object.value(QStringLiteral("name")).toString();
    profile.accountId = object.value(QStringLiteral("accountId")).toString();
    profile.incomeCategoryId = object.value(
        QStringLiteral("incomeCategoryId")).toString();
    profile.expenseCategoryId = object.value(
        QStringLiteral("expenseCategoryId")).toString();
    profile.encoding = object.value(QStringLiteral("encoding"))
        .toString(QStringLiteral("auto"));
    profile.delimiter = object.value(QStringLiteral("delimiter"))
        .toString(QStringLiteral("auto"));
    profile.dateFormat = object.value(QStringLiteral("dateFormat"))
        .toString(QStringLiteral("auto"));
    profile.amountMode = object.value(QStringLiteral("amountMode"))
        .toString(QStringLiteral("signed"));
    profile.headerRow = object.value(QStringLiteral("headerRow")).toInt();
    profile.dateColumn = object.value(QStringLiteral("dateColumn")).toInt(-1);
    profile.amountColumn = object.value(QStringLiteral("amountColumn")).toInt(-1);
    profile.incomeColumn = object.value(QStringLiteral("incomeColumn")).toInt(-1);
    profile.expenseColumn = object.value(QStringLiteral("expenseColumn")).toInt(-1);
    profile.descriptionColumn = object.value(
        QStringLiteral("descriptionColumn")).toInt(-1);
    profile.idColumn = object.value(QStringLiteral("idColumn")).toInt(-1);
    profile.categoryColumn = object.value(
        QStringLiteral("categoryColumn")).toInt(-1);
    profile.positiveMeansIncome = object.value(
        QStringLiteral("positiveMeansIncome")).toBool(true);
    return profile;
}

CsvCodec::ReadOptions BankCsvImporter::readOptions(
    const BankCsvProfile& profile
    )
{
    CsvCodec::ReadOptions options;
    if (profile.delimiter == QStringLiteral("semicolon")) {
        options.delimiter = QLatin1Char(';');
    } else if (profile.delimiter == QStringLiteral("comma")) {
        options.delimiter = QLatin1Char(',');
    } else if (profile.delimiter == QStringLiteral("tab")) {
        options.delimiter = QLatin1Char('\t');
    }

    if (profile.encoding == QStringLiteral("utf8")) {
        options.encoding = CsvCodec::Encoding::Utf8;
    } else if (profile.encoding == QStringLiteral("windows1251")) {
        options.encoding = CsvCodec::Encoding::Windows1251;
    } else if (profile.encoding == QStringLiteral("utf16le")) {
        options.encoding = CsvCodec::Encoding::Utf16Le;
    } else if (profile.encoding == QStringLiteral("utf16be")) {
        options.encoding = CsvCodec::Encoding::Utf16Be;
    }
    return options;
}

bool BankCsvImporter::parseAmountMinor(
    const QString& text,
    qint64& minorUnits
    )
{
    QString normalized = text.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    bool negativeByParentheses = normalized.startsWith(QLatin1Char('(')) &&
        normalized.endsWith(QLatin1Char(')'));
    const bool negativeByTrailingSign = normalized.endsWith(QLatin1Char('-'));
    normalized.replace(QChar(0x00A0), QString());
    normalized.replace(QChar(0x202F), QString());
    normalized.remove(QLatin1Char(' '));
    normalized.remove(QLatin1Char('\''));
    normalized.replace(QChar(0x2212), QLatin1Char('-'));

    QString numeric;
    numeric.reserve(normalized.size());
    for (const QChar character : normalized) {
        if (character.isDigit() || character == QLatin1Char('-') ||
            character == QLatin1Char('+') || character == QLatin1Char(',') ||
            character == QLatin1Char('.')) {
            numeric.append(character);
        }
    }
    if (numeric.isEmpty()) {
        return false;
    }
    if (negativeByTrailingSign) {
        numeric.chop(1);
        numeric.prepend(QLatin1Char('-'));
    }

    const qsizetype comma = numeric.lastIndexOf(QLatin1Char(','));
    const qsizetype dot = numeric.lastIndexOf(QLatin1Char('.'));
    const qsizetype decimal = qMax(comma, dot);
    if (comma >= 0 && dot >= 0) {
        const QChar thousands = comma < dot ? QLatin1Char(',') : QLatin1Char('.');
        numeric.remove(thousands);
    } else if (decimal >= 0 && numeric.size() - decimal - 1 == 3) {
        numeric.remove(numeric[decimal]);
    }
    if (numeric.contains(QLatin1Char(','))) {
        numeric.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    bool ok = false;
    double amount = QLocale::c().toDouble(numeric, &ok);
    if (!ok || !std::isfinite(amount)) {
        return false;
    }
    if (negativeByParentheses) {
        amount = -std::abs(amount);
    }
    const long double scaled = static_cast<long double>(amount) * 100.0L;
    if (scaled > std::numeric_limits<qint64>::max() ||
        scaled < std::numeric_limits<qint64>::min()) {
        return false;
    }
    minorUnits = static_cast<qint64>(std::llround(scaled));
    return true;
}

QDateTime BankCsvImporter::parseDateTime(
    const QString& text,
    const QString& format
    )
{
    const QString value = text.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    if (format != QStringLiteral("auto")) {
        QDateTime dateTime = QDateTime::fromString(value, format);
        if (dateTime.isValid()) {
            dateTime.setTimeSpec(Qt::LocalTime);
            return dateTime;
        }
        return dateAtNoon(QDate::fromString(value, format));
    }

    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    if (dateTime.isValid()) {
        return dateTime;
    }
    for (const QString& candidate : {
             QStringLiteral("dd.MM.yyyy HH:mm:ss"),
             QStringLiteral("dd.MM.yyyy HH:mm"),
             QStringLiteral("yyyy-MM-dd HH:mm:ss"),
             QStringLiteral("yyyy-MM-dd HH:mm")}) {
        dateTime = QDateTime::fromString(value, candidate);
        if (dateTime.isValid()) {
            dateTime.setTimeSpec(Qt::LocalTime);
            return dateTime;
        }
    }
    for (const QString& candidate : {
             QStringLiteral("dd.MM.yyyy"),
             QStringLiteral("d.M.yyyy"),
             QStringLiteral("dd.MM.yy"),
             QStringLiteral("yyyy-MM-dd"),
             QStringLiteral("dd/MM/yyyy"),
             QStringLiteral("dd/MM/yy"),
             QStringLiteral("MM/dd/yyyy")}) {
        const QDate date = QDate::fromString(value, candidate);
        if (date.isValid()) {
            return dateAtNoon(date);
        }
    }
    return {};
}

BankCsvParseResult BankCsvImporter::parse(
    const QString& filePath,
    const BankCsvProfile& profile
    )
{
    BankCsvParseResult result;
    const CsvCodec::ReadResult csv = CsvCodec::readFile(
        filePath, readOptions(profile));
    result.delimiter = csv.delimiter;
    result.encoding = csv.encoding;
    if (!csv.error.isEmpty()) {
        result.errors.append(csv.error);
        return result;
    }
    if (profile.headerRow < 0 || profile.headerRow >= csv.rows.size()) {
        result.errors.append(QStringLiteral("Header row is outside the CSV file"));
        return result;
    }

    for (qsizetype rowIndex = profile.headerRow + 1;
         rowIndex < csv.rows.size(); ++rowIndex) {
        const QStringList& row = csv.rows[rowIndex];
        if (rowIsEmpty(row)) {
            continue;
        }

        const QString dateText = fieldAt(row, profile.dateColumn);
        const QString signedAmountText = fieldAt(row, profile.amountColumn);
        const QString incomeText = fieldAt(row, profile.incomeColumn);
        const QString expenseText = fieldAt(row, profile.expenseColumn);
        if (dateText.isEmpty() && signedAmountText.isEmpty() &&
            incomeText.isEmpty() && expenseText.isEmpty()) {
            continue;
        }

        const QDateTime occurredAt = parseDateTime(
            dateText, profile.dateFormat);
        qint64 signedMinor = 0;
        bool amountOk = false;
        if (profile.amountMode == QStringLiteral("separate")) {
            qint64 incomeMinor = 0;
            qint64 expenseMinor = 0;
            const bool hasIncome = !incomeText.isEmpty() &&
                parseAmountMinor(incomeText, incomeMinor) && incomeMinor != 0;
            const bool hasExpense = !expenseText.isEmpty() &&
                parseAmountMinor(expenseText, expenseMinor) && expenseMinor != 0;
            if (hasIncome != hasExpense) {
                signedMinor = hasIncome
                    ? positiveMagnitude(incomeMinor)
                    : -positiveMagnitude(expenseMinor);
                amountOk = true;
            }
        } else {
            amountOk = parseAmountMinor(signedAmountText, signedMinor) &&
                signedMinor != 0;
            if (amountOk && !profile.positiveMeansIncome) {
                signedMinor = -signedMinor;
            }
        }

        if (!occurredAt.isValid() || !amountOk) {
            ++result.rejected;
            if (result.errors.size() < 5) {
                result.errors.append(QStringLiteral(
                    "Row %1 has an invalid date or amount").arg(rowIndex + 1));
            }
            continue;
        }

        BankCsvOperation operation;
        operation.occurredAt = occurredAt;
        operation.signedMinor = signedMinor;
        operation.description = fieldAt(row, profile.descriptionColumn);
        operation.externalId = fieldAt(row, profile.idColumn);
        operation.categoryName = fieldAt(row, profile.categoryColumn);
        operation.fingerprint = normalizedFingerprint(row);
        operation.sourceRow = static_cast<int>(rowIndex + 1);
        result.operations.append(operation);
    }
    return result;
}
