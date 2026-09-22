#pragma once

#include "Currency.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <utility>

struct BudgetCategoryLimit
{
    QString categoryId;
    qint64 limitMinor = 0;
};

class Budget
{
public:
    Budget(
        QString id,
        QString name,
        Currency currency,
        qint64 defaultLimitMinor,
        bool allAccounts,
        bool allCategories,
        QStringList accountIds = {},
        QVector<BudgetCategoryLimit> categoryLimits = {},
        QDate startsOn = {}
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , currency_(currency)
        , defaultLimitMinor_(defaultLimitMinor)
        , allAccounts_(allAccounts)
        , allCategories_(allCategories)
        , accountIds_(std::move(accountIds))
        , categoryLimits_(std::move(categoryLimits))
        , startsOn_(std::move(startsOn))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& name() const noexcept { return name_; }
    Currency currency() const noexcept { return currency_; }
    qint64 defaultLimitMinor() const noexcept { return defaultLimitMinor_; }
    bool allAccounts() const noexcept { return allAccounts_; }
    bool allCategories() const noexcept { return allCategories_; }
    const QStringList& accountIds() const noexcept { return accountIds_; }
    const QVector<BudgetCategoryLimit>& categoryLimits() const noexcept
    {
        return categoryLimits_;
    }
    const QDate& startsOn() const noexcept { return startsOn_; }

private:
    QString id_;
    QString name_;
    Currency currency_;
    qint64 defaultLimitMinor_ = 0;
    bool allAccounts_ = true;
    bool allCategories_ = true;
    QStringList accountIds_;
    QVector<BudgetCategoryLimit> categoryLimits_;
    QDate startsOn_;
};
