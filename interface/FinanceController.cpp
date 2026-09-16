#include "FinanceController.h"

#include "../services/DateSliceCalculator.h"
#include "../services/CapitalSnapshotStore.h"
#include "../services/CsvCodec.h"
#include "../services/CryptoParser.h"
#include "../services/TransactionDateFilter.h"
#include "../services/TronUsdtParser.h"

#include <QDebug>
#include <QCoreApplication>
#include <QLocale>
#include <QThreadPool>
#include <QUuid>

#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
bool isTransfer(const Transaction& transaction)
{
    return transaction.categoryId() == QStringLiteral("transfer-in") ||
           transaction.categoryId() == QStringLiteral("transfer-out");
}

QString transferId(const Transaction& transaction)
{
    if (transaction.categoryId() == QStringLiteral("transfer-out") &&
        transaction.id().endsWith(QStringLiteral("-out"))) {
        return transaction.id().left(transaction.id().size() - 4);
    }
    if (transaction.categoryId() == QStringLiteral("transfer-in") &&
        transaction.id().endsWith(QStringLiteral("-in"))) {
        return transaction.id().left(transaction.id().size() - 3);
    }
    return {};
}

qint64 scaledInvestmentValueMinor(
    const qint64 quantityMicros,
    const qint64 priceMicros
    )
{
    if (quantityMicros <= 0 || priceMicros <= 0) {
        return 0;
    }
    using Int128 = __int128_t;
    constexpr Int128 divisor = 10'000'000'000LL;
    const Int128 product = static_cast<Int128>(quantityMicros) *
        static_cast<Int128>(priceMicros);
    const Int128 roundedMinor = (product + divisor / 2) / divisor;
    return roundedMinor >= std::numeric_limits<qint64>::max()
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(roundedMinor);
}

void addAccountFinancialRoles(
    QVariantMap& item,
    const Account& account,
    const qint64 balanceMinor
    )
{
    const bool creditCard = account.isCreditCard();

    item["isCrypto"] = false;
    item["isCreditCard"] = creditCard;
    item["creditLimitMinor"] = account.creditLimitMinor();
    item["initialDebtMinor"] = account.debtMinor(account.initialBalanceMinor());
    item["debtMinor"] = account.debtMinor(balanceMinor);
    item["availableCreditMinor"] = account.availableCreditMinor(balanceMinor);
}

struct CryptoSpec
{
    QString symbol;
    QString network;
    int decimals = 0;
};

bool cryptoSpec(const QString& requestedSymbol, CryptoSpec& spec)
{
    const QString symbol = requestedSymbol.trimmed().toUpper();
    if (symbol == QStringLiteral("USDT")) {
        spec = {symbol, QStringLiteral("TRON"), 6};
        return true;
    }
    if (symbol == QStringLiteral("BTC")) {
        spec = {symbol, QStringLiteral("BITCOIN"), 8};
        return true;
    }
    if (symbol == QStringLiteral("ETH")) {
        spec = {symbol, QStringLiteral("ETHEREUM"), 8};
        return true;
    }
    return false;
}

bool validCryptoAddress(const CryptoSpec& spec, const QString& address)
{
    if (spec.network == QStringLiteral("TRON")) {
        return isValidTronAddress(address);
    }
    if (spec.network == QStringLiteral("BITCOIN")) {
        return isValidBitcoinAddress(address);
    }
    return isValidEthereumAddress(address);
}

QString normalizedCryptoAddress(const CryptoSpec& spec, const QString& address)
{
    if (spec.network == QStringLiteral("ETHEREUM")) {
        return normalizeEthereumAddress(address);
    }
    if (spec.network == QStringLiteral("BITCOIN")) {
        return normalizeBitcoinAddress(address);
    }
    return address.trimmed();
}

QString cryptoNetworkLabel(const CryptoWallet& wallet)
{
    if (wallet.network() == QStringLiteral("TRON")) {
        return QStringLiteral("TRC-20");
    }
    if (wallet.network() == QStringLiteral("BITCOIN")) {
        return QStringLiteral("Bitcoin");
    }
    return QStringLiteral("Ethereum");
}

QString formatCryptoAmount(const qint64 balanceAtomic, const int decimals)
{
    qint64 scale = 1;
    for (int index = 0; index < decimals; ++index) {
        scale *= 10;
    }
    const qint64 whole = balanceAtomic / scale;
    const qint64 fraction = balanceAtomic % scale;
    if (fraction == 0) {
        return QString::number(whole);
    }

    QString fractionText = QStringLiteral("%1").arg(
        fraction, decimals, 10, QLatin1Char('0'));
    while (fractionText.endsWith(QLatin1Char('0'))) {
        fractionText.chop(1);
    }
    return QString::number(whole) + QLatin1Char('.') + fractionText;
}

bool parsePositiveMicros(const QString& text, qint64& result)
{
    QString normalized = text.trimmed();
    normalized.remove(QLatin1Char(' '));
    normalized.remove(QChar(0x00A0));
    normalized.replace(QLatin1Char(','), QLatin1Char('.'));
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9]+(?:\\.[0-9]{1,6})?$"));
    if (!pattern.match(normalized).hasMatch()) {
        return false;
    }

    const QStringList parts = normalized.split(QLatin1Char('.'));
    bool ok = false;
    const qint64 whole = parts.constFirst().toLongLong(&ok);
    if (!ok) {
        return false;
    }
    QString fraction = parts.size() == 2 ? parts.at(1) : QString();
    fraction = fraction.leftJustified(6, QLatin1Char('0'));
    const qint64 fractional = fraction.isEmpty() ? 0 : fraction.toLongLong(&ok);
    if (!ok || whole > (std::numeric_limits<qint64>::max() - fractional) /
            InvestmentPosition::Scale) {
        return false;
    }
    result = whole * InvestmentPosition::Scale + fractional;
    return result > 0;
}

QString formatMicros(const qint64 value, const int maximumFractionDigits = 6)
{
    const qint64 whole = value / InvestmentPosition::Scale;
    const qint64 fraction = value % InvestmentPosition::Scale;
    if (fraction == 0 || maximumFractionDigits <= 0) {
        return QString::number(whole);
    }
    QString fractionText = QStringLiteral("%1").arg(
        fraction, 6, 10, QLatin1Char('0'));
    fractionText.truncate(maximumFractionDigits);
    while (fractionText.endsWith(QLatin1Char('0'))) {
        fractionText.chop(1);
    }
    return fractionText.isEmpty()
        ? QString::number(whole)
        : QString::number(whole) + QLatin1Char(',') + fractionText;
}

QString investmentTypeName(const InvestmentInstrumentType type)
{
    switch (type) {
    case InvestmentInstrumentType::Stock:
        return QCoreApplication::translate("FinanceController", "Акция");
    case InvestmentInstrumentType::Etf:
    case InvestmentInstrumentType::Fund:
        return QCoreApplication::translate("FinanceController", "Фонд");
    case InvestmentInstrumentType::Bond:
        return QCoreApplication::translate("FinanceController", "Облигация");
    case InvestmentInstrumentType::Other:
        return QCoreApplication::translate("FinanceController", "Другой инструмент");
    }
    return {};
}
}

FinanceController::FinanceController(QObject* parent)
    : QObject(parent)
    , currencyConverter_(rateProvider_)
    , balanceCalculator_(currencyConverter_)
{
    QObject::connect(
        &rateProvider_,
        &CbrCurrencyRateProvider::ratesUpdated,
        this,
        [this]()
        {
            emit balanceChanged();
            emit transactionsChanged();
            emit currencyRatesChanged();
            emit cryptoWalletsChanged();
        });

    QObject::connect(
        &cryptoProvider_,
        &CryptoProvider::balanceUpdated,
        this,
        [this](
            const QString& walletId,
            const qint64 balanceAtomic,
            const QDateTime& fetchedAtUtc
            )
        {
            finishCryptoWalletRequest(walletId);
            for (CryptoWallet& wallet : cryptoWallets_) {
                if (wallet.id() != walletId) {
                    continue;
                }
                if (!repository_.updateCryptoWalletBalance(
                        walletId, balanceAtomic, fetchedAtUtc)) {
                    qWarning() << "Failed to save crypto-wallet balance:"
                               << repository_.lastError();
                    return;
                }
                wallet.setBalance(balanceAtomic, fetchedAtUtc);
                emit cryptoWalletsChanged();
                emit balanceChanged();
                return;
            }
        });
    QObject::connect(
        &cryptoProvider_,
        &CryptoProvider::transactionsUpdated,
        this,
        [this](
            const QString& walletId,
            const QVector<CryptoTransaction>& transactions,
            const QDateTime& fetchedAtUtc
            )
        {
            finishCryptoWalletRequest(walletId);
            if (!repository_.replaceCryptoTransactions(
                    walletId, transactions, fetchedAtUtc)) {
                qWarning() << "Failed to save crypto transaction history:"
                           << repository_.lastError();
                return;
            }

            cryptoTransactions_.erase(
                std::remove_if(
                    cryptoTransactions_.begin(),
                    cryptoTransactions_.end(),
                    [&walletId](const CryptoTransaction& transaction)
                    {
                        return transaction.walletId() == walletId;
                    }),
                cryptoTransactions_.end());
            cryptoTransactions_ += transactions;
            for (CryptoWallet& wallet : cryptoWallets_) {
                if (wallet.id() == walletId) {
                    wallet.setHistoryFetchedAt(fetchedAtUtc);
                    break;
                }
            }
            emit cryptoTransactionsChanged();
            emit cryptoWalletsChanged();
        });
    QObject::connect(
        &cryptoProvider_,
        &CryptoProvider::priceUpdated,
        this,
        [this](
            const QString& symbol,
            const qint64 priceUsdMicros,
            const QDateTime& fetchedAtUtc
            )
        {
            if (!repository_.saveCryptoPrice(
                    symbol, priceUsdMicros, fetchedAtUtc)) {
                qWarning() << "Failed to save crypto price:"
                           << repository_.lastError();
                return;
            }
            cryptoPricesUsdMicros_[symbol] = priceUsdMicros;
            cryptoPricesFetchedAtUtc_[symbol] = fetchedAtUtc.toUTC();
            emit cryptoWalletsChanged();
            emit balanceChanged();
        });
    QObject::connect(
        &cryptoProvider_,
        &CryptoProvider::requestFailed,
        this,
        [this](const QString& walletId, const QString& message)
        {
            if (!walletId.isEmpty()) {
                finishCryptoWalletRequest(walletId);
            }
            setCryptoLastError(message);
            emit cryptoWalletsChanged();
        });
    QObject::connect(
        &cryptoProvider_,
        &CryptoProvider::requestFinished,
        this,
        &FinanceController::finishCryptoRequest);

    QObject::connect(
        &investmentProvider_,
        &MoexInvestmentProvider::searchSucceeded,
        this,
        [this](const QVector<InvestmentMarketInstrument>& instruments)
        {
            investmentSearchResults_ = instruments;
            investmentSearchBusy_ = false;
            investmentQuoteBusy_ = false;
            selectedInvestmentSearchIndex_ = -1;
            requestedSearchQuoteId_.clear();
            investmentLastError_ = instruments.isEmpty()
                ? tr("По запросу не найдено акций или фондов")
                : QString();
            emit investmentSearchResultsChanged();
            emit investmentSearchStateChanged();
        });
    QObject::connect(
        &investmentProvider_,
        &MoexInvestmentProvider::searchFailed,
        this,
        [this](const QString& message)
        {
            investmentSearchBusy_ = false;
            setInvestmentLastError(message);
            emit investmentSearchStateChanged();
        });
    QObject::connect(
        &investmentProvider_,
        &MoexInvestmentProvider::quoteSucceeded,
        this,
        [this](
            const QString& instrumentId,
            const qint64 priceMicros,
            const QString& currencyCode,
            const QDateTime& quotedAtUtc
            )
        {
            for (InvestmentMarketInstrument& result : investmentSearchResults_) {
                if (result.id() == instrumentId) {
                    result.setQuote(priceMicros, currencyCode, quotedAtUtc);
                    break;
                }
            }

            for (const InvestmentInstrument& instrument : investmentInstruments_) {
                if (instrument.id() != instrumentId) {
                    continue;
                }
                if (!repository_.saveInvestmentQuote(InvestmentQuote(
                        instrumentId, priceMicros, quotedAtUtc))) {
                    qWarning() << "Failed to save investment quote:"
                               << repository_.lastError();
                } else {
                    investmentQuotes_ = repository_.loadInvestmentQuotes();
                    emit investmentPositionsChanged();
                    emit accountsChanged();
                    emit balanceChanged();
                }
                break;
            }

            if (requestedSearchQuoteId_ == instrumentId) {
                requestedSearchQuoteId_.clear();
                investmentQuoteBusy_ = false;
                investmentLastError_.clear();
                emit investmentSearchResultsChanged();
                emit investmentSearchStateChanged();
            }
            finishInvestmentQuoteRequest(instrumentId);
        });
    QObject::connect(
        &investmentProvider_,
        &MoexInvestmentProvider::quoteFailed,
        this,
        [this](const QString& instrumentId, const QString& message)
        {
            if (requestedSearchQuoteId_ == instrumentId) {
                requestedSearchQuoteId_.clear();
                investmentQuoteBusy_ = false;
                setInvestmentLastError(message);
                emit investmentSearchStateChanged();
            }
            finishInvestmentQuoteRequest(instrumentId);
        });

    cryptoRefreshTimer_.setSingleShot(true);
    QObject::connect(
        &cryptoRefreshTimer_,
        &QTimer::timeout,
        this,
        &FinanceController::refreshCryptoWallets);

    if (!repository_.isOpen()) {
        qWarning() << "Failed to open finance database:"
                   << repository_.lastError();
        return;
    }

    appCurrency_ = currencyFromString(repository_.loadAppCurrency());
    uiLanguage_ = repository_.loadUiLanguage() == QStringLiteral("en")
        ? QStringLiteral("en")
        : QStringLiteral("ru");
    manualUsdToRubRate_ = repository_.loadManualUsdToRubRate();
    manualEurToRubRate_ = repository_.loadManualEurToRubRate();
    if (!rateProvider_.setManualRates(
            manualUsdToRubRate_, manualEurToRubRate_)) {
        qWarning() << "Failed to apply stored manual currency rates";
    }
    automaticCurrencyRates_ = repository_.loadAutomaticCurrencyRates();
    rateProvider_.setAutomaticUpdatesEnabled(automaticCurrencyRates_);
    selectedAsset_ = assetTypeFromString(repository_.loadSelectedAsset());
    transactions_ = repository_.loadTransactions();
    categories_ = repository_.loadCategories();
    accounts_ = repository_.loadAccounts();
    cryptoWallets_ = repository_.loadCryptoWallets();
    cryptoTransactions_ = repository_.loadCryptoTransactions();
    investmentInstruments_ = repository_.loadInvestmentInstruments();
    investmentPositions_ = repository_.loadInvestmentPositions();
    investmentQuotes_ = repository_.loadInvestmentQuotes();
    if (!cryptoWallets_.isEmpty()) {
        selectedCryptoWalletId_ = cryptoWallets_.constFirst().id();
    }
    for (const QString& symbol : {QStringLiteral("USDT"),
                                  QStringLiteral("BTC"),
                                  QStringLiteral("ETH")}) {
        const FinanceRepository::CryptoPriceSnapshot price =
            repository_.loadCryptoPrice(symbol);
        if (price.priceUsdMicros > 0) {
            cryptoPricesUsdMicros_.insert(symbol, price.priceUsdMicros);
        }
        if (price.fetchedAtUtc.isValid()) {
            cryptoPricesFetchedAtUtc_.insert(symbol, price.fetchedAtUtc);
        }
    }
    if (!cryptoPricesUsdMicros_.contains(QStringLiteral("USDT"))) {
        cryptoPricesUsdMicros_.insert(QStringLiteral("USDT"), 1'000'000);
    }
    lastCryptoRefreshAttemptUtc_ = repository_.loadCryptoRefreshAttemptUtc();
    archivedCategoryIds_ = repository_.loadArchivedCategoryIds();
    summary_ = repository_.loadSummary();

    QTimer::singleShot(
        0,
        this,
        &FinanceController::scheduleCapitalSnapshot);
    QTimer::singleShot(
        0,
        this,
        &FinanceController::scheduleInitialCryptoRefresh);
    if (selectedAsset_ == AssetType::Investment) {
        QTimer::singleShot(0, this, &FinanceController::refreshInvestmentQuotes);
    }
}

