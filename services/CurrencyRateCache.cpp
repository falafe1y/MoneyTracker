#include "CurrencyRateCache.h"

#include "CurrencyRateProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QXmlStreamReader>

#include <cmath>
#include <limits>
#include <utility>

namespace
{

constexpr int kCacheFormatVersion = 1;

constexpr qsizetype currencyIndex(const Currency currency)
{
    return static_cast<qsizetype>(currency);
}

void setError(QString* error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

bool readPositiveInteger(
    const QJsonObject& object,
    const QString& key,
    qint64& value
    )
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        return false;
    }

    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || number <= 0.0 ||
        number > static_cast<double>(std::numeric_limits<qint64>::max()) ||
        std::floor(number) != number) {
        return false;
    }

    value = static_cast<qint64>(number);
    return true;
}

bool parsePositiveNumber(const QString& text, double& value)
{
    QString normalized = text.trimmed();
    normalized.replace(',', '.');

    bool ok = false;
    const double parsed = normalized.toDouble(&ok);
    if (!ok || !std::isfinite(parsed) || parsed <= 0.0) {
        return false;
    }

    value = parsed;
    return true;
}

QDate parseCbrDate(const QString& text)
{
    QDate date = QDate::fromString(text, QStringLiteral("dd.MM.yyyy"));
    if (!date.isValid()) {
        date = QDate::fromString(text, QStringLiteral("dd/MM/yyyy"));
    }
    return date;
}

struct CbrQuote
{
    QString code;
    int nominal = 0;
    double valueInRubles = 0.0;
    bool nominalValid = false;
    bool valueValid = false;
};

CbrQuote readQuote(QXmlStreamReader& xml)
{
    CbrQuote quote;

    while (xml.readNextStartElement()) {
        const auto name = xml.name();

        if (name == QStringLiteral("CharCode")) {
            quote.code = xml.readElementText().trimmed().toUpper();
        } else if (name == QStringLiteral("Nominal")) {
            bool ok = false;
            const int nominal = xml.readElementText().trimmed().toInt(&ok);
            quote.nominalValid = ok && nominal > 0;
            quote.nominal = nominal;
        } else if (name == QStringLiteral("Value")) {
            quote.valueValid = parsePositiveNumber(
                xml.readElementText(), quote.valueInRubles);
        } else {
            xml.skipCurrentElement();
        }
    }

    return quote;
}

bool validSnapshot(const CurrencyRateSnapshot& snapshot)
{
    return snapshot.sourceDate.isValid() &&
           snapshot.fetchedAtUtc.isValid() &&
           snapshot.ratesToUsd[currencyIndex(Currency::RUB)] > 0 &&
           snapshot.ratesToUsd[currencyIndex(Currency::USD)] ==
               kCurrencyRateScale &&
           snapshot.ratesToUsd[currencyIndex(Currency::EUR)] > 0;
}

qint64 scaledRate(const double rate)
{
    if (!std::isfinite(rate) || rate <= 0.0 ||
        rate > static_cast<double>(std::numeric_limits<qint64>::max())) {
        return 0;
    }

    return static_cast<qint64>(std::llround(rate));
}

} // namespace

CurrencyRateCache::CurrencyRateCache(QString filePath)
    : filePath_(std::move(filePath))
{
}

bool CurrencyRateCache::load(
    CurrencyRateSnapshot& snapshot,
    QString* error
    ) const
{
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        setError(error, QStringLiteral("Invalid currency-rate cache JSON"));
        return false;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("formatVersion")).toInt(-1) !=
            kCacheFormatVersion ||
        object.value(QStringLiteral("source")).toString() !=
            QStringLiteral("cbr.ru") ||
        !object.value(QStringLiteral("ratesToUsd")).isObject()) {
        setError(error, QStringLiteral("Unsupported currency-rate cache format"));
        return false;
    }

    CurrencyRateSnapshot candidate;
    candidate.sourceDate = QDate::fromString(
        object.value(QStringLiteral("sourceDate")).toString(), Qt::ISODate);
    candidate.fetchedAtUtc = QDateTime::fromString(
        object.value(QStringLiteral("fetchedAtUtc")).toString(),
        Qt::ISODateWithMs).toUTC();

    const QJsonObject rates =
        object.value(QStringLiteral("ratesToUsd")).toObject();
    if (!readPositiveInteger(
            rates,
            QStringLiteral("RUB"),
            candidate.ratesToUsd[currencyIndex(Currency::RUB)]) ||
        !readPositiveInteger(
            rates,
            QStringLiteral("USD"),
            candidate.ratesToUsd[currencyIndex(Currency::USD)]) ||
        !readPositiveInteger(
            rates,
            QStringLiteral("EUR"),
            candidate.ratesToUsd[currencyIndex(Currency::EUR)]) ||
        !validSnapshot(candidate)) {
        setError(error, QStringLiteral("Invalid currency-rate cache values"));
        return false;
    }

    snapshot = candidate;
    return true;
}

