#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

class CryptoTransaction
{
public:
    CryptoTransaction(
        QString walletId,
        QString transactionId,
        QString fromAddress,
        QString toAddress,
        qint64 amountAtomic,
        QDateTime occurredAtUtc
        )
        : walletId_(std::move(walletId))
        , transactionId_(std::move(transactionId))
        , fromAddress_(std::move(fromAddress))
        , toAddress_(std::move(toAddress))
        , amountAtomic_(amountAtomic)
        , occurredAtUtc_(std::move(occurredAtUtc))
    {
    }

    const QString& walletId() const noexcept { return walletId_; }
    const QString& transactionId() const noexcept { return transactionId_; }
    const QString& fromAddress() const noexcept { return fromAddress_; }
    const QString& toAddress() const noexcept { return toAddress_; }
    qint64 amountAtomic() const noexcept { return amountAtomic_; }
    const QDateTime& occurredAtUtc() const noexcept { return occurredAtUtc_; }

private:
    QString walletId_;
    QString transactionId_;
    QString fromAddress_;
    QString toAddress_;
    qint64 amountAtomic_ = 0;
    QDateTime occurredAtUtc_;
};
