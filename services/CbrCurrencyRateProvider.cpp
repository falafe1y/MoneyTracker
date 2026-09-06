#include "CbrCurrencyRateProvider.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <limits>

namespace
{

constexpr qsizetype currencyIndex(const Currency currency)
{
    return static_cast<qsizetype>(currency);
}

constexpr qsizetype kMaximumResponseSize = 256 * 1024;

} // namespace

CbrCurrencyRateProvider::CbrCurrencyRateProvider(QObject* parent)
    : QObject(parent)
    , cache_(defaultCacheFilePath())
{
    CurrencyRateSnapshot cached;
    if (cache_.load(cached)) {
        ratesToUsd_ = cached.ratesToUsd;
        lastSuccessfulFetchUtc_ = cached.fetchedAtUtc;
    }

    refreshTimer_.setSingleShot(true);
    QObject::connect(
        &refreshTimer_,
        &QTimer::timeout,
        this,
        &CbrCurrencyRateProvider::requestRefresh);

    QTimer::singleShot(
        0,
        this,
        &CbrCurrencyRateProvider::scheduleInitialRefresh);
}

qint64 CbrCurrencyRateProvider::rateToUsd(const Currency currency) const
{
    return ratesToUsd_[currencyIndex(currency)];
}

void CbrCurrencyRateProvider::scheduleInitialRefresh()
{
    if (!lastSuccessfulFetchUtc_.isValid()) {
        requestRefresh();
        return;
    }

    const qint64 ageMs = lastSuccessfulFetchUtc_.msecsTo(
        QDateTime::currentDateTimeUtc());
    if (ageMs < 0) {
        scheduleNextRefresh(kRefreshIntervalMs);
    } else if (ageMs >= kRefreshIntervalMs) {
        requestRefresh();
    } else {
        scheduleNextRefresh(kRefreshIntervalMs - ageMs);
    }
}

void CbrCurrencyRateProvider::requestRefresh()
{
    if (activeReply_ != nullptr) {
        return;
    }

    QNetworkRequest request{
        QUrl(QStringLiteral("https://www.cbr.ru/scripts/XML_daily_eng.asp"))};
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("MoneyTracker/0.1"));
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("application/xml,text/xml"));
    request.setTransferTimeout(15'000);

    activeReply_ = networkAccessManager_.get(request);
    QObject::connect(
        activeReply_,
        &QNetworkReply::finished,
        this,
        [this, reply = activeReply_]()
        {
            finishRefresh(reply);
        });
}

void CbrCurrencyRateProvider::finishRefresh(QNetworkReply* reply)
{
    if (reply != activeReply_) {
        reply->deleteLater();
        return;
    }
    activeReply_ = nullptr;

    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray response = reply->readAll();
    const bool networkSucceeded =
        reply->error() == QNetworkReply::NoError &&
        statusCode == 200 &&
        response.size() <= kMaximumResponseSize;

    if (networkSucceeded) {
        CurrencyRateSnapshot updated;
        QString updateError;
        if (updateCurrencyRateCacheFromCbrResponse(
                cache_,
                response,
                QDateTime::currentDateTimeUtc(),
                updated,
                &updateError)) {
            ratesToUsd_ = updated.ratesToUsd;
            lastSuccessfulFetchUtc_ = updated.fetchedAtUtc;
            emit ratesUpdated();
        } else {
            qWarning() << "Currency rates were not updated:" << updateError;
        }
    } else {
        qWarning() << "Currency-rate request failed:"
                   << reply->errorString()
                   << "HTTP status" << statusCode;
    }

    reply->deleteLater();
    scheduleNextRefresh(kRefreshIntervalMs);
}

void CbrCurrencyRateProvider::scheduleNextRefresh(const qint64 delayMs)
{
    const qint64 boundedDelay = std::clamp<qint64>(
        delayMs,
        1,
        std::numeric_limits<int>::max());
    refreshTimer_.start(static_cast<int>(boundedDelay));
}

QString CbrCurrencyRateProvider::defaultCacheFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/currency-rates.json");
}
