#pragma once

#include "../core/CryptoTransaction.h"
#include "../core/CryptoWallet.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QVector>

class QNetworkReply;
class QNetworkRequest;
class QUrl;

class CryptoProvider final : public QObject
{
    Q_OBJECT

public:
    explicit CryptoProvider(QObject* parent = nullptr);

    bool requestBalance(const CryptoWallet& wallet);
    bool requestTransactions(const CryptoWallet& wallet);
    bool requestPrices();

signals:
    void balanceUpdated(
        const QString& walletId,
        qint64 balanceAtomic,
        const QDateTime& fetchedAtUtc
        );
    void priceUpdated(
        const QString& symbol,
        qint64 priceUsdMicros,
        const QDateTime& fetchedAtUtc
        );
    void transactionsUpdated(
        const QString& walletId,
        const QVector<CryptoTransaction>& transactions,
        const QDateTime& fetchedAtUtc
        );
    void requestFailed(const QString& walletId, const QString& message);
    void requestFinished();

private:
    QNetworkRequest makeRequest(const QUrl& url) const;
    QUrl ethereumApiUrl(
        const QString& action,
        const CryptoWallet& wallet,
        bool includeHistoryParameters
        ) const;
    void finishBalanceRequest(
        QNetworkReply* reply,
        const CryptoWallet& wallet
        );
    void finishPriceRequest(QNetworkReply* reply);
    void finishTransactionRequest(
        QNetworkReply* reply,
        const CryptoWallet& wallet
        );

    QNetworkAccessManager networkAccessManager_;
    QSet<QString> activeBalanceRequests_;
    QSet<QString> activeTransactionRequests_;
    bool priceRequestActive_ = false;
};
