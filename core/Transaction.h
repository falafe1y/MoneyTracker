#pragma once

#include "Money.h"
#include "TransactionRecipient.h"

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
        QString description = {},
        QString projectId = {},
        TransactionRecipient recipient = {}
        )
        : id_(std::move(id))
        , accountId_(std::move(accountId))
        , categoryId_(std::move(categoryId))
        , money_(money)
        , type_(type)
        , date_(std::move(date))
        , description_(std::move(description))
        , projectId_(std::move(projectId))
        , recipient_(std::move(recipient))
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

    const QString& projectId() const noexcept
    {
        return projectId_;
    }

    const TransactionRecipient& recipient() const noexcept { return recipient_; }

private:
    QString id_;
    QString accountId_;
    QString categoryId_;
    Money money_;
    TransactionType type_;
    QDateTime date_;
    QString description_;
    QString projectId_;
    TransactionRecipient recipient_;
};
