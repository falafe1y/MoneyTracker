#pragma once

#include "../core/Transaction.h"
#include "../services/BalanceCalculator.h"
#include "../services/CurrencyConverter.h"
#include "../services/TestCurrencyRateProvider.h"

#include <QObject>
#include <QVariantList>
#include <QVector>

class FinanceController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        qint64 balanceMinorUnits
            READ balanceMinorUnits
                NOTIFY balanceChanged
        )

    Q_PROPERTY(
        QString balanceCurrency
            READ balanceCurrency
                NOTIFY balanceChanged
        )

    Q_PROPERTY(
        qint64 incomeMinorUnits
            READ incomeMinorUnits
                NOTIFY balanceChanged
        )

    Q_PROPERTY(
        qint64 expenseMinorUnits
            READ expenseMinorUnits
                NOTIFY balanceChanged
        )

    Q_PROPERTY(
        QString appCurrency
            READ appCurrency
                WRITE setAppCurrency
                    NOTIFY appCurrencyChanged
        )

    Q_PROPERTY(
        QVariantList transactions
            READ transactions
                NOTIFY transactionsChanged
        )

public:
    explicit FinanceController(QObject* parent = nullptr);

    qint64 balanceMinorUnits() const;
    QString balanceCurrency() const;

    qint64 incomeMinorUnits() const;
    qint64 expenseMinorUnits() const;

    QString appCurrency() const;
    void setAppCurrency(const QString& currency);

    QVariantList transactions() const;

    Q_INVOKABLE void addIncome(
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& currency
        );

    Q_INVOKABLE void addExpense(
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& currency
        );

    Q_INVOKABLE qint64 convertTransaction(
        int transactionIndex,
        const QString& targetCurrency
        ) const;

signals:
    void balanceChanged();
    void transactionsChanged();
    void appCurrencyChanged();

private:
    void addTransaction(
        qint64 minorUnits,
        TransactionType type,
        const QString& description,
        const QString& categoryId,
        Currency currency
        );

    static Currency currencyFromString(
        const QString& currency
        );

    TestCurrencyRateProvider rateProvider_;
    CurrencyConverter currencyConverter_;
    BalanceCalculator balanceCalculator_;

    QVector<Transaction> transactions_;

    Currency appCurrency_ = Currency::RUB;
};