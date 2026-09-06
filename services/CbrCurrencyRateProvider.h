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

signals:
    void ratesUpdated();

private:
    void scheduleInitialRefresh();
    void requestRefresh();
    void finishRefresh(QNetworkReply* reply);
    void scheduleNextRefresh(qint64 delayMs);

    static QString defaultCacheFilePath();

    static constexpr qint64 kRefreshIntervalMs =
        12LL * 60LL * 60LL * 1000LL;

    CurrencyRateCache cache_;
    std::array<qint64, 3> ratesToUsd_{
        11'000,
        kCurrencyRateScale,
        1'170'000
    };
    QDateTime lastSuccessfulFetchUtc_;
    QNetworkAccessManager networkAccessManager_;
    QTimer refreshTimer_;
    QNetworkReply* activeReply_ = nullptr;
};
