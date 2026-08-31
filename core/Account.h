#pragma once

#include <QString>
#include <utility>

enum class AccountType {
    Household,
    Crypto,
    Investment
};

class Account
{
public:
    Account(
        QString id,
        QString name,
        AccountType type
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , type_(type)
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

private:
    QString id_;
    QString name_;
    AccountType type_;
};