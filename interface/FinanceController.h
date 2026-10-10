#pragma once

#include "../core/Transaction.h"
#include "../persistence/FinanceRepository.h"
#include "../services/BalanceCalculator.h"
#include "../services/BankCsvImporter.h"
#include "../services/CapitalHistoryCalculator.h"
#include "../services/InvestmentValuation.h"
#include "../services/CbrCurrencyRateProvider.h"
#include "../services/CurrencyConverter.h"
#include "../services/CryptoProvider.h"
#include "../services/BybitProvider.h"
#include "../services/MoexInvestmentProvider.h"
#include "../services/FinancialGoalCalculator.h"
#include "../core/FinancialTrajectory.h"

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
#include <optional>

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
        double manualRubToRubRate
            READ manualRubToRubRate
                NOTIFY manualCurrencyRatesChanged
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

    Q_PROPERTY(double manualCnyToRubRate READ manualCnyToRubRate NOTIFY manualCurrencyRatesChanged)

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

    Q_PROPERTY(QVariantList investmentOperations READ investmentOperations NOTIFY investmentPositionsChanged)
    Q_PROPERTY(bool investmentValuationIncomplete READ investmentValuationIncomplete NOTIFY balanceChanged)
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
        QVariantList expenseHistoryByMonthRub
            READ expenseHistoryByMonthRub
                NOTIFY analyticsChanged
        )

    Q_PROPERTY(
        QVariantList capitalHistoryRub
            READ capitalHistoryRub
                NOTIFY analyticsChanged
        )

    Q_PROPERTY(
        QString analyticsCurrency
            READ analyticsCurrency
                WRITE setAnalyticsCurrency
                    NOTIFY analyticsChanged
        )

    Q_PROPERTY(
        QVariantList capitalHistoryAnalyticsCurrency
            READ capitalHistoryAnalyticsCurrency
                NOTIFY analyticsChanged
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
        QVariantList budgets
            READ budgets
                NOTIFY budgetsChanged
        )

    Q_PROPERTY(
        QString selectedBudgetId
            READ selectedBudgetId
                WRITE setSelectedBudgetId
                    NOTIFY selectedBudgetIdChanged
        )

    Q_PROPERTY(
        QString selectedBudgetMonth
            READ selectedBudgetMonth
                WRITE setSelectedBudgetMonth
                    NOTIFY selectedBudgetMonthChanged
        )

    Q_PROPERTY(
        QVariantList budgetTransactions
            READ budgetTransactions
                NOTIFY budgetsChanged
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

    Q_PROPERTY(QVariantList financialGoals READ financialGoals NOTIFY financialGoalsChanged)
    Q_PROPERTY(QVariantList goalSources READ goalSources NOTIFY financialGoalsChanged)
    Q_PROPERTY(QVariantMap financialTrajectory READ financialTrajectory NOTIFY financialTrajectoryChanged)
    Q_PROPERTY(QString notesText READ notesText NOTIFY notesChanged)
    Q_PROPERTY(bool notesDirty READ notesDirty NOTIFY notesChanged)
    Q_PROPERTY(bool notesAvailable READ notesAvailable NOTIFY notesChanged)
    Q_PROPERTY(QString notesError READ notesError NOTIFY notesChanged)

public:
    QVariantList financialGoals() const;
    QVariantList goalSources() const;
    Q_INVOKABLE QVariantMap saveFinancialGoal(const QVariantMap& values);
    Q_INVOKABLE bool deleteFinancialGoal(const QString& id);
    QVariantMap financialTrajectory();
    Q_INVOKABLE QVariantMap saveFinancialTrajectorySettings(const QVariantMap& values);

    explicit FinanceController(QObject* parent = nullptr, const QString& databasePath = {});

    QString notesText() const;
    bool notesDirty() const;
    bool notesAvailable() const;
    QString notesError() const;
    Q_INVOKABLE void setNotesText(const QString& text);
    Q_INVOKABLE bool saveNotes();

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
    double manualRubToRubRate() const;
    double manualUsdToRubRate() const;
    double manualEurToRubRate() const;
    QVariantList currentCurrencyRates() const;
    Q_INVOKABLE bool saveManualCurrencyRates(
        double rublesPerRub,
        double rublesPerUsd,
        double rublesPerEur
        );

    double manualCnyToRubRate() const;
    Q_INVOKABLE bool saveManualCurrencyRates(double rublesPerRub, double rublesPerUsd,
        double rublesPerEur, double rublesPerCny);

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
    QVariantList investmentOperations() const;
    bool investmentValuationIncomplete() const;
    Q_INVOKABLE QVariantMap saveInvestmentPosition(const QVariantMap& values);
    Q_INVOKABLE QVariantMap recordInvestmentOperation(const QVariantMap& values);
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
    QVariantList expenseHistoryByMonthRub() const;
    QVariantList capitalHistoryRub() const;
    QString analyticsCurrency() const;
    void setAnalyticsCurrency(const QString& currency);
    QVariantList capitalHistoryAnalyticsCurrency() const;
    QVariantList bankCsvProfiles() const;
    QVariantList scheduledTransactions() const;
    QVariantList projects() const;
    QString selectedProjectId() const;
    void setSelectedProjectId(const QString& id);
    QVariantList projectTransactions() const;
    QVariantList budgets() const;
    QString selectedBudgetId() const;
    void setSelectedBudgetId(const QString& id);
    QString selectedBudgetMonth() const;
    void setSelectedBudgetMonth(const QString& month);
    QVariantList budgetTransactions() const;
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
        qint64 creditLimitMinor,
        double depositAnnualRatePercent = 0.0,
        const QString& depositPayoutFrequency = QString(),
        int depositPayoutDay = 1
        );

    Q_INVOKABLE bool updateAccount(
        const QString& id,
        const QString& name,
        const QString& type,
        const QString& currency,
        qint64 initialBalanceMinor,
        qint64 creditLimitMinor,
        double depositAnnualRatePercent = 0.0,
        const QString& depositPayoutFrequency = QString(),
        int depositPayoutDay = 1
        );

    Q_INVOKABLE bool deleteAccount(const QString& id);

    Q_INVOKABLE bool addProject(const QString& name);
    Q_INVOKABLE bool renameProject(
        const QString& id,
        const QString& name
        );
    Q_INVOKABLE bool deleteProject(const QString& id);
    Q_INVOKABLE QVariantMap saveBudget(const QVariantMap& values);
    Q_INVOKABLE bool deleteBudget(const QString& id);
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
    Q_INVOKABLE QVariantMap saveCryptoConnection(const QVariantMap& values);
    Q_INVOKABLE QVariantList cryptoExchangeHoldings() const;
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
    Q_INVOKABLE bool deleteHistoryRows(const QVariantList& rows);

    Q_INVOKABLE QVariantMap exportTransactionsCsv(const QUrl& fileUrl) const;
    Q_INVOKABLE QVariantMap backupDatabase(const QUrl& fileUrl);
    Q_INVOKABLE QVariantMap restoreDatabase(const QUrl& fileUrl);
    Q_INVOKABLE QVariantMap clearAllData();
    Q_INVOKABLE QVariantMap importTransactionsCsv(const QUrl& fileUrl);
    Q_INVOKABLE QVariantMap inspectBankCsv(
        const QUrl& fileUrl,
        int headerRow,
        const QString& delimiter = QStringLiteral("auto"),
        const QString& encoding = QStringLiteral("auto")
        ) const;
    Q_INVOKABLE int inspectBankCsvAsync(const QUrl& fileUrl, int headerRow,
        const QString& delimiter = QStringLiteral("auto"),
        const QString& encoding = QStringLiteral("auto"));
    Q_INVOKABLE QVariantMap saveBankCsvProfile(const QVariantMap& values);
    Q_INVOKABLE QVariantMap previewBankImport(
        const QUrl& fileUrl,
        const QVariantMap& values
        ) const;
    Q_INVOKABLE bool deleteBankCsvProfile(const QString& id);
    Q_INVOKABLE QVariantMap importBankCsv(
        const QUrl& fileUrl,
        const QString& profileId,
        const QVariantMap& options = QVariantMap()
        );
    Q_INVOKABLE QString normalizeBankCategoryText(const QString& text) const;
    Q_INVOKABLE QVariantMap resolveBankImportRows(const QVariantList& rows, const QVariantMap& choices) const;
    Q_INVOKABLE QVariantMap bankImportTransferDetails(const QVariantMap& row,
        const QVariantMap& choice) const;
    Q_INVOKABLE QVariantMap bankRecipientRules() const;
    Q_INVOKABLE QVariantMap deleteBankRecipientRule(const QString& pattern,
        const QString& field, const QString& matchMode, const QString& type);
    Q_INVOKABLE QVariantMap bankCategoryRules() const;
    Q_INVOKABLE QVariantMap deleteBankCategoryRule(const QString& pattern,
        const QString& matchMode, const QString& type);
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
    void analyticsChanged();
    void bankCsvProfilesChanged();
    void bankCsvInspectionFinished(int requestId, const QVariantMap& result);
    void scheduledTransactionsChanged();
    void projectsChanged();
    void selectedProjectIdChanged();
    void budgetsChanged();
    void financialGoalsChanged();
    void financialTrajectoryChanged();
    void selectedBudgetIdChanged();
    void selectedBudgetMonthChanged();
    void notesChanged();

