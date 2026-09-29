#pragma once

#include "Asset.h"
#include "Currency.h"

#include <QString>
#include <QtGlobal>
#include <utility>

enum class AccountType {
    Cash = 0,
    DebitCard = 1,
    CreditCard = 2,
    // Value 3 belonged to the removed savings type in existing databases.
    Other = 4,
    CryptoWallet = 5,
    Brokerage = 6,
    Deposit = 7
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
        qint64 initialBalanceMinor = 0,
        qint64 creditLimitMinor = 0
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , assetType_(assetType)
        , type_(type)
        , currency_(currency)
        , initialBalanceMinor_(initialBalanceMinor)
        , creditLimitMinor_(creditLimitMinor)
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

    qint64 creditLimitMinor() const noexcept
    {
        return creditLimitMinor_;
    }

    bool isCreditCard() const noexcept
    {
        return type_ == AccountType::CreditCard;
    }

    qint64 debtMinor(const qint64 currentBalanceMinor) const noexcept
    {
        return isCreditCard() && currentBalanceMinor < 0
            ? -currentBalanceMinor
            : 0;
    }

    qint64 availableCreditMinor(const qint64 currentBalanceMinor) const noexcept
    {
        if (!isCreditCard()) {
            return 0;
        }
        const qint64 available = creditLimitMinor_ + currentBalanceMinor;
        return available > 0 ? available : 0;
    }

private:
    QString id_;
    QString name_;
    AssetType assetType_;
    AccountType type_;
    Currency currency_;
    qint64 initialBalanceMinor_;
    qint64 creditLimitMinor_;
};