qint64 FinanceController::balanceMinorUnits() const
{
    using Int128 = __int128_t;
    const Int128 total =
        static_cast<Int128>(assetBalanceMinor(AssetType::Fiat)) +
        static_cast<Int128>(assetBalanceMinor(AssetType::Crypto)) +
        static_cast<Int128>(assetBalanceMinor(AssetType::Investment));
    if (total > std::numeric_limits<qint64>::max()) {
        return std::numeric_limits<qint64>::max();
    }
    if (total < std::numeric_limits<qint64>::min()) {
        return std::numeric_limits<qint64>::min();
    }
    return static_cast<qint64>(total);
}

QString FinanceController::balanceCurrency() const
{
    return currencyCode(appCurrency_);
}

qint64 FinanceController::incomeMinorUnits() const
{
    return convertedTotal(dateFilteredSummary().income);
}

qint64 FinanceController::expenseMinorUnits() const
{
    return convertedTotal(dateFilteredSummary().expense);
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

    if (repository_.isOpen() &&
        !repository_.saveAppCurrency(currencyCode(newCurrency))) {
        qWarning() << "Failed to save application currency:"
                   << repository_.lastError();
    }

    emit appCurrencyChanged();
    emit balanceChanged();
    emit transactionsChanged();
    emit currencyRatesChanged();
    emit cryptoWalletsChanged();
}

QString FinanceController::uiLanguage() const
{
    return uiLanguage_;
}

void FinanceController::setUiLanguage(const QString& language)
{
    const QString normalizedLanguage = language.trimmed().toLower() == QStringLiteral("en")
        ? QStringLiteral("en")
        : QStringLiteral("ru");
    if (normalizedLanguage == uiLanguage_) {
        return;
    }

    uiLanguage_ = normalizedLanguage;
    if (repository_.isOpen() && !repository_.saveUiLanguage(uiLanguage_)) {
        qWarning() << "Failed to save UI language:" << repository_.lastError();
    }
    emit uiLanguageChanged();
}

void FinanceController::retranslate()
{
    emit categoriesChanged();
    emit accountsChanged();
    emit transactionsChanged();
}

bool FinanceController::automaticCurrencyRates() const
{
    return automaticCurrencyRates_;
}

void FinanceController::setAutomaticCurrencyRates(const bool enabled)
{
    if (enabled == automaticCurrencyRates_) {
        return;
    }
    if (repository_.isOpen() &&
        !repository_.saveAutomaticCurrencyRates(enabled)) {
        qWarning() << "Failed to save automatic currency-rate setting:"
                   << repository_.lastError();
        return;
    }

    automaticCurrencyRates_ = enabled;
    rateProvider_.setAutomaticUpdatesEnabled(enabled);
    emit automaticCurrencyRatesChanged();
}

double FinanceController::manualUsdToRubRate() const
{
    return manualUsdToRubRate_;
}

double FinanceController::manualEurToRubRate() const
{
    return manualEurToRubRate_;
}

QVariantList FinanceController::currentCurrencyRates() const
{
    QVariantList result;
    const qint64 targetRate = rateProvider_.rateToUsd(appCurrency_);
    if (targetRate <= 0) {
        return result;
    }

    for (const Currency currency : {Currency::RUB, Currency::USD, Currency::EUR}) {
        QVariantMap item;
        item[QStringLiteral("code")] = currencyCode(currency);
        item[QStringLiteral("rate")] =
            static_cast<double>(rateProvider_.rateToUsd(currency)) /
            static_cast<double>(targetRate);
        result.append(item);
    }
    return result;
}

bool FinanceController::saveManualCurrencyRates(
    const double rublesPerUsd,
    const double rublesPerEur
    )
{
    if (!rateProvider_.setManualRates(rublesPerUsd, rublesPerEur)) {
        return false;
    }
    if (repository_.isOpen() &&
        !repository_.saveManualCurrencyRates(rublesPerUsd, rublesPerEur)) {
        qWarning() << "Failed to save manual currency rates:"
                   << repository_.lastError();
        rateProvider_.setManualRates(
            manualUsdToRubRate_, manualEurToRubRate_);
        return false;
    }

    manualUsdToRubRate_ = rublesPerUsd;
    manualEurToRubRate_ = rublesPerEur;
    emit manualCurrencyRatesChanged();
    return true;
}

QVariantMap FinanceController::exportTransactionsCsv(const QUrl& fileUrl) const
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty()) {
        result["error"] = tr("Не выбран файл для экспорта");
        return result;
    }
    if (!filePath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".csv");
    }

    const QStringList header{
        QStringLiteral("ledgera_csv_version"), QStringLiteral("operation_id"),
        QStringLiteral("date"), QStringLiteral("type"),
        QStringLiteral("source_account_id"), QStringLiteral("source_account_name"),
        QStringLiteral("target_account_id"), QStringLiteral("target_account_name"),
        QStringLiteral("category_id"), QStringLiteral("category_name"),
        QStringLiteral("amount_minor"), QStringLiteral("target_amount_minor"),
        QStringLiteral("currency"), QStringLiteral("target_currency"),
        QStringLiteral("description")};
    QVector<QStringList> rows{header};
    QSet<QString> exportedTransfers;

    const auto accountById = [this](const QString& id) -> const Account* {
        for (const Account& account : accounts_) {
            if (account.id() == id) return &account;
        }
        return nullptr;
    };
    const auto categoryById = [this](const QString& id) -> const Category* {
        for (const Category& category : categories_) {
            if (category.id() == id) return &category;
        }
        return nullptr;
    };

    for (const Transaction& transaction : transactions_) {
        const Account* source = accountById(transaction.accountId());
        if (!source) continue;

        if (isTransfer(transaction)) {
            const QString id = transferId(transaction);
            if (id.isEmpty() || exportedTransfers.contains(id)) continue;
            const Transaction* outgoing = nullptr;
            const Transaction* incoming = nullptr;
            for (const Transaction& candidate : transactions_) {
                if (transferId(candidate) != id) continue;
                if (candidate.categoryId() == QStringLiteral("transfer-out")) {
                    outgoing = &candidate;
                } else if (candidate.categoryId() == QStringLiteral("transfer-in")) {
                    incoming = &candidate;
                }
            }
            if (!outgoing || !incoming) continue;
            const Account* outgoingAccount = accountById(outgoing->accountId());
            const Account* incomingAccount = accountById(incoming->accountId());
            if (!outgoingAccount || !incomingAccount) continue;
            rows.append({QStringLiteral("1"), id,
                         outgoing->date().toUTC().toString(Qt::ISODateWithMs),
                         QStringLiteral("transfer"), outgoingAccount->id(),
                         outgoingAccount->name(), incomingAccount->id(),
                         incomingAccount->name(), QString(), QString(),
                         QString::number(outgoing->money().minorUnits()),
                         QString::number(incoming->money().minorUnits()),
                         currencyCode(outgoingAccount->currency()),
                         currencyCode(incomingAccount->currency()),
                         outgoing->description()});
            exportedTransfers.insert(id);
            continue;
        }

        const Category* category = categoryById(transaction.categoryId());
        rows.append({QStringLiteral("1"), transaction.id(),
                     transaction.date().toUTC().toString(Qt::ISODateWithMs),
                     transaction.type() == TransactionType::Income
                         ? QStringLiteral("income") : QStringLiteral("expense"),
                     source->id(), source->name(), QString(), QString(),
                     transaction.categoryId(), category ? category->name() : QString(),
                     QString::number(transaction.money().minorUnits()), QString(),
                     currencyCode(source->currency()), QString(),
                     transaction.description()});
    }

    QString error;
    if (!CsvCodec::writeFile(filePath, rows, error)) {
        result["error"] = error;
        return result;
    }
    result["ok"] = true;
    result["count"] = rows.size() - 1;
    result["path"] = QFileInfo(filePath).absoluteFilePath();
    return result;
}

