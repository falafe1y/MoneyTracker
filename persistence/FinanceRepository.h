#pragma once

#include "../core/Transaction.h"
#include "../core/RecurringTransaction.h"
#include "../core/Category.h"
#include "../core/Account.h"
#include "../core/CryptoWallet.h"
#include "../core/CryptoTransaction.h"
#include "../core/InvestmentInstrument.h"
#include "../core/InvestmentPosition.h"
#include "../core/InvestmentQuote.h"
#include "../core/Project.h"
#include "../core/Budget.h"

#include <QSqlDatabase>
#include <QDate>
#include <QDateTime>
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

    struct CryptoPriceSnapshot
    {
        qint64 priceUsdMicros = 0;
        QDateTime fetchedAtUtc;
    };

    struct BankCsvProfileRecord
    {
        QString id;
        QString name;
        QString configurationJson;
    };

    struct RecurringOccurrence
    {
        QDate date;
        Transaction transaction;
    };

    QVector<Budget> loadBudgets();
    qint64 loadBudgetLimit(
        const QString& budgetId,
        const QDate& month,
        qint64 fallback
        );
    bool insertBudget(const Budget& budget, const QDate& month);
    bool updateBudget(const Budget& budget, const QDate& month);
    bool archiveBudget(const QString& id);
    bool ensureBudgetMonth(
        const QString& budgetId,
        const QDate& month,
        qint64 limitMinor
        );

    explicit FinanceRepository(const QString& databasePath = {});
    ~FinanceRepository();

    FinanceRepository(const FinanceRepository&) = delete;
    FinanceRepository& operator=(const FinanceRepository&) = delete;

    bool isOpen() const;
    QString lastError() const;
    QVector<Transaction> loadTransactions();
    QVector<Project> loadProjects();
    QVector<RecurringTransaction> loadRecurringTransactions();
    QVector<Category> loadCategories();
    QVector<Account> loadAccounts();
    QVector<CryptoWallet> loadCryptoWallets();
    QVector<CryptoTransaction> loadCryptoTransactions();
    QVector<InvestmentInstrument> loadInvestmentInstruments();
    QVector<InvestmentPosition> loadInvestmentPositions();
    QVector<InvestmentQuote> loadInvestmentQuotes();
    CryptoPriceSnapshot loadCryptoPrice(const QString& symbol) const;
    QDateTime loadCryptoRefreshAttemptUtc() const;
    QSet<QString> loadArchivedCategoryIds();
    Summary loadSummary();
    bool insertTransaction(const Transaction& transaction);
    bool insertProject(const Project& project);
    bool updateProject(const Project& project);
    bool archiveProject(const QString& id);
    bool insertTransfer(
        const Transaction& outgoing,
        const Transaction& incoming
        );
    bool insertTransactions(const QVector<Transaction>& transactions);
    bool updateTransaction(const Transaction& transaction);
    bool replaceTransaction(
        const QString& currentId,
        const Transaction& replacement
        );
    bool replaceTransactionWithTransfer(
        const QString& currentId,
        const Transaction& outgoing,
        const Transaction& incoming
        );
    bool deleteTransaction(const QString& id);
    bool insertRecurringTransaction(
        const RecurringTransaction& recurring
        );
    bool updateRecurringTransaction(
        const RecurringTransaction& recurring
        );
    bool deleteRecurringTransaction(const QString& id);
    bool materializeRecurringOccurrences(
        const QString& recurringId,
        const QVector<RecurringOccurrence>& occurrences,
        const QDate& generatedThrough,
        int* insertedCount = nullptr
        );
    bool insertCategory(const Category& category);
    bool insertAccount(const Account& account);
    bool updateAccount(const Account& account);
    bool deleteAccount(const QString& id);
    bool insertCryptoWallet(const CryptoWallet& wallet);
    bool updateCryptoWalletBalance(
        const QString& id,
        qint64 balanceAtomic,
        const QDateTime& fetchedAtUtc
        );
    bool deleteCryptoWallet(const QString& id);
    bool replaceCryptoTransactions(
        const QString& walletId,
        const QVector<CryptoTransaction>& transactions,
        const QDateTime& fetchedAtUtc
        );
    bool saveCryptoPrice(
        const QString& symbol,
        qint64 priceUsdMicros,
        const QDateTime& fetchedAtUtc
        );
    bool saveCryptoRefreshAttemptUtc(const QDateTime& attemptedAtUtc);
    bool insertInvestmentInstrument(const InvestmentInstrument& instrument);
    bool updateInvestmentInstrument(const InvestmentInstrument& instrument);
    bool archiveInvestmentInstrument(const QString& id);
    bool insertInvestmentPosition(const InvestmentPosition& position);
    bool updateInvestmentPosition(const InvestmentPosition& position);
    bool deleteInvestmentPosition(const QString& id);
    bool archiveInvestmentPosition(const QString& id);
    bool saveInvestmentQuote(const InvestmentQuote& quote);
    bool updateCategoryName(const QString& id, const QString& name);
    bool archiveCategory(const QString& id);
    QString loadAppCurrency() const;
    bool saveAppCurrency(const QString& currency);
    QString loadSelectedAsset() const;
    bool saveSelectedAsset(const QString& asset);
    QString loadUiLanguage() const;
    bool saveUiLanguage(const QString& language);
    bool loadAutomaticCurrencyRates() const;
    double loadManualUsdToRubRate() const;
    double loadManualEurToRubRate() const;
    bool saveAutomaticCurrencyRates(bool enabled);
    bool saveManualCurrencyRates(
        double rublesPerUsd,
        double rublesPerEur
        );
    QVector<BankCsvProfileRecord> loadBankCsvProfiles();
    bool saveBankCsvProfile(
        const QString& id,
        const QString& name,
        const QString& configurationJson
        );
    bool deleteBankCsvProfile(const QString& id);

private:
    bool initializeSchema();
    bool migrateLegacySchema();
    bool seedDefaults();
    void setLastError(const QString& error);

    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};
