#include "TronUsdtProvider.h"

#include "TronUsdtParser.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace
{

constexpr qsizetype kMaximumResponseSize = 256 * 1024;
const QString kUsdtContract =
    QStringLiteral("TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t");

QNetworkRequest makeRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("MoneyTracker/0.1"));
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("application/json"));
    request.setTransferTimeout(15'000);
    return request;
}

bool successfulResponse(QNetworkReply* reply, const QByteArray& response)
{
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return reply->error() == QNetworkReply::NoError &&
           statusCode == 200 &&
           response.size() <= kMaximumResponseSize;
}

} // namespace

TronUsdtProvider::TronUsdtProvider(QObject* parent)
    : QObject(parent)
{
}

bool TronUsdtProvider::requestBalance(
    const QString& walletId,
    const QString& address
    )
{
    const QString normalizedAddress = address.trimmed();
    const QString parameter = tronUsdtBalanceParameter(normalizedAddress);
    if (walletId.isEmpty() || parameter.isEmpty() ||
        activeWalletRequests_.contains(walletId)) {
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("owner_address"), normalizedAddress);
    payload.insert(QStringLiteral("contract_address"), kUsdtContract);
    payload.insert(
        QStringLiteral("function_selector"),
        QStringLiteral("balanceOf(address)"));
    payload.insert(QStringLiteral("parameter"), parameter);
    payload.insert(QStringLiteral("visible"), true);

    QNetworkRequest request = makeRequest(QUrl(
        QStringLiteral("https://api.trongrid.io/wallet/triggerconstantcontract")));
    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json"));

    activeWalletRequests_.insert(walletId);
    QNetworkReply* reply = networkAccessManager_.post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, walletId]()
        {
            finishBalanceRequest(reply, walletId);
        });
    return true;
}

bool TronUsdtProvider::requestPrice()
{
    if (priceRequestActive_) {
        return false;
    }

    QUrl url(QStringLiteral("https://api.coingecko.com/api/v3/simple/price"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), QStringLiteral("tether"));
    query.addQueryItem(QStringLiteral("vs_currencies"), QStringLiteral("usd"));
    query.addQueryItem(
        QStringLiteral("include_last_updated_at"),
        QStringLiteral("true"));
    url.setQuery(query);

    priceRequestActive_ = true;
    QNetworkReply* reply = networkAccessManager_.get(makeRequest(url));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            finishPriceRequest(reply);
        });
    return true;
}

bool TronUsdtProvider::requestTransactions(
    const QString& walletId,
    const QString& address
    )
{
    const QString normalizedAddress = address.trimmed();
    if (walletId.isEmpty() || !isValidTronAddress(normalizedAddress) ||
        activeTransactionRequests_.contains(walletId)) {
        return false;
    }

    QUrl url(QStringLiteral(
        "https://api.trongrid.io/v1/accounts/%1/transactions/trc20")
        .arg(normalizedAddress));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("only_confirmed"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    query.addQueryItem(
        QStringLiteral("order_by"),
        QStringLiteral("block_timestamp,desc"));
    query.addQueryItem(QStringLiteral("contract_address"), kUsdtContract);
    url.setQuery(query);

    activeTransactionRequests_.insert(walletId);
    QNetworkReply* reply = networkAccessManager_.get(makeRequest(url));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, walletId]()
        {
            finishTransactionRequest(reply, walletId);
        });
    return true;
}

void TronUsdtProvider::finishBalanceRequest(
    QNetworkReply* reply,
    const QString& walletId
    )
{
    activeWalletRequests_.remove(walletId);
    const QByteArray response = reply->readAll();
    qint64 balanceAtomic = 0;
    QString error;
    if (successfulResponse(reply, response) &&
        parseTronUsdtBalanceResponse(response, balanceAtomic, &error)) {
        emit balanceUpdated(
            walletId,
            balanceAtomic,
            QDateTime::currentDateTimeUtc());
    } else {
        if (error.isEmpty()) {
            error = reply->errorString();
        }
        emit requestFailed(walletId, error);
    }

    reply->deleteLater();
    emit requestFinished();
}

void TronUsdtProvider::finishPriceRequest(QNetworkReply* reply)
{
    priceRequestActive_ = false;
    const QByteArray response = reply->readAll();
    qint64 priceUsdMicros = 0;
    QString error;
    if (successfulResponse(reply, response) &&
        parseCoinGeckoUsdtPriceResponse(response, priceUsdMicros, &error)) {
        emit priceUpdated(priceUsdMicros, QDateTime::currentDateTimeUtc());
    } else {
        if (error.isEmpty()) {
            error = reply->errorString();
        }
        emit requestFailed(QString(), error);
    }

    reply->deleteLater();
    emit requestFinished();
}

void TronUsdtProvider::finishTransactionRequest(
    QNetworkReply* reply,
    const QString& walletId
    )
{
    activeTransactionRequests_.remove(walletId);
    const QByteArray response = reply->readAll();
    QVector<CryptoTransaction> transactions;
    QString error;
    if (successfulResponse(reply, response) &&
        parseTronUsdtTransactionsResponse(
            response, walletId, transactions, &error)) {
        emit transactionsUpdated(
            walletId,
            transactions,
            QDateTime::currentDateTimeUtc());
    } else {
        if (error.isEmpty()) {
            error = reply->errorString();
        }
        emit requestFailed(walletId, error);
    }

    reply->deleteLater();
    emit requestFinished();
}