QVariantMap FinanceController::importTransactionsCsv(const QUrl& fileUrl)
{
    QVariantMap result{{QStringLiteral("ok"), false},
                       {QStringLiteral("imported"), 0},
                       {QStringLiteral("skipped"), 0}};
    const QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty()) {
        result["error"] = tr("Не выбран CSV-файл");
        return result;
    }
    const CsvCodec::ReadResult csv = CsvCodec::readFile(filePath);
    if (!csv.error.isEmpty()) {
        result["error"] = csv.error;
        return result;
    }
    const QStringList expectedHeader{
        QStringLiteral("ledgera_csv_version"), QStringLiteral("operation_id"),
        QStringLiteral("date"), QStringLiteral("type"),
        QStringLiteral("source_account_id"), QStringLiteral("source_account_name"),
        QStringLiteral("target_account_id"), QStringLiteral("target_account_name"),
        QStringLiteral("category_id"), QStringLiteral("category_name"),
        QStringLiteral("amount_minor"), QStringLiteral("target_amount_minor"),
        QStringLiteral("currency"), QStringLiteral("target_currency"),
        QStringLiteral("description")};
    if (csv.rows.isEmpty() || csv.rows.first() != expectedHeader) {
        result["error"] = tr("Неверный формат CSV Ledgera");
        return result;
    }

    const auto resolveAccount = [this](
        const QString& id, const QString& name, const QString& currency
        ) -> const Account* {
        for (const Account& account : accounts_) {
            if (account.id() == id && currencyCode(account.currency()) == currency) {
                return &account;
            }
        }
        const Account* match = nullptr;
        for (const Account& account : accounts_) {
            if (account.name().compare(name, Qt::CaseInsensitive) == 0 &&
                currencyCode(account.currency()) == currency) {
                if (match) return nullptr;
                match = &account;
            }
        }
        return match;
    };
    const auto resolveCategory = [this](
        const QString& id, const QString& name, const TransactionType type
        ) -> const Category* {
        for (const Category& category : categories_) {
            if (category.id() == id &&
                ((type == TransactionType::Income && category.type() == CategoryType::Income) ||
                 (type == TransactionType::Expense && category.type() == CategoryType::Expense))) {
                return &category;
            }
        }
        const Category* match = nullptr;
        for (const Category& category : categories_) {
            const bool sameType =
                (type == TransactionType::Income && category.type() == CategoryType::Income) ||
                (type == TransactionType::Expense && category.type() == CategoryType::Expense);
            if (sameType && category.name().compare(name, Qt::CaseInsensitive) == 0) {
                if (match) return nullptr;
                match = &category;
            }
        }
        return match;
    };

    QSet<QString> existingIds;
    for (const Transaction& transaction : transactions_) existingIds.insert(transaction.id());
    QSet<QString> pendingIds;
    QVector<Transaction> importedTransactions;
    int operationCount = 0;
    int skipped = 0;

    for (qsizetype rowIndex = 1; rowIndex < csv.rows.size(); ++rowIndex) {
        const QStringList& row = csv.rows[rowIndex];
        if (row.size() == 1 && row.first().isEmpty()) continue;
        if (row.size() != expectedHeader.size() || row[0] != QStringLiteral("1")) {
            result["error"] = tr("Ошибка в строке %1: неверное число столбцов или версия")
                                  .arg(rowIndex + 1);
            return result;
        }
        QString operationId = row[1].trimmed();
        if (operationId.isEmpty()) {
            operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        const QDateTime date = QDateTime::fromString(row[2], Qt::ISODateWithMs);
        bool amountOk = false;
        const qint64 amount = row[10].toLongLong(&amountOk);
        if (!date.isValid() || !amountOk || amount <= 0) {
            result["error"] = tr("Ошибка в строке %1: неверная дата или сумма")
                                  .arg(rowIndex + 1);
            return result;
        }
        const QString type = row[3].trimmed().toLower();
        const Account* source = resolveAccount(row[4], row[5], row[12]);
        if (!source) {
            result["error"] = tr("Ошибка в строке %1: исходный счёт не найден")
                                  .arg(rowIndex + 1);
            return result;
        }

        if (type == QStringLiteral("transfer")) {
            bool targetAmountOk = false;
            const qint64 targetAmount = row[11].toLongLong(&targetAmountOk);
            const Account* target = resolveAccount(row[6], row[7], row[13]);
            const QString outgoingId = operationId + QStringLiteral("-out");
            const QString incomingId = operationId + QStringLiteral("-in");
            if (existingIds.contains(outgoingId) || existingIds.contains(incomingId)) {
                ++skipped;
                continue;
            }
            if (!target || target == source || !targetAmountOk || targetAmount <= 0 ||
                pendingIds.contains(outgoingId) || pendingIds.contains(incomingId)) {
                result["error"] = tr("Ошибка в строке %1: неверные данные перевода")
                                      .arg(rowIndex + 1);
                return result;
            }
            importedTransactions.append(Transaction(
                outgoingId, source->id(), QStringLiteral("transfer-out"),
                Money(amount, source->currency()), TransactionType::Expense,
                date, row[14]));
            importedTransactions.append(Transaction(
                incomingId, target->id(), QStringLiteral("transfer-in"),
                Money(targetAmount, target->currency()), TransactionType::Income,
                date, row[14]));
            pendingIds.insert(outgoingId);
            pendingIds.insert(incomingId);
            ++operationCount;
            continue;
        }

        if (type != QStringLiteral("income") && type != QStringLiteral("expense")) {
            result["error"] = tr("Ошибка в строке %1: неизвестный тип операции")
                                  .arg(rowIndex + 1);
            return result;
        }
        if (existingIds.contains(operationId)) {
            ++skipped;
            continue;
        }
        if (pendingIds.contains(operationId)) {
            result["error"] = tr("Ошибка в строке %1: повторяющийся идентификатор")
                                  .arg(rowIndex + 1);
            return result;
        }
        const TransactionType transactionType = type == QStringLiteral("income")
            ? TransactionType::Income : TransactionType::Expense;
        const Category* category = resolveCategory(row[8], row[9], transactionType);
        if (!category) {
            result["error"] = tr("Ошибка в строке %1: категория не найдена")
                                  .arg(rowIndex + 1);
            return result;
        }
        importedTransactions.append(Transaction(
            operationId, source->id(), category->id(),
            Money(amount, source->currency()), transactionType, date, row[14]));
        pendingIds.insert(operationId);
        ++operationCount;
    }

    if (!repository_.insertTransactions(importedTransactions)) {
        result["error"] = repository_.lastError();
        return result;
    }
    transactions_ = repository_.loadTransactions();
    summary_ = repository_.loadSummary();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    result["ok"] = true;
    result["imported"] = operationCount;
    result["skipped"] = skipped;
    return result;
}

QVariantList FinanceController::transactions() const
{
    QVariantList result;

    for (const Transaction& transaction : dateFilteredTransactions()) {
        QVariantMap item;

        item["id"] = transaction.id();
        item["accountId"] = transaction.accountId();
        item["categoryId"] = transaction.categoryId();
        item["categoryName"] = categoryName(transaction.categoryId());

        item["amount"] = transaction.money().minorUnits();
        item["displayAmount"] = currencyConverter_.convert(
            transaction.money(), appCurrency_).minorUnits();

        item["currency"] = currencyCode(
            transaction.money().currency()
            );

        const bool transfer = isTransfer(transaction);
        item["type"] = transfer
            ? QStringLiteral("transfer")
            : transaction.type() == TransactionType::Income
                ? QStringLiteral("income")
                : QStringLiteral("expense");
        item["direction"] = transaction.categoryId() == QStringLiteral("transfer-in")
            ? QStringLiteral("in")
            : transaction.categoryId() == QStringLiteral("transfer-out")
                ? QStringLiteral("out")
                : QString();

        item["date"] = transaction.date().toString(
            Qt::ISODate
            );

        item["description"] = transactionDisplayDescription(transaction);
        item["rawDescription"] = transaction.description();

        result.append(item);
    }

    return result;
}

