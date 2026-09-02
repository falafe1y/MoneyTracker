#pragma once

#include "Asset.h"
#include "Currency.h"

#include <QString>
#include <QtGlobal>
#include <utility>

enum class AccountType {
    Cash,
    DebitCard,
    CreditCard,
    Savings,
    Other,
    CryptoWallet,
    Brokerage,
    Deposit
};

class Account
{
public:
    Account(
        QString id,
        QString name,
        AssetType assetType,
        AccountType type,
        Currency currency,
        qint64 initialBalanceMinor = 0
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , assetType_(assetType)
        , type_(type)
        , currency_(currency)
        , initialBalanceMinor_(initialBalanceMinor)
    {
    }

    const QString& id() const noexcept
    {
        return id_;
    }

    const QString& name() const noexcept
    {
        return name_;
    }

    AccountType type() const noexcept
    {
        return type_;
    }

    AssetType assetType() const noexcept
    {
        return assetType_;
    }

    Currency currency() const noexcept
    {
        return currency_;
    }

    qint64 initialBalanceMinor() const noexcept
    {
        return initialBalanceMinor_;
    }

private:
    QString id_;
    QString name_;
    AssetType assetType_;
    AccountType type_;
    Currency currency_;
    qint64 initialBalanceMinor_;
};
