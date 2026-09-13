#include "MoexInvestmentParser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <cmath>
#include <limits>

namespace
{
void setError(QString* error, const QString& value)
{
    if (error) {
        *error = value;
    }
}

QHash<QString, int> columnIndexes(const QJsonObject& table)
{
    QHash<QString, int> result;
    const QJsonArray columns = table.value(QStringLiteral("columns")).toArray();
    for (int index = 0; index < columns.size(); ++index) {
        result.insert(columns.at(index).toString().toLower(), index);
    }
    return result;
}

QJsonValue cell(
    const QJsonArray& row,
    const QHash<QString, int>& columns,
    const QString& name
    )
{
    const int index = columns.value(name.toLower(), -1);
    return index >= 0 && index < row.size() ? row.at(index) : QJsonValue();
}

bool instrumentType(
    const QString& rawType,
    const QString& rawGroup,
    InvestmentInstrumentType& result
    )
{
    const QString type = rawType.toLower();
    const QString group = rawGroup.toLower();
    if (type.contains(QStringLiteral("ppif")) ||
        type.contains(QStringLiteral("etf")) ||
        type.contains(QStringLiteral("mutual")) ||
        group.contains(QStringLiteral("ppif")) ||
        group.contains(QStringLiteral("etf"))) {
        result = InvestmentInstrumentType::Etf;
        return true;
    }
    if (type == QStringLiteral("common_share") ||
        type == QStringLiteral("preferred_share") ||
        type == QStringLiteral("depositary_receipt") ||
        group == QStringLiteral("stock_shares") ||
        group == QStringLiteral("stock_dr")) {
        result = InvestmentInstrumentType::Stock;
        return true;
    }
    return false;
}

double positiveNumber(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        return std::isfinite(number) && number > 0.0 ? number : 0.0;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok && std::isfinite(number) && number > 0.0 ? number : 0.0;
    }
    return 0.0;
}

QString normalizedCurrency(QString value)
{
    value = value.trimmed().toUpper();
    if (value == QStringLiteral("SUR")) {
        return QStringLiteral("RUB");
    }
    if (value == QStringLiteral("RUB") ||
        value == QStringLiteral("USD") ||
        value == QStringLiteral("EUR")) {
        return value;
    }
    return {};
}
}

bool parseMoexInvestmentSearch(
    const QByteArray& payload,
    QVector<InvestmentMarketInstrument>& instruments,
    QString* error
    )
{
    instruments.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Московская биржа вернула некорректный ответ"));
        return false;
    }

    const QJsonObject table = document.object().value(
        QStringLiteral("securities")).toObject();
    const QHash<QString, int> columns = columnIndexes(table);
    if (!columns.contains(QStringLiteral("secid")) ||
        !columns.contains(QStringLiteral("primary_boardid"))) {
        setError(error, QStringLiteral("В ответе Московской биржи нет обязательных полей"));
        return false;
    }

    QSet<QString> seenIds;
    const QJsonArray rows = table.value(QStringLiteral("data")).toArray();
    for (const QJsonValue& rowValue : rows) {
        const QJsonArray row = rowValue.toArray();
        InvestmentInstrumentType type;
        if (!instrumentType(
                cell(row, columns, QStringLiteral("type")).toString(),
                cell(row, columns, QStringLiteral("group")).toString(),
                type)) {
            continue;
        }

        const QString symbol = cell(
            row, columns, QStringLiteral("secid")).toString().trimmed().toUpper();
        const QString board = cell(
            row, columns, QStringLiteral("primary_boardid")).toString().trimmed();
        if (symbol.isEmpty() || board.isEmpty() || seenIds.contains(symbol)) {
            continue;
        }

        QString name = cell(
            row, columns, QStringLiteral("shortname")).toString().trimmed();
        if (name.isEmpty()) {
            name = cell(row, columns, QStringLiteral("name")).toString().trimmed();
        }
        if (name.isEmpty()) {
            name = symbol;
        }

        seenIds.insert(symbol);
        instruments.append(InvestmentMarketInstrument(
            QStringLiteral("moex:") + symbol,
            symbol,
            cell(row, columns, QStringLiteral("isin")).toString().trimmed().toUpper(),
            name,
            type,
            board));
        if (instruments.size() == 20) {
            break;
        }
    }
    return true;
}

bool parseMoexInvestmentQuote(
    const QByteArray& payload,
    const QString& primaryBoardId,
    qint64& priceMicros,
    QString& currencyCode,
    QString* error
    )
{
    priceMicros = 0;
    currencyCode.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Московская биржа вернула некорректную котировку"));
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject securities = root.value(QStringLiteral("securities")).toObject();
    const QJsonObject marketData = root.value(QStringLiteral("marketdata")).toObject();
    const QHash<QString, int> securityColumns = columnIndexes(securities);
    const QHash<QString, int> marketColumns = columnIndexes(marketData);

    QJsonArray selectedSecurity;
    for (const QJsonValue& rowValue : securities.value(QStringLiteral("data")).toArray()) {
        const QJsonArray row = rowValue.toArray();
        const QString board = cell(
            row, securityColumns, QStringLiteral("boardid")).toString();
        if (selectedSecurity.isEmpty() || board == primaryBoardId) {
            selectedSecurity = row;
        }
        if (board == primaryBoardId) {
            break;
        }
    }

    QJsonArray selectedMarketData;
    for (const QJsonValue& rowValue : marketData.value(QStringLiteral("data")).toArray()) {
        const QJsonArray row = rowValue.toArray();
        const QString board = cell(
            row, marketColumns, QStringLiteral("boardid")).toString();
        if (selectedMarketData.isEmpty() || board == primaryBoardId) {
            selectedMarketData = row;
        }
        if (board == primaryBoardId) {
            break;
        }
    }

    double price = 0.0;
    for (const QString& column : {
             QStringLiteral("last"),
             QStringLiteral("marketprice"),
             QStringLiteral("lcurrentprice"),
             QStringLiteral("legalcloseprice")}) {
        price = positiveNumber(cell(selectedMarketData, marketColumns, column));
        if (price > 0.0) {
            break;
        }
    }
    if (price <= 0.0) {
        price = positiveNumber(cell(
            selectedSecurity, securityColumns, QStringLiteral("prevprice")));
    }

    currencyCode = normalizedCurrency(cell(
        selectedSecurity, securityColumns, QStringLiteral("currencyid")).toString());
    if (price <= 0.0 || currencyCode.isEmpty()) {
        setError(error, currencyCode.isEmpty()
            ? QStringLiteral("Валюта инструмента пока не поддерживается")
            : QStringLiteral("Для инструмента пока нет доступной рыночной цены"));
        return false;
    }

    const long double scaled = static_cast<long double>(price) * 1'000'000.0L;
    if (scaled > static_cast<long double>(std::numeric_limits<qint64>::max())) {
        setError(error, QStringLiteral("Котировка выходит за допустимый диапазон"));
        return false;
    }
    priceMicros = static_cast<qint64>(std::llround(scaled));
    return priceMicros > 0;
}