QVariantList FinanceController::categories() const
{
    QVariantList result;
    result.reserve(categories_.size());

    for (const Category& category : categories_) {
        if (archivedCategoryIds_.contains(category.id())) {
            continue;
        }

        QVariantMap item;
        item["label"] = categoryDisplayName(category);
        item["value"] = category.id();
        item["type"] = category.type() == CategoryType::Income
            ? QStringLiteral("income")
            : QStringLiteral("expense");
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::accounts() const
{
    QVariantList result;
    for (const Account& account : accounts_) {
        if (account.assetType() != selectedAsset_) {
            continue;
        }

        QVariantMap item;
        item["id"] = account.id();
        item["name"] = accountDisplayName(account);
        item["rawName"] = account.name();
        item["type"] = accountTypeToString(account.type());
        item["asset"] = assetTypeToString(account.assetType());
        item["currency"] = currencyCode(account.currency());
        item["initialBalanceMinor"] = account.initialBalanceMinor();
        const qint64 balanceMinor = accountBalanceMinor(account);
        item["balanceMinor"] = balanceMinor;
        addAccountFinancialRoles(item, account, balanceMinor);
        item["transactionCount"] = accountTransactionCount(account.id());
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::allAccounts() const
{
    QVariantList result;
    for (const Account& account : accounts_) {
        QVariantMap item;
        item["id"] = account.id();
        item["name"] = accountDisplayName(account);
        item["rawName"] = account.name();
        item["asset"] = assetTypeToString(account.assetType());
        item["assetTitle"] = account.assetType() == AssetType::Fiat
            ? tr("Фиат")
            : account.assetType() == AssetType::Crypto
                ? tr("Крипта")
                : tr("Инвестиции");
        item["currency"] = currencyCode(account.currency());
        item["displayName"] = item["assetTitle"].toString()
            + QStringLiteral(" · ") + accountDisplayName(account)
            + QStringLiteral(" · ") + currencyCode(account.currency());
        const qint64 balanceMinor = accountBalanceMinor(account);
        item["balanceMinor"] = balanceMinor;
        item["initialBalanceMinor"] = account.initialBalanceMinor();
        addAccountFinancialRoles(item, account, balanceMinor);
        item["transactionCount"] = accountTransactionCount(account.id());
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::assetSummaries() const
{
    QVariantList result;
    for (const AssetType asset : {AssetType::Fiat,
                                  AssetType::Crypto,
                                  AssetType::Investment}) {
        QVariantMap item;
        item["code"] = assetTypeToString(asset);
        item["balanceMinor"] = assetBalanceMinor(asset);
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::cryptoWallets() const
{
    QVariantList result;
    result.reserve(cryptoWallets_.size());
    for (const CryptoWallet& wallet : cryptoWallets_) {
        QVariantMap item;
        item[QStringLiteral("id")] = wallet.id();
        item[QStringLiteral("name")] = wallet.symbol();
        item[QStringLiteral("type")] = QStringLiteral("crypto_wallet");
        item[QStringLiteral("asset")] = QStringLiteral("crypto");
        item[QStringLiteral("isCrypto")] = true;
        item[QStringLiteral("isCreditCard")] = false;
        item[QStringLiteral("symbol")] = wallet.symbol();
        item[QStringLiteral("network")] = cryptoNetworkLabel(wallet);
        item[QStringLiteral("address")] = wallet.address();
        item[QStringLiteral("balanceAtomic")] = wallet.balanceAtomic();
        item[QStringLiteral("balanceText")] = formatCryptoAmount(
            wallet.balanceAtomic(), wallet.decimals());
        item[QStringLiteral("valueMinor")] = cryptoWalletValueMinor(wallet);
        item[QStringLiteral("balanceMinor")] = item[QStringLiteral("valueMinor")];
        item[QStringLiteral("currency")] = currencyCode(appCurrency_);
        item[QStringLiteral("hasSnapshot")] =
            wallet.balanceFetchedAtUtc().isValid();
        item[QStringLiteral("updatedAt")] = wallet.balanceFetchedAtUtc();
        item[QStringLiteral("refreshing")] =
            refreshingCryptoWalletIds_.contains(wallet.id());
        item[QStringLiteral("priceUsd")] =
            static_cast<double>(cryptoPricesUsdMicros_.value(wallet.symbol())) /
            1'000'000.0;
        item[QStringLiteral("priceHasSnapshot")] =
            cryptoPricesFetchedAtUtc_.value(wallet.symbol()).isValid();
        result.append(item);
    }
    return result;
}

bool FinanceController::cryptoRefreshing() const
{
    return cryptoRefreshing_;
}

QString FinanceController::cryptoLastError() const
{
    return cryptoLastError_;
}

QString FinanceController::selectedCryptoWalletId() const
{
    return selectedCryptoWalletId_;
}

void FinanceController::setSelectedCryptoWalletId(const QString& walletId)
{
    if (walletId == selectedCryptoWalletId_) {
        return;
    }
    if (!walletId.isEmpty()) {
        const bool exists = std::any_of(
            cryptoWallets_.cbegin(),
            cryptoWallets_.cend(),
            [&walletId](const CryptoWallet& wallet)
            {
                return wallet.id() == walletId;
            });
        if (!exists) {
            return;
        }
    }
    selectedCryptoWalletId_ = walletId;
    emit selectedCryptoWalletIdChanged();
    emit cryptoTransactionsChanged();
}

QVariantList FinanceController::cryptoTransactions() const
{
    QVariantList result;
    QVector<const CryptoTransaction*> visibleTransactions;
    visibleTransactions.reserve(cryptoTransactions_.size());
    for (const CryptoTransaction& transaction : cryptoTransactions_) {
        if (!selectedCryptoWalletId_.isEmpty() &&
            transaction.walletId() != selectedCryptoWalletId_) {
            continue;
        }
        visibleTransactions.append(&transaction);
    }
    std::sort(
        visibleTransactions.begin(),
        visibleTransactions.end(),
        [](const CryptoTransaction* left, const CryptoTransaction* right)
        {
            return left->occurredAtUtc() > right->occurredAtUtc();
        });

    result.reserve(visibleTransactions.size());
    for (const CryptoTransaction* transactionPointer : visibleTransactions) {
        const CryptoTransaction& transaction = *transactionPointer;
        const auto wallet = std::find_if(
            cryptoWallets_.cbegin(),
            cryptoWallets_.cend(),
            [&transaction](const CryptoWallet& candidate)
            {
                return candidate.id() == transaction.walletId();
            });
        if (wallet == cryptoWallets_.cend()) {
            continue;
        }

        const bool outgoing = transaction.fromAddress() == wallet->address();
        QVariantMap item;
        item[QStringLiteral("walletId")] = transaction.walletId();
        item[QStringLiteral("walletAddress")] = wallet->address();
        item[QStringLiteral("transactionId")] = transaction.transactionId();
        item[QStringLiteral("direction")] = outgoing
            ? QStringLiteral("out")
            : QStringLiteral("in");
        QString counterparty = outgoing
            ? transaction.toAddress()
            : transaction.fromAddress();
        if (counterparty == QStringLiteral("contract creation")) {
            counterparty = tr("Создание контракта");
        }
        item[QStringLiteral("counterparty")] = counterparty;
        item[QStringLiteral("amountAtomic")] = transaction.amountAtomic();
        item[QStringLiteral("amountText")] = formatCryptoAmount(
            transaction.amountAtomic(), wallet->decimals());
        item[QStringLiteral("symbol")] = wallet->symbol();
        item[QStringLiteral("occurredAt")] = transaction.occurredAtUtc();
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::investmentAccounts() const
{
    QVariantList result;
    for (const Account& account : accounts_) {
        if (account.assetType() != AssetType::Investment) {
            continue;
        }
        QVariantMap item;
        item[QStringLiteral("id")] = account.id();
        item[QStringLiteral("name")] = accountDisplayName(account);
        item[QStringLiteral("currency")] = currencyCode(account.currency());
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::investmentPositions() const
{
    QVariantList result;
    for (const InvestmentPosition& position : investmentPositions_) {
        if (!selectedAccountId_.isEmpty() &&
            position.accountId() != selectedAccountId_) {
            continue;
        }
        const auto instrument = std::find_if(
            investmentInstruments_.cbegin(), investmentInstruments_.cend(),
            [&position](const InvestmentInstrument& candidate) {
                return candidate.id() == position.instrumentId();
            });
        const auto account = std::find_if(
            accounts_.cbegin(), accounts_.cend(),
            [&position](const Account& candidate) {
                return candidate.id() == position.accountId();
            });
        if (instrument == investmentInstruments_.cend() ||
            account == accounts_.cend()) {
            continue;
        }
        const auto quote = std::find_if(
            investmentQuotes_.cbegin(), investmentQuotes_.cend(),
            [&position](const InvestmentQuote& candidate) {
                return candidate.instrumentId() == position.instrumentId();
            });

        const bool hasQuote = quote != investmentQuotes_.cend();
        const long double value = hasQuote
            ? static_cast<long double>(position.quantityMicros()) *
                static_cast<long double>(quote->priceMicros()) /
                1'000'000'000'000.0L
            : 0.0L;
        QVariantMap item;
        item[QStringLiteral("id")] = position.id();
        item[QStringLiteral("accountId")] = account->id();
        item[QStringLiteral("accountName")] = accountDisplayName(*account);
        item[QStringLiteral("instrumentId")] = instrument->id();
        item[QStringLiteral("symbol")] = instrument->symbol();
        item[QStringLiteral("isin")] = instrument->isin();
        item[QStringLiteral("name")] = instrument->name();
        item[QStringLiteral("typeName")] = investmentTypeName(instrument->type());
        item[QStringLiteral("currency")] = currencyCode(instrument->currency());
        item[QStringLiteral("quantityText")] = formatMicros(position.quantityMicros());
        item[QStringLiteral("averagePriceText")] =
            formatMicros(position.averagePriceMicros(), 2);
        item[QStringLiteral("averageValueMinor")] = scaledInvestmentValueMinor(
            position.quantityMicros(), position.averagePriceMicros());
        item[QStringLiteral("createdAt")] = position.createdAtUtc();
        item[QStringLiteral("hasQuote")] = hasQuote;
        item[QStringLiteral("priceText")] = hasQuote
            ? formatMicros(quote->priceMicros(), 2) : QString();
        item[QStringLiteral("marketValueText")] = hasQuote
            ? QLocale().toString(static_cast<double>(value), 'f', 2) : QString();
        item[QStringLiteral("quotedAt")] = hasQuote
            ? quote->quotedAtUtc() : QDateTime();
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::investmentSearchResults() const
{
    QVariantList result;
    for (int index = 0; index < investmentSearchResults_.size(); ++index) {
        const InvestmentMarketInstrument& instrument =
            investmentSearchResults_.at(index);
        QVariantMap item;
        item[QStringLiteral("index")] = index;
        item[QStringLiteral("id")] = instrument.id();
        item[QStringLiteral("symbol")] = instrument.symbol();
        item[QStringLiteral("isin")] = instrument.isin();
        item[QStringLiteral("name")] = instrument.name();
        item[QStringLiteral("typeName")] = investmentTypeName(instrument.type());
        item[QStringLiteral("boardId")] = instrument.primaryBoardId();
        item[QStringLiteral("hasPrice")] = instrument.priceMicros() > 0;
        item[QStringLiteral("priceText")] = instrument.priceMicros() > 0
            ? formatMicros(instrument.priceMicros(), 2) : QString();
        item[QStringLiteral("currency")] = instrument.currencyCode();
        item[QStringLiteral("selected")] =
            index == selectedInvestmentSearchIndex_;
        result.append(item);
    }
    return result;
}

bool FinanceController::investmentSearchBusy() const
{
    return investmentSearchBusy_;
}

bool FinanceController::investmentQuoteBusy() const
{
    return investmentQuoteBusy_;
}

bool FinanceController::investmentRefreshing() const
{
    return investmentRefreshing_;
}

QString FinanceController::investmentLastError() const
{
    return investmentLastError_;
}

int FinanceController::selectedInvestmentSearchIndex() const
{
    return selectedInvestmentSearchIndex_;
}

QString FinanceController::selectedAsset() const
{
    return assetTypeToString(selectedAsset_);
}

void FinanceController::setSelectedAsset(const QString& asset)
{
    const AssetType newAsset = assetTypeFromString(asset);
    if (newAsset == selectedAsset_) {
        return;
    }

    selectedAsset_ = newAsset;
    selectedAccountId_.clear();
    if (selectedAsset_ == AssetType::Crypto &&
        selectedCryptoWalletId_.isEmpty() && !cryptoWallets_.isEmpty()) {
        selectedCryptoWalletId_ = cryptoWallets_.constFirst().id();
        emit selectedCryptoWalletIdChanged();
        emit cryptoTransactionsChanged();
    }
    if (!repository_.saveSelectedAsset(assetTypeToString(newAsset))) {
        qWarning() << "Failed to save selected asset:"
                   << repository_.lastError();
    }

    emit selectedAssetChanged();
    emit selectedAccountIdChanged();
    emit accountsChanged();
    if (selectedAsset_ == AssetType::Crypto) {
        scheduleInitialCryptoRefresh();
    } else if (selectedAsset_ == AssetType::Investment) {
        refreshInvestmentQuotes();
    }
}

QString FinanceController::selectedAccountId() const
{
    return selectedAccountId_;
}

void FinanceController::setSelectedAccountId(const QString& accountId)
{
    if (accountId == selectedAccountId_) {
        return;
    }

    if (!accountId.isEmpty()) {
        bool belongsToSelectedAsset = false;
        for (const Account& account : accounts_) {
            if (account.id() == accountId && account.assetType() == selectedAsset_) {
                belongsToSelectedAsset = true;
                break;
            }
        }
        if (!belongsToSelectedAsset) {
            return;
        }
    }

    selectedAccountId_ = accountId;
    emit selectedAccountIdChanged();
    if (selectedAsset_ == AssetType::Investment) {
        emit investmentPositionsChanged();
    }
}

bool FinanceController::dateFilterActive() const
{
    return dateFilterFrom_.isValid() && dateFilterTo_.isValid();
}

QString FinanceController::dateFilterFrom() const
{
    return dateFilterFrom_.toString(Qt::ISODate);
}

QString FinanceController::dateFilterTo() const
{
    return dateFilterTo_.toString(Qt::ISODate);
}

bool FinanceController::setDateFilter(
    const QDateTime& from,
    const QDateTime& to
    )
{
    if (!from.isValid() || !to.isValid()) {
        return false;
    }

    const QDate newFrom = from.toLocalTime().date();
    const QDate newTo = to.toLocalTime().date();
    if (newFrom > newTo) {
        return false;
    }
    if (newFrom == dateFilterFrom_ && newTo == dateFilterTo_) {
        return true;
    }

    dateFilterFrom_ = newFrom;
    dateFilterTo_ = newTo;
    emit dateFilterChanged();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    return true;
}

void FinanceController::clearDateFilter()
{
    if (!dateFilterActive()) {
        return;
    }

    dateFilterFrom_ = {};
    dateFilterTo_ = {};
    emit dateFilterChanged();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
}

bool FinanceController::addAccount(
    const QString& name,
    const QString& type,
    const QString& currency,
    const qint64 initialBalanceMinor,
    const qint64 creditLimitMinor
    )
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 60) {
        return false;
    }

    for (const Account& existing : accounts_) {
        if (existing.assetType() == selectedAsset_ &&
            (existing.name().compare(normalizedName, Qt::CaseInsensitive) == 0 ||
             accountDisplayName(existing).compare(
                 normalizedName, Qt::CaseInsensitive) == 0)) {
            return false;
        }
    }

    const AccountType accountType = accountTypeFromString(type);
    const bool validType =
        (selectedAsset_ == AssetType::Fiat &&
         static_cast<int>(accountType) <= static_cast<int>(AccountType::Other)) ||
        (selectedAsset_ == AssetType::Crypto &&
         (accountType == AccountType::CryptoWallet ||
          accountType == AccountType::Other)) ||
        (selectedAsset_ == AssetType::Investment &&
         (accountType == AccountType::Brokerage ||
          accountType == AccountType::Deposit ||
          accountType == AccountType::Other));
    if (!validType) {
        return false;
    }

    const bool creditCard = accountType == AccountType::CreditCard;
    if ((creditCard && (initialBalanceMinor > 0 || creditLimitMinor <= 0)) ||
        creditLimitMinor < 0) {
        return false;
    }
    const qint64 resolvedCreditLimitMinor = creditCard ? creditLimitMinor : 0;

    const Currency accountCurrency = currencyFromString(currency);
    const Account account(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        normalizedName,
        selectedAsset_,
        accountType,
        accountCurrency,
        initialBalanceMinor,
        resolvedCreditLimitMinor);

    if (!repository_.insertAccount(account)) {
        qWarning() << "Failed to save account:" << repository_.lastError();
        return false;
    }

    accounts_.append(account);
    summary_.balance[currencyIndex(accountCurrency)] += initialBalanceMinor;
    emit accountsChanged();
    emit balanceChanged();
    return true;
}

bool FinanceController::updateAccount(
    const QString& id,
    const QString& name,
    const QString& type,
    const QString& currency,
    const qint64 initialBalanceMinor,
    const qint64 creditLimitMinor
    )
{
    const QString normalizedName = name.trimmed();
    if (id.isEmpty() || normalizedName.isEmpty() || normalizedName.size() > 60) {
        return false;
    }

    int accountIndex = -1;
    for (int index = 0; index < accounts_.size(); ++index) {
        if (accounts_[index].id() == id) {
            accountIndex = index;
            break;
        }
    }
    if (accountIndex < 0) {
        return false;
    }

    const Account& original = accounts_[accountIndex];
    const QString storedName = normalizedName == accountDisplayName(original)
        ? original.name()
        : normalizedName;
    for (const Account& existing : accounts_) {
        if (existing.id() != id &&
            existing.assetType() == original.assetType() &&
            (existing.name().compare(storedName, Qt::CaseInsensitive) == 0 ||
             accountDisplayName(existing).compare(
                 normalizedName, Qt::CaseInsensitive) == 0)) {
            return false;
        }
    }

    const AccountType accountType = accountTypeFromString(type);
    const bool validType =
        (original.assetType() == AssetType::Fiat &&
         static_cast<int>(accountType) <= static_cast<int>(AccountType::Other)) ||
        (original.assetType() == AssetType::Crypto &&
         (accountType == AccountType::CryptoWallet ||
          accountType == AccountType::Other)) ||
        (original.assetType() == AssetType::Investment &&
         (accountType == AccountType::Brokerage ||
          accountType == AccountType::Deposit ||
          accountType == AccountType::Other));
    if (!validType) {
        return false;
    }

    const bool creditCard = accountType == AccountType::CreditCard;
    if ((creditCard && (initialBalanceMinor > 0 || creditLimitMinor <= 0)) ||
        creditLimitMinor < 0) {
        return false;
    }
    const qint64 resolvedCreditLimitMinor = creditCard ? creditLimitMinor : 0;

    const Currency accountCurrency = currencyFromString(currency);
    if (accountCurrency != original.currency() &&
        accountTransactionCount(id) > 0) {
        return false;
    }

    const Account updated(
        original.id(),
        storedName,
        original.assetType(),
        accountType,
        accountCurrency,
        initialBalanceMinor,
        resolvedCreditLimitMinor);
    if (!repository_.isOpen() || !repository_.updateAccount(updated)) {
        qWarning() << "Failed to update account:" << repository_.lastError();
        return false;
    }

    accounts_[accountIndex] = updated;
    summary_ = repository_.loadSummary();
    emit accountsChanged();
    emit balanceChanged();
    emit transactionsChanged();
    return true;
}

bool FinanceController::deleteAccount(const QString& id)
{
    int accountIndex = -1;
    for (int index = 0; index < accounts_.size(); ++index) {
        if (accounts_[index].id() == id) {
            accountIndex = index;
            break;
        }
    }
    if (accountIndex < 0) {
        return false;
    }
    if (!repository_.isOpen() || !repository_.deleteAccount(id)) {
        qWarning() << "Failed to delete account:" << repository_.lastError();
        return false;
    }

    accounts_.removeAt(accountIndex);
    transactions_ = repository_.loadTransactions();
    investmentPositions_ = repository_.loadInvestmentPositions();
    summary_ = repository_.loadSummary();

    if (selectedAccountId_ == id) {
        selectedAccountId_.clear();
        emit selectedAccountIdChanged();
    }
    emit accountsChanged();
    emit investmentPositionsChanged();
    emit transactionsChanged();
    emit balanceChanged();
    return true;
}

QVariantMap FinanceController::addCryptoWallet(
    const QString& symbol,
    const QString& address
    )
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    CryptoSpec spec;
    if (selectedAsset_ != AssetType::Crypto) {
        result[QStringLiteral("error")] = tr(
            "Сначала выберите актив «Крипта»");
        return result;
    }
    if (!cryptoSpec(symbol, spec)) {
        result[QStringLiteral("error")] = tr(
            "Выберите поддерживаемую криптовалюту");
        return result;
    }
    const QString normalizedAddress = normalizedCryptoAddress(spec, address);
    if (!validCryptoAddress(spec, normalizedAddress)) {
        result[QStringLiteral("error")] = spec.symbol == QStringLiteral("USDT")
            ? tr("Введите корректный публичный адрес TRON, начинающийся с T")
            : spec.symbol == QStringLiteral("BTC")
                ? tr("Введите корректный адрес Bitcoin Mainnet")
                : tr("Введите корректный адрес Ethereum Mainnet, начинающийся с 0x");
        return result;
    }

    for (const CryptoWallet& wallet : cryptoWallets_) {
        if (wallet.network() == spec.network &&
            wallet.address() == normalizedAddress) {
            result[QStringLiteral("error")] = tr(
                "Этот кошелёк уже добавлен");
            return result;
        }
    }

    const CryptoWallet wallet(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        normalizedAddress,
        spec.network,
        spec.symbol,
        spec.decimals);
    if (!repository_.insertCryptoWallet(wallet)) {
        qWarning() << "Failed to save crypto wallet:"
                   << repository_.lastError();
        result[QStringLiteral("error")] = tr(
            "Не удалось сохранить криптокошелёк");
        return result;
    }

    cryptoWallets_.append(wallet);
    if (selectedCryptoWalletId_.isEmpty()) {
        selectedCryptoWalletId_ = wallet.id();
        emit selectedCryptoWalletIdChanged();
        emit cryptoTransactionsChanged();
    }
    setCryptoLastError(QString());
    emit cryptoWalletsChanged();
    lastCryptoRefreshAttemptUtc_ = QDateTime::currentDateTimeUtc();
    if (!repository_.saveCryptoRefreshAttemptUtc(
            lastCryptoRefreshAttemptUtc_)) {
        qWarning() << "Failed to save crypto refresh time:"
                   << repository_.lastError();
    }
    startCryptoBalanceRequest(cryptoWallets_.constLast());
    startCryptoTransactionRequest(cryptoWallets_.constLast());

    const QDateTime priceFetchedAtUtc =
        cryptoPricesFetchedAtUtc_.value(spec.symbol);
    const qint64 priceAgeMs = priceFetchedAtUtc.isValid()
        ? priceFetchedAtUtc.msecsTo(QDateTime::currentDateTimeUtc())
        : kCryptoRefreshIntervalMs;
    if (priceAgeMs < 0 || priceAgeMs >= kCryptoRefreshIntervalMs) {
        startCryptoPriceRequest();
    }
    scheduleNextCryptoRefresh(kCryptoRefreshIntervalMs);

    result[QStringLiteral("ok")] = true;
    return result;
}

bool FinanceController::deleteCryptoWallet(const QString& id)
{
    int walletIndex = -1;
    for (int index = 0; index < cryptoWallets_.size(); ++index) {
        if (cryptoWallets_[index].id() == id) {
            walletIndex = index;
            break;
        }
    }
    if (walletIndex < 0 || !repository_.deleteCryptoWallet(id)) {
        if (walletIndex >= 0) {
            qWarning() << "Failed to delete crypto wallet:"
                       << repository_.lastError();
        }
        return false;
    }

    refreshingCryptoWalletIds_.remove(id);
    pendingCryptoWalletRequests_.remove(id);
    cryptoTransactions_.erase(
        std::remove_if(
            cryptoTransactions_.begin(),
            cryptoTransactions_.end(),
            [&id](const CryptoTransaction& transaction)
            {
                return transaction.walletId() == id;
            }),
        cryptoTransactions_.end());
    cryptoWallets_.removeAt(walletIndex);
    if (selectedCryptoWalletId_ == id) {
        selectedCryptoWalletId_ = cryptoWallets_.isEmpty()
            ? QString()
            : cryptoWallets_.constFirst().id();
        emit selectedCryptoWalletIdChanged();
    }
    if (cryptoWallets_.isEmpty()) {
        cryptoRefreshTimer_.stop();
    }
    emit cryptoWalletsChanged();
    emit cryptoTransactionsChanged();
    emit balanceChanged();
    return true;
}

void FinanceController::refreshCryptoWallets()
{
    if (cryptoWallets_.isEmpty() || pendingCryptoRequests_ > 0) {
        return;
    }

    setCryptoLastError(QString());
    lastCryptoRefreshAttemptUtc_ = QDateTime::currentDateTimeUtc();
    if (!repository_.saveCryptoRefreshAttemptUtc(
            lastCryptoRefreshAttemptUtc_)) {
        qWarning() << "Failed to save crypto refresh time:"
                   << repository_.lastError();
    }
    startCryptoPriceRequest();
    for (const CryptoWallet& wallet : std::as_const(cryptoWallets_)) {
        startCryptoBalanceRequest(wallet);
        startCryptoTransactionRequest(wallet);
    }
    scheduleNextCryptoRefresh(kCryptoRefreshIntervalMs);
}

void FinanceController::searchInvestmentInstruments(const QString& query)
{
    const QString normalized = query.trimmed();
    if (normalized.size() < 2) {
        setInvestmentLastError(tr("Введите хотя бы два символа тикера или ISIN"));
        return;
    }
    if (investmentSearchBusy_) {
        return;
    }
    investmentSearchResults_.clear();
    selectedInvestmentSearchIndex_ = -1;
    requestedSearchQuoteId_.clear();
    investmentSearchBusy_ = true;
    investmentQuoteBusy_ = false;
    investmentLastError_.clear();
    emit investmentSearchResultsChanged();
    emit investmentSearchStateChanged();
    investmentProvider_.search(normalized);
}

void FinanceController::selectInvestmentSearchResult(const int index)
{
    if (index < 0 || index >= investmentSearchResults_.size() ||
        investmentQuoteBusy_) {
        return;
    }
    selectedInvestmentSearchIndex_ = index;
    investmentLastError_.clear();
    emit investmentSearchResultsChanged();
    emit investmentSearchStateChanged();

    const InvestmentMarketInstrument& instrument =
        investmentSearchResults_.at(index);
    if (instrument.priceMicros() > 0) {
        return;
    }
    investmentQuoteBusy_ = true;
    requestedSearchQuoteId_ = instrument.id();
    emit investmentSearchStateChanged();
    investmentProvider_.requestQuote(instrument);
}

QVariantMap FinanceController::addInvestmentPosition(
    const QString& accountId,
    const int searchResultIndex,
    const QString& quantity,
    const QString& averagePrice
    )
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    if (selectedAsset_ != AssetType::Investment) {
        result[QStringLiteral("error")] = tr(
            "Сначала выберите актив «Инвестиции»");
        return result;
    }
    if (searchResultIndex < 0 ||
        searchResultIndex >= investmentSearchResults_.size()) {
        result[QStringLiteral("error")] = tr("Выберите акцию или фонд");
        return result;
    }
    const auto account = std::find_if(
        accounts_.cbegin(), accounts_.cend(),
        [&accountId](const Account& candidate) {
            return candidate.id() == accountId &&
                candidate.assetType() == AssetType::Investment;
        });
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Выберите инвестиционный счёт");
        return result;
    }

    const InvestmentMarketInstrument& marketInstrument =
        investmentSearchResults_.at(searchResultIndex);
    if (marketInstrument.priceMicros() <= 0 ||
        marketInstrument.currencyCode().isEmpty()) {
        result[QStringLiteral("error")] = tr(
            "Сначала получите рыночную цену инструмента");
        return result;
    }
    const Currency instrumentCurrency = currencyFromString(
        marketInstrument.currencyCode());
    if (account->currency() != instrumentCurrency) {
        result[QStringLiteral("error")] = tr(
            "Валюта счёта должна совпадать с валютой инструмента (%1)")
                .arg(marketInstrument.currencyCode());
        return result;
    }

    qint64 quantityMicros = 0;
    if (!parsePositiveMicros(quantity, quantityMicros)) {
        result[QStringLiteral("error")] = tr(
            "Введите количество больше нуля (до 6 знаков после запятой)");
        return result;
    }
    qint64 averagePriceMicros = marketInstrument.priceMicros();
    if (!averagePrice.trimmed().isEmpty() &&
        !parsePositiveMicros(averagePrice, averagePriceMicros)) {
        result[QStringLiteral("error")] = tr(
            "Введите корректную среднюю цену");
        return result;
    }
    const bool duplicate = std::any_of(
        investmentPositions_.cbegin(), investmentPositions_.cend(),
        [&accountId, &marketInstrument](const InvestmentPosition& position) {
            return position.accountId() == accountId &&
                position.instrumentId() == marketInstrument.id();
        });
    if (duplicate) {
        result[QStringLiteral("error")] = tr(
            "Этот инструмент уже добавлен на выбранный счёт");
        return result;
    }

    const InvestmentInstrument instrument(
        marketInstrument.id(), marketInstrument.symbol(), marketInstrument.isin(),
        marketInstrument.name(), marketInstrument.type(), instrumentCurrency,
        QStringLiteral("MOEX"), marketInstrument.primaryBoardId());
    const auto existing = std::find_if(
        investmentInstruments_.cbegin(), investmentInstruments_.cend(),
        [&instrument](const InvestmentInstrument& candidate) {
            return candidate.id() == instrument.id();
        });
    const bool insertedInstrument = existing == investmentInstruments_.cend();
    if ((insertedInstrument && !repository_.insertInvestmentInstrument(instrument)) ||
        (!insertedInstrument && !repository_.updateInvestmentInstrument(instrument))) {
        qWarning() << "Failed to save investment instrument:"
                   << repository_.lastError();
        result[QStringLiteral("error")] = tr("Не удалось сохранить инструмент");
        return result;
    }

    const InvestmentPosition position(
        QUuid::createUuid().toString(QUuid::WithoutBraces), accountId,
        instrument.id(), quantityMicros, averagePriceMicros);
    if (!repository_.insertInvestmentPosition(position)) {
        if (insertedInstrument &&
            !repository_.archiveInvestmentInstrument(instrument.id())) {
            qWarning() << "Failed to roll back investment instrument:"
                       << repository_.lastError();
        }
        result[QStringLiteral("error")] = tr("Не удалось сохранить позицию");
        return result;
    }
    if (!repository_.saveInvestmentQuote(InvestmentQuote(
            instrument.id(), marketInstrument.priceMicros(),
            marketInstrument.quotedAtUtc()))) {
        qWarning() << "Failed to save initial investment quote:"
                   << repository_.lastError();
    }
    investmentInstruments_ = repository_.loadInvestmentInstruments();
    investmentPositions_ = repository_.loadInvestmentPositions();
    investmentQuotes_ = repository_.loadInvestmentQuotes();
    emit investmentPositionsChanged();
    emit accountsChanged();
    emit balanceChanged();
    result[QStringLiteral("ok")] = true;
    return result;
}

QVariantMap FinanceController::updateInvestmentPosition(
    const QString& positionId,
    const QString& accountId,
    const int searchResultIndex,
    const QString& quantity,
    const QString& averagePrice
    )
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    const auto current = std::find_if(
        investmentPositions_.cbegin(), investmentPositions_.cend(),
        [&positionId](const InvestmentPosition& position) {
            return position.id() == positionId;
        });
    if (current == investmentPositions_.cend()) {
        result[QStringLiteral("error")] = tr("Инвестиционная позиция не найдена");
        return result;
    }

    const auto account = std::find_if(
        accounts_.cbegin(), accounts_.cend(),
        [&accountId](const Account& candidate) {
            return candidate.id() == accountId &&
                candidate.assetType() == AssetType::Investment;
        });
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Выберите инвестиционный счёт");
        return result;
    }

    const auto currentInstrument = std::find_if(
        investmentInstruments_.cbegin(), investmentInstruments_.cend(),
        [&current](const InvestmentInstrument& candidate) {
            return candidate.id() == current->instrumentId();
        });
    if (currentInstrument == investmentInstruments_.cend()) {
        result[QStringLiteral("error")] = tr("Инвестиционный инструмент не найден");
        return result;
    }

    const InvestmentMarketInstrument* selectedMarketInstrument = nullptr;
    QString targetInstrumentId = currentInstrument->id();
    Currency targetCurrency = currentInstrument->currency();
    if (searchResultIndex >= 0) {
        if (searchResultIndex >= investmentSearchResults_.size()) {
            result[QStringLiteral("error")] = tr("Выберите акцию или фонд");
            return result;
        }
        selectedMarketInstrument = &investmentSearchResults_.at(searchResultIndex);
        if (selectedMarketInstrument->priceMicros() <= 0 ||
            selectedMarketInstrument->currencyCode().isEmpty()) {
            result[QStringLiteral("error")] = tr(
                "Сначала получите рыночную цену инструмента");
            return result;
        }
        targetInstrumentId = selectedMarketInstrument->id();
        targetCurrency = currencyFromString(
            selectedMarketInstrument->currencyCode());
    } else if (searchResultIndex != -1) {
        result[QStringLiteral("error")] = tr("Выберите акцию или фонд");
        return result;
    }

    if (account->currency() != targetCurrency) {
        result[QStringLiteral("error")] = tr(
            "Валюта счёта должна совпадать с валютой инструмента (%1)")
                .arg(currencyCode(targetCurrency));
        return result;
    }

    qint64 quantityMicros = 0;
    if (!parsePositiveMicros(quantity, quantityMicros)) {
        result[QStringLiteral("error")] = tr(
            "Введите количество больше нуля (до 6 знаков после запятой)");
        return result;
    }

    qint64 averagePriceMicros = selectedMarketInstrument
        ? selectedMarketInstrument->priceMicros()
        : current->averagePriceMicros();
    if (!averagePrice.trimmed().isEmpty() &&
        !parsePositiveMicros(averagePrice, averagePriceMicros)) {
        result[QStringLiteral("error")] = tr("Введите корректную среднюю цену");
        return result;
    }

    const bool duplicate = std::any_of(
        investmentPositions_.cbegin(), investmentPositions_.cend(),
        [&positionId, &accountId,
         &targetInstrumentId](const InvestmentPosition& position) {
            return position.id() != positionId &&
                position.accountId() == accountId &&
                position.instrumentId() == targetInstrumentId;
        });
    if (duplicate) {
        result[QStringLiteral("error")] = tr(
            "Этот инструмент уже добавлен на выбранный счёт");
        return result;
    }

    bool insertedInstrument = false;
    if (selectedMarketInstrument) {
        const InvestmentInstrument selectedInstrument(
            selectedMarketInstrument->id(), selectedMarketInstrument->symbol(),
            selectedMarketInstrument->isin(), selectedMarketInstrument->name(),
            selectedMarketInstrument->type(), targetCurrency,
            QStringLiteral("MOEX"),
            selectedMarketInstrument->primaryBoardId());
        const auto existing = std::find_if(
            investmentInstruments_.cbegin(), investmentInstruments_.cend(),
            [&selectedInstrument](const InvestmentInstrument& candidate) {
                return candidate.id() == selectedInstrument.id();
            });
        insertedInstrument = existing == investmentInstruments_.cend();
        if ((insertedInstrument &&
             !repository_.insertInvestmentInstrument(selectedInstrument)) ||
            (!insertedInstrument &&
             !repository_.updateInvestmentInstrument(selectedInstrument))) {
            qWarning() << "Failed to save edited investment instrument:"
                       << repository_.lastError();
            result[QStringLiteral("error")] = tr(
                "Не удалось сохранить инструмент");
            return result;
        }
    }

    const InvestmentPosition updated(
        current->id(), accountId, targetInstrumentId, quantityMicros,
        averagePriceMicros, current->createdAtUtc(),
        QDateTime::currentDateTimeUtc());
    if (!repository_.updateInvestmentPosition(updated)) {
        if (insertedInstrument &&
            !repository_.archiveInvestmentInstrument(targetInstrumentId)) {
            qWarning() << "Failed to roll back edited investment instrument:"
                       << repository_.lastError();
        }
        qWarning() << "Failed to update investment position:"
                   << repository_.lastError();
        result[QStringLiteral("error")] = tr("Не удалось обновить позицию");
        return result;
    }

    if (selectedMarketInstrument &&
        !repository_.saveInvestmentQuote(InvestmentQuote(
            targetInstrumentId, selectedMarketInstrument->priceMicros(),
            selectedMarketInstrument->quotedAtUtc()))) {
        qWarning() << "Failed to save edited investment quote:"
                   << repository_.lastError();
    }
    investmentInstruments_ = repository_.loadInvestmentInstruments();
    investmentPositions_ = repository_.loadInvestmentPositions();
    investmentQuotes_ = repository_.loadInvestmentQuotes();
    emit investmentPositionsChanged();
    emit accountsChanged();
    emit balanceChanged();
    result[QStringLiteral("ok")] = true;
    return result;
}

bool FinanceController::deleteInvestmentPosition(const QString& id)
{
    if (!repository_.deleteInvestmentPosition(id)) {
        qWarning() << "Failed to delete investment position:"
                   << repository_.lastError();
        return false;
    }
    investmentPositions_ = repository_.loadInvestmentPositions();
    emit investmentPositionsChanged();
    emit accountsChanged();
    emit balanceChanged();
    return true;
}

void FinanceController::refreshInvestmentQuotes()
{
    if (investmentRefreshing_) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSet<QString> usedInstrumentIds;
    for (const InvestmentPosition& position : std::as_const(investmentPositions_)) {
        usedInstrumentIds.insert(position.instrumentId());
    }
    for (const InvestmentInstrument& instrument :
         std::as_const(investmentInstruments_)) {
        if (!usedInstrumentIds.contains(instrument.id()) ||
            instrument.marketCode() != QStringLiteral("MOEX") ||
            instrument.primaryBoardId().isEmpty()) {
            continue;
        }
        const auto quote = std::find_if(
            investmentQuotes_.cbegin(), investmentQuotes_.cend(),
            [&instrument](const InvestmentQuote& candidate) {
                return candidate.instrumentId() == instrument.id();
            });
        if (quote != investmentQuotes_.cend()) {
            const qint64 age = quote->quotedAtUtc().msecsTo(now);
            if (age >= 0 && age < 6 * 60 * 60 * 1000) {
                continue;
            }
        }
        refreshingInvestmentIds_.insert(instrument.id());
        investmentProvider_.requestQuote(InvestmentMarketInstrument(
            instrument.id(), instrument.symbol(), instrument.isin(),
            instrument.name(), instrument.type(), instrument.primaryBoardId()));
    }
    const bool refreshing = !refreshingInvestmentIds_.isEmpty();
    if (investmentRefreshing_ != refreshing) {
        investmentRefreshing_ = refreshing;
        emit investmentRefreshingChanged();
    }
}

bool FinanceController::addCategory(
    const QString& name,
    const QString& type
    )
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 60) {
        return false;
    }

    const CategoryType categoryType = type == QStringLiteral("income")
        ? CategoryType::Income
        : CategoryType::Expense;

    for (const Category& existing : categories_) {
        if (!archivedCategoryIds_.contains(existing.id()) &&
            existing.type() == categoryType &&
            (existing.name().compare(normalizedName, Qt::CaseInsensitive) == 0 ||
             categoryDisplayName(existing).compare(
                 normalizedName, Qt::CaseInsensitive) == 0)) {
            return false;
        }
    }

    const Category category(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        normalizedName,
        categoryType);

    if (!repository_.isOpen() || !repository_.insertCategory(category)) {
        qWarning() << "Failed to save category:" << repository_.lastError();
        return false;
    }

    categories_.append(category);
    emit categoriesChanged();
    return true;
}

bool FinanceController::renameCategory(
    const QString& id,
    const QString& name
    )
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 60 ||
        archivedCategoryIds_.contains(id)) {
        return false;
    }

    int categoryIndex = -1;
    for (int index = 0; index < categories_.size(); ++index) {
        const Category& category = categories_[index];
        if (category.id() == id) {
            categoryIndex = index;
            break;
        }
    }

    if (categoryIndex < 0) {
        return false;
    }

    if (normalizedName == categoryDisplayName(categories_[categoryIndex])) {
        return true;
    }

    const CategoryType type = categories_[categoryIndex].type();
    for (const Category& category : categories_) {
        if (category.id() != id &&
            !archivedCategoryIds_.contains(category.id()) &&
            category.type() == type &&
            (category.name().compare(normalizedName, Qt::CaseInsensitive) == 0 ||
             categoryDisplayName(category).compare(
                 normalizedName, Qt::CaseInsensitive) == 0)) {
            return false;
        }
    }

    if (!repository_.updateCategoryName(id, normalizedName)) {
        qWarning() << "Failed to rename category:" << repository_.lastError();
        return false;
    }

    categories_[categoryIndex] = Category(id, normalizedName, type);
    emit categoriesChanged();
    emit transactionsChanged();
    return true;
}

bool FinanceController::deleteCategory(const QString& id)
{
    if (id.isEmpty() || archivedCategoryIds_.contains(id)) {
        return false;
    }

    bool found = false;
    for (const Category& category : categories_) {
        if (category.id() == id) {
            found = true;
            break;
        }
    }
    if (!found || !repository_.archiveCategory(id)) {
        qWarning() << "Failed to archive category:" << repository_.lastError();
        return false;
    }

    archivedCategoryIds_.insert(id);
    emit categoriesChanged();
    emit transactionsChanged();
    return true;
}

QString FinanceController::categoryName(const QString& id) const
{
    for (const Category& category : categories_) {
        if (category.id() == id) {
            return categoryDisplayName(category);
        }
    }
    return tr("Без категории");
}

qint64 FinanceController::convertTransaction(
    int transactionIndex,
    const QString& targetCurrency
    ) const
{
    const QVector<Transaction> visibleTransactions = dateFilteredTransactions();
    if (transactionIndex < 0 ||
        transactionIndex >= visibleTransactions.size()) {
        return 0;
    }

    const Currency target = currencyFromString(
        targetCurrency
        );

    const Money converted = currencyConverter_.convert(
        visibleTransactions[transactionIndex].money(),
        target
        );

    return converted.minorUnits();
}

bool FinanceController::addIncome(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency,
    const QString& accountId,
    const QDateTime& occurredAt
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Income,
        description,
        categoryId,
        currencyFromString(currency),
        accountId,
        occurredAt
        );
}

bool FinanceController::addExpense(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency,
    const QString& accountId,
    const QDateTime& occurredAt
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Expense,
        description,
        categoryId,
        currencyFromString(currency),
        accountId,
        occurredAt
        );
}

bool FinanceController::addTransfer(
    const qint64 sourceMinorUnits,
    const QString& description,
    const QString& sourceAccountId,
    const QString& targetAccountId,
    const QDateTime& occurredAt
    )
{
    if (sourceMinorUnits <= 0 || sourceAccountId.isEmpty() ||
        targetAccountId.isEmpty() || sourceAccountId == targetAccountId ||
        !occurredAt.isValid()) {
        return false;
    }

    const Account* source = nullptr;
    const Account* target = nullptr;
    for (const Account& account : accounts_) {
        if (account.id() == sourceAccountId) source = &account;
        if (account.id() == targetAccountId) target = &account;
    }
    if (!source || !target) {
        return false;
    }

    const QString transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 targetMinorUnits = currencyConverter_.convert(
        Money(sourceMinorUnits, source->currency()), target->currency()).minorUnits();
    if (targetMinorUnits <= 0) {
        return false;
    }

    const QString normalizedDescription = description.trimmed();
    const Transaction outgoing(
        transferId + QStringLiteral("-out"), source->id(),
        QStringLiteral("transfer-out"), Money(sourceMinorUnits, source->currency()),
        TransactionType::Expense, occurredAt, normalizedDescription);
    const Transaction incoming(
        transferId + QStringLiteral("-in"), target->id(),
        QStringLiteral("transfer-in"), Money(targetMinorUnits, target->currency()),
        TransactionType::Income, occurredAt, normalizedDescription);

    if (!repository_.isOpen() || !repository_.insertTransfer(outgoing, incoming)) {
        qWarning() << "Failed to save transfer:" << repository_.lastError();
        return false;
    }

    transactions_.prepend(incoming);
    transactions_.prepend(outgoing);
    summary_ = repository_.loadSummary();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    return true;
}

bool FinanceController::updateTransaction(
    const QString& id,
    const qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& accountId,
    const QString& type,
    const QDateTime& occurredAt
    )
{
    return updateOperation(
        id,
        minorUnits,
        description,
        categoryId,
        accountId,
        type,
        QString(),
        occurredAt
        );
}

bool FinanceController::updateOperation(
    const QString& id,
    const qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& accountId,
    const QString& type,
    const QString& targetAccountId,
    const QDateTime& occurredAt
    )
{
    if (id.isEmpty() || minorUnits <= 0 || !occurredAt.isValid()) {
        return false;
    }

    int transactionIndex = -1;
    for (int index = 0; index < transactions_.size(); ++index) {
        if (transactions_[index].id() == id) {
            transactionIndex = index;
            break;
        }
    }
    if (transactionIndex < 0) {
        return false;
    }
    const Transaction& original = transactions_[transactionIndex];
    const bool originalIsTransfer = isTransfer(original);
    const QString normalizedType = type.trimmed().toLower();
    if (normalizedType != QStringLiteral("income") &&
        normalizedType != QStringLiteral("expense") &&
        normalizedType != QStringLiteral("transfer")) {
        return false;
    }

    if (normalizedType == QStringLiteral("transfer")) {
        if (accountId.isEmpty() || targetAccountId.isEmpty() ||
            accountId == targetAccountId) {
            return false;
        }

        const Account* source = nullptr;
        const Account* target = nullptr;
        for (const Account& account : accounts_) {
            if (account.id() == accountId) source = &account;
            if (account.id() == targetAccountId) target = &account;
        }
        if (!source || !target) {
            return false;
        }

        const qint64 targetMinorUnits = currencyConverter_.convert(
            Money(minorUnits, source->currency()),
            target->currency()).minorUnits();
        if (targetMinorUnits <= 0) {
            return false;
        }

        const QString currentTransferId = transferId(original);
        if (originalIsTransfer && currentTransferId.isEmpty()) {
            return false;
        }
        const QString resolvedTransferId = originalIsTransfer
            ? currentTransferId
            : QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString normalizedDescription = description.trimmed();
        const Transaction outgoing(
            resolvedTransferId + QStringLiteral("-out"),
            source->id(),
            QStringLiteral("transfer-out"),
            Money(minorUnits, source->currency()),
            TransactionType::Expense,
            occurredAt,
            normalizedDescription);
        const Transaction incoming(
            resolvedTransferId + QStringLiteral("-in"),
            target->id(),
            QStringLiteral("transfer-in"),
            Money(targetMinorUnits, target->currency()),
            TransactionType::Income,
            occurredAt,
            normalizedDescription);

        if (!repository_.isOpen() ||
            !repository_.replaceTransactionWithTransfer(id, outgoing, incoming)) {
            qWarning() << "Failed to replace operation with transfer:"
                       << repository_.lastError();
            return false;
        }

        transactions_ = repository_.loadTransactions();
        summary_ = repository_.loadSummary();
        emit transactionsChanged();
        emit balanceChanged();
        emit accountsChanged();
        return true;
    }

    const Account* selectedAccount = nullptr;
    for (const Account& account : accounts_) {
        if (account.id() == accountId) {
            selectedAccount = &account;
            break;
        }
    }
    if (!selectedAccount || selectedAccount->assetType() != selectedAsset_) {
        return false;
    }

    const TransactionType transactionType =
        normalizedType == QStringLiteral("income")
            ? TransactionType::Income
            : TransactionType::Expense;
    const CategoryType requiredCategoryType =
        transactionType == TransactionType::Income
            ? CategoryType::Income
            : CategoryType::Expense;

    bool categoryIsValid = false;
    for (const Category& category : categories_) {
        if (category.id() == categoryId &&
            category.type() == requiredCategoryType &&
            (!archivedCategoryIds_.contains(categoryId) ||
             original.categoryId() == categoryId)) {
            categoryIsValid = true;
            break;
        }
    }
    if (!categoryIsValid) {
        return false;
    }

    const Transaction updated(
        original.id(),
        selectedAccount->id(),
        categoryId,
        Money(minorUnits, selectedAccount->currency()),
        transactionType,
        occurredAt,
        description
        );

    const bool saved = repository_.isOpen() &&
        (originalIsTransfer
            ? repository_.replaceTransaction(id, updated)
            : repository_.updateTransaction(updated));
    if (!saved) {
        qWarning() << "Failed to update transaction:"
                   << repository_.lastError();
        return false;
    }

    transactions_ = repository_.loadTransactions();
    summary_ = repository_.loadSummary();

    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    return true;
}

QVariantMap FinanceController::transferDetails(const QString& id) const
{
    const Transaction* selected = nullptr;
    for (const Transaction& transaction : transactions_) {
        if (transaction.id() == id) {
            selected = &transaction;
            break;
        }
    }
    if (!selected || !isTransfer(*selected)) {
        return {};
    }

    const QString idPrefix = transferId(*selected);
    if (idPrefix.isEmpty()) {
        return {};
    }

    const Transaction* outgoing = nullptr;
    const Transaction* incoming = nullptr;
    const QString outgoingId = idPrefix + QStringLiteral("-out");
    const QString incomingId = idPrefix + QStringLiteral("-in");
    for (const Transaction& transaction : transactions_) {
        if (transaction.id() == outgoingId &&
            transaction.categoryId() == QStringLiteral("transfer-out")) {
            outgoing = &transaction;
        } else if (transaction.id() == incomingId &&
                   transaction.categoryId() == QStringLiteral("transfer-in")) {
            incoming = &transaction;
        }
    }
    if (!outgoing || !incoming) {
        return {};
    }

    QVariantMap result;
    result["sourceAccountId"] = outgoing->accountId();
    result["targetAccountId"] = incoming->accountId();
    result["sourceAmount"] = outgoing->money().minorUnits();
    return result;
}

bool FinanceController::deleteTransaction(const QString& id)
{
    int transactionIndex = -1;
    for (int index = 0; index < transactions_.size(); ++index) {
        if (transactions_[index].id() == id) {
            transactionIndex = index;
            break;
        }
    }
    if (transactionIndex < 0) {
        return false;
    }
    if (!repository_.isOpen() || !repository_.deleteTransaction(id)) {
        qWarning() << "Failed to delete transaction:"
                   << repository_.lastError();
        return false;
    }

    transactions_ = repository_.loadTransactions();
    summary_ = repository_.loadSummary();

    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    return true;
}

bool FinanceController::addTransaction(
    qint64 minorUnits,
    TransactionType type,
    const QString& description,
    const QString& categoryId,
    Currency currency,
    const QString& accountId,
    const QDateTime& occurredAt
    )
{
    if (minorUnits <= 0 || !occurredAt.isValid()) {
        return false;
    }

    QString resolvedAccountId = accountId;
    const Account* selectedAccount = nullptr;
    for (const Account& account : accounts_) {
        if (account.id() == resolvedAccountId) {
            selectedAccount = &account;
            break;
        }
    }

    if (!selectedAccount || selectedAccount->assetType() != selectedAsset_) {
        return false;
    }

    currency = selectedAccount->currency();

    const Transaction transaction(
            QUuid::createUuid().toString(
                QUuid::WithoutBraces
                ),
            resolvedAccountId,
            categoryId,
            Money(
                minorUnits,
                currency
            ),
            type,
            occurredAt,
            description
            );

    if (!repository_.isOpen() ||
        !repository_.insertTransaction(transaction)) {
        qWarning() << "Failed to save transaction:"
                   << repository_.lastError();
        return false;
    }

    transactions_.prepend(transaction);

    auto& amounts = type == TransactionType::Income
        ? summary_.income
        : summary_.expense;
    amounts[currencyIndex(currency)] += minorUnits;
    summary_.balance[currencyIndex(currency)] +=
        type == TransactionType::Income ? minorUnits : -minorUnits;

    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    return true;
}

qint64 FinanceController::accountBalanceMinor(const Account& account) const
{
    qint64 balance = account.initialBalanceMinor();
    for (const Transaction& transaction : transactions_) {
        if (transaction.accountId() != account.id()) {
            continue;
        }
        if (dateFilterActive() &&
            !TransactionDateFilter::isOnOrBefore(
                transaction, dateFilterTo_)) {
            continue;
        }
        balance += transaction.type() == TransactionType::Income
            ? transaction.money().minorUnits()
            : -transaction.money().minorUnits();
    }
    if (account.assetType() == AssetType::Investment) {
        const qint64 positionValue = investmentAccountValueMinor(account.id());
        if (positionValue > 0 &&
            balance > std::numeric_limits<qint64>::max() - positionValue) {
            return std::numeric_limits<qint64>::max();
        }
        balance += positionValue;
    }
    return balance;
}

qint64 FinanceController::investmentAccountValueMinor(
    const QString& accountId
    ) const
{
    using Int128 = __int128_t;
    Int128 totalMinor = 0;
    for (const InvestmentPosition& position : investmentPositions_) {
        if (position.accountId() != accountId) {
            continue;
        }
        const auto quote = std::find_if(
            investmentQuotes_.cbegin(), investmentQuotes_.cend(),
            [&position](const InvestmentQuote& candidate) {
                return candidate.instrumentId() == position.instrumentId();
            });
        if (quote == investmentQuotes_.cend() || quote->priceMicros() <= 0) {
            continue;
        }
        totalMinor += scaledInvestmentValueMinor(
            position.quantityMicros(), quote->priceMicros());
        if (totalMinor >= std::numeric_limits<qint64>::max()) {
            return std::numeric_limits<qint64>::max();
        }
    }
    return static_cast<qint64>(totalMinor);
}

int FinanceController::accountTransactionCount(const QString& accountId) const
{
    int count = 0;
    for (const Transaction& transaction : transactions_) {
        if (transaction.accountId() == accountId &&
            (!dateFilterActive() || TransactionDateFilter::contains(
                transaction, dateFilterFrom_, dateFilterTo_))) {
            ++count;
        }
    }
    return count;
}

qint64 FinanceController::assetBalanceMinor(const AssetType asset) const
{
    qint64 total = 0;
    for (const Account& account : accounts_) {
        if (account.assetType() != asset) {
            continue;
        }
        total += currencyConverter_.convert(
            Money(accountBalanceMinor(account), account.currency()),
            appCurrency_).minorUnits();
    }
    if (asset == AssetType::Crypto) {
        const qint64 trackedWallets = cryptoWalletsTotalMinor();
        if (trackedWallets > 0 && total >
                std::numeric_limits<qint64>::max() - trackedWallets) {
            return std::numeric_limits<qint64>::max();
        }
        total += trackedWallets;
    }
    return total;
}

qint64 FinanceController::cryptoWalletValueMinor(
    const CryptoWallet& wallet
    ) const
{
    const qint64 priceUsdMicros = cryptoPricesUsdMicros_.value(
        wallet.symbol());
    if (priceUsdMicros <= 0 || wallet.balanceAtomic() <= 0 ||
        wallet.decimals() < 0 || wallet.decimals() > 18) {
        return 0;
    }

    // Prices use 6 decimal places and fiat money uses 2. The wallet-specific
    // scale keeps USDT at 6 places and BTC/ETH at 8 while all multiplication
    // remains integer-only.
    using Int128 = __int128_t;
    Int128 divisor = 10'000;
    for (int index = 0; index < wallet.decimals(); ++index) {
        divisor *= 10;
    }
    const Int128 product = static_cast<Int128>(wallet.balanceAtomic()) *
        static_cast<Int128>(priceUsdMicros);
    const Int128 roundedUsdMinor = (product + divisor / 2) / divisor;
    const Int128 maximum = std::numeric_limits<qint64>::max();
    const qint64 usdMinor = roundedUsdMinor > maximum
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(roundedUsdMinor);
    return currencyConverter_.convert(
        Money(usdMinor, Currency::USD), appCurrency_).minorUnits();
}

qint64 FinanceController::cryptoWalletsTotalMinor() const
{
    qint64 total = 0;
    for (const CryptoWallet& wallet : cryptoWallets_) {
        const qint64 value = cryptoWalletValueMinor(wallet);
        if (value > 0 && total > std::numeric_limits<qint64>::max() - value) {
            return std::numeric_limits<qint64>::max();
        }
        total += value;
    }
    return total;
}

void FinanceController::scheduleCapitalSnapshot()
{
    const CapitalSnapshot snapshot{
        currencyCode(appCurrency_),
        assetBalanceMinor(AssetType::Fiat),
        assetBalanceMinor(AssetType::Crypto),
        assetBalanceMinor(AssetType::Investment)
    };
    const QString filePath = CapitalSnapshotStore::defaultFilePath();
    const QDate currentDate = QDate::currentDate();

    QThreadPool::globalInstance()->start(
        [filePath, currentDate, snapshot]()
        {
            const CapitalSnapshotStore::SaveResult result =
                CapitalSnapshotStore::saveIfNeeded(
                    filePath, currentDate, snapshot);
            if (result.status == CapitalSnapshotStore::SaveStatus::Failed) {
                qWarning() << "Failed to save daily capital snapshot:"
                           << result.error;
            }
        });
}

void FinanceController::scheduleInitialCryptoRefresh()
{
    if (cryptoWallets_.isEmpty() || pendingCryptoRequests_ > 0) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    qint64 nextDelayMs = kCryptoRefreshIntervalMs;
    bool hasStaleSnapshot = false;
    const auto considerTimestamp = [&](const QDateTime& fetchedAtUtc)
    {
        if (!fetchedAtUtc.isValid()) {
            hasStaleSnapshot = true;
            return;
        }
        const qint64 ageMs = fetchedAtUtc.msecsTo(now);
        if (ageMs < 0) {
            return;
        }
        if (ageMs >= kCryptoRefreshIntervalMs) {
            hasStaleSnapshot = true;
            return;
        }
        nextDelayMs = std::min(
            nextDelayMs,
            kCryptoRefreshIntervalMs - ageMs);
    };

    for (const CryptoWallet& wallet : std::as_const(cryptoWallets_)) {
        considerTimestamp(cryptoPricesFetchedAtUtc_.value(wallet.symbol()));
        considerTimestamp(wallet.balanceFetchedAtUtc());
        considerTimestamp(wallet.historyFetchedAtUtc());
    }

    if (hasStaleSnapshot) {
        if (lastCryptoRefreshAttemptUtc_.isValid()) {
            const qint64 attemptAgeMs =
                lastCryptoRefreshAttemptUtc_.msecsTo(now);
            if (attemptAgeMs >= 0 &&
                attemptAgeMs < kCryptoRefreshIntervalMs) {
                scheduleNextCryptoRefresh(
                    kCryptoRefreshIntervalMs - attemptAgeMs);
                return;
            }
        }
        QTimer::singleShot(
            0,
            this,
            &FinanceController::refreshCryptoWallets);
    } else {
        scheduleNextCryptoRefresh(nextDelayMs);
    }
}

void FinanceController::scheduleNextCryptoRefresh(const qint64 delayMs)
{
    if (cryptoWallets_.isEmpty()) {
        cryptoRefreshTimer_.stop();
        return;
    }
    const qint64 boundedDelay = std::clamp<qint64>(
        delayMs, 1, std::numeric_limits<int>::max());
    cryptoRefreshTimer_.start(static_cast<int>(boundedDelay));
}

void FinanceController::startCryptoBalanceRequest(const CryptoWallet& wallet)
{
    if (!cryptoProvider_.requestBalance(wallet)) {
        return;
    }
    beginCryptoWalletRequest(wallet.id());
    ++pendingCryptoRequests_;
    if (!cryptoRefreshing_) {
        cryptoRefreshing_ = true;
        emit cryptoRefreshingChanged();
    }
    emit cryptoWalletsChanged();
}

void FinanceController::startCryptoTransactionRequest(
    const CryptoWallet& wallet
    )
{
    if (!cryptoProvider_.requestTransactions(wallet)) {
        return;
    }
    beginCryptoWalletRequest(wallet.id());
    ++pendingCryptoRequests_;
    if (!cryptoRefreshing_) {
        cryptoRefreshing_ = true;
        emit cryptoRefreshingChanged();
    }
    emit cryptoWalletsChanged();
}

void FinanceController::startCryptoPriceRequest()
{
    if (!cryptoProvider_.requestPrices()) {
        return;
    }
    ++pendingCryptoRequests_;
    if (!cryptoRefreshing_) {
        cryptoRefreshing_ = true;
        emit cryptoRefreshingChanged();
    }
}

void FinanceController::finishCryptoRequest()
{
    if (pendingCryptoRequests_ > 0) {
        --pendingCryptoRequests_;
    }
    if (pendingCryptoRequests_ == 0 && cryptoRefreshing_) {
        cryptoRefreshing_ = false;
        emit cryptoRefreshingChanged();
    }
}

void FinanceController::beginCryptoWalletRequest(const QString& walletId)
{
    pendingCryptoWalletRequests_[walletId] =
        pendingCryptoWalletRequests_.value(walletId) + 1;
    refreshingCryptoWalletIds_.insert(walletId);
}

void FinanceController::finishCryptoWalletRequest(const QString& walletId)
{
    const int remaining = pendingCryptoWalletRequests_.value(walletId) - 1;
    if (remaining > 0) {
        pendingCryptoWalletRequests_[walletId] = remaining;
        return;
    }
    pendingCryptoWalletRequests_.remove(walletId);
    refreshingCryptoWalletIds_.remove(walletId);
}

void FinanceController::setCryptoLastError(const QString& error)
{
    if (cryptoLastError_ == error) {
        return;
    }
    cryptoLastError_ = error;
    emit cryptoLastErrorChanged();
}

void FinanceController::finishInvestmentQuoteRequest(
    const QString& instrumentId
    )
{
    refreshingInvestmentIds_.remove(instrumentId);
    if (refreshingInvestmentIds_.isEmpty() && investmentRefreshing_) {
        investmentRefreshing_ = false;
        emit investmentRefreshingChanged();
    }
}

void FinanceController::setInvestmentLastError(const QString& error)
{
    if (investmentLastError_ == error) {
        return;
    }
    investmentLastError_ = error;
    emit investmentSearchStateChanged();
}

QVector<Transaction> FinanceController::dateFilteredTransactions() const
{
    if (!dateFilterActive()) {
        return transactions_;
    }
    return TransactionDateFilter::between(
        transactions_, dateFilterFrom_, dateFilterTo_);
}

FinanceRepository::Summary FinanceController::dateFilteredSummary() const
{
    if (!dateFilterActive()) {
        return summary_;
    }

    const DateSliceCalculator::Totals totals = DateSliceCalculator::calculate(
        accounts_, transactions_, dateFilterFrom_, dateFilterTo_);
    FinanceRepository::Summary result;
    result.balance = totals.balance;
    result.income = totals.income;
    result.expense = totals.expense;
    return result;
}

QString FinanceController::accountDisplayName(const Account& account) const
{
    const QString currency = currencyCode(account.currency());
    const QString defaultId = QStringLiteral("household-") + currency.toLower();
    const QString defaultName = QStringLiteral("Основной ") + currency;
    if (account.id() == defaultId && account.name() == defaultName) {
        return tr("Основной %1").arg(currency);
    }
    return account.name();
}

QString FinanceController::categoryDisplayName(const Category& category) const
{
    const QString& id = category.id();
    const QString& name = category.name();
    if (id == QStringLiteral("salary") && name == QStringLiteral("Зарплата"))
        return tr("Зарплата");
    if (id == QStringLiteral("freelance") && name == QStringLiteral("Фриланс"))
        return tr("Фриланс");
    if (id == QStringLiteral("gift") && name == QStringLiteral("Подарок"))
        return tr("Подарок");
    if (id == QStringLiteral("investment") && name == QStringLiteral("Инвестиции"))
        return tr("Инвестиции");
    if (id == QStringLiteral("other_income") && name == QStringLiteral("Другой доход"))
        return tr("Другой доход");
    if (id == QStringLiteral("groceries") && name == QStringLiteral("Продукты"))
        return tr("Продукты");
    if (id == QStringLiteral("transport") && name == QStringLiteral("Транспорт"))
        return tr("Транспорт");
    if (id == QStringLiteral("housing") && name == QStringLiteral("Жилье"))
        return tr("Жилье");
    if (id == QStringLiteral("health") && name == QStringLiteral("Здоровье"))
        return tr("Здоровье");
    if (id == QStringLiteral("entertainment") && name == QStringLiteral("Развлечения"))
        return tr("Развлечения");
    if (id == QStringLiteral("shopping") && name == QStringLiteral("Покупки"))
        return tr("Покупки");
    if (id == QStringLiteral("other_expense") && name == QStringLiteral("Другое"))
        return tr("Другое");
    if ((id == QStringLiteral("transfer-in") ||
         id == QStringLiteral("transfer-out")) &&
        name == QStringLiteral("Перевод")) {
        return tr("Перевод");
    }
    return name;
}

QString FinanceController::transactionDisplayDescription(
    const Transaction& transaction
    ) const
{
    const QString description = transaction.description();
    if (!isTransfer(transaction)) {
        return description;
    }

    const QString idPrefix = transferId(transaction);
    if (idPrefix.isEmpty()) {
        return description.isEmpty() ? tr("Перевод") : description;
    }

    const Transaction* outgoing = nullptr;
    const Transaction* incoming = nullptr;
    for (const Transaction& candidate : transactions_) {
        if (candidate.id() == idPrefix + QStringLiteral("-out"))
            outgoing = &candidate;
        else if (candidate.id() == idPrefix + QStringLiteral("-in"))
            incoming = &candidate;
    }
    if (!outgoing || !incoming) {
        return description.isEmpty() ? tr("Перевод") : description;
    }

    const Account* source = nullptr;
    const Account* target = nullptr;
    for (const Account& account : accounts_) {
        if (account.id() == outgoing->accountId()) source = &account;
        if (account.id() == incoming->accountId()) target = &account;
    }
    if (!source || !target) {
        return description.isEmpty() ? tr("Перевод") : description;
    }

    const QString legacyDefault = QStringLiteral("Перевод: %1 → %2")
        .arg(source->name(), target->name());
    if (!description.isEmpty() && description != legacyDefault) {
        return description;
    }
    return tr("Перевод: %1 → %2")
        .arg(accountDisplayName(*source), accountDisplayName(*target));
}

int FinanceController::currencyIndex(const Currency currency)
{
    return static_cast<int>(currency);
}

AssetType FinanceController::assetTypeFromString(const QString& asset)
{
    if (asset.trimmed().toLower() == QStringLiteral("crypto")) {
        return AssetType::Crypto;
    }
    if (asset.trimmed().toLower() == QStringLiteral("investment")) {
        return AssetType::Investment;
    }
    return AssetType::Fiat;
}

QString FinanceController::assetTypeToString(const AssetType asset)
{
    switch (asset) {
    case AssetType::Fiat:
        return QStringLiteral("fiat");
    case AssetType::Crypto:
        return QStringLiteral("crypto");
    case AssetType::Investment:
        return QStringLiteral("investment");
    }
    return QStringLiteral("fiat");
}

AccountType FinanceController::accountTypeFromString(const QString& type)
{
    const QString value = type.trimmed().toLower();
    if (value == QStringLiteral("cash")) return AccountType::Cash;
    if (value == QStringLiteral("debit_card")) return AccountType::DebitCard;
    if (value == QStringLiteral("credit_card")) return AccountType::CreditCard;
    if (value == QStringLiteral("savings")) return AccountType::Savings;
    if (value == QStringLiteral("crypto_wallet")) return AccountType::CryptoWallet;
    if (value == QStringLiteral("brokerage")) return AccountType::Brokerage;
    if (value == QStringLiteral("deposit")) return AccountType::Deposit;
    return AccountType::Other;
}

QString FinanceController::accountTypeToString(const AccountType type)
{
    switch (type) {
    case AccountType::Cash: return QStringLiteral("cash");
    case AccountType::DebitCard: return QStringLiteral("debit_card");
    case AccountType::CreditCard: return QStringLiteral("credit_card");
    case AccountType::Savings: return QStringLiteral("savings");
    case AccountType::CryptoWallet: return QStringLiteral("crypto_wallet");
    case AccountType::Brokerage: return QStringLiteral("brokerage");
    case AccountType::Deposit: return QStringLiteral("deposit");
    case AccountType::Other: return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

qint64 FinanceController::convertedTotal(
    const std::array<qint64, 3>& amounts
    ) const
{
    qint64 total = 0;

    for (int index = 0;
         index < static_cast<int>(amounts.size());
         ++index) {
        const Currency currency = static_cast<Currency>(index);
        total += currencyConverter_.convert(
            Money(amounts[index], currency),
            appCurrency_
            ).minorUnits();
    }

    return total;
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
