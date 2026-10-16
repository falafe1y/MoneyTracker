#pragma once

#include "CurrencyRateCache.h"
#include "../core/AnalyticsHistory.h"
#include "CurrencyRateProvider.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

#include <array>

class QNetworkReply;

class CbrCurrencyRateProvider final
    : public QObject
    , public CurrencyRateProvider
{
    Q_OBJECT

public:
    explicit CbrCurrencyRateProvider(QObject* parent = nullptr);

    qint64 rateToUsd(Currency currency) const override;
    qint64 rateToRubMicros(const QString& code) const;
    bool automaticUpdatesEnabled() const;
    QVector<HistoricalFxRate> historicalRates() const;
    void requestUsdHistory(QDate from, QDate to);
    QVector<HistoricalFxRate> takeHistory();
    void setAutomaticUpdatesEnabled(bool enabled);
    bool setManualRates(
        double rublesPerRub,
        double rublesPerUsd,
        double rublesPerEur,
        double rublesPerCny = kDefaultCnyToRubRate
        );

signals:
    void ratesUpdated();
    void historyReady();

private:
    void scheduleInitialRefresh();
    void requestRefresh();
    void finishRefresh(QNetworkReply* reply);
    void finishUsdHistory(QNetworkReply* reply);
    void scheduleNextRefresh(qint64 delayMs);

    static QString defaultCacheFilePath();
    static bool buildRatesToUsd(
        double rublesPerRub,
        double rublesPerUsd,
        double rublesPerEur,
        double rublesPerCny,
        std::array<qint64, kCurrencyCount>& ratesToUsd
        );

    static constexpr qint64 kRefreshIntervalMs =
        12LL * 60LL * 60LL * 1000LL;

    CurrencyRateCache cache_;
    QHash<QString,qint64> extraRatesToRubMicros_;
    std::array<qint64, kCurrencyCount> automaticRatesToUsd_{
        11'000'000,
        kCurrencyRateScale,
        1'170'000'000,
        137'500'000
    };
    std::array<qint64, kCurrencyCount> manualRatesToUsd_{
        11'000'000,
        kCurrencyRateScale,
        1'170'000'000,
        137'500'000
    };
    bool automaticUpdatesEnabled_ = true;
    QDateTime lastSuccessfulFetchUtc_;
    QDate automaticRateDate_;
    QDateTime automaticRatesObservedAt_;
    bool automaticCnyObserved_ = false;
    QNetworkAccessManager networkAccessManager_;
    QTimer refreshTimer_;
    QNetworkReply* activeReply_ = nullptr;
    QNetworkReply* historyReply_ = nullptr;
    QVector<HistoricalFxRate> history_;
};
