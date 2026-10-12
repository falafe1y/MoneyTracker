#pragma once
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVector>
#include <optional>

// Amounts are in the native currency. Missing values are SQL NULL, not zero.
struct AssetDailyValuation {
    enum Kind { Fiat, Crypto, Investment };
    Kind kind = Fiat;
    QString sourceId, accountId, instrumentId, currency;
    std::optional<qint64> valueMinor, quantityAtomic, priceMicros, costBasisMinor;
    int quantityDecimals = 6;
    QDateTime valuedAtUtc;
    QString source, issues, detailsJson = QStringLiteral("{}");
};
struct HistoricalFxRate {
    QString currency, source;
    QDate rateDate;
    QDateTime observedAtUtc;
    qint64 rubPerUnitMicros = 0;
};
struct DailyValuationBatch {
    QDate date;
    QDateTime observedAtUtc;
    QVector<AssetDailyValuation> assets;
    QVector<HistoricalFxRate> rates;
    std::optional<qint64> totalMinor;
    QString totalCurrency;
};
