#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>

class QNetworkReply;

class TronUsdtProvider final : public QObject
{
    Q_OBJECT

public:
    explicit TronUsdtProvider(QObject* parent = nullptr);

    bool requestBalance(const QString& walletId, const QString& address);
    bool requestPrice();

signals:
    void balanceUpdated(
        const QString& walletId,
        qint64 balanceAtomic,
        const QDateTime& fetchedAtUtc
        );
    void priceUpdated(qint64 priceUsdMicros, const QDateTime& fetchedAtUtc);
    void requestFailed(const QString& walletId, const QString& message);
    void requestFinished();

private:
    void finishBalanceRequest(
        QNetworkReply* reply,
        const QString& walletId
        );
    void finishPriceRequest(QNetworkReply* reply);

    QNetworkAccessManager networkAccessManager_;
    QSet<QString> activeWalletRequests_;
    bool priceRequestActive_ = false;
};
