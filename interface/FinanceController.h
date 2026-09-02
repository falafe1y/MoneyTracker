#pragma once

#include "../core/Transaction.h"
#include "../persistence/FinanceRepository.h"
#include "../services/BalanceCalculator.h"
#include "../services/CurrencyConverter.h"
#include "../services/TestCurrencyRateProvider.h"

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QSet>

#include <array>

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

    Q_PROPERTY(
        QVariantList categories
            READ categories
                NOTIFY categoriesChanged
        )

    Q_PROPERTY(
        QString selectedAsset
            READ selectedAsset
                WRITE setSelectedAsset
                    NOTIFY selectedAssetChanged
        )

    Q_PROPERTY(
        QVariantList accounts
            READ accounts
                NOTIFY accountsChanged
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
    QVariantList categories() const;
    QVariantList accounts() const;

    QString selectedAsset() const;
    void setSelectedAsset(const QString& asset);

    Q_INVOKABLE bool addAccount(
        const QString& name,
        const QString& type,
        const QString& currency,
        qint64 initialBalanceMinor
        );

    Q_INVOKABLE bool addCategory(
        const QString& name,
        const QString& type
        );

    Q_INVOKABLE bool renameCategory(
        const QString& id,
        const QString& name
        );

    Q_INVOKABLE bool deleteCategory(const QString& id);
    Q_INVOKABLE QString categoryName(const QString& id) const;

    Q_INVOKABLE bool addIncome(
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& currency
        );

    Q_INVOKABLE bool addExpense(
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
    void categoriesChanged();
    void selectedAssetChanged();
    void accountsChanged();
    void appCurrencyChanged();

private:
    static int currencyIndex(Currency currency);
    static AssetType assetTypeFromString(const QString& asset);
    static QString assetTypeToString(AssetType asset);
    static AccountType accountTypeFromString(const QString& type);
    static QString accountTypeToString(AccountType type);

    qint64 convertedTotal(
        const std::array<qint64, 3>& amounts
        ) const;

    bool addTransaction(
        qint64 minorUnits,
        TransactionType type,
        const QString& description,
        const QString& categoryId,
        Currency currency
        );

    static Currency currencyFromString(
        const QString& currency
        );

    FinanceRepository repository_;
    TestCurrencyRateProvider rateProvider_;
    CurrencyConverter currencyConverter_;
    BalanceCalculator balanceCalculator_;

    QVector<Transaction> transactions_;
    QVector<Category> categories_;
    QVector<Account> accounts_;
    QSet<QString> archivedCategoryIds_;
    FinanceRepository::Summary summary_;

    Currency appCurrency_ = Currency::RUB;
    AssetType selectedAsset_ = AssetType::Fiat;
};
