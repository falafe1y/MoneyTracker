#pragma once

#include "../core/Transaction.h"
#include "../persistence/FinanceRepository.h"
#include "../services/BalanceCalculator.h"
#include "../services/BankCsvImporter.h"
#include "../services/CapitalHistoryCalculator.h"
#include "../services/CbrCurrencyRateProvider.h"
#include "../services/CurrencyConverter.h"
#include "../services/CryptoProvider.h"
#include "../services/MoexInvestmentProvider.h"

#include <QDateTime>
#include <QDate>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <QVector>
#include <QSet>
#include <QTimer>

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
        QString uiLanguage
            READ uiLanguage
                WRITE setUiLanguage
                    NOTIFY uiLanguageChanged
        )

    Q_PROPERTY(
        bool automaticCurrencyRates
            READ automaticCurrencyRates
                WRITE setAutomaticCurrencyRates
                    NOTIFY automaticCurrencyRatesChanged
        )

    Q_PROPERTY(
        double manualUsdToRubRate
            READ manualUsdToRubRate
                NOTIFY manualCurrencyRatesChanged
        )

    Q_PROPERTY(
        double manualEurToRubRate
            READ manualEurToRubRate
                NOTIFY manualCurrencyRatesChanged
        )

    Q_PROPERTY(
        QVariantList currentCurrencyRates
            READ currentCurrencyRates
                NOTIFY currencyRatesChanged
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

    Q_PROPERTY(
        QVariantList allAccounts
            READ allAccounts
                NOTIFY accountsChanged
        )

    Q_PROPERTY(
        QVariantList assetSummaries
            READ assetSummaries
                NOTIFY balanceChanged
        )

    Q_PROPERTY(
        QVariantList cryptoWallets
            READ cryptoWallets
                NOTIFY cryptoWalletsChanged
        )

    Q_PROPERTY(
        bool cryptoRefreshing
            READ cryptoRefreshing
                NOTIFY cryptoRefreshingChanged
        )

    Q_PROPERTY(
        QString cryptoLastError
            READ cryptoLastError
                NOTIFY cryptoLastErrorChanged
        )

    Q_PROPERTY(
        QString selectedCryptoWalletId
            READ selectedCryptoWalletId
                WRITE setSelectedCryptoWalletId
                    NOTIFY selectedCryptoWalletIdChanged
        )

    Q_PROPERTY(
        QVariantList cryptoTransactions
            READ cryptoTransactions
                NOTIFY cryptoTransactionsChanged
        )

    Q_PROPERTY(
        QVariantList investmentAccounts
            READ investmentAccounts
                NOTIFY accountsChanged
        )

    Q_PROPERTY(
        QVariantList investmentPositions
            READ investmentPositions
                NOTIFY investmentPositionsChanged
        )

    Q_PROPERTY(
        QVariantList investmentSearchResults
            READ investmentSearchResults
                NOTIFY investmentSearchResultsChanged
        )

    Q_PROPERTY(
        bool investmentSearchBusy
            READ investmentSearchBusy
                NOTIFY investmentSearchStateChanged
        )

    Q_PROPERTY(
        bool investmentQuoteBusy
            READ investmentQuoteBusy
                NOTIFY investmentSearchStateChanged
        )

    Q_PROPERTY(
        bool investmentRefreshing
            READ investmentRefreshing
                NOTIFY investmentRefreshingChanged
        )

    Q_PROPERTY(
        QString investmentLastError
            READ investmentLastError
                NOTIFY investmentSearchStateChanged
        )

    Q_PROPERTY(
        int selectedInvestmentSearchIndex
            READ selectedInvestmentSearchIndex
                NOTIFY investmentSearchStateChanged
        )

    Q_PROPERTY(
        QString selectedAccountId
            READ selectedAccountId
                WRITE setSelectedAccountId
                    NOTIFY selectedAccountIdChanged
        )

    Q_PROPERTY(
        bool dateFilterActive
            READ dateFilterActive
                NOTIFY dateFilterChanged
        )

    Q_PROPERTY(
        QString dateFilterFrom
            READ dateFilterFrom
                NOTIFY dateFilterChanged
        )

    Q_PROPERTY(
        QString dateFilterTo
            READ dateFilterTo
                NOTIFY dateFilterChanged
        )

    Q_PROPERTY(
        QVariantList capitalHistory
            READ capitalHistory
                NOTIFY capitalHistoryChanged
        )

    Q_PROPERTY(
        QVariantList bankCsvProfiles
            READ bankCsvProfiles
                NOTIFY bankCsvProfilesChanged
        )

    Q_PROPERTY(
        QVariantList scheduledTransactions
            READ scheduledTransactions
                NOTIFY scheduledTransactionsChanged
        )

    Q_PROPERTY(
        QVariantList projects
            READ projects
                NOTIFY projectsChanged
        )

    Q_PROPERTY(
        QString selectedProjectId
            READ selectedProjectId
                WRITE setSelectedProjectId
                    NOTIFY selectedProjectIdChanged
        )

    Q_PROPERTY(
        QVariantList projectTransactions
            READ projectTransactions
                NOTIFY projectsChanged
        )

public:
    explicit FinanceController(QObject* parent = nullptr);

    qint64 balanceMinorUnits() const;
    QString balanceCurrency() const;

    qint64 incomeMinorUnits() const;
    qint64 expenseMinorUnits() const;

    QString appCurrency() const;
    void setAppCurrency(const QString& currency);

    QString uiLanguage() const;
    void setUiLanguage(const QString& language);
    void retranslate();

    bool automaticCurrencyRates() const;
    void setAutomaticCurrencyRates(bool enabled);
    double manualUsdToRubRate() const;
    double manualEurToRubRate() const;
    QVariantList currentCurrencyRates() const;
    Q_INVOKABLE bool saveManualCurrencyRates(
        double rublesPerUsd,
        double rublesPerEur
        );

    QVariantList transactions() const;
    QVariantList categories() const;
    QVariantList accounts() const;
    QVariantList allAccounts() const;
    QVariantList assetSummaries() const;
    QVariantList cryptoWallets() const;
    bool cryptoRefreshing() const;
    QString cryptoLastError() const;
    QString selectedCryptoWalletId() const;
    void setSelectedCryptoWalletId(const QString& walletId);
    QVariantList cryptoTransactions() const;
    QVariantList investmentAccounts() const;
    QVariantList investmentPositions() const;
    QVariantList investmentSearchResults() const;
    bool investmentSearchBusy() const;
    bool investmentQuoteBusy() const;
    bool investmentRefreshing() const;
    QString investmentLastError() const;
    int selectedInvestmentSearchIndex() const;

    QString selectedAsset() const;
    void setSelectedAsset(const QString& asset);
    QString selectedAccountId() const;
    void setSelectedAccountId(const QString& accountId);

    bool dateFilterActive() const;
    QString dateFilterFrom() const;
    QString dateFilterTo() const;
    QVariantList capitalHistory() const;
    QVariantList bankCsvProfiles() const;
    QVariantList scheduledTransactions() const;
    QVariantList projects() const;
    QString selectedProjectId() const;
    void setSelectedProjectId(const QString& id);
    QVariantList projectTransactions() const;
    Q_INVOKABLE bool setDateFilter(
        const QDateTime& from,
        const QDateTime& to
        );
    Q_INVOKABLE void clearDateFilter();

    Q_INVOKABLE bool addAccount(
        const QString& name,
        const QString& type,
        const QString& currency,
        qint64 initialBalanceMinor,
        qint64 creditLimitMinor
        );

    Q_INVOKABLE bool updateAccount(
        const QString& id,
        const QString& name,
        const QString& type,
        const QString& currency,
        qint64 initialBalanceMinor,
        qint64 creditLimitMinor
        );

    Q_INVOKABLE bool deleteAccount(const QString& id);

    Q_INVOKABLE bool addProject(const QString& name);
    Q_INVOKABLE bool renameProject(
        const QString& id,
        const QString& name
        );
    Q_INVOKABLE bool deleteProject(const QString& id);
    Q_INVOKABLE bool addProjectTransaction(
        const QString& projectId,
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& accountId,
        const QString& type,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE QVariantMap addCryptoWallet(
        const QString& symbol,
        const QString& address
        );
    Q_INVOKABLE bool deleteCryptoWallet(const QString& id);
    Q_INVOKABLE void refreshCryptoWallets();
    Q_INVOKABLE void searchInvestmentInstruments(const QString& query);
    Q_INVOKABLE void selectInvestmentSearchResult(int index);
    Q_INVOKABLE QVariantMap addInvestmentPosition(
        const QString& accountId,
        int searchResultIndex,
        const QString& quantity,
        const QString& averagePrice
        );
    Q_INVOKABLE QVariantMap updateInvestmentPosition(
        const QString& positionId,
        const QString& accountId,
        int searchResultIndex,
        const QString& quantity,
        const QString& averagePrice
        );
    Q_INVOKABLE bool deleteInvestmentPosition(const QString& id);
    Q_INVOKABLE void refreshInvestmentQuotes();

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
        const QString& currency,
        const QString& accountId,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE bool addExpense(
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& currency,
        const QString& accountId,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE bool addTransfer(
        qint64 sourceMinorUnits,
        const QString& description,
        const QString& sourceAccountId,
        const QString& targetAccountId,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE bool updateTransaction(
        const QString& id,
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& accountId,
        const QString& type,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE bool updateOperation(
        const QString& id,
        qint64 minorUnits,
        const QString& description,
        const QString& categoryId,
        const QString& accountId,
        const QString& type,
        const QString& targetAccountId,
        const QDateTime& occurredAt
        );

    Q_INVOKABLE QVariantMap transferDetails(const QString& id) const;

    Q_INVOKABLE bool deleteTransaction(const QString& id);

    Q_INVOKABLE QVariantMap exportTransactionsCsv(const QUrl& fileUrl) const;
    Q_INVOKABLE QVariantMap importTransactionsCsv(const QUrl& fileUrl);
    Q_INVOKABLE QVariantMap inspectBankCsv(
        const QUrl& fileUrl,
        int headerRow,
        const QString& delimiter = QStringLiteral("auto"),
        const QString& encoding = QStringLiteral("auto")
        ) const;
    Q_INVOKABLE QVariantMap saveBankCsvProfile(const QVariantMap& values);
    Q_INVOKABLE bool deleteBankCsvProfile(const QString& id);
    Q_INVOKABLE QVariantMap importBankCsv(
        const QUrl& fileUrl,
        const QString& profileId
        );
    Q_INVOKABLE QVariantMap saveScheduledTransaction(
        const QVariantMap& values
        );
    Q_INVOKABLE bool deleteScheduledTransaction(const QString& id);

    Q_INVOKABLE qint64 convertTransaction(
        int transactionIndex,
        const QString& targetCurrency
        ) const;

signals:
    void balanceChanged();
    void transactionsChanged();
    void categoriesChanged();
    void selectedAssetChanged();
    void selectedAccountIdChanged();
    void accountsChanged();
    void appCurrencyChanged();
    void uiLanguageChanged();
    void automaticCurrencyRatesChanged();
    void manualCurrencyRatesChanged();
    void currencyRatesChanged();
    void dateFilterChanged();
    void cryptoWalletsChanged();
    void cryptoRefreshingChanged();
    void cryptoLastErrorChanged();
    void selectedCryptoWalletIdChanged();
    void cryptoTransactionsChanged();
    void investmentPositionsChanged();
    void investmentSearchResultsChanged();
    void investmentSearchStateChanged();
    void investmentRefreshingChanged();
    void capitalHistoryChanged();
    void bankCsvProfilesChanged();
    void scheduledTransactionsChanged();
    void projectsChanged();
    void selectedProjectIdChanged();

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
        Currency currency,
        const QString& accountId,
        const QDateTime& occurredAt,
        const QString& projectId = {}
        );

    QVariantMap transactionToVariant(const Transaction& transaction) const;
    bool hasProject(const QString& id) const;

    qint64 accountBalanceMinor(const Account& account) const;
    qint64 investmentAccountValueMinor(const QString& accountId) const;
    int accountTransactionCount(const QString& accountId) const;
    qint64 assetBalanceMinor(AssetType asset) const;
    qint64 cryptoAmountValueMinor(
        const CryptoWallet& wallet,
        qint64 amountAtomic
        ) const;
    qint64 cryptoWalletValueMinor(const CryptoWallet& wallet) const;
    qint64 cryptoWalletsTotalMinor() const;
    void rebuildCapitalHistory();
    void scheduleRecurringMaterialization();
    void materializeRecurringTransactions();
    void scheduleInitialCryptoRefresh();
    void scheduleNextCryptoRefresh(qint64 delayMs);
    void startCryptoBalanceRequest(const CryptoWallet& wallet);
    void startCryptoTransactionRequest(const CryptoWallet& wallet);
    void startCryptoPriceRequest();
    void beginCryptoWalletRequest(const QString& walletId);
    void finishCryptoWalletRequest(const QString& walletId);
    void finishCryptoRequest();
    void setCryptoLastError(const QString& error);
    void finishInvestmentQuoteRequest(const QString& instrumentId);
    void setInvestmentLastError(const QString& error);
    QVector<Transaction> dateFilteredTransactions() const;
    FinanceRepository::Summary dateFilteredSummary() const;
    QString accountDisplayName(const Account& account) const;
    QString categoryDisplayName(const Category& category) const;
    QString transactionDisplayDescription(const Transaction& transaction) const;

    static Currency currencyFromString(
        const QString& currency
        );

    FinanceRepository repository_;
    CbrCurrencyRateProvider rateProvider_;
    CurrencyConverter currencyConverter_;
    BalanceCalculator balanceCalculator_;
    CryptoProvider cryptoProvider_;
    MoexInvestmentProvider investmentProvider_;

    QVector<Transaction> transactions_;
    QVector<Category> categories_;
    QVector<Account> accounts_;
    QVector<CryptoWallet> cryptoWallets_;
    QVector<CryptoTransaction> cryptoTransactions_;
    QVector<InvestmentInstrument> investmentInstruments_;
    QVector<InvestmentPosition> investmentPositions_;
    QVector<InvestmentQuote> investmentQuotes_;
    QVector<InvestmentMarketInstrument> investmentSearchResults_;
    CapitalHistorySeries capitalHistorySeries_;
    QVector<BankCsvProfile> bankCsvProfiles_;
    QVector<RecurringTransaction> recurringTransactions_;
    QVector<Project> projects_;
    QSet<QString> archivedCategoryIds_;
    FinanceRepository::Summary summary_;
    bool recurringMaterializationScheduled_ = false;

    Currency appCurrency_ = Currency::RUB;
    QString uiLanguage_ = QStringLiteral("ru");
    bool automaticCurrencyRates_ = true;
    double manualUsdToRubRate_ = 90.909090909;
    double manualEurToRubRate_ = 106.363636364;
    AssetType selectedAsset_ = AssetType::Fiat;
    QString selectedAccountId_;
    QString selectedCryptoWalletId_;
    QString selectedProjectId_;
    QDate dateFilterFrom_;
    QDate dateFilterTo_;

    QHash<QString, qint64> cryptoPricesUsdMicros_;
    QHash<QString, QDateTime> cryptoPricesFetchedAtUtc_;
    QDateTime lastCryptoRefreshAttemptUtc_;
    QTimer cryptoRefreshTimer_;
    QTimer recurringTimer_;
    QSet<QString> refreshingCryptoWalletIds_;
    QHash<QString, int> pendingCryptoWalletRequests_;
    int pendingCryptoRequests_ = 0;
    bool cryptoRefreshing_ = false;
    QString cryptoLastError_;

    QSet<QString> refreshingInvestmentIds_;
    QString requestedSearchQuoteId_;
    int selectedInvestmentSearchIndex_ = -1;
    bool investmentSearchBusy_ = false;
    bool investmentQuoteBusy_ = false;
    bool investmentRefreshing_ = false;
    QString investmentLastError_;

    static constexpr qint64 kCryptoRefreshIntervalMs =
        6LL * 60LL * 60LL * 1000LL;
};
