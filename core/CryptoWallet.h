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
        QString network = QStringLiteral("TRON"),
        QString symbol = QStringLiteral("USDT"),
        int decimals = 6,
        qint64 balanceAtomic = 0,
        QDateTime balanceFetchedAtUtc = {},
        QDateTime historyFetchedAtUtc = {}
        )
        : id_(std::move(id))
        , address_(std::move(address))
        , network_(std::move(network))
        , symbol_(std::move(symbol))
        , decimals_(decimals)
        , balanceAtomic_(balanceAtomic)
        , balanceFetchedAtUtc_(std::move(balanceFetchedAtUtc))
        , historyFetchedAtUtc_(std::move(historyFetchedAtUtc))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& address() const noexcept { return address_; }
    const QString& network() const noexcept { return network_; }
    const QString& symbol() const noexcept { return symbol_; }
    int decimals() const noexcept { return decimals_; }
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
    QString network_;
    QString symbol_;
    int decimals_ = 6;
    qint64 balanceAtomic_ = 0;
    QDateTime balanceFetchedAtUtc_;
    QDateTime historyFetchedAtUtc_;
};
