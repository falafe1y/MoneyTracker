#pragma once

#include "../core/Currency.h"

#include <QByteArray>
#include <QHash>
#include <QDate>
#include <QDateTime>
#include <QString>

#include <array>

struct CurrencyRateSnapshot
{
    std::array<qint64, kCurrencyCount> ratesToUsd{};
    QHash<QString, qint64> extraRatesToRubMicros;
    QDate sourceDate;
    QDateTime fetchedAtUtc;
};

class CurrencyRateCache final
{
public:
    explicit CurrencyRateCache(QString filePath);

    bool load(
        CurrencyRateSnapshot& snapshot,
        QString* error = nullptr
        ) const;

    bool replace(
        const CurrencyRateSnapshot& snapshot,
        QString* error = nullptr
        ) const;

private:
    QString filePath_;
};

bool parseCbrCurrencyRates(
    const QByteArray& response,
    const QDateTime& fetchedAtUtc,
    CurrencyRateSnapshot& snapshot,
    QString* error = nullptr
    );

bool updateCurrencyRateCacheFromCbrResponse(
    const CurrencyRateCache& cache,
    const QByteArray& response,
    const QDateTime& fetchedAtUtc,
    CurrencyRateSnapshot& snapshot,
    QString* error = nullptr
    );
