#include "FinanceController.h"

#include <QUuid>

FinanceController::FinanceController(QObject* parent)
    : QObject(parent)
    , currencyConverter_(rateProvider_)
    , balanceCalculator_(currencyConverter_)
{
}

qint64 FinanceController::balanceMinorUnits() const
{
    return balanceCalculator_
        .calculate(transactions_, appCurrency_)
        .minorUnits();
}

QString FinanceController::balanceCurrency() const
{
    return currencyCode(appCurrency_);
}

qint64 FinanceController::incomeMinorUnits() const
{
    qint64 total = 0;

    for (const Transaction& transaction : transactions_) {
        if (transaction.type() != TransactionType::Income) {
            continue;
        }

        const Money converted = currencyConverter_.convert(
            transaction.money(),
            appCurrency_
            );

        total += converted.minorUnits();
    }

    return total;
}

qint64 FinanceController::expenseMinorUnits() const
{
    qint64 total = 0;

    for (const Transaction& transaction : transactions_) {
        if (transaction.type() != TransactionType::Expense) {
            continue;
        }

        const Money converted = currencyConverter_.convert(
            transaction.money(),
            appCurrency_
            );

        total += converted.minorUnits();
    }

    return total;
}

QString FinanceController::appCurrency() const
{
    return currencyCode(appCurrency_);
}

void FinanceController::setAppCurrency(const QString& currency)
{
    const Currency newCurrency = currencyFromString(currency);

    if (newCurrency == appCurrency_) {
        return;
    }

    appCurrency_ = newCurrency;

    emit appCurrencyChanged();
    emit balanceChanged();
}

QVariantList FinanceController::transactions() const
{
    QVariantList result;

    for (const Transaction& transaction : transactions_) {
        QVariantMap item;

        item["id"] = transaction.id();
        item["accountId"] = transaction.accountId();
        item["categoryId"] = transaction.categoryId();

        item["amount"] = transaction.money().minorUnits();

        item["currency"] = currencyCode(
            transaction.money().currency()
            );

        item["type"] =
            transaction.type() == TransactionType::Income
                ? "income"
                : "expense";

        item["date"] = transaction.date().toString(
            Qt::ISODate
            );

        item["description"] = transaction.description();

        result.append(item);
    }

    return result;
}

qint64 FinanceController::convertTransaction(
    int transactionIndex,
    const QString& targetCurrency
    ) const
{
    if (transactionIndex < 0 ||
        transactionIndex >= transactions_.size()) {
        return 0;
    }

    const Currency target = currencyFromString(
        targetCurrency
        );

    const Money converted = currencyConverter_.convert(
        transactions_[transactionIndex].money(),
        target
        );

    return converted.minorUnits();
}

void FinanceController::addIncome(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency
    )
{
    addTransaction(
        minorUnits,
        TransactionType::Income,
        description,
        categoryId,
        currencyFromString(currency)
        );
}

void FinanceController::addExpense(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency
    )
{
    addTransaction(
        minorUnits,
        TransactionType::Expense,
        description,
        categoryId,
        currencyFromString(currency)
        );
}

void FinanceController::addTransaction(
    qint64 minorUnits,
    TransactionType type,
    const QString& description,
    const QString& categoryId,
    Currency currency
    )
{
    if (minorUnits <= 0) {
        return;
    }

    transactions_.prepend(
        Transaction(
            QUuid::createUuid().toString(
                QUuid::WithoutBraces
                ),
            "household",
            categoryId,
            Money(
                minorUnits,
                currency
                ),
            type,
            QDateTime::currentDateTime(),
            description
            )
        );

    emit transactionsChanged();
    emit balanceChanged();
}

Currency FinanceController::currencyFromString(
    const QString& currency
    )
{
    const QString normalized = currency.trimmed().toUpper();

    if (normalized == QStringLiteral("USD")) {
        return Currency::USD;
    }

    if (normalized == QStringLiteral("EUR")) {
        return Currency::EUR;
    }

    return Currency::RUB;
}