#include "CryptoProvider.h"

#include "CryptoParser.h"
#include "TronUsdtParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace
{

constexpr qsizetype kMaximumResponseSize = 2 * 1024 * 1024;
const QString kUsdtContract =
    QStringLiteral("TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t");

bool successfulResponse(QNetworkReply* reply, const QByteArray& response)
{
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return reply->error() == QNetworkReply::NoError &&
           statusCode == 200 &&
           response.size() <= kMaximumResponseSize;
}

QString responseError(QNetworkReply* reply, const QByteArray& response)
{
    if (response.size() > kMaximumResponseSize) {
        return QStringLiteral("Crypto API response is too large");
    }
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 429) {
        return QStringLiteral("Crypto API rate limit exceeded");
    }
    return reply->errorString();
}

bool isSupportedWallet(const CryptoWallet& wallet)
{
    return (wallet.network() == QStringLiteral("TRON") &&
            wallet.symbol() == QStringLiteral("USDT") &&
            wallet.decimals() == 6) ||
           (wallet.network() == QStringLiteral("BITCOIN") &&
            wallet.symbol() == QStringLiteral("BTC") &&
            wallet.decimals() == 8) ||
           (wallet.network() == QStringLiteral("ETHEREUM") &&
            wallet.symbol() == QStringLiteral("ETH") &&
            wallet.decimals() == 8);
}

void addTronGridApiKey(QNetworkRequest& request)
{
    const QByteArray apiKey = qgetenv("LEDGERA_TRONGRID_API_KEY").trimmed();
    if (!apiKey.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("TRON-PRO-API-KEY"), apiKey);
    }
}

} // namespace

CryptoProvider::CryptoProvider(QObject* parent)
    : QObject(parent)
{
}

QNetworkRequest CryptoProvider::makeRequest(const QUrl& url) const
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

QUrl CryptoProvider::ethereumApiUrl(
    const QString& action,
    const CryptoWallet& wallet,
    const bool includeHistoryParameters
    ) const
{
    const QString apiKey = qEnvironmentVariable("LEDGERA_BLOCKSCOUT_API_KEY")
        .trimmed();
    QUrl url(apiKey.isEmpty()
        ? QStringLiteral("https://eth.blockscout.com/api")
        : QStringLiteral("https://api.blockscout.com/v2/api"));
    QUrlQuery query;
    if (!apiKey.isEmpty()) {
        query.addQueryItem(QStringLiteral("apikey"), apiKey);
        query.addQueryItem(QStringLiteral("chainid"), QStringLiteral("1"));
    }
    query.addQueryItem(QStringLiteral("module"), QStringLiteral("account"));
    query.addQueryItem(QStringLiteral("action"), action);
    query.addQueryItem(QStringLiteral("address"), wallet.address());
    if (includeHistoryParameters) {
        query.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("offset"), QStringLiteral("50"));
        query.addQueryItem(QStringLiteral("sort"), QStringLiteral("desc"));
    }
    url.setQuery(query);
    return url;
}

bool CryptoProvider::requestBalance(const CryptoWallet& wallet)
{
    if (wallet.id().isEmpty() || !isSupportedWallet(wallet) ||
        activeBalanceRequests_.contains(wallet.id())) {
        return false;
    }

    QNetworkReply* reply = nullptr;
    if (wallet.network() == QStringLiteral("TRON")) {
        const QString parameter = tronUsdtBalanceParameter(wallet.address());
        if (parameter.isEmpty()) {
            return false;
        }
        QJsonObject payload;
        payload.insert(QStringLiteral("owner_address"), wallet.address());
        payload.insert(QStringLiteral("contract_address"), kUsdtContract);
        payload.insert(
            QStringLiteral("function_selector"),
            QStringLiteral("balanceOf(address)"));
        payload.insert(QStringLiteral("parameter"), parameter);
        payload.insert(QStringLiteral("visible"), true);

        QNetworkRequest request = makeRequest(QUrl(QStringLiteral(
            "https://api.trongrid.io/wallet/triggerconstantcontract")));
        addTronGridApiKey(request);
        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            QStringLiteral("application/json"));
        reply = networkAccessManager_.post(
            request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    } else if (wallet.network() == QStringLiteral("BITCOIN")) {
        if (!isValidBitcoinAddress(wallet.address())) {
            return false;
        }
        reply = networkAccessManager_.get(makeRequest(QUrl(QStringLiteral(
            "https://mempool.space/api/address/%1").arg(wallet.address()))));
    } else {
        if (!isValidEthereumAddress(wallet.address())) {
            return false;
        }
        reply = networkAccessManager_.get(makeRequest(ethereumApiUrl(
            QStringLiteral("balance"), wallet, false)));
    }

    activeBalanceRequests_.insert(wallet.id());
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, wallet]()
        {
            finishBalanceRequest(reply, wallet);
        });
    return true;
}

