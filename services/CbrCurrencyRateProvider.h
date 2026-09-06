#pragma once

#include "CurrencyRateCache.h"
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
    bool automaticUpdatesEnabled() const;
    void setAutomaticUpdatesEnabled(bool enabled);
    bool setManualRates(double rublesPerUsd, double rublesPerEur);

signals:
    void ratesUpdated();

private:
    void scheduleInitialRefresh();
    void requestRefresh();
    void finishRefresh(QNetworkReply* reply);
    void scheduleNextRefresh(qint64 delayMs);

    static QString defaultCacheFilePath();
    static bool buildRatesToUsd(
        double rublesPerUsd,
        double rublesPerEur,
        std::array<qint64, 3>& ratesToUsd
        );

    static constexpr qint64 kRefreshIntervalMs =
        12LL * 60LL * 60LL * 1000LL;

    CurrencyRateCache cache_;
    std::array<qint64, 3> automaticRatesToUsd_{
        11'000'000,
        kCurrencyRateScale,
        1'170'000'000
    };
    std::array<qint64, 3> manualRatesToUsd_{
        11'000'000,
        kCurrencyRateScale,
        1'170'000'000
    };
    bool automaticUpdatesEnabled_ = true;
    QDateTime lastSuccessfulFetchUtc_;
    QNetworkAccessManager networkAccessManager_;
    QTimer refreshTimer_;
    QNetworkReply* activeReply_ = nullptr;
};
