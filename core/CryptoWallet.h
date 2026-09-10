#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

class CryptoWallet
{
public:
    CryptoWallet(
        QString id,
        QString address,
        qint64 balanceAtomic = 0,
        QDateTime balanceFetchedAtUtc = {},
        QDateTime historyFetchedAtUtc = {}
        )
        : id_(std::move(id))
        , address_(std::move(address))
        , balanceAtomic_(balanceAtomic)
        , balanceFetchedAtUtc_(std::move(balanceFetchedAtUtc))
        , historyFetchedAtUtc_(std::move(historyFetchedAtUtc))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& address() const noexcept { return address_; }
    qint64 balanceAtomic() const noexcept { return balanceAtomic_; }
    const QDateTime& balanceFetchedAtUtc() const noexcept
    {
        return balanceFetchedAtUtc_;
    }
    const QDateTime& historyFetchedAtUtc() const noexcept
    {
        return historyFetchedAtUtc_;
    }

    void setBalance(
        const qint64 balanceAtomic,
        const QDateTime& fetchedAtUtc
        )
    {
        balanceAtomic_ = balanceAtomic;
        balanceFetchedAtUtc_ = fetchedAtUtc.toUTC();
    }

    void setHistoryFetchedAt(const QDateTime& fetchedAtUtc)
    {
        historyFetchedAtUtc_ = fetchedAtUtc.toUTC();
    }

private:
    QString id_;
    QString address_;
    qint64 balanceAtomic_ = 0;
    QDateTime balanceFetchedAtUtc_;
    QDateTime historyFetchedAtUtc_;
};
