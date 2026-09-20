#pragma once

#include "Transaction.h"

#include <QDate>
#include <QString>
#include <utility>

enum class RecurrenceType
{
    Weekly,
    MonthlyDay,
    MonthlyWeekday,
    Daily
};

class RecurringTransaction
{
public:
    RecurringTransaction(
        QString id,
        QString name,
        QString accountId,
        QString categoryId,
        TransactionType transactionType,
        qint64 amountMinor,
        Currency currency,
        RecurrenceType recurrenceType,
        int weekday,
        int dayOfMonth,
        int weekOfMonth,
        QDate startsOn,
        QDate generatedThrough = {}
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , accountId_(std::move(accountId))
        , categoryId_(std::move(categoryId))
        , transactionType_(transactionType)
        , amountMinor_(amountMinor)
        , currency_(currency)
        , recurrenceType_(recurrenceType)
        , weekday_(weekday)
        , dayOfMonth_(dayOfMonth)
        , weekOfMonth_(weekOfMonth)
        , startsOn_(startsOn)
        , generatedThrough_(generatedThrough)
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& name() const noexcept { return name_; }
    const QString& accountId() const noexcept { return accountId_; }
    const QString& categoryId() const noexcept { return categoryId_; }
    TransactionType transactionType() const noexcept { return transactionType_; }
    qint64 amountMinor() const noexcept { return amountMinor_; }
    Currency currency() const noexcept { return currency_; }
    RecurrenceType recurrenceType() const noexcept { return recurrenceType_; }
    int weekday() const noexcept { return weekday_; }
    int dayOfMonth() const noexcept { return dayOfMonth_; }
    int weekOfMonth() const noexcept { return weekOfMonth_; }
    const QDate& startsOn() const noexcept { return startsOn_; }
    const QDate& generatedThrough() const noexcept { return generatedThrough_; }

private:
    QString id_;
    QString name_;
    QString accountId_;
    QString categoryId_;
    TransactionType transactionType_;
    qint64 amountMinor_ = 0;
    Currency currency_ = Currency::RUB;
    RecurrenceType recurrenceType_ = RecurrenceType::MonthlyDay;
    int weekday_ = 1;
    int dayOfMonth_ = 1;
    int weekOfMonth_ = 1;
    QDate startsOn_;
    QDate generatedThrough_;
};
