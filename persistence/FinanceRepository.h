#pragma once

#include "../core/Transaction.h"
#include "../services/BankCategoryMatcher.h"
#include "../core/RecurringTransaction.h"
#include "../core/DepositSettings.h"
#include "../core/Category.h"
#include "../core/Account.h"
#include "../core/CryptoWallet.h"
#include "../core/CryptoTransaction.h"
#include "../core/InvestmentInstrument.h"
#include "../core/InvestmentPosition.h"
#include "../core/InvestmentQuote.h"
#include "../core/InvestmentOperation.h"
#include "../core/Project.h"
#include "../core/Budget.h"
#include "../core/FinancialGoal.h"
#include "../core/FinancialTrajectory.h"

#include <QSqlDatabase>
#include <QDate>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include <QVector>
#include <QVariant>

#include <array>
#include <optional>

class FinanceRepository
{
public:
    struct Summary
    {
        std::array<qint64, kCurrencyCount> balance{};
        std::array<qint64, kCurrencyCount> income{};
        std::array<qint64, kCurrencyCount> expense{};
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

    struct DepositInterestOccurrence
    {
        QDate date;
        Transaction transaction;
    };

    QVector<FinancialGoal> loadFinancialGoals();
    bool saveFinancialGoal(const FinancialGoal& goal, bool editing);
    bool deleteFinancialGoal(const QString& id);

    QVector<CapitalSnapshot> loadCapitalSnapshots();
    bool saveCapitalSnapshot(const CapitalSnapshot& snapshot);
    FinancialTrajectorySettings loadFinancialTrajectorySettings() const;
    bool saveFinancialTrajectorySettings(
        const FinancialTrajectorySettings& settings);

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
    bool backupDatabase(const QString& destinationPath);
    // Existing primary keys keep their current values. The merge is atomic.
    bool restoreDatabase(const QString& sourcePath, qint64* added = nullptr,
                         qint64* skipped = nullptr);
    bool clearAllUserData();
    QVector<Transaction> loadTransactions();
    QVector<Project> loadProjects();
    QVector<RecurringTransaction> loadRecurringTransactions();
    QVector<DepositSettings> loadDepositSettings();
    QVector<Category> loadCategories();
    QVector<Account> loadAccounts();
    QVector<CryptoWallet> loadCryptoWallets();
    QString cryptoWalletName(const QString& id) const;
    bool setCryptoWalletName(const QString& id, const QString& name);
    QVariantList loadCryptoExchanges() const;
    bool saveCryptoExchange(const QVariantMap& account);
    bool saveCryptoExchangeSnapshot(const QString& id, const QVariantList& holdings,
                                   const QVariantList& operations, qint64 fetchedAtMs);
    bool deleteCryptoExchange(const QString& id);
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
    bool insertTransactions(const QVector<Transaction>& transactions,
                            const QVector<BankCategoryRule>& rules = {},
                            const QVector<BankRecipientRule>& recipientRules = {},
                            const QMap<QString, QString>& bankLinks = {},
                            const QSet<QString>& replacedBankTransactions = {});
    QMap<QString, QString> loadBankImportLinks(QString* error = nullptr) const;
    QVector<BankRecipientRule> loadBankRecipientRules(QString* error = nullptr) const;
    bool deleteBankRecipientRule(const QString& pattern, const QString& field, const QString& mode, CategoryType type);
    QVector<BankCategoryRule> loadBankCategoryRules(QString* error = nullptr) const;
    bool deleteBankCategoryRule(const QString& pattern, const QString& matchMode,
                                CategoryType type);
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
    bool deleteHistoryRows(const QVariantList& rows);
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
    bool insertDepositAccount(
        const Account& account,
        const DepositSettings& settings
        );
    bool updateAccount(const Account& account);
    bool updateAccountAndDeposit(
        const Account& account,
        const std::optional<DepositSettings>& settings
        );
    bool deleteAccount(const QString& id);
    bool materializeDepositInterest(
        const DepositSettings& settings,
        const QVector<DepositInterestOccurrence>& occurrences,
        const QDate& generatedThrough,
        int* insertedCount = nullptr
        );
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
    QVector<InvestmentOperation> loadInvestmentOperations();
    bool saveInvestmentBundle(const InvestmentInstrument& instrument,
        const InvestmentPosition& position, const std::optional<InvestmentQuote>& quote,
        bool editing, const std::optional<InvestmentOperation>& operation = {});
    bool bookInvestmentOperation(const InvestmentOperation& operation,
        const InvestmentPosition& expected, const InvestmentPosition& updated, bool close,
        const std::optional<InvestmentQuote>& quote = {});
    bool hasInvestmentOperations(const QString& positionId) const;
    bool updateCategoryName(const QString& id, const QString& name);
    bool archiveCategory(const QString& id);
    QString loadAppCurrency() const;
    bool saveAppCurrency(const QString& currency);
    QString loadAnalyticsCurrency() const;
    bool saveAnalyticsCurrency(const QString& currency);
    QString loadSelectedAsset() const;
    bool saveSelectedAsset(const QString& asset);
    QString loadUiLanguage() const;
    bool saveUiLanguage(const QString& language);
    bool loadAutomaticCurrencyRates() const;
    double loadManualRubToRubRate() const;
    double loadManualUsdToRubRate() const;
    double loadManualEurToRubRate() const;
    double loadManualCnyToRubRate() const;
    bool saveAutomaticCurrencyRates(bool enabled);
    bool saveManualCurrencyRates(
        double rublesPerRub,
        double rublesPerUsd,
        double rublesPerEur,
        double rublesPerCny = kDefaultCnyToRubRate
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
    bool migrateInvestmentSchema();
    bool migrateBankImportSchema();
    bool migrateCurrencySchema();
    bool seedDefaults();
    void setLastError(const QString& error);

    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};