bool CryptoProvider::requestPrices()
{
    if (priceRequestActive_) {
        return false;
    }
    const QByteArray proApiKey = qgetenv(
        "LEDGERA_COINGECKO_PRO_API_KEY").trimmed();
    const QByteArray demoApiKey = qgetenv(
        "LEDGERA_COINGECKO_DEMO_API_KEY").trimmed();
    QUrl url(proApiKey.isEmpty()
        ? QStringLiteral("https://api.coingecko.com/api/v3/simple/price")
        : QStringLiteral("https://pro-api.coingecko.com/api/v3/simple/price"));
    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("ids"),
        QStringLiteral("tether,bitcoin,ethereum"));
    query.addQueryItem(QStringLiteral("vs_currencies"), QStringLiteral("usd"));
    query.addQueryItem(
        QStringLiteral("include_last_updated_at"),
        QStringLiteral("true"));
    url.setQuery(query);

    QNetworkRequest request = makeRequest(url);
    if (!proApiKey.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("x-cg-pro-api-key"), proApiKey);
    } else if (!demoApiKey.isEmpty()) {
        request.setRawHeader(
            QByteArrayLiteral("x-cg-demo-api-key"), demoApiKey);
    }
    priceRequestActive_ = true;
    QNetworkReply* reply = networkAccessManager_.get(request);
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

bool CryptoProvider::requestTransactions(const CryptoWallet& wallet)
{
    if (wallet.id().isEmpty() || !isSupportedWallet(wallet) ||
        activeTransactionRequests_.contains(wallet.id())) {
        return false;
    }

    QUrl url;
    if (wallet.network() == QStringLiteral("TRON")) {
        if (!isValidTronAddress(wallet.address())) {
            return false;
        }
        url = QUrl(QStringLiteral(
            "https://api.trongrid.io/v1/accounts/%1/transactions/trc20")
            .arg(wallet.address()));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("only_confirmed"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
        query.addQueryItem(
            QStringLiteral("order_by"),
            QStringLiteral("block_timestamp,desc"));
        query.addQueryItem(QStringLiteral("contract_address"), kUsdtContract);
        url.setQuery(query);
    } else if (wallet.network() == QStringLiteral("BITCOIN")) {
        if (!isValidBitcoinAddress(wallet.address())) {
            return false;
        }
        url = QUrl(QStringLiteral("https://mempool.space/api/address/%1/txs")
            .arg(wallet.address()));
    } else {
        if (!isValidEthereumAddress(wallet.address())) {
            return false;
        }
        url = ethereumApiUrl(QStringLiteral("txlist"), wallet, true);
    }

    activeTransactionRequests_.insert(wallet.id());
    QNetworkRequest request = makeRequest(url);
    if (wallet.network() == QStringLiteral("TRON")) {
        addTronGridApiKey(request);
    }
    QNetworkReply* reply = networkAccessManager_.get(request);
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, wallet]()
        {
            finishTransactionRequest(reply, wallet);
        });
    return true;
}

void CryptoProvider::finishBalanceRequest(
    QNetworkReply* reply,
    const CryptoWallet& wallet
    )
{
    activeBalanceRequests_.remove(wallet.id());
    const QByteArray response = reply->readAll();
    qint64 balanceAtomic = 0;
    QString error;
    bool parsed = false;
    if (successfulResponse(reply, response)) {
        if (wallet.network() == QStringLiteral("TRON")) {
            parsed = parseTronUsdtBalanceResponse(
                response, balanceAtomic, &error);
        } else if (wallet.network() == QStringLiteral("BITCOIN")) {
            parsed = parseBitcoinBalanceResponse(
                response, balanceAtomic, &error);
        } else {
            parsed = parseEthereumBalanceResponse(
                response, balanceAtomic, &error);
        }
    }
    if (parsed) {
        emit balanceUpdated(
            wallet.id(), balanceAtomic, QDateTime::currentDateTimeUtc());
    } else {
        if (error.isEmpty()) {
            error = responseError(reply, response);
        }
        emit requestFailed(wallet.id(), error);
    }

    reply->deleteLater();
    emit requestFinished();
}

void CryptoProvider::finishPriceRequest(QNetworkReply* reply)
{
    priceRequestActive_ = false;
    const QByteArray response = reply->readAll();
    QHash<QString, qint64> prices;
    QString error;
    if (successfulResponse(reply, response) &&
        parseCoinGeckoPricesResponse(response, prices, &error)) {
        const QDateTime fetchedAtUtc = QDateTime::currentDateTimeUtc();
        for (auto iterator = prices.cbegin(); iterator != prices.cend(); ++iterator) {
            emit priceUpdated(iterator.key(), iterator.value(), fetchedAtUtc);
        }
    } else {
        if (error.isEmpty()) {
            error = responseError(reply, response);
        }
        emit requestFailed(QString(), error);
    }

    reply->deleteLater();
    emit requestFinished();
}

void CryptoProvider::finishTransactionRequest(
    QNetworkReply* reply,
    const CryptoWallet& wallet
    )
{
    activeTransactionRequests_.remove(wallet.id());
    const QByteArray response = reply->readAll();
    QVector<CryptoTransaction> transactions;
    QString error;
    bool parsed = false;
    if (successfulResponse(reply, response)) {
        if (wallet.network() == QStringLiteral("TRON")) {
            parsed = parseTronUsdtTransactionsResponse(
                response, wallet.id(), transactions, &error);
        } else if (wallet.network() == QStringLiteral("BITCOIN")) {
            parsed = parseBitcoinTransactionsResponse(
                response,
                wallet.id(),
                wallet.address(),
                transactions,
                &error);
        } else {
            parsed = parseEthereumTransactionsResponse(
                response,
                wallet.id(),
                wallet.address(),
                transactions,
                &error);
        }
    }
    if (parsed) {
        emit transactionsUpdated(
            wallet.id(), transactions, QDateTime::currentDateTimeUtc());
    } else {
        if (error.isEmpty()) {
            error = responseError(reply, response);
        }
        emit requestFailed(wallet.id(), error);
    }

    reply->deleteLater();
    emit requestFinished();
}
