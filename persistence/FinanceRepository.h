#pragma once

#include "../core/Transaction.h"
#include "../core/Category.h"
#include "../core/Account.h"
#include "../core/CryptoWallet.h"
#include "../core/CryptoTransaction.h"
#include "../core/InvestmentInstrument.h"
#include "../core/InvestmentPosition.h"
#include "../core/InvestmentQuote.h"

#include <QSqlDatabase>
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

    explicit FinanceRepository(const QString& databasePath = {});
    ~FinanceRepository();

    FinanceRepository(const FinanceRepository&) = delete;
    FinanceRepository& operator=(const FinanceRepository&) = delete;

    bool isOpen() const;
    QString lastError() const;
    QVector<Transaction> loadTransactions();
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