private:
    static int currencyIndex(Currency currency);
    static AssetType assetTypeFromString(const QString& asset);
    static QString assetTypeToString(AssetType asset);
    static std::optional<AccountType> accountTypeFromString(const QString& type);
    static QString accountTypeToString(AccountType type);

    qint64 convertedTotal(
        const std::array<qint64, kCurrencyCount>& amounts
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
    bool hasBudget(const QString& id) const;
    bool transactionMatchesBudget(
        const Transaction& transaction,
        const Budget& budget
        ) const;
    void refreshBudgetMonthLimits();
    void loadNotes();

    qint64 accountBalanceMinor(const Account& account) const;
    qint64 investmentAccountValueMinor(const QString& accountId) const;
    InvestmentValuation investmentPositionValue(const InvestmentPosition& position) const;
    void reloadInvestments();
    int accountTransactionCount(const QString& accountId) const;
    qint64 assetBalanceMinor(AssetType asset) const;
    qint64 cryptoAmountValueMinor(
        const CryptoWallet& wallet,
        qint64 amountAtomic
        ) const;
    qint64 cryptoAmountValueMinorInCurrency(
        const CryptoWallet& wallet,
        qint64 amountAtomic,
        Currency currency
        ) const;
    qint64 cryptoWalletValueMinor(const CryptoWallet& wallet) const;
    qint64 cryptoWalletsTotalMinor() const;
    CapitalHistorySeries calculateCapitalHistory(
        Currency currency,
        bool applyDateFilter
        ) const;
    QVariantList capitalHistoryToVariant(
        const CapitalHistorySeries& series,
        Currency currency
        ) const;
    void rebuildCapitalHistory();
    void scheduleRecurringMaterialization();
    void materializeRecurringTransactions();
    int materializeDepositInterest();
    const DepositSettings* depositSettingsForAccount(
        const QString& accountId
        ) const;
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

    QVariantMap buildBankCsvInspection(const CsvCodec::ReadResult& csv,
        const QString& filePath, int headerRow) const;
    int bankCsvInspectionRequestId_ = 0;
    FinanceRepository repository_;
    CbrCurrencyRateProvider rateProvider_;
    CurrencyConverter currencyConverter_;
    BalanceCalculator balanceCalculator_;
    CryptoProvider cryptoProvider_;
    BybitProvider bybitProvider_;
    QVariantList cryptoExchanges_;
    QHash<QString, QString> cryptoWalletNames_;
    QHash<QString, QPair<QString, QString>> exchangeCredentials_;
    QHash<QString, QString> exchangeErrors_;
    QHash<QString, QString> exchangeCredentialWarnings_;
    QSet<QString> loadingExchangeCredentials_;
    QTimer exchangeRefreshTimer_;
    void refreshCryptoExchanges(bool force = true);
    void startExchangeRefresh(const QString& id);
    qint64 exchangeUsdMinor(const QVariantMap& account) const;
    MoexInvestmentProvider investmentProvider_;

    QVector<Transaction> transactions_;
    QVector<Category> categories_;
    QVector<Account> accounts_;
    QVector<CryptoWallet> cryptoWallets_;
    QVector<CryptoTransaction> cryptoTransactions_;
    QVector<InvestmentInstrument> investmentInstruments_;
    QVector<InvestmentPosition> investmentPositions_;
    QVector<InvestmentQuote> investmentQuotes_;
    QVector<InvestmentOperation> investmentOperations_;
    QVector<InvestmentMarketInstrument> investmentSearchResults_;
    CapitalHistorySeries capitalHistorySeries_;
    QVector<BankCsvProfile> bankCsvProfiles_;
    QVector<RecurringTransaction> recurringTransactions_;
    QVector<DepositSettings> depositSettings_;
    QVector<Project> projects_;
    QVector<Budget> budgets_;
    QVector<FinancialGoal> financialGoals_;
    QVector<GoalAssetValue> goalAssetValues() const;
    void captureCapitalSnapshot(bool overwriteToday = true);
    FinancialTrajectorySettings trajectorySettings_;
    QDate lastCapitalSnapshotDate_;
    QSet<QString> archivedCategoryIds_;
    FinanceRepository::Summary summary_;
    bool recurringMaterializationScheduled_ = false;

    Currency appCurrency_ = Currency::RUB;
    Currency analyticsCurrency_ = Currency::USD;
    QString uiLanguage_ = QStringLiteral("ru");
    bool automaticCurrencyRates_ = true;
    double manualRubToRubRate_ = 1.0;
    double manualUsdToRubRate_ = 90.909090909;
    double manualEurToRubRate_ = 106.363636364;
    double manualCnyToRubRate_ = kDefaultCnyToRubRate;
    AssetType selectedAsset_ = AssetType::Fiat;
    QString selectedAccountId_;
    QString selectedCryptoWalletId_;
    QString selectedProjectId_;
    QString selectedBudgetId_;
    QDate selectedBudgetMonth_;
    QHash<QString, qint64> budgetMonthLimits_;
    QDate dateFilterFrom_;
    QDate dateFilterTo_;

    QHash<QString, qint64> cryptoPricesUsdMicros_;
    QHash<QString, QDateTime> cryptoPricesFetchedAtUtc_;
    QDateTime lastCryptoRefreshAttemptUtc_;
    QTimer cryptoRefreshTimer_;
    QTimer recurringTimer_;
    QTimer notesSaveTimer_;
    QString notesFilePath_;
    QString notesText_;
    QString notesError_;
    bool notesDirty_ = false;
    bool notesAvailable_ = true;
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
