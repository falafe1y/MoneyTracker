#pragma once

#include "../core/Transaction.h"
#include "../core/Category.h"
#include "../core/Account.h"

#include <QSqlDatabase>
#include <QSet>
#include <QVector>

#include <array>

class FinanceRepository
{
public:
    struct Summary
    {
        std::array<qint64, 3> balance{};
        std::array<qint64, 3> income{};
        std::array<qint64, 3> expense{};
    };

    explicit FinanceRepository(const QString& databasePath = {});
    ~FinanceRepository();

    FinanceRepository(const FinanceRepository&) = delete;
    FinanceRepository& operator=(const FinanceRepository&) = delete;

    bool isOpen() const;
    QString lastError() const;
    QVector<Transaction> loadTransactions();
    QVector<Category> loadCategories();
    QVector<Account> loadAccounts();
    QSet<QString> loadArchivedCategoryIds();
    Summary loadSummary();
    bool insertTransaction(const Transaction& transaction);
    bool updateTransaction(const Transaction& transaction);
    bool deleteTransaction(const QString& id);
    bool insertCategory(const Category& category);
    bool insertAccount(const Account& account);
    bool updateCategoryName(const QString& id, const QString& name);
    bool archiveCategory(const QString& id);
    QString loadAppCurrency() const;
    bool saveAppCurrency(const QString& currency);
    QString loadSelectedAsset() const;
    bool saveSelectedAsset(const QString& asset);

private:
    bool initializeSchema();
    bool migrateLegacySchema();
    bool seedDefaults();
    void setLastError(const QString& error);

    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};
