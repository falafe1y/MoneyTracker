#pragma once

#include "Money.h"

#include <QDateTime>
#include <QString>
#include <utility>

enum class TransactionType {
    Income,
    Expense
};

class Transaction
{
public:
    Transaction(
        QString id,
        QString accountId,
        QString categoryId,
        Money money,
        TransactionType type,
        QDateTime date,
        QString description = {}
        )
        : id_(std::move(id))
        , accountId_(std::move(accountId))
        , categoryId_(std::move(categoryId))
        , money_(money)
        , type_(type)
        , date_(std::move(date))
        , description_(std::move(description))
    {
    }

    const QString& id() const noexcept
    {
        return id_;
    }

    const QString& accountId() const noexcept
    {
        return accountId_;
    }

    const QString& categoryId() const noexcept
    {
        return categoryId_;
    }

    const Money& money() const noexcept
    {
        return money_;
    }

    TransactionType type() const noexcept
    {
        return type_;
    }

    const QDateTime& date() const noexcept
    {
        return date_;
    }

    const QString& description() const noexcept
    {
        return description_;
    }

private:
    QString id_;
    QString accountId_;
    QString categoryId_;
    Money money_;
    TransactionType type_;
    QDateTime date_;
    QString description_;
};