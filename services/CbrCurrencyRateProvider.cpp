#include "CbrCurrencyRateProvider.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <cmath>
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
        automaticRatesToUsd_ = cached.ratesToUsd;
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
    const auto& rates = automaticUpdatesEnabled_
        ? automaticRatesToUsd_
        : manualRatesToUsd_;
    return rates[currencyIndex(currency)];
}

bool CbrCurrencyRateProvider::automaticUpdatesEnabled() const
{
    return automaticUpdatesEnabled_;
}

void CbrCurrencyRateProvider::setAutomaticUpdatesEnabled(const bool enabled)
{
    if (enabled == automaticUpdatesEnabled_) {
        return;
    }

    automaticUpdatesEnabled_ = enabled;
    refreshTimer_.stop();

    if (activeReply_ != nullptr) {
        QNetworkReply* reply = activeReply_;
        activeReply_ = nullptr;
        reply->abort();
        reply->deleteLater();
    }

    emit ratesUpdated();
    if (automaticUpdatesEnabled_) {
        scheduleInitialRefresh();
    }
}

bool CbrCurrencyRateProvider::setManualRates(
    const double rublesPerUsd,
    const double rublesPerEur
    )
{
    std::array<qint64, 3> candidate;
    if (!buildRatesToUsd(rublesPerUsd, rublesPerEur, candidate)) {
        return false;
    }

    if (candidate == manualRatesToUsd_) {
        return true;
    }
    manualRatesToUsd_ = candidate;
    if (!automaticUpdatesEnabled_) {
        emit ratesUpdated();
    }
    return true;
}

void CbrCurrencyRateProvider::scheduleInitialRefresh()
{
    if (!automaticUpdatesEnabled_) {
        return;
    }
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
    if (!automaticUpdatesEnabled_ || activeReply_ != nullptr) {
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

    if (automaticUpdatesEnabled_ && networkSucceeded) {
        CurrencyRateSnapshot updated;
        QString updateError;
        if (updateCurrencyRateCacheFromCbrResponse(
                cache_,
                response,
                QDateTime::currentDateTimeUtc(),
                updated,
                &updateError)) {
            automaticRatesToUsd_ = updated.ratesToUsd;
            lastSuccessfulFetchUtc_ = updated.fetchedAtUtc;
            emit ratesUpdated();
        } else {
            qWarning() << "Currency rates were not updated:" << updateError;
        }
    } else if (automaticUpdatesEnabled_) {
        qWarning() << "Currency-rate request failed:"
                   << reply->errorString()
                   << "HTTP status" << statusCode;
    }

    reply->deleteLater();
    if (automaticUpdatesEnabled_) {
        scheduleNextRefresh(kRefreshIntervalMs);
    }
}

void CbrCurrencyRateProvider::scheduleNextRefresh(const qint64 delayMs)
{
    if (!automaticUpdatesEnabled_) {
        return;
    }
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

bool CbrCurrencyRateProvider::buildRatesToUsd(
    const double rublesPerUsd,
    const double rublesPerEur,
    std::array<qint64, 3>& ratesToUsd
    )
{
    if (!std::isfinite(rublesPerUsd) || rublesPerUsd <= 0.0 ||
        !std::isfinite(rublesPerEur) || rublesPerEur <= 0.0) {
        return false;
    }

    const double rubRate =
        static_cast<double>(kCurrencyRateScale) / rublesPerUsd;
    const double eurRate =
        static_cast<double>(kCurrencyRateScale) *
        rublesPerEur / rublesPerUsd;
    if (!std::isfinite(rubRate) || rubRate < 1.0 ||
        rubRate > static_cast<double>(std::numeric_limits<qint64>::max()) ||
        !std::isfinite(eurRate) || eurRate < 1.0 ||
        eurRate > static_cast<double>(std::numeric_limits<qint64>::max())) {
        return false;
    }

    ratesToUsd = {
        static_cast<qint64>(std::llround(rubRate)),
        kCurrencyRateScale,
        static_cast<qint64>(std::llround(eurRate))
    };
    return true;
}
