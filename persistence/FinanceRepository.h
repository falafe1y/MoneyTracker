#pragma once

#include "../core/Transaction.h"
#include "../core/Category.h"

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

    FinanceRepository();
    ~FinanceRepository();

    FinanceRepository(const FinanceRepository&) = delete;
    FinanceRepository& operator=(const FinanceRepository&) = delete;

    bool isOpen() const;
    QString lastError() const;
    QVector<Transaction> loadTransactions();
    QVector<Category> loadCategories();
    QSet<QString> loadArchivedCategoryIds();
    Summary loadSummary();
    bool insertTransaction(const Transaction& transaction);
    bool insertCategory(const Category& category);
    bool updateCategoryName(const QString& id, const QString& name);
    bool archiveCategory(const QString& id);
    QString loadAppCurrency() const;
    bool saveAppCurrency(const QString& currency);

private:
    bool initializeSchema();
    bool seedDefaults();
    QString accountIdForCurrency(Currency currency) const;
    void setLastError(const QString& error);

    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};