bool CurrencyRateCache::replace(
    const CurrencyRateSnapshot& snapshot,
    QString* error
    ) const
{
    if (!validSnapshot(snapshot)) {
        setError(error, QStringLiteral("Refusing to cache invalid currency rates"));
        return false;
    }

    const QFileInfo fileInfo(filePath_);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setError(error, QStringLiteral("Cannot create currency-rate cache directory"));
        return false;
    }

    QJsonObject rates;
    rates.insert(
        QStringLiteral("RUB"),
        snapshot.ratesToUsd[currencyIndex(Currency::RUB)]);
    rates.insert(
        QStringLiteral("USD"),
        snapshot.ratesToUsd[currencyIndex(Currency::USD)]);
    rates.insert(
        QStringLiteral("EUR"),
        snapshot.ratesToUsd[currencyIndex(Currency::EUR)]);

    QJsonObject object;
    object.insert(QStringLiteral("formatVersion"), kCacheFormatVersion);
    object.insert(QStringLiteral("source"), QStringLiteral("cbr.ru"));
    object.insert(
        QStringLiteral("sourceDate"),
        snapshot.sourceDate.toString(Qt::ISODate));
    object.insert(
        QStringLiteral("fetchedAtUtc"),
        snapshot.fetchedAtUtc.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("ratesToUsd"), rates);

    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);

    QSaveFile file(filePath_);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }

    if (file.write(data) != data.size()) {
        setError(error, file.errorString());
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }

    return true;
}

bool parseCbrCurrencyRates(
    const QByteArray& response,
    const QDateTime& fetchedAtUtc,
    CurrencyRateSnapshot& snapshot,
    QString* error
    )
{
    if (response.isEmpty() || !fetchedAtUtc.isValid()) {
        setError(error, QStringLiteral("Empty response or invalid fetch time"));
        return false;
    }

    // The CBR endpoint declares Windows-1251. All fields used here are ASCII,
    // so decoding the byte stream as Latin-1 keeps their bytes unchanged and
    // avoids requiring Qt5Compat solely for a legacy text codec.
    const QString xmlText = QString::fromLatin1(response);
    QXmlStreamReader xml(xmlText);
    if (!xml.readNextStartElement() ||
        xml.name() != QStringLiteral("ValCurs")) {
        setError(error, QStringLiteral("Unexpected CBR response root element"));
        return false;
    }

    const QDate sourceDate = parseCbrDate(
        xml.attributes().value(QStringLiteral("Date")).toString());
    if (!sourceDate.isValid()) {
        setError(error, QStringLiteral("CBR response has no valid rate date"));
        return false;
    }

    const QDate fetchDate = fetchedAtUtc.toUTC().date();
    if (sourceDate < fetchDate.addDays(-14) ||
        sourceDate > fetchDate.addDays(3)) {
        setError(error, QStringLiteral("CBR response contains a stale rate date"));
        return false;
    }

    double rublesPerUsd = 0.0;
    double rublesPerEur = 0.0;
    bool usdFound = false;
    bool eurFound = false;

    while (xml.readNextStartElement()) {
        if (xml.name() != QStringLiteral("Valute")) {
            xml.skipCurrentElement();
            continue;
        }

        const CbrQuote quote = readQuote(xml);
        if (quote.code != QStringLiteral("USD") &&
            quote.code != QStringLiteral("EUR")) {
            continue;
        }
        if (!quote.nominalValid || !quote.valueValid) {
            setError(error, QStringLiteral("CBR response contains an invalid quote"));
            return false;
        }

        const double rublesPerUnit =
            quote.valueInRubles / static_cast<double>(quote.nominal);
        if (!std::isfinite(rublesPerUnit) || rublesPerUnit <= 0.0) {
            setError(error, QStringLiteral("CBR response contains a non-positive rate"));
            return false;
        }

        if (quote.code == QStringLiteral("USD")) {
            if (usdFound) {
                setError(error, QStringLiteral("CBR response contains duplicate USD rates"));
                return false;
            }
            rublesPerUsd = rublesPerUnit;
            usdFound = true;
        } else {
            if (eurFound) {
                setError(error, QStringLiteral("CBR response contains duplicate EUR rates"));
                return false;
            }
            rublesPerEur = rublesPerUnit;
            eurFound = true;
        }
    }

    if (xml.hasError()) {
        setError(error, QStringLiteral("Malformed CBR XML response"));
        return false;
    }
    if (!usdFound || !eurFound) {
        setError(error, QStringLiteral("CBR response does not contain USD and EUR rates"));
        return false;
    }

    CurrencyRateSnapshot candidate;
    candidate.sourceDate = sourceDate;
    candidate.fetchedAtUtc = fetchedAtUtc.toUTC();
    candidate.ratesToUsd[currencyIndex(Currency::USD)] = kCurrencyRateScale;
    candidate.ratesToUsd[currencyIndex(Currency::RUB)] = scaledRate(
        static_cast<double>(kCurrencyRateScale) / rublesPerUsd);
    candidate.ratesToUsd[currencyIndex(Currency::EUR)] = scaledRate(
        static_cast<double>(kCurrencyRateScale) *
        rublesPerEur / rublesPerUsd);

    if (!validSnapshot(candidate)) {
        setError(error, QStringLiteral("Calculated currency rates are invalid"));
        return false;
    }

    snapshot = candidate;
    return true;
}

bool updateCurrencyRateCacheFromCbrResponse(
    const CurrencyRateCache& cache,
    const QByteArray& response,
    const QDateTime& fetchedAtUtc,
    CurrencyRateSnapshot& snapshot,
    QString* error
    )
{
    CurrencyRateSnapshot candidate;
    if (!parseCbrCurrencyRates(
            response, fetchedAtUtc, candidate, error)) {
        return false;
    }

    CurrencyRateSnapshot cached;
    if (cache.load(cached) && candidate.sourceDate < cached.sourceDate) {
        setError(error, QStringLiteral("Refusing to replace cache with older rates"));
        return false;
    }

    if (!cache.replace(candidate, error)) {
        return false;
    }

    snapshot = candidate;
    return true;
}
