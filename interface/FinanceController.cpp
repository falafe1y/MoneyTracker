#include "FinanceController.h"
#include "../services/ExchangeCredentials.h"
#include "../services/MoexInvestmentParser.h"
#include <QtConcurrent>
#include <QFutureWatcher>

#include "../services/BankCsvImporter.h"
#include "../services/BankCategoryMatcher.h"
#include "../services/BankImportReview.h"
#include "../services/CapitalHistoryCalculator.h"
#include "../services/DateSliceCalculator.h"
#include "../services/DepositInterestCalculator.h"
#include "../services/RecurringScheduleCalculator.h"
#include "../services/CsvCodec.h"
#include "../services/CryptoParser.h"
#include "../services/TransactionDateFilter.h"
#include "../services/TronUsdtParser.h"
#include "../services/FinancialTrajectoryCalculator.h"

#include <QDebug>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimeZone>
#include <QUuid>

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
QVariantMap bankOperationRow(const BankCsvOperation& operation, const QString& rowKey,
    const QString& currency, const QString& fallbackCategoryId, const QString& statementAccountId)
{
    return {{"rowKey", rowKey}, {"sourceRow", operation.sourceRow},
        {"fingerprint", operation.reviewFingerprint}, {"occurredAt", operation.occurredAt},
        {"date", operation.occurredAt.toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))},
        {"signedMinor", operation.signedMinor}, {"currency", currency}, {"statementAccountId", statementAccountId},
        {"type", operation.signedMinor > 0 ? QStringLiteral("income") : QStringLiteral("expense")},
        {"description", operation.description}, {"rawRecipient", operation.rawRecipient},
        {"recipientId", operation.recipientId}, {"bankCategory", operation.categoryName},
        {"fallbackCategoryId", fallbackCategoryId}};
}

QVariantMap investmentTermsToVariant(const InvestmentTerms& terms);
bool isTransfer(const Transaction& transaction)
{
    return transaction.categoryId() == QStringLiteral("transfer-in") ||
           transaction.categoryId() == QStringLiteral("transfer-out");
}

bool parseDepositParameters(
    const double annualRatePercent,
    const QString& payoutFrequency,
    const int requestedPayoutDay,
    int& annualRateBasisPoints,
    DepositPayoutFrequency& frequency,
    int& payoutDay
    )
{
    if (!std::isfinite(annualRatePercent) ||
        annualRatePercent <= 0.0 || annualRatePercent > 1'000.0) {
        return false;
    }
    annualRateBasisPoints = static_cast<int>(
        std::llround(annualRatePercent * 100.0));
    if (annualRateBasisPoints <= 0 || annualRateBasisPoints > 100'000) {
        return false;
    }

    const QString normalized = payoutFrequency.trimmed().toLower();
    if (normalized == QStringLiteral("daily")) {
        frequency = DepositPayoutFrequency::Daily;
        payoutDay = 0;
        return true;
    }
    if (normalized == QStringLiteral("monthly") &&
        requestedPayoutDay >= 1 && requestedPayoutDay <= 31) {
        frequency = DepositPayoutFrequency::Monthly;
        payoutDay = requestedPayoutDay;
        return true;
    }
    return false;
}

QString depositPayoutFrequencyToString(
    const DepositPayoutFrequency frequency
    )
{
    return frequency == DepositPayoutFrequency::Daily
        ? QStringLiteral("daily")
        : QStringLiteral("monthly");
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

qint64 saturatedCapitalAdd(const qint64 left, const qint64 right)
{
    using Int128 = __int128_t;
    const Int128 total = static_cast<Int128>(left) +
        static_cast<Int128>(right);
    if (total > std::numeric_limits<qint64>::max()) {
        return std::numeric_limits<qint64>::max();
    }
    if (total < std::numeric_limits<qint64>::min()) {
        return std::numeric_limits<qint64>::min();
    }
    return static_cast<qint64>(total);
}

qint64 saturatedCapitalSubtract(const qint64 left, const qint64 right)
{
    using Int128 = __int128_t;
    const Int128 total = static_cast<Int128>(left) -
        static_cast<Int128>(right);
    if (total > std::numeric_limits<qint64>::max()) {
        return std::numeric_limits<qint64>::max();
    }
    if (total < std::numeric_limits<qint64>::min()) {
        return std::numeric_limits<qint64>::min();
    }
    return static_cast<qint64>(total);
}

qint64 positiveMinorMagnitude(const qint64 value)
{
    using Int128 = __int128_t;
    const Int128 magnitude = value < 0
        ? -static_cast<Int128>(value)
        : static_cast<Int128>(value);
    return magnitude > std::numeric_limits<qint64>::max()
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(magnitude);
}

QString bankCsvTransactionId(
    const QString& accountId,
    const BankCsvOperation& operation,
    const int fingerprintOccurrence)
{
    const QString identity = !operation.externalId.isEmpty()
        ? QStringLiteral("external|") + accountId + QLatin1Char('|') +
            operation.externalId.trimmed()
        : QStringLiteral("row|") + accountId + QLatin1Char('|') +
            operation.fingerprint + QLatin1Char('|') +
            QString::number(fingerprintOccurrence);
    return QStringLiteral("bankcsv-") + QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString legacyBankCsvTransactionId(
    const QString& accountId,
    const BankCsvOperation& operation,
    const int fingerprintOccurrence)
{
    if (!operation.externalId.isEmpty())
        return bankCsvTransactionId(accountId, operation, fingerprintOccurrence);
    const QString identity = QStringLiteral("row|") + accountId + QLatin1Char('|') +
        operation.legacyFingerprint + QLatin1Char('|') +
        QString::number(fingerprintOccurrence);
    return QStringLiteral("bankcsv-") + QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool looksLikeTransfer(const QString& description)
{
    const QString value = description.toLower();
    return value.contains(QStringLiteral("перевод")) ||
        value.contains(QStringLiteral("сбп")) ||
        value.contains(QStringLiteral("transfer")) ||
        value.contains(QStringLiteral("пополнение карты"));
}

QVariantMap bankCsvProfileToVariant(const BankCsvProfile& profile)
{
    return {
        {QStringLiteral("id"), profile.id},
        {QStringLiteral("name"), profile.name},
        {QStringLiteral("accountId"), profile.accountId},
        {QStringLiteral("incomeCategoryId"), profile.incomeCategoryId},
        {QStringLiteral("expenseCategoryId"), profile.expenseCategoryId},
        {QStringLiteral("encoding"), profile.encoding},
        {QStringLiteral("delimiter"), profile.delimiter},
        {QStringLiteral("dateFormat"), profile.dateFormat},
        {QStringLiteral("amountMode"), profile.amountMode},
        {QStringLiteral("headerRow"), profile.headerRow},
        {QStringLiteral("dateColumn"), profile.dateColumn},
        {QStringLiteral("amountColumn"), profile.amountColumn},
        {QStringLiteral("incomeColumn"), profile.incomeColumn},
        {QStringLiteral("expenseColumn"), profile.expenseColumn},
        {QStringLiteral("descriptionColumn"), profile.descriptionColumn},
        {QStringLiteral("recipientColumn"), profile.recipientColumn},
        {QStringLiteral("recipientIdColumn"), profile.recipientIdColumn},
        {QStringLiteral("idColumn"), profile.idColumn},
        {QStringLiteral("categoryColumn"), profile.categoryColumn},
        {QStringLiteral("directionColumn"), profile.directionColumn},
        {QStringLiteral("currencyColumn"), profile.currencyColumn},
        {QStringLiteral("positiveMeansIncome"), profile.positiveMeansIncome}
    };
}

BankCsvProfile bankCsvProfileFromVariant(const QVariantMap& values)
{
    BankCsvProfile profile;
    profile.id = values.value(QStringLiteral("id")).toString().trimmed();
    profile.name = values.value(QStringLiteral("name")).toString().trimmed();
    profile.accountId = values.value(
        QStringLiteral("accountId")).toString();
    profile.incomeCategoryId = values.value(
        QStringLiteral("incomeCategoryId")).toString();
    profile.expenseCategoryId = values.value(
        QStringLiteral("expenseCategoryId")).toString();
    profile.encoding = values.value(
        QStringLiteral("encoding"), QStringLiteral("auto")).toString();
    profile.delimiter = values.value(
        QStringLiteral("delimiter"), QStringLiteral("auto")).toString();
    profile.dateFormat = values.value(
        QStringLiteral("dateFormat"), QStringLiteral("auto")).toString();
    profile.amountMode = values.value(
        QStringLiteral("amountMode"), QStringLiteral("signed")).toString();
    profile.headerRow = values.value(QStringLiteral("headerRow"), 0).toInt();
    profile.dateColumn = values.value(
        QStringLiteral("dateColumn"), -1).toInt();
    profile.amountColumn = values.value(
        QStringLiteral("amountColumn"), -1).toInt();
    profile.incomeColumn = values.value(
        QStringLiteral("incomeColumn"), -1).toInt();
    profile.expenseColumn = values.value(
        QStringLiteral("expenseColumn"), -1).toInt();
    profile.recipientColumn = values.value(QStringLiteral("recipientColumn"), -1).toInt();
    profile.recipientIdColumn = values.value(QStringLiteral("recipientIdColumn"), -1).toInt();
    profile.descriptionColumn = values.value(
        QStringLiteral("descriptionColumn"), -1).toInt();
    profile.idColumn = values.value(QStringLiteral("idColumn"), -1).toInt();
    profile.categoryColumn = values.value(
        QStringLiteral("categoryColumn"), -1).toInt();
    profile.directionColumn = values.value(
        QStringLiteral("directionColumn"), -1).toInt();
    profile.currencyColumn = values.value(
        QStringLiteral("currencyColumn"), -1).toInt();
    profile.positiveMeansIncome = values.value(
        QStringLiteral("positiveMeansIncome"), true).toBool();
    return profile;
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
    const quint64 magnitude=value<0?static_cast<quint64>(-(value+1))+1:static_cast<quint64>(value);
    const auto whole=magnitude/InvestmentPosition::Scale, fraction=magnitude%InvestmentPosition::Scale;
    const auto sign=value<0?QStringLiteral("-"):QString();
    if(fraction==0 || maximumFractionDigits<=0)return sign+QString::number(whole);
    auto decimals=QStringLiteral("%1").arg(fraction,6,10,QLatin1Char('0'));
    decimals.truncate(maximumFractionDigits);while(decimals.endsWith(QLatin1Char('0')))decimals.chop(1);
    return sign+QString::number(whole)+(decimals.isEmpty()?QString():QStringLiteral(",")+decimals);
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
    case InvestmentInstrumentType::PreferredStock: return QCoreApplication::translate("FinanceController", "Привилегированная акция");
    case InvestmentInstrumentType::DepositaryReceipt: return QCoreApplication::translate("FinanceController", "Депозитарная расписка");
    case InvestmentInstrumentType::Metal: return QCoreApplication::translate("FinanceController", "Драгоценный металл");
    case InvestmentInstrumentType::Currency: return QCoreApplication::translate("FinanceController", "Валюта");
    case InvestmentInstrumentType::Future: return QCoreApplication::translate("FinanceController", "Фьючерс");
    case InvestmentInstrumentType::Option: return QCoreApplication::translate("FinanceController", "Опцион");
    case InvestmentInstrumentType::Other: return QCoreApplication::translate("FinanceController", "Другой инструмент");
    }
    return {};
}
}

FinanceController::FinanceController(QObject* parent, const QString& databasePath)
    : QObject(parent)
    , repository_(databasePath)
    , currencyConverter_(rateProvider_)
    , balanceCalculator_(currencyConverter_)
{
    notesSaveTimer_.setSingleShot(true);
    notesSaveTimer_.setInterval(700);
    connect(&notesSaveTimer_, &QTimer::timeout, this,
            [this]() { saveNotes(); });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, [this]() { saveNotes(); });
    loadNotes();

    QObject::connect(
        this,
        &FinanceController::balanceChanged,
        this,
        [this]()
        {
            rebuildCapitalHistory();
            emit capitalHistoryChanged();
            emit analyticsChanged();
        });

    QObject::connect(this, &FinanceController::balanceChanged,
                     this, &FinanceController::financialGoalsChanged);
    QObject::connect(this, &FinanceController::balanceChanged,
                     this, [this]() {
                         captureCapitalSnapshot();
                         emit financialTrajectoryChanged();
                     });
    QObject::connect(this, &FinanceController::transactionsChanged,
                     this, &FinanceController::financialTrajectoryChanged);
    QObject::connect(this, &FinanceController::accountsChanged,
                     this, &FinanceController::financialGoalsChanged);
    QObject::connect(this, &FinanceController::cryptoWalletsChanged,
                     this, &FinanceController::financialGoalsChanged);
    QObject::connect(this, &FinanceController::investmentPositionsChanged,
                     this, &FinanceController::financialGoalsChanged);
    QObject::connect(this, &FinanceController::uiLanguageChanged,
                     this, &FinanceController::financialGoalsChanged);
    auto* goalClock = new QTimer(this);
    goalClock->setInterval(60'000);
    connect(goalClock, &QTimer::timeout, this, &FinanceController::financialGoalsChanged);
    goalClock->start(); // Deadlines and future-dated transactions can change while open.
    auto* trajectoryClock = new QTimer(this);
    trajectoryClock->setInterval(60'000);
    connect(trajectoryClock, &QTimer::timeout, this, [this]() {
        captureCapitalSnapshot(false);
    });
    trajectoryClock->start(); // Capture the next day even if no balance signal fires.

    QObject::connect(this, &FinanceController::transactionsChanged,
                     this, &FinanceController::budgetsChanged);
    QObject::connect(this, &FinanceController::accountsChanged,
                     this, &FinanceController::budgetsChanged);
    QObject::connect(this, &FinanceController::categoriesChanged,
                     this, &FinanceController::budgetsChanged);
    QObject::connect(this, &FinanceController::appCurrencyChanged,
                     this, &FinanceController::budgetsChanged);

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
            emit projectsChanged();
            emit budgetsChanged();
        });

    connect(&bybitProvider_, &BybitProvider::snapshotReady, this,
            [this](const QString& id, const QVariantList& holdings, const QVariantList& operations, qint64 fetchedAtMs) {
        if (!repository_.saveCryptoExchangeSnapshot(id, holdings, operations, fetchedAtMs)) {
            exchangeErrors_[id] = tr("Не удалось сохранить данные Bybit");
            setCryptoLastError(exchangeErrors_[id]);
            return;
        }
        cryptoExchanges_ = repository_.loadCryptoExchanges();
        exchangeErrors_.remove(id);
        emit cryptoWalletsChanged(); emit cryptoTransactionsChanged(); emit balanceChanged();
    });
    connect(&bybitProvider_, &BybitProvider::finished, this, [this](const QString& id, const QString& error) {
        finishCryptoWalletRequest(id); finishCryptoRequest();
        if (!error.isEmpty()) { exchangeErrors_[id] = error; setCryptoLastError(error); }
        emit cryptoWalletsChanged();
    });
    exchangeRefreshTimer_.setInterval(60'000);
    connect(&exchangeRefreshTimer_, &QTimer::timeout, this, [this] { refreshCryptoExchanges(false); });
    exchangeRefreshTimer_.start();

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
            emit balanceChanged();
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
            if (cryptoWallets_.isEmpty()) return;
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
            if (cryptoWallets_.isEmpty()) return;
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
                ? tr("По запросу не найдено инвестиционных инструментов")
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
    QObject::connect(&investmentProvider_, &MoexInvestmentProvider::instrumentResolved, this,
        [this](const InvestmentMarketInstrument& instrument) {
            for(auto& result:investmentSearchResults_)if(result.id()==instrument.id()){result=instrument;break;}
            emit investmentSearchResultsChanged();
        });
    QObject::connect(
        &investmentProvider_,
        &MoexInvestmentProvider::quoteSucceeded,
        this,
        [this](const InvestmentMarketInstrument& quoted)
        {
            const auto& instrumentId=quoted.id();
            for(auto& found:investmentSearchResults_)if(found.id()==instrumentId){found=quoted;break;}
            for(const auto& instrument:investmentInstruments_)if(instrument.id()==instrumentId){
                if(!repository_.saveInvestmentQuote(InvestmentQuote(instrumentId,quoted.priceMicros(),quoted.quotedAtUtc(),quoted.terms())))
                    qWarning()<<"Failed to save investment quote:"<<repository_.lastError();
                else {investmentQuotes_=repository_.loadInvestmentQuotes();emit investmentPositionsChanged();emit accountsChanged();emit balanceChanged();}
                break;
            }
            if(requestedSearchQuoteId_==instrumentId){requestedSearchQuoteId_.clear();investmentQuoteBusy_=false;investmentLastError_.clear();
                emit investmentSearchResultsChanged();emit investmentSearchStateChanged();}
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

    recurringTimer_.setSingleShot(true);
    QObject::connect(
        &recurringTimer_,
        &QTimer::timeout,
        this,
        &FinanceController::materializeRecurringTransactions);

    if (!repository_.isOpen()) {
        qWarning() << "Failed to open finance database:"
                   << repository_.lastError();
        return;
    }

    appCurrency_ = currencyFromString(repository_.loadAppCurrency());
    const Currency storedAnalyticsCurrency = currencyFromString(
        repository_.loadAnalyticsCurrency());
    analyticsCurrency_ = storedAnalyticsCurrency == Currency::EUR
        ? Currency::EUR
        : Currency::USD;
    uiLanguage_ = repository_.loadUiLanguage() == QStringLiteral("en")
        ? QStringLiteral("en")
        : QStringLiteral("ru");
    manualRubToRubRate_ = repository_.loadManualRubToRubRate();
    manualUsdToRubRate_ = repository_.loadManualUsdToRubRate();
    manualEurToRubRate_ = repository_.loadManualEurToRubRate();
    if (!rateProvider_.setManualRates(
            manualRubToRubRate_,
            manualUsdToRubRate_,
            manualEurToRubRate_)) {
        qWarning() << "Failed to apply stored manual currency rates";
    }
    automaticCurrencyRates_ = repository_.loadAutomaticCurrencyRates();
    rateProvider_.setAutomaticUpdatesEnabled(automaticCurrencyRates_);
    selectedAsset_ = assetTypeFromString(repository_.loadSelectedAsset());
    transactions_ = repository_.loadTransactions();
    projects_ = repository_.loadProjects();
    recurringTransactions_ = repository_.loadRecurringTransactions();
    categories_ = repository_.loadCategories();
    accounts_ = repository_.loadAccounts();
    depositSettings_ = repository_.loadDepositSettings();
    budgets_ = repository_.loadBudgets();
    financialGoals_ = repository_.loadFinancialGoals();
    trajectorySettings_ = repository_.loadFinancialTrajectorySettings();
    cryptoWallets_ = repository_.loadCryptoWallets();
    for (const auto& wallet : cryptoWallets_) cryptoWalletNames_[wallet.id()] = repository_.cryptoWalletName(wallet.id());
    cryptoExchanges_ = repository_.loadCryptoExchanges();
    QTimer::singleShot(0, this, [this] { refreshCryptoExchanges(false); });
    cryptoTransactions_ = repository_.loadCryptoTransactions();
    investmentInstruments_ = repository_.loadInvestmentInstruments();
    investmentPositions_ = repository_.loadInvestmentPositions();
    investmentQuotes_ = repository_.loadInvestmentQuotes();
    investmentOperations_ = repository_.loadInvestmentOperations();
    if (!projects_.isEmpty()) {
        selectedProjectId_ = projects_.constFirst().id();
    }
    selectedBudgetMonth_ = QDate(
        QDate::currentDate().year(), QDate::currentDate().month(), 1);
    if (!budgets_.isEmpty()) {
        selectedBudgetId_ = budgets_.constFirst().id();
    }
    refreshBudgetMonthLimits();
    if (!cryptoWallets_.isEmpty()) {
        selectedCryptoWalletId_ = cryptoWallets_.constFirst().id();
    } else if (!cryptoExchanges_.isEmpty()) {
        selectedCryptoWalletId_ = cryptoExchanges_.constFirst().toMap().value("id").toString();
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
    for (const FinanceRepository::BankCsvProfileRecord& record :
         repository_.loadBankCsvProfiles()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            record.configurationJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject()) {
            qWarning() << "Invalid bank CSV profile:" << record.id;
            continue;
        }
        BankCsvProfile profile = BankCsvProfile::fromJson(document.object());
        profile.id = record.id;
        profile.name = record.name;
        bankCsvProfiles_.append(profile);
    }

    rebuildCapitalHistory();
    captureCapitalSnapshot();
    scheduleRecurringMaterialization();
    QTimer::singleShot(
        0,
        this,
        &FinanceController::scheduleInitialCryptoRefresh);
    if (selectedAsset_ == AssetType::Investment) {
        QTimer::singleShot(0, this, &FinanceController::refreshInvestmentQuotes);
    }
}

QString FinanceController::notesText() const
{
    return notesText_;
}

bool FinanceController::notesDirty() const
{
    return notesDirty_;
}

bool FinanceController::notesAvailable() const
{
    return notesAvailable_;
}

QString FinanceController::notesError() const
{
    return notesError_;
}

void FinanceController::loadNotes()
{
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        notesAvailable_ = false;
        notesError_ = tr("Не удалось найти каталог для заметок");
        emit notesChanged();
        return;
    }

    notesFilePath_ = QDir(directory).filePath(QStringLiteral("notes.txt"));
    if (!QFile::exists(notesFilePath_)) {
        return;
    }

    QFile file(notesFilePath_);
    if (!file.open(QIODevice::ReadOnly)) {
        notesAvailable_ = false;
        notesError_ = tr("Не удалось открыть заметки: %1").arg(file.errorString());
        emit notesChanged();
        return;
    }
    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        notesAvailable_ = false;
        notesError_ = tr("Не удалось прочитать заметки: %1").arg(file.errorString());
        emit notesChanged();
        return;
    }
    notesText_ = QString::fromUtf8(contents);
    emit notesChanged();
}

void FinanceController::setNotesText(const QString& text)
{
    if (!notesAvailable_ || notesText_ == text) {
        return;
    }
    notesText_ = text;
    notesDirty_ = true;
    notesError_.clear();
    notesSaveTimer_.start();
    emit notesChanged();
}

bool FinanceController::saveNotes()
{
    if (!notesDirty_) {
        return notesError_.isEmpty();
    }
    const QString directory = QFileInfo(notesFilePath_).absolutePath();
    if (notesFilePath_.isEmpty() || !QDir().mkpath(directory)) {
        notesError_ = tr("Не удалось создать каталог для заметок");
        emit notesChanged();
        return false;
    }

    QSaveFile file(notesFilePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        notesError_ = tr("Не удалось сохранить заметки: %1").arg(file.errorString());
        emit notesChanged();
        return false;
    }
    const QByteArray contents = notesText_.toUtf8();
    if (file.write(contents) != contents.size() || !file.commit()) {
        notesError_ = tr("Не удалось сохранить заметки: %1").arg(file.errorString());
        emit notesChanged();
        return false;
    }
    notesSaveTimer_.stop();
    notesDirty_ = false;
    notesError_.clear();
    emit notesChanged();
    return true;
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
    emit projectsChanged();
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
    emit scheduledTransactionsChanged();
    emit projectsChanged();
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

double FinanceController::manualRubToRubRate() const
{
    return manualRubToRubRate_;
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
    const qint64 rubRate = rateProvider_.rateToUsd(Currency::RUB);
    if (rubRate <= 0) {
        return result;
    }

    for (const Currency currency : {Currency::RUB, Currency::USD, Currency::EUR}) {
        QVariantMap item;
        item[QStringLiteral("code")] = currencyCode(currency);
        item[QStringLiteral("rate")] =
            static_cast<double>(rateProvider_.rateToUsd(currency)) /
            static_cast<double>(rubRate);
        result.append(item);
    }
    return result;
}

bool FinanceController::saveManualCurrencyRates(
    const double rublesPerRub,
    const double rublesPerUsd,
    const double rublesPerEur
    )
{
    if (!rateProvider_.setManualRates(
            rublesPerRub, rublesPerUsd, rublesPerEur)) {
        return false;
    }
    if (repository_.isOpen() &&
        !repository_.saveManualCurrencyRates(
            rublesPerRub, rublesPerUsd, rublesPerEur)) {
        qWarning() << "Failed to save manual currency rates:"
                   << repository_.lastError();
        rateProvider_.setManualRates(
            manualRubToRubRate_,
            manualUsdToRubRate_,
            manualEurToRubRate_);
        return false;
    }

    manualRubToRubRate_ = rublesPerRub;
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

QVariantMap FinanceController::backupDatabase(const QUrl& fileUrl)
{
    QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), tr("Не выбрано место для резервной копии")}};
    }
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".sqlite3");
    if (!repository_.backupDatabase(path)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), repository_.lastError()}};
    }
    return {{QStringLiteral("ok"), true}, {QStringLiteral("path"), path}};
}

QVariantMap FinanceController::restoreDatabase(const QUrl& fileUrl)
{
    qint64 added = 0, skipped = 0;
    if (!repository_.restoreDatabase(fileUrl.toLocalFile(), &added, &skipped)) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), repository_.lastError()}};
    }
    transactions_ = repository_.loadTransactions();
    projects_ = repository_.loadProjects();
    recurringTransactions_ = repository_.loadRecurringTransactions();
    categories_ = repository_.loadCategories();
    archivedCategoryIds_ = repository_.loadArchivedCategoryIds();
    accounts_ = repository_.loadAccounts();
    depositSettings_ = repository_.loadDepositSettings();
    budgets_ = repository_.loadBudgets();
    financialGoals_ = repository_.loadFinancialGoals();
    trajectorySettings_ = repository_.loadFinancialTrajectorySettings();
    cryptoWallets_ = repository_.loadCryptoWallets();
    for (const auto& wallet : cryptoWallets_) cryptoWalletNames_[wallet.id()] = repository_.cryptoWalletName(wallet.id());
    cryptoExchanges_ = repository_.loadCryptoExchanges();
    cryptoTransactions_ = repository_.loadCryptoTransactions();
    investmentInstruments_ = repository_.loadInvestmentInstruments();
    investmentPositions_ = repository_.loadInvestmentPositions();
    investmentQuotes_ = repository_.loadInvestmentQuotes();
    investmentOperations_ = repository_.loadInvestmentOperations();
    for (const QString& symbol : {QStringLiteral("USDT"), QStringLiteral("BTC"), QStringLiteral("ETH")}) {
        const auto price = repository_.loadCryptoPrice(symbol);
        if (price.priceUsdMicros > 0) cryptoPricesUsdMicros_[symbol] = price.priceUsdMicros;
        if (price.fetchedAtUtc.isValid()) cryptoPricesFetchedAtUtc_[symbol] = price.fetchedAtUtc;
    }
    bankCsvProfiles_.clear();
    for (const auto& record : repository_.loadBankCsvProfiles()) {
        const auto document = QJsonDocument::fromJson(record.configurationJson.toUtf8());
        if (!document.isObject()) continue;
        auto profile = BankCsvProfile::fromJson(document.object());
        profile.id = record.id;
        profile.name = record.name;
        bankCsvProfiles_.append(profile);
    }
    summary_ = repository_.loadSummary();
    if (selectedProjectId_.isEmpty() && !projects_.isEmpty()) {
        selectedProjectId_ = projects_.constFirst().id();
        emit selectedProjectIdChanged();
    }
    if (selectedBudgetId_.isEmpty() && !budgets_.isEmpty()) {
        selectedBudgetId_ = budgets_.constFirst().id();
        emit selectedBudgetIdChanged();
    }
    if (selectedCryptoWalletId_.isEmpty()) {
        if (!cryptoWallets_.isEmpty()) selectedCryptoWalletId_ = cryptoWallets_.constFirst().id();
        else if (!cryptoExchanges_.isEmpty()) selectedCryptoWalletId_ = cryptoExchanges_.constFirst().toMap().value("id").toString();
        emit selectedCryptoWalletIdChanged();
    }
    refreshBudgetMonthLimits();
    emit accountsChanged();
    emit categoriesChanged();
    emit transactionsChanged();
    emit projectsChanged();
    emit scheduledTransactionsChanged();
    emit budgetsChanged();
    emit financialGoalsChanged();
    emit bankCsvProfilesChanged();
    emit cryptoWalletsChanged();
    emit cryptoTransactionsChanged();
    emit investmentPositionsChanged();
    emit balanceChanged(); // Rebuilds history, analytics, goals and the current capital snapshot.
    scheduleRecurringMaterialization();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("added"), added},
            {QStringLiteral("skipped"), skipped}};
}

QVariantMap FinanceController::clearAllData()
{
    if (!repository_.clearAllUserData()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), repository_.lastError()}};
    }

    notesSaveTimer_.stop();
    const bool notesRemoved = notesFilePath_.isEmpty() ||
        !QFile::exists(notesFilePath_) || QFile::remove(notesFilePath_);
    if (notesRemoved) {
        notesText_.clear();
        notesDirty_ = false;
        notesAvailable_ = true;
        notesError_.clear();
    } else {
        notesError_ = tr("Не удалось удалить файл заметок");
    }
    emit notesChanged();

    recurringTimer_.stop();
    recurringMaterializationScheduled_ = false;
    transactions_.clear();
    projects_.clear();
    recurringTransactions_.clear();
    categories_ = repository_.loadCategories();
    accounts_.clear();
    depositSettings_.clear();
    budgets_.clear();
    financialGoals_.clear();
    trajectorySettings_ = repository_.loadFinancialTrajectorySettings();
    for (const auto& value : cryptoExchanges_) {
        const auto id = value.toMap().value("id").toString();
        if (refreshingCryptoWalletIds_.contains(id)) { bybitProvider_.cancel(id); finishCryptoWalletRequest(id); finishCryptoRequest(); }
        (void)QtConcurrent::run([id] { ExchangeCredentials::remove(id); });
    }
    cryptoExchanges_.clear(); exchangeCredentials_.clear(); exchangeErrors_.clear(); exchangeCredentialWarnings_.clear(); cryptoWalletNames_.clear();
    cryptoWallets_.clear();
    cryptoTransactions_.clear();
    investmentInstruments_.clear();
    investmentPositions_.clear();
    investmentOperations_.clear();
    investmentQuotes_.clear();
    investmentSearchResults_.clear();
    selectedInvestmentSearchIndex_ = -1;
    requestedSearchQuoteId_.clear();
    investmentSearchBusy_ = false;
    investmentQuoteBusy_ = false;
    investmentRefreshing_ = false;
    refreshingInvestmentIds_.clear();
    investmentLastError_.clear();
    bankCsvProfiles_.clear();
    archivedCategoryIds_ = repository_.loadArchivedCategoryIds();
    cryptoPricesUsdMicros_.clear();
    cryptoPricesFetchedAtUtc_.clear();
    cryptoPricesUsdMicros_.insert(QStringLiteral("USDT"), 1'000'000);
    lastCryptoRefreshAttemptUtc_ = {};
    refreshingCryptoWalletIds_.clear();
    pendingCryptoWalletRequests_.clear();
    pendingCryptoRequests_ = 0;
    cryptoRefreshing_ = false;
    cryptoLastError_.clear();
    selectedAccountId_.clear();
    selectedCryptoWalletId_.clear();
    selectedProjectId_.clear();
    selectedBudgetId_.clear();
    selectedBudgetMonth_ = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1);
    budgetMonthLimits_.clear();
    dateFilterFrom_ = {};
    dateFilterTo_ = {};
    lastCapitalSnapshotDate_ = {};
    appCurrency_ = Currency::RUB;
    analyticsCurrency_ = Currency::USD;
    uiLanguage_ = QStringLiteral("ru");
    automaticCurrencyRates_ = true;
    manualRubToRubRate_ = 1.0;
    manualUsdToRubRate_ = 90.909090909;
    manualEurToRubRate_ = 106.363636364;
    selectedAsset_ = AssetType::Fiat;
    rateProvider_.setManualRates(
        manualRubToRubRate_, manualUsdToRubRate_, manualEurToRubRate_);
    rateProvider_.setAutomaticUpdatesEnabled(true);
    summary_ = repository_.loadSummary();

    emit transactionsChanged();
    emit categoriesChanged();
    emit accountsChanged();
    emit cryptoWalletsChanged();
    emit cryptoRefreshingChanged();
    emit cryptoLastErrorChanged();
    emit cryptoTransactionsChanged();
    emit investmentPositionsChanged();
    emit investmentSearchResultsChanged();
    emit investmentSearchStateChanged();
    emit investmentRefreshingChanged();
    emit projectsChanged();
    emit scheduledTransactionsChanged();
    emit budgetsChanged();
    emit financialGoalsChanged();
    emit bankCsvProfilesChanged();
    emit selectedAssetChanged();
    emit selectedAccountIdChanged();
    emit selectedCryptoWalletIdChanged();
    emit selectedProjectIdChanged();
    emit selectedBudgetIdChanged();
    emit selectedBudgetMonthChanged();
    emit appCurrencyChanged();
    emit analyticsChanged();
    emit uiLanguageChanged();
    emit automaticCurrencyRatesChanged();
    emit manualCurrencyRatesChanged();
    emit currencyRatesChanged();
    emit dateFilterChanged();
    emit financialTrajectoryChanged();
    emit balanceChanged();
    if (!notesRemoved) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 tr("Финансовые данные очищены, но файл заметок удалить не удалось")}};
    }
    return {{QStringLiteral("ok"), true}};
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

QVariantList FinanceController::bankCsvProfiles() const
{
    QVariantList result;
    result.reserve(bankCsvProfiles_.size());
    for (const BankCsvProfile& profile : bankCsvProfiles_) {
        result.append(bankCsvProfileToVariant(profile));
    }
    return result;
}

QVariantList FinanceController::scheduledTransactions() const
{
    QVariantList result;
    const QLocale locale(uiLanguage_ == QStringLiteral("en")
        ? QLocale::English
        : QLocale::Russian);
    const QDate today = QDate::currentDate();

    for (const RecurringTransaction& recurring : recurringTransactions_) {
        QString accountName;
        for (const Account& account : accounts_) {
            if (account.id() == recurring.accountId()) {
                accountName = accountDisplayName(account);
                break;
            }
        }

        QString category;
        for (const Category& candidate : categories_) {
            if (candidate.id() == recurring.categoryId()) {
                category = categoryDisplayName(candidate);
                break;
            }
        }

        QString recurrence;
        QString schedule;
        if (recurring.recurrenceType() == RecurrenceType::Daily) {
            recurrence = QStringLiteral("daily");
            schedule = tr("Ежедневно");
        } else if (recurring.recurrenceType() == RecurrenceType::Weekly) {
            recurrence = QStringLiteral("weekly");
            schedule = tr("Еженедельно: %1").arg(locale.standaloneDayName(
                recurring.weekday(), QLocale::LongFormat));
        } else if (recurring.recurrenceType() == RecurrenceType::MonthlyDay) {
            recurrence = QStringLiteral("monthly_day");
            schedule = tr("Ежемесячно: %1-е число")
                .arg(recurring.dayOfMonth());
        } else {
            recurrence = QStringLiteral("monthly_weekday");
            const QStringList weeks{
                tr("первая"), tr("вторая"), tr("третья"),
                tr("четвёртая"), tr("последняя")};
            schedule = tr("Ежемесячно: %1 неделя, %2")
                .arg(weeks.value(recurring.weekOfMonth() - 1))
                .arg(locale.standaloneDayName(
                    recurring.weekday(), QLocale::LongFormat));
        }

        QDate nextFrom = today;
        if (recurring.generatedThrough().isValid() &&
            recurring.generatedThrough() >= nextFrom) {
            nextFrom = recurring.generatedThrough().addDays(1);
        }
        const QDate next = RecurringScheduleCalculator::nextOccurrence(
            recurring, nextFrom);

        QVariantMap item;
        item[QStringLiteral("id")] = recurring.id();
        item[QStringLiteral("name")] = recurring.name();
        item[QStringLiteral("accountId")] = recurring.accountId();
        item[QStringLiteral("accountName")] = accountName;
        item[QStringLiteral("categoryId")] = recurring.categoryId();
        item[QStringLiteral("categoryName")] = category;
        item[QStringLiteral("type")] =
            recurring.transactionType() == TransactionType::Income
                ? QStringLiteral("income")
                : QStringLiteral("expense");
        item[QStringLiteral("amount")] = recurring.amountMinor();
        item[QStringLiteral("currency")] = currencyCode(recurring.currency());
        item[QStringLiteral("recurrence")] = recurrence;
        item[QStringLiteral("weekday")] = recurring.weekday();
        item[QStringLiteral("dayOfMonth")] = recurring.dayOfMonth();
        item[QStringLiteral("weekOfMonth")] = recurring.weekOfMonth();
        item[QStringLiteral("startsOn")] =
            recurring.startsOn().toString(Qt::ISODate);
        item[QStringLiteral("nextDate")] = next.toString(Qt::ISODate);
        item[QStringLiteral("scheduleText")] = schedule;
        result.append(item);
    }

    std::sort(
        result.begin(), result.end(),
        [](const QVariant& left, const QVariant& right)
        {
            const QVariantMap leftMap = left.toMap();
            const QVariantMap rightMap = right.toMap();
            const QString leftDate = leftMap.value(
                QStringLiteral("nextDate")).toString();
            const QString rightDate = rightMap.value(
                QStringLiteral("nextDate")).toString();
            if (leftDate != rightDate) {
                return leftDate < rightDate;
            }
            return leftMap.value(QStringLiteral("name")).toString()
                .localeAwareCompare(
                    rightMap.value(QStringLiteral("name")).toString()) < 0;
        });
    return result;
}

QVariantMap FinanceController::inspectBankCsv(
    const QUrl& fileUrl,
    const int headerRow,
    const QString& delimiter,
    const QString& encoding
    ) const
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    const QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty()) {
        result[QStringLiteral("error")] = tr("Не выбран файл выписки");
        return result;
    }

    BankCsvProfile optionsProfile;
    optionsProfile.delimiter = delimiter;
    optionsProfile.encoding = encoding;
    const CsvCodec::ReadResult csv = BankCsvImporter::readTable(
        filePath, optionsProfile);
    return buildBankCsvInspection(csv, filePath, headerRow);
}

int FinanceController::inspectBankCsvAsync(const QUrl& fileUrl, const int headerRow,
    const QString& delimiter, const QString& encoding)
{
    const int requestId = ++bankCsvInspectionRequestId_;
    const QString filePath = fileUrl.toLocalFile();
    BankCsvProfile profile;
    profile.delimiter = delimiter; profile.encoding = encoding;
    auto* watcher = new QFutureWatcher<CsvCodec::ReadResult>(this);
    connect(watcher, &QFutureWatcher<CsvCodec::ReadResult>::finished, this,
        [this, watcher, requestId, filePath, headerRow] {
            const auto table = watcher->result();
            watcher->deleteLater();
            emit bankCsvInspectionFinished(requestId,
                buildBankCsvInspection(table, filePath, headerRow));
        });
    watcher->setFuture(QtConcurrent::run([filePath, profile] {
        return BankCsvImporter::readTable(filePath, profile);
    }));
    return requestId;
}

QVariantMap FinanceController::buildBankCsvInspection(const CsvCodec::ReadResult& csv,
    const QString& filePath, const int headerRow) const
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    if (!csv.error.isEmpty()) {
        result[QStringLiteral("error")] = csv.error;
        return result;
    }
    const int effectiveHeaderRow = filePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive) ? 0 : headerRow;
    if (effectiveHeaderRow < 0 || effectiveHeaderRow >= csv.rows.size()) {
        result[QStringLiteral("error")] = tr("Строка заголовков вне файла");
        return result;
    }

    QVariantList headers;
    const QStringList& sourceHeaders = csv.rows[effectiveHeaderRow];
    headers.reserve(sourceHeaders.size());
    for (qsizetype index = 0; index < sourceHeaders.size(); ++index) {
        QString label = sourceHeaders[index].trimmed();
        if (label.isEmpty()) {
            label = tr("Столбец %1").arg(index + 1);
        }
        headers.append(QVariantMap{
            {QStringLiteral("label"), label},
            {QStringLiteral("value"), index}
        });
    }

    QVariantList preview;
    const int previewLimit = filePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive) ? 1000 : 5;
    for (qsizetype index = effectiveHeaderRow + 1;
         index < csv.rows.size() && preview.size() < previewLimit; ++index) {
        bool empty = true;
        for (const QString& field : csv.rows[index]) {
            if (!field.trimmed().isEmpty()) {
                empty = false;
                break;
            }
        }
        if (!empty) {
            QStringList fields = csv.rows[index];
            if (previewLimit > 5 && fields.size() > 1 && fields[1].isEmpty())
                fields[1] = tr("Сумма не распознана — строка будет отклонена");
            preview.append(fields.join(QStringLiteral(" | ")));
        }
    }

    QString detectedDelimiter = QStringLiteral("auto");
    if (csv.delimiter == QLatin1Char(',')) {
        detectedDelimiter = QStringLiteral("comma");
    } else if (csv.delimiter == QLatin1Char('\t')) {
        detectedDelimiter = QStringLiteral("tab");
    }
    QString detectedEncoding = csv.encoding == QStringLiteral("XLSX")
        ? QStringLiteral("auto") : QStringLiteral("utf8");
    if (csv.encoding == QStringLiteral("Windows-1251")) {
        detectedEncoding = QStringLiteral("windows1251");
    } else if (csv.encoding == QStringLiteral("UTF-16LE")) {
        detectedEncoding = QStringLiteral("utf16le");
    } else if (csv.encoding == QStringLiteral("UTF-16BE")) {
        detectedEncoding = QStringLiteral("utf16be");
    }

    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("headers")] = headers;
    result[QStringLiteral("preview")] = preview;
    result[QStringLiteral("rowCount")] = csv.rows.size();
    result[QStringLiteral("previewTruncated")] = csv.rows.size() - effectiveHeaderRow - 1 > previewLimit;
    result[QStringLiteral("detectedDelimiter")] = detectedDelimiter;
    result[QStringLiteral("detectedEncoding")] = detectedEncoding;
    result[QStringLiteral("encodingLabel")] = csv.encoding;
    result[QStringLiteral("delimiterLabel")] = (csv.encoding == QStringLiteral("XLSX") || csv.encoding.startsWith(QStringLiteral("PDF")))
        ? tr("не используется")
        : csv.delimiter == QLatin1Char('\t') ? tr("табуляция")
                                             : QString(csv.delimiter);
    return result;
}

QVariantMap FinanceController::previewBankImport(
    const QUrl& fileUrl,
    const QVariantMap& values) const
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    const QString filePath = fileUrl.toLocalFile();
    const BankCsvProfile profile = bankCsvProfileFromVariant(values);
    if (filePath.isEmpty() || profile.dateColumn < 0 ||
        (profile.amountMode == QStringLiteral("signed") && profile.amountColumn < 0) ||
        (profile.amountMode == QStringLiteral("separate") &&
         (profile.incomeColumn < 0 || profile.expenseColumn < 0))) {
        result[QStringLiteral("error")] = tr("Сначала сопоставьте дату и сумму");
        return result;
    }
    const auto account = std::find_if(accounts_.cbegin(), accounts_.cend(),
        [&profile](const Account& candidate) {
            return candidate.id() == profile.accountId &&
                candidate.assetType() == AssetType::Fiat;
        });
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Выбранный фиатный счёт не найден");
        return result;
    }
    const BankCsvParseResult parsed = BankCsvImporter::parse(filePath, profile);
    if (parsed.operations.isEmpty()) {
        result[QStringLiteral("error")] = parsed.errors.isEmpty()
            ? tr("В файле не найдено операций")
            : parsed.errors.join(QStringLiteral("; "));
        return result;
    }
    QVariantList operationRows;
    QSet<QString> previewIds;
    int reviewCount = 0;
    qint64 incomeMinor = 0;
    qint64 expenseMinor = 0;
    int currencyMismatches = 0;
    int duplicates = 0;
    int possibleTransfers = 0;
    QDate firstDate;
    QDate lastDate;
    const QString accountCurrency = currencyCode(account->currency());
    QSet<QString> existingIds;
    for (const auto& transaction : transactions_) {
        existingIds.insert(transaction.id());
        if (isTransfer(transaction)) existingIds.insert(transferId(transaction));
    }
    QString linkError;
    const auto bankLinks = repository_.loadBankImportLinks(&linkError);
    if (!linkError.isEmpty()) { result["error"] = linkError; return result; }
    for (auto it = bankLinks.cbegin(); it != bankLinks.cend(); ++it) existingIds.insert(it.key());
    QHash<QString, int> fingerprintOccurrences;
    QHash<QString, int> legacyFingerprintOccurrences;
    for (const auto& operation : parsed.operations) {
        if (!operation.currencyCode.isEmpty() &&
            operation.currencyCode != accountCurrency) {
            ++currencyMismatches;
            continue;
        }
        const int occurrence = operation.externalId.isEmpty()
            ? fingerprintOccurrences[operation.fingerprint]++ : 0;
        const int legacyOccurrence = operation.externalId.isEmpty()
            ? legacyFingerprintOccurrences[operation.legacyFingerprint]++ : 0;
        const QString rowKey = bankCsvTransactionId(account->id(), operation, occurrence);
        const bool duplicate = existingIds.contains(rowKey) || previewIds.contains(rowKey) ||
            existingIds.contains(legacyBankCsvTransactionId(account->id(), operation, legacyOccurrence));
        previewIds.insert(rowKey);
        if (duplicate) ++duplicates;
        if (!duplicate) {
            const bool income = operation.signedMinor > 0;
            operationRows.append(bankOperationRow(operation, rowKey, accountCurrency,
                income ? profile.incomeCategoryId : profile.expenseCategoryId, account->id()));
        }
        if (looksLikeTransfer(operation.description)) {
            const auto match = std::find_if(
                transactions_.cbegin(), transactions_.cend(),
                [&operation, &account](const Transaction& candidate) {
                    return candidate.accountId() != account->id() &&
                        !isTransfer(candidate) &&
                        candidate.money().currency() == account->currency() &&
                        candidate.money().minorUnits() ==
                            positiveMinorMagnitude(operation.signedMinor) &&
                        ((operation.signedMinor > 0) !=
                         (candidate.type() == TransactionType::Income)) &&
                        std::abs(candidate.date().date().daysTo(
                            operation.occurredAt.date())) <= 2 &&
                        looksLikeTransfer(candidate.description());
                });
            if (match != transactions_.cend()) ++possibleTransfers;
        }
        if (operation.signedMinor > 0)
            incomeMinor = saturatedCapitalAdd(incomeMinor, operation.signedMinor);
        else
            expenseMinor = saturatedCapitalAdd(
                expenseMinor, positiveMinorMagnitude(operation.signedMinor));
        const QDate date = operation.occurredAt.date();
        if (!firstDate.isValid() || date < firstDate) firstDate = date;
        if (!lastDate.isValid() || date > lastDate) lastDate = date;
    }
    const auto reviewed = resolveBankImportRows(operationRows, {});
    if (!reviewed.value("ok").toBool()) return reviewed;
    operationRows = reviewed.value("operationRows").toList();
    for (const auto& row : operationRows)
        if (row.toMap().value("needsReview").toBool()) ++reviewCount;
    result[QStringLiteral("operationRows")] = operationRows;
    result[QStringLiteral("reviewCount")] = reviewCount;
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("operationCount")] = parsed.operations.size() - currencyMismatches;
    result[QStringLiteral("newCount")] = parsed.operations.size() -
        currencyMismatches - duplicates;
    result[QStringLiteral("duplicateCount")] = duplicates;
    result[QStringLiteral("possibleTransfers")] = possibleTransfers;
    result[QStringLiteral("incomeMinor")] = incomeMinor;
    result[QStringLiteral("expenseMinor")] = expenseMinor;
    result[QStringLiteral("currencyMismatches")] = currencyMismatches;
    result[QStringLiteral("rejected")] = parsed.rejected;
    result[QStringLiteral("firstDate")] =
        firstDate.toString(QStringLiteral("dd.MM.yyyy"));
    result[QStringLiteral("lastDate")] =
        lastDate.toString(QStringLiteral("dd.MM.yyyy"));
    result[QStringLiteral("currency")] = accountCurrency;
    result[QStringLiteral("warnings")] = parsed.errors;
    return result;
}

QVariantMap FinanceController::saveBankCsvProfile(const QVariantMap& values)
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    BankCsvProfile profile = bankCsvProfileFromVariant(values);
    if (profile.id.isEmpty()) {
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (profile.name.isEmpty() || profile.headerRow < 0 ||
        profile.dateColumn < 0) {
        result[QStringLiteral("error")] =
            tr("Заполните название, строку заголовков и столбец даты");
        return result;
    }
    if (profile.amountMode == QStringLiteral("signed")) {
        if (profile.amountColumn < 0) {
            result[QStringLiteral("error")] = tr("Выберите столбец суммы");
            return result;
        }
    } else if (profile.amountMode == QStringLiteral("separate")) {
        if (profile.incomeColumn < 0 || profile.expenseColumn < 0 ||
            profile.incomeColumn == profile.expenseColumn) {
            result[QStringLiteral("error")] =
                tr("Выберите разные столбцы дохода и расхода");
            return result;
        }
    } else {
        result[QStringLiteral("error")] = tr("Неизвестный способ хранения суммы");
        return result;
    }

    const QSet<QString> allowedDelimiters{
        QStringLiteral("auto"), QStringLiteral("semicolon"),
        QStringLiteral("comma"), QStringLiteral("tab")};
    const QSet<QString> allowedEncodings{
        QStringLiteral("auto"), QStringLiteral("utf8"),
        QStringLiteral("windows1251"), QStringLiteral("utf16le"),
        QStringLiteral("utf16be")};
    if (!allowedDelimiters.contains(profile.delimiter) ||
        !allowedEncodings.contains(profile.encoding)) {
        result[QStringLiteral("error")] = tr("Неверный разделитель или кодировка");
        return result;
    }

    const auto account = std::find_if(
        accounts_.cbegin(), accounts_.cend(),
        [&profile](const Account& candidate)
        {
            return candidate.id() == profile.accountId &&
                candidate.assetType() == AssetType::Fiat;
        });
    const auto categoryIsValid = [this](
        const QString& id,
        const CategoryType type
        )
    {
        return std::any_of(
            categories_.cbegin(), categories_.cend(),
            [this, &id, type](const Category& category)
            {
                return category.id() == id && category.type() == type &&
                    !archivedCategoryIds_.contains(id);
            });
    };
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Выбранный фиатный счёт не найден");
        return result;
    }
    if (!categoryIsValid(profile.incomeCategoryId, CategoryType::Income) ||
        !categoryIsValid(profile.expenseCategoryId, CategoryType::Expense)) {
        result[QStringLiteral("error")] = tr("Выберите категории дохода и расхода");
        return result;
    }
    for (const BankCsvProfile& existing : std::as_const(bankCsvProfiles_)) {
        if (existing.id != profile.id &&
            existing.name.compare(profile.name, Qt::CaseInsensitive) == 0) {
            result[QStringLiteral("error")] =
                tr("Профиль с таким названием уже существует");
            return result;
        }
    }

    const QString configuration = QString::fromUtf8(
        QJsonDocument(profile.toJson()).toJson(QJsonDocument::Compact));
    if (!repository_.saveBankCsvProfile(
            profile.id, profile.name, configuration)) {
        result[QStringLiteral("error")] = repository_.lastError();
        return result;
    }

    const auto existing = std::find_if(
        bankCsvProfiles_.begin(), bankCsvProfiles_.end(),
        [&profile](const BankCsvProfile& candidate)
        {
            return candidate.id == profile.id;
        });
    if (existing == bankCsvProfiles_.end()) {
        bankCsvProfiles_.append(profile);
    } else {
        *existing = profile;
    }
    std::sort(
        bankCsvProfiles_.begin(), bankCsvProfiles_.end(),
        [](const BankCsvProfile& left, const BankCsvProfile& right)
        {
            return left.name.localeAwareCompare(right.name) < 0;
        });
    emit bankCsvProfilesChanged();
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("id")] = profile.id;
    return result;
}

bool FinanceController::deleteBankCsvProfile(const QString& id)
{
    const auto existing = std::find_if(
        bankCsvProfiles_.begin(), bankCsvProfiles_.end(),
        [&id](const BankCsvProfile& profile)
        {
            return profile.id == id;
        });
    if (existing == bankCsvProfiles_.end() ||
        !repository_.deleteBankCsvProfile(id)) {
        return false;
    }
    bankCsvProfiles_.erase(existing);
    emit bankCsvProfilesChanged();
    return true;
}

QVariantMap FinanceController::saveScheduledTransaction(
    const QVariantMap& values
    )
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    QString id = values.value(QStringLiteral("id")).toString().trimmed();
    const QString name = values.value(
        QStringLiteral("name")).toString().trimmed();
    const QString accountId = values.value(
        QStringLiteral("accountId")).toString();
    const QString categoryId = values.value(
        QStringLiteral("categoryId")).toString();
    const QString type = values.value(
        QStringLiteral("type")).toString().trimmed().toLower();
    const qint64 amount = values.value(
        QStringLiteral("amount")).toLongLong();
    const QString amountCurrencyCode = values.value(
        QStringLiteral("currency")).toString().trimmed().toUpper();
    const QString recurrence = values.value(
        QStringLiteral("recurrence")).toString().trimmed().toLower();
    const int weekday = values.value(
        QStringLiteral("weekday"), 1).toInt();
    const int dayOfMonth = values.value(
        QStringLiteral("dayOfMonth"), 1).toInt();
    const int weekOfMonth = values.value(
        QStringLiteral("weekOfMonth"), 1).toInt();
    const QDate startsOn = QDate::fromString(
        values.value(QStringLiteral("startsOn")).toString(), Qt::ISODate);

    if (name.isEmpty() || amount <= 0 || !startsOn.isValid()) {
        result[QStringLiteral("error")] =
            tr("Заполните название, сумму и дату начала");
        return result;
    }
    if (amountCurrencyCode != QStringLiteral("RUB") &&
        amountCurrencyCode != QStringLiteral("USD") &&
        amountCurrencyCode != QStringLiteral("EUR")) {
        result[QStringLiteral("error")] = tr("Выберите валюту суммы");
        return result;
    }
    if (type != QStringLiteral("income") &&
        type != QStringLiteral("expense")) {
        result[QStringLiteral("error")] = tr("Выберите тип операции");
        return result;
    }

    RecurrenceType recurrenceType;
    if (recurrence == QStringLiteral("daily")) {
        recurrenceType = RecurrenceType::Daily;
    } else if (recurrence == QStringLiteral("weekly")) {
        recurrenceType = RecurrenceType::Weekly;
    } else if (recurrence == QStringLiteral("monthly_day")) {
        recurrenceType = RecurrenceType::MonthlyDay;
    } else if (recurrence == QStringLiteral("monthly_weekday")) {
        recurrenceType = RecurrenceType::MonthlyWeekday;
    } else {
        result[QStringLiteral("error")] = tr("Выберите расписание");
        return result;
    }
    if (weekday < 1 || weekday > 7 ||
        dayOfMonth < 1 || dayOfMonth > 31 ||
        weekOfMonth < 1 || weekOfMonth > 5) {
        result[QStringLiteral("error")] = tr("Проверьте параметры расписания");
        return result;
    }

    const auto account = std::find_if(
        accounts_.cbegin(), accounts_.cend(),
        [&accountId](const Account& candidate)
        {
            return candidate.id() == accountId &&
                candidate.assetType() == AssetType::Fiat;
        });
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Выберите фиатный счёт");
        return result;
    }

    const CategoryType requiredCategory = type == QStringLiteral("income")
        ? CategoryType::Income
        : CategoryType::Expense;
    const bool categoryIsValid = std::any_of(
        categories_.cbegin(), categories_.cend(),
        [this, &categoryId, requiredCategory](const Category& category)
        {
            return category.id() == categoryId &&
                category.type() == requiredCategory &&
                !archivedCategoryIds_.contains(categoryId);
        });
    if (!categoryIsValid) {
        result[QStringLiteral("error")] = tr("Выберите подходящую категорию");
        return result;
    }

    const auto existing = std::find_if(
        recurringTransactions_.cbegin(), recurringTransactions_.cend(),
        [&id](const RecurringTransaction& recurring)
        {
            return recurring.id() == id;
        });
    if (!id.isEmpty() && existing == recurringTransactions_.cend()) {
        result[QStringLiteral("error")] =
            tr("Запланированная операция не найдена");
        return result;
    }
    const bool editing = existing != recurringTransactions_.cend();
    if (!editing) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    const QDate generatedThrough = editing
        ? existing->generatedThrough()
        : (startsOn > QDate::currentDate()
            ? startsOn.addDays(-1)
            : QDate::currentDate().addDays(-1));
    const RecurringTransaction recurring(
        id,
        name,
        accountId,
        categoryId,
        type == QStringLiteral("income")
            ? TransactionType::Income
            : TransactionType::Expense,
        amount,
        currencyFromString(amountCurrencyCode),
        recurrenceType,
        weekday,
        dayOfMonth,
        weekOfMonth,
        startsOn,
        generatedThrough);

    const bool saved = editing
        ? repository_.updateRecurringTransaction(recurring)
        : repository_.insertRecurringTransaction(recurring);
    if (!saved) {
        result[QStringLiteral("error")] = repository_.lastError();
        return result;
    }

    recurringTransactions_ = repository_.loadRecurringTransactions();
    emit scheduledTransactionsChanged();
    scheduleRecurringMaterialization();
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("id")] = id;
    return result;
}

bool FinanceController::deleteScheduledTransaction(const QString& id)
{
    const bool exists = std::any_of(
        recurringTransactions_.cbegin(), recurringTransactions_.cend(),
        [&id](const RecurringTransaction& recurring)
        {
            return recurring.id() == id;
        });
    if (!exists || !repository_.deleteRecurringTransaction(id)) {
        return false;
    }
    recurringTransactions_ = repository_.loadRecurringTransactions();
    emit scheduledTransactionsChanged();
    return true;
}

QString FinanceController::normalizeBankCategoryText(const QString& text) const
{
    return BankCategoryMatcher::normalize(text);
}

QVariantMap FinanceController::bankCategoryRules() const
{
    QString error;
    const auto rules = repository_.loadBankCategoryRules(&error);
    QVariantList items;
    for (const auto& rule : rules) {
        QString name;
        for (const auto& category : categories_)
            if (category.id() == rule.categoryId) { name = categoryDisplayName(category); break; }
        items.append(QVariantMap{{QStringLiteral("pattern"), rule.pattern},
            {QStringLiteral("matchMode"), rule.matchMode}, {QStringLiteral("field"), rule.field},
            {QStringLiteral("categoryId"), rule.categoryId},
            {QStringLiteral("categoryName"), name},
            {QStringLiteral("type"), rule.type == CategoryType::Income ? QStringLiteral("income") : QStringLiteral("expense")}});
    }
    return {{QStringLiteral("ok"), error.isEmpty()}, {QStringLiteral("error"), error},
            {QStringLiteral("items"), items}};
}

QVariantMap FinanceController::deleteBankCategoryRule(const QString& pattern,
    const QString& matchMode, const QString& type)
{
    if ((type != QStringLiteral("income") && type != QStringLiteral("expense")) ||
        (matchMode != QStringLiteral("exact") && matchMode != QStringLiteral("contains")))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), tr("Некорректное правило")}};
    const bool ok = repository_.deleteBankCategoryRule(BankCategoryMatcher::normalize(pattern), matchMode,
        type == QStringLiteral("income") ? CategoryType::Income : CategoryType::Expense);
    return {{QStringLiteral("ok"), ok}, {QStringLiteral("error"), ok ? QString() : repository_.lastError()}};
}

QVariantMap FinanceController::resolveBankImportRows(const QVariantList& rows,
    const QVariantMap& choices) const
{
    QString error;
    const auto categories = repository_.loadBankCategoryRules(&error);
    if (!error.isEmpty()) return {{"ok", false}, {"error", error}};
    const auto recipients = repository_.loadBankRecipientRules(&error);
    if (!error.isEmpty()) return {{"ok", false}, {"error", error}};
    auto review = BankImportReview::resolve(rows, choices, categories_,
        archivedCategoryIds_, categories, recipients);
    if (review.error.isEmpty()) {
        for (auto& value : review.rows) {
            auto row = value.toMap();
            if (row.value("type").toString() != "transfer") continue;
            const auto choice = choices.value(row.value("rowKey").toString()).toMap();
            const auto details = bankImportTransferDetails(row, choice);
            if (choice.value("confirmed").toBool() && !details.value("ok").toBool()) {
                review.error = details.value("error").toString(); break;
            }
            row["needsReview"] = !choice.value("confirmed").toBool() || !details.value("ok").toBool();
            row["reason"] = row.value("needsReview").toBool()
                ? details.value("error", tr("Подтвердите перевод")) : tr("Перевод подтверждён");
            value = row;
        }
    }
    return {{"ok", review.error.isEmpty()}, {"error", review.error}, {"operationRows", review.rows}};
}

QVariantMap FinanceController::bankImportTransferDetails(const QVariantMap& row,
    const QVariantMap& choice) const
{
    QVariantMap result{{"ok", false}};
    const auto fail = [&](const QString& message) { auto failed = result; failed["error"] = message; return failed; };
    const Account* source = nullptr;
    const Account* target = nullptr;
    for (const auto& account : accounts_) {
        if (account.id() == choice.value("sourceAccountId").toString()) source = &account;
        if (account.id() == choice.value("targetAccountId").toString()) target = &account;
    }
    if (!source || !target || source->assetType() != AssetType::Fiat || target->assetType() != AssetType::Fiat)
        return fail(tr("Выберите счёт списания и счёт зачисления"));
    if (source->id() == target->id()) return fail(tr("Для перевода нужны разные счета"));
    const qint64 signedMinor = row.value("signedMinor").toLongLong();
    const bool income = signedMinor > 0;
    const Account* own = income ? target : source;
    const Account* peer = income ? source : target;
    if (!signedMinor || own->id() != row.value("statementAccountId").toString()
        || currencyCode(own->currency()) != row.value("currency").toString())
        return fail(tr("Счёт выписки должен совпадать со счётом %1")
            .arg(income ? tr("зачисления") : tr("списания")));
    const qint64 ownMinor = positiveMinorMagnitude(signedMinor);
    qint64 peerMinor = ownMinor;
    if (source->currency() != target->currency()) {
        qint64 peerMicros = 0;
        if (!parsePositiveMicros(choice.value("peerAmount").toString(), peerMicros) || peerMicros % 10'000 != 0)
            return fail(tr("Укажите фактическую сумму на втором счёте в его валюте, до 2 знаков после запятой"));
        peerMinor = peerMicros / 10'000;
    }
    const auto date = row.value("occurredAt").toDateTime();
    if (!date.isValid()) return fail(tr("Проверьте дату перевода"));
    result["sourceMinor"] = income ? peerMinor : ownMinor;
    result["targetMinor"] = income ? ownMinor : peerMinor;
    QString error;
    const auto links = repository_.loadBankImportLinks(&error);
    if (!error.isEmpty()) return fail(error);
    QSet<QString> linkedLegs;
    for (auto it = links.cbegin(); it != links.cend(); ++it) linkedLegs.insert(it.value());
    QVariantList matches;
    for (const auto& transaction : transactions_) {
        if (std::abs(transaction.date().toLocalTime().date().daysTo(date.toLocalTime().date())) > 2) continue;
        if (isTransfer(transaction)) {
            if (transaction.accountId() != own->id() || linkedLegs.contains(transaction.id())
                || transaction.type() != (income ? TransactionType::Income : TransactionType::Expense)
                || transaction.money().currency() != own->currency() || transaction.money().minorUnits() != ownMinor) continue;
            const QString id = transferId(transaction);
            const auto opposite = std::find_if(transactions_.cbegin(), transactions_.cend(), [&](const auto& candidate) {
                return candidate.id() == id + (income ? QStringLiteral("-out") : QStringLiteral("-in"))
                    && candidate.accountId() == peer->id() && candidate.money().currency() == peer->currency()
                    && candidate.money().minorUnits() == peerMinor
                    && candidate.categoryId() == (income ? "transfer-out" : "transfer-in");
            });
            if (id.isEmpty() || opposite == transactions_.cend()) continue;
            matches.append(QVariantMap{{"value", transaction.id()}, {"kind", "transfer"},
                {"label", tr("Уже учтённый перевод · %1 · %2").arg(transaction.date().toLocalTime().toString("dd.MM.yyyy"), transaction.description())}});
        } else if (transaction.id().startsWith("bankcsv-") && transaction.projectId().isEmpty()
            && transaction.accountId() == peer->id() && transaction.money().currency() == peer->currency()
            && transaction.money().minorUnits() == peerMinor
            && transaction.type() == (income ? TransactionType::Expense : TransactionType::Income)) {
            matches.append(QVariantMap{{"value", transaction.id()}, {"kind", "operation"},
                {"label", tr("Объединить с операцией второго счёта · %1 · %2")
                    .arg(transaction.date().toLocalTime().toString("dd.MM.yyyy"), transaction.description())}});
        }
    }
    result["matches"] = matches;
    if (!matches.isEmpty() && !choice.contains("transferMatchId"))
        return fail(tr("Найдена похожая операция. Выберите её или создание нового перевода"));
    const QString selected = choice.value("transferMatchId").toString();
    if (!selected.isEmpty()) {
        const auto match = std::find_if(matches.cbegin(), matches.cend(), [&](const auto& item) {
            return item.toMap().value("value").toString() == selected;
        });
        if (match == matches.cend()) return fail(tr("Выбранный перевод изменился. Обновите предварительный просмотр"));
        result["matchKind"] = match->toMap().value("kind");
    }
    result["ok"] = true;
    return result;
}

QVariantMap FinanceController::bankRecipientRules() const
{
    QString error;
    const auto rules = repository_.loadBankRecipientRules(&error);
    QVariantList items;
    for (const auto& rule : rules)
        items.append(QVariantMap{{"pattern", rule.pattern}, {"field", rule.field},
            {"matchMode", rule.matchMode}, {"recipientName", rule.name},
            {"type", rule.type == CategoryType::Income ? QStringLiteral("income") : QStringLiteral("expense")}});
    return {{"ok", error.isEmpty()}, {"error", error}, {"items", items}};
}

QVariantMap FinanceController::deleteBankRecipientRule(const QString& pattern,
    const QString& field, const QString& matchMode, const QString& type)
{
    if ((type != "income" && type != "expense") || (matchMode != "exact" && matchMode != "contains") ||
        (field != "description" && field != "recipient" && field != "bank_recipient" && field != "recipient_id"))
        return {{"ok", false}, {"error", tr("Некорректное правило получателя")}};
    const bool ok = repository_.deleteBankRecipientRule(BankRecipientMatcher::normalize(pattern), field,
        matchMode, type == "income" ? CategoryType::Income : CategoryType::Expense);
    return {{"ok", ok}, {"error", ok ? QString() : repository_.lastError()}};
}

QVariantMap FinanceController::importBankCsv(
    const QUrl& fileUrl,
    const QString& profileId,
    const QVariantMap& options
    )
{
    QVariantMap result{
        {QStringLiteral("ok"), false},
        {QStringLiteral("imported"), 0},
        {QStringLiteral("skipped"), 0},
        {QStringLiteral("rejected"), 0}
    };
    const QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty()) {
        result[QStringLiteral("error")] = tr("Не выбран файл выписки");
        return result;
    }
    const auto profile = std::find_if(
        bankCsvProfiles_.cbegin(), bankCsvProfiles_.cend(),
        [&profileId](const BankCsvProfile& candidate)
        {
            return candidate.id == profileId;
        });
    if (profile == bankCsvProfiles_.cend()) {
        result[QStringLiteral("error")] = tr("Профиль импорта не найден");
        return result;
    }
    const auto account = std::find_if(
        accounts_.cbegin(), accounts_.cend(),
        [&profile](const Account& candidate)
        {
            return candidate.id() == profile->accountId &&
                candidate.assetType() == AssetType::Fiat;
        });
    if (account == accounts_.cend()) {
        result[QStringLiteral("error")] = tr("Счёт из профиля не найден");
        return result;
    }

    const BankCsvParseResult parsed = BankCsvImporter::parse(
        filePath, *profile);
    result[QStringLiteral("rejected")] = parsed.rejected;
    if (parsed.operations.isEmpty()) {
        result[QStringLiteral("error")] = parsed.errors.isEmpty()
            ? tr("В выписке не найдено операций")
            : parsed.errors.join(QStringLiteral("; "));
        return result;
    }

    const auto categoryById = [this](
        const QString& id,
        const CategoryType type
        ) -> const Category*
    {
        for (const Category& category : categories_) {
            if (id != QStringLiteral("transfer-in") && id != QStringLiteral("transfer-out") &&
                category.id() == id && category.type() == type && !archivedCategoryIds_.contains(id)) {
                return &category;
            }
        }
        return nullptr;
    };
    QString ruleError;
    const auto rules = repository_.loadBankCategoryRules(&ruleError);
    if (!ruleError.isEmpty()) { result["error"] = ruleError; return result; }
    const auto recipientRules = repository_.loadBankRecipientRules(&ruleError);
    if (!ruleError.isEmpty()) { result["error"] = ruleError; return result; }
    const QVariantMap choices = options.value("choices").toMap();
    const bool requireReview = options.value("requireReview", true).toBool();
    QSet<QString> usedChoices, existingIds, pendingIds;
    for (const auto& transaction : std::as_const(transactions_)) {
        existingIds.insert(transaction.id());
        if (isTransfer(transaction)) existingIds.insert(transferId(transaction));
    }
    const auto previousLinks = repository_.loadBankImportLinks(&ruleError);
    if (!ruleError.isEmpty()) { result["error"] = ruleError; return result; }
    for (auto it = previousLinks.cbegin(); it != previousLinks.cend(); ++it) existingIds.insert(it.key());
    QHash<QString, int> fingerprintOccurrences, legacyFingerprintOccurrences;
    QVariantList rows;
    int skipped = 0;
    int currencyRejected = 0;
    const QString accountCurrency = currencyCode(account->currency());
    for (const auto& operation : parsed.operations) {
        if (!operation.currencyCode.isEmpty() && operation.currencyCode != accountCurrency) {
            ++currencyRejected;
            continue;
        }
        const int occurrence = operation.externalId.isEmpty()
            ? fingerprintOccurrences[operation.fingerprint]++ : 0;
        const int legacyOccurrence = operation.externalId.isEmpty()
            ? legacyFingerprintOccurrences[operation.legacyFingerprint]++ : 0;
        const QString transactionId = bankCsvTransactionId(account->id(), operation, occurrence);
        if (existingIds.contains(transactionId) || pendingIds.contains(transactionId) ||
            existingIds.contains(legacyBankCsvTransactionId(account->id(), operation, legacyOccurrence))) {
            ++skipped;
            continue;
        }
        pendingIds.insert(transactionId);
        usedChoices.insert(transactionId);
        rows.append(bankOperationRow(operation, transactionId, accountCurrency,
            operation.signedMinor > 0 ? profile->incomeCategoryId : profile->expenseCategoryId, account->id()));
    }
    const auto review = BankImportReview::resolve(rows, choices, categories_, archivedCategoryIds_, rules, recipientRules);
    if (!review.error.isEmpty()) { result["error"] = review.error; return result; }
    QVector<Transaction> imported;
    QMap<QString, QString> newLinks;
    QSet<QString> replacedBankTransactions, matchedOperations;
    int importedOperations = 0, transferCount = 0;
    for (const auto& value : review.rows) {
        const auto row = value.toMap();
        const bool income = row.value("signedMinor").toLongLong() > 0;
        const QString categoryId = row.value("categoryId").toString();
        if (row.value("type").toString() == "transfer") {
            const auto choice = choices.value(row.value("rowKey").toString()).toMap();
            const auto details = bankImportTransferDetails(row, choice);
            if (!choice.value("confirmed").toBool() || !details.value("ok").toBool()) {
                result["error"] = details.value("error", tr("Подтвердите перевод в строке %1").arg(row.value("sourceRow").toInt()));
                return result;
            }
            const QString matchId = choice.value("transferMatchId").toString();
            if (!matchId.isEmpty() && matchedOperations.contains(matchId)) {
                result["error"] = tr("Одна операция выбрана для нескольких переводов. Проверьте совпадения."); return result;
            }
            if (!matchId.isEmpty()) matchedOperations.insert(matchId);
            const QString rowKey = row.value("rowKey").toString();
            if (details.value("matchKind").toString() == "transfer") {
                newLinks.insert(rowKey, matchId);
            } else {
                const QString id = rowKey;
                const QString sourceId = choice.value("sourceAccountId").toString();
                const QString targetId = choice.value("targetAccountId").toString();
                const auto source = std::find_if(accounts_.cbegin(), accounts_.cend(), [&](const auto& a) { return a.id() == sourceId; });
                const auto target = std::find_if(accounts_.cbegin(), accounts_.cend(), [&](const auto& a) { return a.id() == targetId; });
                const auto opposite = std::find_if(transactions_.cbegin(), transactions_.cend(), [&](const auto& t) { return t.id() == matchId; });
                const bool combine = details.value("matchKind").toString() == "operation";
                const auto ownDate = row.value("occurredAt").toDateTime();
                const auto description = row.value("description").toString();
                const auto peerDate = combine ? opposite->date() : ownDate;
                const auto peerDescription = combine ? opposite->description() : description;
                imported.append(Transaction(id + "-out", sourceId, "transfer-out",
                    Money(details.value("sourceMinor").toLongLong(), source->currency()), TransactionType::Expense,
                    income ? peerDate : ownDate, income ? peerDescription : description));
                imported.append(Transaction(id + "-in", targetId, "transfer-in",
                    Money(details.value("targetMinor").toLongLong(), target->currency()), TransactionType::Income,
                    income ? ownDate : peerDate, income ? description : peerDescription));
                newLinks.insert(rowKey, id + (income ? "-in" : "-out"));
                if (combine) {
                    replacedBankTransactions.insert(matchId);
                    newLinks.insert(matchId, id + (income ? "-out" : "-in"));
                }
            }
            ++importedOperations; ++transferCount;
            continue;
        }
        if (!categoryById(categoryId, income ? CategoryType::Income : CategoryType::Expense) ||
            (requireReview && row.value("needsReview").toBool())) {
            result["error"] = tr("Проверьте получателя и категорию в строке %1").arg(row.value("sourceRow").toInt());
            return result;
        }
        ++importedOperations;
        TransactionRecipient recipient{{}, {}, QStringLiteral("unresolved")};
        if (row.value("recipientStatus").toString() == "known")
            recipient = {row.value("merchant").toString(), row.value("recipientKey").toString(), row.value("recipientSource").toString()};
        const auto description = row.value("description").toString();
        imported.append(Transaction(row.value("rowKey").toString(),
            account->id(), categoryId,
            Money(positiveMinorMagnitude(row.value("signedMinor").toLongLong()), account->currency()),
            income ? TransactionType::Income : TransactionType::Expense,
            row.value("occurredAt").toDateTime(),
            description.isEmpty() ? tr("Импорт из банковской выписки") : description, {}, recipient));
    }
    if (imported.isEmpty() && currencyRejected > 0 && skipped == 0) {
        result[QStringLiteral("error")] = tr(
            "Все операции имеют валюту, отличную от валюты выбранного счёта");
        result[QStringLiteral("rejected")] = parsed.rejected + currencyRejected;
        return result;
    }

    // Reject stale choices for removed/changed rows; duplicates from a previous successful import are harmless.
    for (auto it = choices.cbegin(); it != choices.cend(); ++it) {
        if (!usedChoices.contains(it.key()) && !existingIds.contains(it.key())) {
            result[QStringLiteral("error")] = tr("Выписка изменилась. Обновите предварительный просмотр.");
            return result;
        }
    }
    if (!repository_.insertTransactions(imported, review.categoryRules, review.recipientRules, newLinks, replacedBankTransactions)) {
        result[QStringLiteral("error")] = repository_.lastError();
        return result;
    }
    transactions_ = repository_.loadTransactions();
    summary_ = repository_.loadSummary();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();

    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("imported")] = importedOperations;
    result[QStringLiteral("transfers")] = transferCount;
    result[QStringLiteral("rememberedRules")] = review.categoryRules.size();
    result[QStringLiteral("rememberedRecipientRules")] = review.recipientRules.size();
    result[QStringLiteral("skipped")] = skipped;
    result[QStringLiteral("rejected")] = parsed.rejected + currencyRejected;
    QStringList warnings = parsed.errors;
    if (currencyRejected > 0) {
        warnings.append(tr("Пропущено строк с валютой, отличной от валюты счёта: %1")
            .arg(currencyRejected));
    }
    result[QStringLiteral("warnings")] = warnings;
    return result;
}

QVariantList FinanceController::transactions() const
{
    QVariantList result;

    for (const Transaction& transaction : dateFilteredTransactions()) {
        result.append(transactionToVariant(transaction));
    }

    return result;
}

QVariantList FinanceController::projects() const
{
    QVariantList result;
    result.reserve(projects_.size());
    for (const Project& project : projects_) {
        qint64 incomeMinor = 0;
        qint64 expenseMinor = 0;
        int operationCount = 0;
        for (const Transaction& transaction : transactions_) {
            if (transaction.projectId() != project.id() ||
                isTransfer(transaction)) {
                continue;
            }
            const qint64 converted = currencyConverter_.convert(
                transaction.money(), appCurrency_).minorUnits();
            if (transaction.type() == TransactionType::Income) {
                incomeMinor += converted;
            } else {
                expenseMinor += converted;
            }
            ++operationCount;
        }

        QVariantMap item;
        item[QStringLiteral("id")] = project.id();
        item[QStringLiteral("name")] = project.name();
        item[QStringLiteral("incomeMinor")] = incomeMinor;
        item[QStringLiteral("expenseMinor")] = expenseMinor;
        item[QStringLiteral("resultMinor")] = incomeMinor - expenseMinor;
        item[QStringLiteral("operationCount")] = operationCount;
        result.append(item);
    }
    return result;
}

QString FinanceController::selectedProjectId() const
{
    return selectedProjectId_;
}

void FinanceController::setSelectedProjectId(const QString& id)
{
    if (id == selectedProjectId_ || (!id.isEmpty() && !hasProject(id))) {
        return;
    }
    selectedProjectId_ = id;
    emit selectedProjectIdChanged();
    emit projectsChanged();
}

QVariantList FinanceController::projectTransactions() const
{
    QVariantList result;
    if (selectedProjectId_.isEmpty()) {
        return result;
    }
    for (const Transaction& transaction : transactions_) {
        if (transaction.projectId() == selectedProjectId_) {
            result.append(transactionToVariant(transaction));
        }
    }
    return result;
}

QVariantList FinanceController::budgets() const
{
    QVariantList result;
    const QDate monthStart(
        selectedBudgetMonth_.year(), selectedBudgetMonth_.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);

    for (const Budget& budget : budgets_) {
        qint64 spentMinor = 0;
        QHash<QString, qint64> spentByCategory;
        bool rateMissing = false;
        int operationCount = 0;
        for (const Transaction& transaction : transactions_) {
            const QDate date = transaction.date().toLocalTime().date();
            if (date < monthStart || date > monthEnd ||
                !transactionMatchesBudget(transaction, budget)) {
                continue;
            }
            if (transaction.money().currency() != budget.currency() &&
                (rateProvider_.rateToUsd(transaction.money().currency()) <= 0 ||
                 rateProvider_.rateToUsd(budget.currency()) <= 0)) {
                rateMissing = true;
                continue;
            }
            const qint64 converted = currencyConverter_.convert(
                transaction.money(), budget.currency()).minorUnits();
            spentMinor = saturatedCapitalAdd(spentMinor, converted);
            spentByCategory[transaction.categoryId()] = saturatedCapitalAdd(
                spentByCategory.value(transaction.categoryId()), converted);
            ++operationCount;
        }

        const qint64 limitMinor = budgetMonthLimits_.value(
            budget.id(), budget.defaultLimitMinor());
        const qint64 remainingMinor = limitMinor - spentMinor;
        const bool appRateMissing = budget.currency() != appCurrency_ &&
            (rateProvider_.rateToUsd(budget.currency()) <= 0 ||
             rateProvider_.rateToUsd(appCurrency_) <= 0);
        const qint64 displayLimitMinor = appRateMissing ? 0
            : currencyConverter_.convert(
                  Money(limitMinor, budget.currency()), appCurrency_).minorUnits();
        const qint64 displaySpentMinor = appRateMissing ? 0
            : currencyConverter_.convert(
                  Money(spentMinor, budget.currency()), appCurrency_).minorUnits();

        QHash<QString, qint64> configuredLimits;
        for (const BudgetCategoryLimit& category : budget.categoryLimits()) {
            configuredLimits.insert(category.categoryId, category.limitMinor);
        }

        QVariantList categoryRows;
        for (const Category& category : categories_) {
            if (category.type() != CategoryType::Expense ||
                category.id() == QStringLiteral("transfer-in") ||
                category.id() == QStringLiteral("transfer-out") ||
                (!budget.allCategories() &&
                 !configuredLimits.contains(category.id()))) {
                continue;
            }
            const qint64 categoryLimit = configuredLimits.value(category.id());
            const qint64 categorySpent = spentByCategory.value(category.id());
            if (budget.allCategories() && categoryLimit == 0 &&
                categorySpent == 0) {
                continue;
            }
            QVariantMap row;
            row[QStringLiteral("categoryId")] = category.id();
            row[QStringLiteral("name")] = categoryDisplayName(category);
            row[QStringLiteral("limitMinor")] = categoryLimit;
            row[QStringLiteral("spentMinor")] = categorySpent;
            row[QStringLiteral("configured")] =
                configuredLimits.contains(category.id());
            row[QStringLiteral("progress")] = categoryLimit > 0
                ? static_cast<double>(categorySpent) / categoryLimit : 0.0;
            categoryRows.append(row);
        }

        QVariantMap item;
        item[QStringLiteral("id")] = budget.id();
        item[QStringLiteral("name")] = budget.name();
        item[QStringLiteral("currency")] = currencyCode(budget.currency());
        item[QStringLiteral("limitMinor")] = limitMinor;
        item[QStringLiteral("spentMinor")] = spentMinor;
        item[QStringLiteral("remainingMinor")] = remainingMinor;
        item[QStringLiteral("displayLimitMinor")] = displayLimitMinor;
        item[QStringLiteral("displaySpentMinor")] = displaySpentMinor;
        item[QStringLiteral("displayCurrency")] = currencyCode(appCurrency_);
        item[QStringLiteral("progress")] = limitMinor > 0
            ? static_cast<double>(spentMinor) / limitMinor : 0.0;
        item[QStringLiteral("operationCount")] = operationCount;
        item[QStringLiteral("rateMissing")] = rateMissing || appRateMissing;
        item[QStringLiteral("allAccounts")] = budget.allAccounts();
        item[QStringLiteral("allCategories")] = budget.allCategories();
        item[QStringLiteral("accountIds")] = budget.accountIds();
        item[QStringLiteral("categoryLimits")] = categoryRows;
        result.append(item);
    }
    return result;
}

QString FinanceController::selectedBudgetId() const
{
    return selectedBudgetId_;
}

void FinanceController::setSelectedBudgetId(const QString& id)
{
    if (id == selectedBudgetId_ || (!id.isEmpty() && !hasBudget(id))) {
        return;
    }
    selectedBudgetId_ = id;
    emit selectedBudgetIdChanged();
    emit budgetsChanged();
}

QString FinanceController::selectedBudgetMonth() const
{
    return selectedBudgetMonth_.toString(Qt::ISODate);
}

void FinanceController::setSelectedBudgetMonth(const QString& month)
{
    QDate parsed = QDate::fromString(month, Qt::ISODate);
    if (!parsed.isValid()) {
        return;
    }
    parsed = QDate(parsed.year(), parsed.month(), 1);
    if (parsed == selectedBudgetMonth_) {
        return;
    }
    selectedBudgetMonth_ = parsed;
    refreshBudgetMonthLimits();
    emit selectedBudgetMonthChanged();
    emit budgetsChanged();
}

QVariantList FinanceController::budgetTransactions() const
{
    QVariantList result;
    const auto budget = std::find_if(
        budgets_.cbegin(), budgets_.cend(), [this](const Budget& candidate)
        {
            return candidate.id() == selectedBudgetId_;
        });
    if (budget == budgets_.cend()) {
        return result;
    }
    const QDate monthStart(
        selectedBudgetMonth_.year(), selectedBudgetMonth_.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);
    for (const Transaction& transaction : transactions_) {
        const QDate date = transaction.date().toLocalTime().date();
        if (date < monthStart || date > monthEnd ||
            !transactionMatchesBudget(transaction, *budget)) {
            continue;
        }
        QVariantMap row = transactionToVariant(transaction);
        row[QStringLiteral("budgetAmountMinor")] = currencyConverter_.convert(
            transaction.money(), budget->currency()).minorUnits();
        row[QStringLiteral("budgetCurrency")] = currencyCode(budget->currency());
        result.append(row);
    }
    return result;
}

QVariantList FinanceController::categories() const
{
    QVariantList result;
    result.reserve(categories_.size());

    for (const Category& category : categories_) {
        if (archivedCategoryIds_.contains(category.id()) ||
            category.id() == QStringLiteral("transfer-in") ||
            category.id() == QStringLiteral("transfer-out")) {
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
        item["isDeposit"] = account.type() == AccountType::Deposit;
        if (const DepositSettings* deposit =
                depositSettingsForAccount(account.id())) {
            item["depositAnnualRatePercent"] =
                deposit->annualRateBasisPoints() / 100.0;
            item["depositPayoutFrequency"] =
                depositPayoutFrequencyToString(deposit->payoutFrequency());
            item["depositPayoutDay"] = deposit->payoutDay();
        }
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
        item["isDeposit"] = account.type() == AccountType::Deposit;
        if (const DepositSettings* deposit =
                depositSettingsForAccount(account.id())) {
            item["depositAnnualRatePercent"] =
                deposit->annualRateBasisPoints() / 100.0;
            item["depositPayoutFrequency"] =
                depositPayoutFrequencyToString(deposit->payoutFrequency());
            item["depositPayoutDay"] = deposit->payoutDay();
        }
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
        item[QStringLiteral("name")] = cryptoWalletNames_.value(wallet.id()).isEmpty() ? wallet.symbol() : cryptoWalletNames_.value(wallet.id());
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
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap();
        const QString id = account.value("id").toString();
        const qint64 fetched = account.value("fetchedAtMs").toLongLong();
        const qint64 usd = exchangeUsdMinor(account);
        const qint64 converted = currencyConverter_.convert(Money(usd, Currency::USD), appCurrency_).minorUnits();
        QVariantMap item{{"id", id}, {"name", account.value("name")}, {"type", "crypto_exchange"},
            {"asset", "crypto"}, {"isCrypto", true}, {"isExchange", true}, {"isCreditCard", false},
            {"symbol", "USD"}, {"network", "Bybit"}, {"address", tr("Биржа")},
            {"balanceText", QString::number(usd / 100.0, 'f', 2)}, {"balanceMinor", converted},
            {"valueMinor", converted}, {"currency", currencyCode(appCurrency_)}, {"hasSnapshot", fetched > 0},
            {"updatedAt", fetched > 0 ? QVariant(QDateTime::fromMSecsSinceEpoch(fetched, QTimeZone::UTC)) : QVariant()},
            {"refreshing", refreshingCryptoWalletIds_.contains(id)}, {"priceHasSnapshot", false},
            {"connectionError", exchangeErrors_.value(id).isEmpty() ? exchangeCredentialWarnings_.value(id) : exchangeErrors_.value(id)}};
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
        bool exists = std::any_of(
            cryptoWallets_.cbegin(),
            cryptoWallets_.cend(),
            [&walletId](const CryptoWallet& wallet)
            {
                return wallet.id() == walletId;
            });
        for (const auto& value : cryptoExchanges_) if (value.toMap().value("id").toString() == walletId) exists = true;
        if (!exists) return;
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
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap(); const QString id = account.value("id").toString();
        if (!selectedCryptoWalletId_.isEmpty() && selectedCryptoWalletId_ != id) continue;
        for (const auto& record : account.value("history").toList()) {
            auto row = record.toMap(); row["walletId"] = id; row["walletAddress"] = QStringLiteral("Bybit");
            row["occurredAt"] = QDateTime::fromMSecsSinceEpoch(row.value("occurredAtMs").toLongLong(), QTimeZone::UTC);
            row["accountName"] = account.value("name"); result.append(row);
        }
    }
    std::sort(result.begin(), result.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("occurredAt").toDateTime() > b.toMap().value("occurredAt").toDateTime();
    });
    return result;
}

QVariantList FinanceController::investmentAccounts() const
{
    QVariantList result;
    for (const Account& account : accounts_) {
        if (account.assetType() != AssetType::Investment ||
            account.type() != AccountType::Brokerage) {
            continue;
        }
        QVariantMap item;
        item[QStringLiteral("id")] = account.id();
        item[QStringLiteral("name")] = accountDisplayName(account);
        item[QStringLiteral("currency")] = currencyCode(account.currency());
        const auto positions=investmentAccountValueMinor(account.id());
        const auto cash=saturatedCapitalSubtract(accountBalanceMinor(account),positions);
        qint64 blocked=0;for(const auto& p:investmentPositions_)if(p.accountId()==account.id())blocked=saturatedCapitalAdd(blocked,p.settings().blockedMarginMinor);
        item["cashMinor"]=cash;item["blockedMarginMinor"]=blocked;item["freeCashMinor"]=saturatedCapitalSubtract(cash,blocked);
        result.append(item);
    }
    return result;
}

QVariantList FinanceController::investmentPositions() const
{
    QVariantList result;
    for(const auto& position:investmentPositions_){
        if(!selectedAccountId_.isEmpty() && selectedAccountId_!=position.accountId())continue;
        const auto i=std::find_if(investmentInstruments_.cbegin(),investmentInstruments_.cend(),[&](const auto& i){return i.id()==position.instrumentId();});
        const auto a=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==position.accountId();});
        const auto q=std::find_if(investmentQuotes_.cbegin(),investmentQuotes_.cend(),[&](const auto& q){return q.instrumentId()==position.instrumentId();});
        if(i==investmentInstruments_.cend() || a==accounts_.cend())continue;
        auto terms=q!=investmentQuotes_.cend() && !q->terms().engine.isEmpty()?q->terms():i->terms();
        if(terms.engine.isEmpty() && i->marketCode()=="MOEX")terms=moexDefaultTerms(i->type());
        if(terms.currencyCode.isEmpty())terms.currencyCode=currencyCode(i->currency());
        auto item=investmentTermsToVariant(terms);const auto value=investmentPositionValue(position);const auto& settings=position.settings();
        item["id"]=position.id();item["accountId"]=a->id();item["accountName"]=accountDisplayName(*a);
        item["instrumentId"]=i->id();item["symbol"]=i->symbol();item["isin"]=i->isin();item["name"]=i->name();
        item["type"]=static_cast<int>(i->type());item["typeName"]=investmentTypeName(i->type());
        item["currency"]=currencyCode(a->currency());item["quantityText"]=formatMicros(position.quantityMicros());
        item["averagePriceText"]=formatMicros(position.averagePriceMicros());
        item["averageValueMinor"]=scaledInvestmentValueMinor(position.quantityMicros(),position.averagePriceMicros());
        item["createdAt"]=position.createdAtUtc();item["hasQuote"]=value.available;item["hasMarketQuote"]=q!=investmentQuotes_.cend();
        item["marketValueMinor"]=value.valueMinor;item["valuationError"]=value.error;
        item["marketValueText"]=value.available?QLocale().toString(value.valueMinor/100.0,'f',2):QString();
        item["priceText"]=q==investmentQuotes_.cend()?QString():formatMicros(q->priceMicros());
        item["quotedAt"]=q==investmentQuotes_.cend()?QDateTime():q->quotedAtUtc();
        item["quotedAtText"]=q==investmentQuotes_.cend()?QString():q->quotedAtUtc().toLocalTime().toString("dd.MM.yyyy HH:mm");
        item["manualValuation"]=settings.manualValuation;item["manualValueText"]=QString::number(settings.manualValueMinor/100.0,'f',2);
        item["valuedAtText"]=settings.valuedAtUtc.toLocalTime().toString("dd.MM.yyyy HH:mm");
        item["direction"]=settings.direction;item["referencePriceText"]=formatMicros(settings.referencePriceMicros);
        item["referenceDate"]=settings.referenceAtUtc.toLocalTime().toString(Qt::ISODate);
        item["blockedMarginText"]=QString::number(settings.blockedMarginMinor/100.0,'f',2);
        item["adjustmentText"]=QString::number(settings.unsettledAdjustmentMinor/100.0,'f',2);
        item["fxRateText"]=settings.manualFxRateMicros>0?formatMicros(settings.manualFxRateMicros):QString();
        item["hasOperations"]=std::any_of(investmentOperations_.cbegin(),investmentOperations_.cend(),[&](const auto& op){return op.positionId==position.id();});
        item["valuationLabel"]=settings.manualValuation?tr("Ручная оценка"):
            (terms.pricing=="future" || terms.pricing=="margined_option")?tr("Ещё не учтённый результат"):
            settings.direction==-1?tr("Обязательство"):tr("Стоимость позиции");
        result.append(item);
    }return result;
}

QVariantList FinanceController::investmentSearchResults() const
{
    QVariantList result;
    for(int index=0;index<investmentSearchResults_.size();++index){const auto& instrument=investmentSearchResults_.at(index);
        auto item=investmentTermsToVariant(instrument.terms());item["index"]=index;item["id"]=instrument.id();
        item["symbol"]=instrument.symbol();item["isin"]=instrument.isin();item["name"]=instrument.name();
        item["type"]=static_cast<int>(instrument.type());item["typeName"]=investmentTypeName(instrument.type());
        item["boardId"]=instrument.primaryBoardId();item["hasPrice"]=instrument.hasQuote();
        item["priceText"]=instrument.hasQuote()?formatMicros(instrument.priceMicros()):QString();
        item["currency"]=instrument.currencyCode();item["selected"]=index==selectedInvestmentSearchIndex_;result.append(item);
    }return result;
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

QVariantList FinanceController::capitalHistory() const
{
    return capitalHistoryToVariant(capitalHistorySeries_, appCurrency_);
}

QVariantList FinanceController::expenseHistoryByMonthRub() const
{
    const QDate today = QDate::currentDate();
    const QDate currentMonth(today.year(), today.month(), 1);
    QDate firstMonth;
    QDate lastMonth;
    QHash<QDate, qint64> totals;
    for (const Transaction& transaction : transactions_) {
        const QDate date = transaction.date().toLocalTime().date();
        if (transaction.type() != TransactionType::Expense ||
            !date.isValid()) {
            continue;
        }
        const QDate month(date.year(), date.month(), 1);
        if (!firstMonth.isValid() || month < firstMonth) {
            firstMonth = month;
        }
        if (!lastMonth.isValid() || month > lastMonth) {
            lastMonth = month;
        }
        const qint64 amount = currencyConverter_.convert(
            transaction.money(), Currency::RUB).minorUnits();
        totals[month] = saturatedCapitalAdd(totals.value(month), amount);
    }

    QVariantList result;
    if (!firstMonth.isValid()) {
        return result;
    }
    firstMonth = std::min(firstMonth, currentMonth);
    lastMonth = std::max(lastMonth, currentMonth);
    const int monthCount =
        (lastMonth.year() - firstMonth.year()) * 12 +
        lastMonth.month() - firstMonth.month() + 1;
    result.reserve(monthCount);
    for (QDate month = firstMonth;
         month.isValid() && month <= lastMonth;
         month = month.addMonths(1)) {
        result.append(QVariantMap{
            {QStringLiteral("date"), month.toString(Qt::ISODate)},
            {QStringLiteral("totalMinor"), totals.value(month)},
            {QStringLiteral("currency"), QStringLiteral("RUB")},
            {QStringLiteral("resolution"), QStringLiteral("month")},
            {QStringLiteral("complete"), month < currentMonth}
        });
    }
    return result;
}

QVariantList FinanceController::capitalHistoryRub() const
{
    return capitalHistoryToVariant(
        calculateCapitalHistory(Currency::RUB, false), Currency::RUB);
}

QString FinanceController::analyticsCurrency() const
{
    return currencyCode(analyticsCurrency_);
}

void FinanceController::setAnalyticsCurrency(const QString& currency)
{
    const Currency requested = currencyFromString(currency);
    const Currency normalized = requested == Currency::EUR
        ? Currency::EUR
        : Currency::USD;
    if (normalized == analyticsCurrency_) {
        return;
    }
    if (repository_.isOpen() &&
        !repository_.saveAnalyticsCurrency(currencyCode(normalized))) {
        qWarning() << "Failed to save analytics currency:"
                   << repository_.lastError();
        return;
    }
    analyticsCurrency_ = normalized;
    emit analyticsChanged();
}

QVariantList FinanceController::capitalHistoryAnalyticsCurrency() const
{
    return capitalHistoryToVariant(
        calculateCapitalHistory(analyticsCurrency_, false),
        analyticsCurrency_);
}

QVariantList FinanceController::capitalHistoryToVariant(
    const CapitalHistorySeries& series,
    const Currency currency
    ) const
{
    const QString resolution =
        series.resolution == CapitalHistoryResolution::Month
        ? QStringLiteral("month")
        : series.resolution == CapitalHistoryResolution::Year
          ? QStringLiteral("year")
          : QStringLiteral("day");

    QVariantList result;
    result.reserve(series.points.size());
    for (const CapitalHistoryPoint& point : series.points) {
        result.append(QVariantMap{
            {QStringLiteral("date"), point.date.toString(Qt::ISODate)},
            {QStringLiteral("totalMinor"), point.totalMinor},
            {QStringLiteral("currency"), currencyCode(currency)},
            {QStringLiteral("resolution"), resolution}
        });
    }
    return result;
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
    const qint64 creditLimitMinor,
    const double depositAnnualRatePercent,
    const QString& depositPayoutFrequency,
    const int depositPayoutDay
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

    const std::optional<AccountType> parsedType = accountTypeFromString(type);
    if (!parsedType) {
        return false;
    }
    const AccountType accountType = *parsedType;
    const bool validType =
        (selectedAsset_ == AssetType::Fiat &&
         (accountType == AccountType::Cash ||
          accountType == AccountType::DebitCard ||
          accountType == AccountType::CreditCard ||
          accountType == AccountType::Other)) ||
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
        creditLimitMinor < 0 ||
        (accountType == AccountType::Deposit && initialBalanceMinor <= 0)) {
        return false;
    }
    const qint64 resolvedCreditLimitMinor = creditCard ? creditLimitMinor : 0;

    int annualRateBasisPoints = 0;
    int payoutDay = 0;
    DepositPayoutFrequency payoutFrequency =
        DepositPayoutFrequency::Monthly;
    if (accountType == AccountType::Deposit &&
        !parseDepositParameters(
            depositAnnualRatePercent,
            depositPayoutFrequency,
            depositPayoutDay,
            annualRateBasisPoints,
            payoutFrequency,
            payoutDay)) {
        return false;
    }

    const Currency accountCurrency = currencyFromString(currency);
    const QString accountId = QUuid::createUuid().toString(
        QUuid::WithoutBraces);
    const Account account(
        accountId,
        normalizedName,
        selectedAsset_,
        accountType,
        accountCurrency,
        initialBalanceMinor,
        resolvedCreditLimitMinor);

    std::optional<DepositSettings> deposit;
    if (accountType == AccountType::Deposit) {
        const QDate today = QDate::currentDate();
        deposit.emplace(
            accountId,
            annualRateBasisPoints,
            payoutFrequency,
            payoutDay,
            DepositInterestCalculator::firstPayoutAfter(
                today, payoutFrequency, payoutDay),
            today);
    }

    const bool saved = deposit
        ? repository_.insertDepositAccount(account, *deposit)
        : repository_.insertAccount(account);
    if (!saved) {
        qWarning() << "Failed to save account:" << repository_.lastError();
        return false;
    }

    accounts_.append(account);
    if (deposit) {
        depositSettings_.append(*deposit);
        scheduleRecurringMaterialization();
    }
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
    const qint64 creditLimitMinor,
    const double depositAnnualRatePercent,
    const QString& depositPayoutFrequency,
    const int depositPayoutDay
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

    const std::optional<AccountType> parsedType = accountTypeFromString(type);
    if (!parsedType) {
        return false;
    }
    const AccountType accountType = *parsedType;
    const bool validType =
        (original.assetType() == AssetType::Fiat &&
         (accountType == AccountType::Cash ||
          accountType == AccountType::DebitCard ||
          accountType == AccountType::CreditCard ||
          accountType == AccountType::Other)) ||
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
        creditLimitMinor < 0 ||
        (accountType == AccountType::Deposit && initialBalanceMinor <= 0)) {
        return false;
    }
    const qint64 resolvedCreditLimitMinor = creditCard ? creditLimitMinor : 0;

    if (accountType != AccountType::Brokerage &&
        std::any_of(
            investmentPositions_.cbegin(), investmentPositions_.cend(),
            [&id](const InvestmentPosition& position)
            {
                return position.accountId() == id;
            })) {
        return false;
    }

    std::optional<DepositSettings> deposit;
    if (accountType == AccountType::Deposit) {
        int annualRateBasisPoints = 0;
        int payoutDay = 0;
        DepositPayoutFrequency payoutFrequency =
            DepositPayoutFrequency::Monthly;
        if (!parseDepositParameters(
                depositAnnualRatePercent,
                depositPayoutFrequency,
                depositPayoutDay,
                annualRateBasisPoints,
                payoutFrequency,
                payoutDay)) {
            return false;
        }

        const DepositSettings* existing = depositSettingsForAccount(id);
        const bool scheduleUnchanged = existing &&
            existing->annualRateBasisPoints() == annualRateBasisPoints &&
            existing->payoutFrequency() == payoutFrequency &&
            existing->payoutDay() == payoutDay;
        const QDate today = QDate::currentDate();
        deposit.emplace(
            id,
            annualRateBasisPoints,
            payoutFrequency,
            payoutDay,
            scheduleUnchanged
                ? existing->startsOn()
                : DepositInterestCalculator::firstPayoutAfter(
                    today, payoutFrequency, payoutDay),
            scheduleUnchanged ? existing->generatedThrough() : today);
    }

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
    if (!repository_.isOpen() ||
        !repository_.updateAccountAndDeposit(updated, deposit)) {
        qWarning() << "Failed to update account:" << repository_.lastError();
        return false;
    }

    accounts_[accountIndex] = updated;
    depositSettings_.erase(
        std::remove_if(
            depositSettings_.begin(), depositSettings_.end(),
            [&id](const DepositSettings& settings)
            {
                return settings.accountId() == id;
            }),
        depositSettings_.end());
    if (deposit) {
        depositSettings_.append(*deposit);
        scheduleRecurringMaterialization();
    }
    summary_ = repository_.loadSummary();
    emit accountsChanged();
    emit balanceChanged();
    emit transactionsChanged();
    emit scheduledTransactionsChanged();
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
    depositSettings_.erase(
        std::remove_if(
            depositSettings_.begin(), depositSettings_.end(),
            [&id](const DepositSettings& settings)
            {
                return settings.accountId() == id;
            }),
        depositSettings_.end());
    transactions_ = repository_.loadTransactions();
    recurringTransactions_ = repository_.loadRecurringTransactions();
    investmentPositions_ = repository_.loadInvestmentPositions();
    summary_ = repository_.loadSummary();

    if (selectedAccountId_ == id) {
        selectedAccountId_.clear();
        emit selectedAccountIdChanged();
    }
    emit accountsChanged();
    emit investmentPositionsChanged();
    emit transactionsChanged();
    emit scheduledTransactionsChanged();
    emit balanceChanged();
    emit projectsChanged();
    return true;
}

bool FinanceController::addProject(const QString& name)
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 80) {
        return false;
    }
    for (const Project& project : projects_) {
        if (project.name().compare(normalizedName, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }

    const Project project(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        normalizedName);
    if (!repository_.insertProject(project)) {
        qWarning() << "Failed to save project:" << repository_.lastError();
        return false;
    }

    projects_.append(project);
    selectedProjectId_ = project.id();
    emit projectsChanged();
    emit selectedProjectIdChanged();
    return true;
}

bool FinanceController::renameProject(
    const QString& id,
    const QString& name
    )
{
    const QString normalizedName = name.trimmed();
    if (id.isEmpty() || normalizedName.isEmpty() ||
        normalizedName.size() > 80) {
        return false;
    }

    int projectIndex = -1;
    for (int index = 0; index < projects_.size(); ++index) {
        const Project& project = projects_.at(index);
        if (project.id() == id) {
            projectIndex = index;
        } else if (project.name().compare(
                       normalizedName, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    if (projectIndex < 0) {
        return false;
    }

    const Project updated(id, normalizedName);
    if (!repository_.updateProject(updated)) {
        qWarning() << "Failed to rename project:" << repository_.lastError();
        return false;
    }
    projects_[projectIndex] = updated;
    emit projectsChanged();
    return true;
}

bool FinanceController::deleteProject(const QString& id)
{
    int projectIndex = -1;
    for (int index = 0; index < projects_.size(); ++index) {
        if (projects_.at(index).id() == id) {
            projectIndex = index;
            break;
        }
    }
    if (projectIndex < 0 || !repository_.archiveProject(id)) {
        if (projectIndex >= 0) {
            qWarning() << "Failed to archive project:"
                       << repository_.lastError();
        }
        return false;
    }

    projects_.removeAt(projectIndex);
    if (selectedProjectId_ == id) {
        const int projectCount = static_cast<int>(projects_.size());
        const int nextIndex = projectIndex < projectCount
            ? projectIndex
            : projectCount - 1;
        selectedProjectId_ = projects_.isEmpty()
            ? QString()
            : projects_.at(nextIndex).id();
        emit selectedProjectIdChanged();
    }
    emit projectsChanged();
    return true;
}

QVariantMap FinanceController::saveBudget(const QVariantMap& values)
{
    QVariantMap result{{QStringLiteral("ok"), false}};
    const QString id = values.value(QStringLiteral("id")).toString().trimmed();
    const QString name = values.value(QStringLiteral("name")).toString().trimmed();
    const QString currencyText = values.value(
        QStringLiteral("currency")).toString().trimmed().toUpper();
    const qint64 limitMinor = values.value(
        QStringLiteral("limitMinor")).toLongLong();
    const bool allAccounts = values.value(
        QStringLiteral("allAccounts"), true).toBool();
    const bool allCategories = values.value(
        QStringLiteral("allCategories"), true).toBool();

    if (name.isEmpty() || name.size() > 80) {
        result[QStringLiteral("error")] = tr(
            "Введите название бюджета длиной до 80 символов");
        return result;
    }
    if (currencyText != QStringLiteral("RUB") &&
        currencyText != QStringLiteral("USD") &&
        currencyText != QStringLiteral("EUR")) {
        result[QStringLiteral("error")] = tr("Выберите валюту бюджета");
        return result;
    }
    if (limitMinor <= 0) {
        result[QStringLiteral("error")] = tr(
            "Месячный лимит должен быть больше нуля");
        return result;
    }

    int editingIndex = -1;
    for (int index = 0; index < budgets_.size(); ++index) {
        const Budget& existing = budgets_.at(index);
        if (existing.id() == id) {
            editingIndex = index;
        } else if (existing.name().compare(name, Qt::CaseInsensitive) == 0) {
            result[QStringLiteral("error")] = tr(
                "Бюджет с таким названием уже существует");
            return result;
        }
    }
    if (!id.isEmpty() && editingIndex < 0) {
        result[QStringLiteral("error")] = tr("Бюджет не найден");
        return result;
    }

    QStringList accountIds;
    QSet<QString> uniqueAccountIds;
    for (const QVariant& value : values.value(
             QStringLiteral("accountIds")).toList()) {
        const QString accountId = value.toString();
        const bool exists = std::any_of(
            accounts_.cbegin(), accounts_.cend(),
            [&accountId](const Account& account)
            {
                return account.id() == accountId;
            });
        if (!accountId.isEmpty() && exists &&
            !uniqueAccountIds.contains(accountId)) {
            uniqueAccountIds.insert(accountId);
            accountIds.append(accountId);
        }
    }
    if (!allAccounts && accountIds.isEmpty()) {
        result[QStringLiteral("error")] = tr(
            "Выберите хотя бы один счёт");
        return result;
    }

    QVector<BudgetCategoryLimit> categoryLimits;
    QSet<QString> uniqueCategoryIds;
    for (const QVariant& value : values.value(
             QStringLiteral("categoryLimits")).toList()) {
        const QVariantMap row = value.toMap();
        const QString categoryId = row.value(
            QStringLiteral("categoryId")).toString();
        const qint64 categoryLimit = row.value(
            QStringLiteral("limitMinor")).toLongLong();
        const bool exists = std::any_of(
            categories_.cbegin(), categories_.cend(),
            [&categoryId](const Category& category)
            {
                return category.id() == categoryId &&
                       category.type() == CategoryType::Expense;
            });
        if (categoryId.isEmpty() || !exists ||
            categoryId == QStringLiteral("transfer-in") ||
            categoryId == QStringLiteral("transfer-out") ||
            uniqueCategoryIds.contains(categoryId) || categoryLimit < 0) {
            continue;
        }
        uniqueCategoryIds.insert(categoryId);
        categoryLimits.append({categoryId, categoryLimit});
    }
    if (!allCategories && categoryLimits.isEmpty()) {
        result[QStringLiteral("error")] = tr(
            "Выберите хотя бы одну категорию расходов");
        return result;
    }

    const QString budgetId = id.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
    const QDate startsOn = editingIndex >= 0
        ? budgets_.at(editingIndex).startsOn() : selectedBudgetMonth_;
    const Budget budget(
        budgetId, name, currencyFromString(currencyText), limitMinor,
        allAccounts, allCategories, accountIds, categoryLimits, startsOn);
    const bool saved = editingIndex >= 0
        ? repository_.updateBudget(budget, selectedBudgetMonth_)
        : repository_.insertBudget(budget, selectedBudgetMonth_);
    if (!saved) {
        qWarning() << "Failed to save budget:" << repository_.lastError();
        result[QStringLiteral("error")] = tr("Не удалось сохранить бюджет");
        return result;
    }

    if (editingIndex >= 0) {
        budgets_[editingIndex] = budget;
    } else {
        budgets_.append(budget);
        selectedBudgetId_ = budget.id();
        emit selectedBudgetIdChanged();
    }
    budgetMonthLimits_[budget.id()] = limitMinor;
    emit budgetsChanged();
    result[QStringLiteral("ok")] = true;
    result[QStringLiteral("id")] = budget.id();
    return result;
}

bool FinanceController::deleteBudget(const QString& id)
{
    int budgetIndex = -1;
    for (int index = 0; index < budgets_.size(); ++index) {
        if (budgets_.at(index).id() == id) {
            budgetIndex = index;
            break;
        }
    }
    if (budgetIndex < 0 || !repository_.archiveBudget(id)) {
        if (budgetIndex >= 0) {
            qWarning() << "Failed to archive budget:"
                       << repository_.lastError();
        }
        return false;
    }
    budgets_.removeAt(budgetIndex);
    budgetMonthLimits_.remove(id);
    if (selectedBudgetId_ == id) {
        const int count = static_cast<int>(budgets_.size());
        const int nextIndex = budgetIndex < count ? budgetIndex : count - 1;
        selectedBudgetId_ = budgets_.isEmpty()
            ? QString() : budgets_.at(nextIndex).id();
        emit selectedBudgetIdChanged();
    }
    emit budgetsChanged();
    return true;
}

qint64 FinanceController::exchangeUsdMinor(const QVariantMap& account) const
{
    long double total = 0;
    for (const auto& value : account.value("holdings").toList()) {
        const auto row = value.toMap();
        if (!row.value("summary").toBool()) continue;
        bool ok = false;
        const double usd = row.value("usdValue").toString().toDouble(&ok);
        if (ok && std::isfinite(usd)) total += usd * 100.0L;
    }
    if (!std::isfinite(total)) return 0;
    return static_cast<qint64>(std::round(std::clamp(total,
        static_cast<long double>(std::numeric_limits<qint64>::min()),
        static_cast<long double>(std::numeric_limits<qint64>::max()))));
}

QVariantList FinanceController::cryptoExchangeHoldings() const
{
    QVariantList result;
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap();
        if (!selectedCryptoWalletId_.isEmpty() && selectedCryptoWalletId_ != account.value("id").toString()) continue;
        for (const auto& entry : account.value("holdings").toList()) {
            auto row = entry.toMap(); row["accountName"] = account.value("name"); result.append(row);
        }
    }
    return result;
}

QVariantMap FinanceController::saveCryptoConnection(const QVariantMap& values)
{
    const QString name = values.value("name").toString().trimmed();
    const QString kind = values.value("kind").toString();
    QString id = values.value("id").toString();
    if (selectedAsset_ != AssetType::Crypto) return {{"ok", false}, {"error", tr("Сначала выберите актив «Крипта»")}};
    if (name.isEmpty() || name.size() > 80) return {{"ok", false}, {"error", tr("Введите название счёта (до 80 символов)")}};
    if (kind == "wallet") {
        if (!id.isEmpty()) {
            const auto found = std::find_if(cryptoWallets_.cbegin(), cryptoWallets_.cend(), [&id](const CryptoWallet& wallet) { return wallet.id() == id; });
            if (found == cryptoWallets_.cend() || !repository_.setCryptoWalletName(id, name)) return {{"ok", false}, {"error", tr("Не удалось сохранить название кошелька")}};
        } else {
            // Keep the existing public-wallet validation and refresh path.
            auto result = addCryptoWallet(values.value("symbol").toString(), values.value("address").toString());
            if (!result.value("ok").toBool()) return result;
            id = cryptoWallets_.constLast().id();
            if (!repository_.setCryptoWalletName(id, name)) return {{"ok", false}, {"error", tr("Кошелёк добавлен, но название не сохранилось")}};
        }
        cryptoWalletNames_[id] = name; setSelectedCryptoWalletId(id); emit cryptoWalletsChanged(); return {{"ok", true}};
    }
    if (kind != "exchange") return {{"ok", false}, {"error", tr("Выберите кошелёк или биржу")}};
    const QString key = values.value("apiKey").toString().trimmed();
    const QString secret = values.value("apiSecret").toString().trimmed();
    if (!id.isEmpty()) {
        bool exists = false;
        for (const auto& value : cryptoExchanges_) if (value.toMap().value("id").toString() == id) exists = true;
        if (!exists) return {{"ok", false}, {"error", tr("Биржевой счёт не найден")}};
        if (refreshingCryptoWalletIds_.contains(id)) return {{"ok", false}, {"error", tr("Дождитесь окончания обновления Bybit")}};
    }
    if (id.isEmpty() || !key.isEmpty() || !secret.isEmpty()) {
        static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_-]{8,256}$"));
        if (!allowed.match(key).hasMatch() || !allowed.match(secret).hasMatch()) return {{"ok", false}, {"error", tr("Введите ключ API и секретный ключ, созданные Bybit")}};
    }
    if (id.isEmpty()) id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!repository_.saveCryptoExchange({{"id", id}, {"name", name}})) return {{"ok", false}, {"error", tr("Не удалось сохранить биржевой счёт")}};
    cryptoExchanges_ = repository_.loadCryptoExchanges();
    if (!key.isEmpty()) {
        exchangeCredentials_[id] = {key, secret}; exchangeCredentialWarnings_.remove(id);
        auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, id] {
            const bool saved = watcher->result(); watcher->deleteLater();
            bool exists = false;
            for (const auto& value : cryptoExchanges_) if (value.toMap().value("id").toString() == id) exists = true;
            if (!exists) { (void)QtConcurrent::run([id] { ExchangeCredentials::remove(id); }); return; }
            if (!saved) { exchangeCredentialWarnings_[id] = tr("Ключ действует до закрытия приложения: системное хранилище недоступно. Для Linux установите libsecret и службу хранения паролей"); setCryptoLastError(exchangeCredentialWarnings_[id]); emit cryptoWalletsChanged(); }
        });
        watcher->setFuture(QtConcurrent::run([id, key, secret] { return ExchangeCredentials::save(id, key, secret); }));
    }
    selectedCryptoWalletId_ = id;
    emit selectedCryptoWalletIdChanged(); emit cryptoWalletsChanged(); emit cryptoTransactionsChanged();
    exchangeErrors_.remove(id); startExchangeRefresh(id);
    return {{"ok", true}};
}

void FinanceController::startExchangeRefresh(const QString& id)
{
    if (refreshingCryptoWalletIds_.contains(id) || loadingExchangeCredentials_.contains(id)) return;
    auto account = std::find_if(cryptoExchanges_.cbegin(), cryptoExchanges_.cend(), [&id](const QVariant& value) { return value.toMap().value("id").toString() == id; });
    if (account == cryptoExchanges_.cend()) return;
    if (!exchangeCredentials_.contains(id)) {
        loadingExchangeCredentials_.insert(id);
        auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
        connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, id] {
            const auto credentials = watcher->result(); watcher->deleteLater(); loadingExchangeCredentials_.remove(id);
            bool exists = false;
            for (const auto& value : cryptoExchanges_) if (value.toMap().value("id").toString() == id) exists = true;
            if (!exists) return;
            if (exchangeCredentials_.contains(id)) { startExchangeRefresh(id); return; }
            if (credentials.first.isEmpty() || credentials.second.isEmpty()) {
                exchangeErrors_[id] = tr("Введите ключи API через «Редактировать» в меню счёта"); setCryptoLastError(exchangeErrors_[id]); emit cryptoWalletsChanged(); return;
            }
            if (!exchangeCredentials_.contains(id)) exchangeCredentials_[id] = credentials;
            startExchangeRefresh(id);
        });
        watcher->setFuture(QtConcurrent::run([id] { return ExchangeCredentials::load(id); }));
        return;
    }
    const qint64 fetched = account->toMap().value("fetchedAtMs").toLongLong();
    const qint64 from = fetched > 0 ? fetched - 2LL * 86400000 : QDateTime::currentMSecsSinceEpoch() - 30LL * 86400000;
    const auto credentials = exchangeCredentials_.value(id);
    if (bybitProvider_.refresh(id, credentials.first, credentials.second, from)) {
        ++pendingCryptoRequests_; beginCryptoWalletRequest(id);
        if (!cryptoRefreshing_) { cryptoRefreshing_ = true; emit cryptoRefreshingChanged(); }
        emit cryptoWalletsChanged();
    }
}

void FinanceController::refreshCryptoExchanges(bool force)
{
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap(); const QString id = account.value("id").toString();
        const qint64 age = QDateTime::currentMSecsSinceEpoch() - account.value("fetchedAtMs").toLongLong();
        if (force || (age >= 5LL * 60000 && !exchangeErrors_.contains(id))) startExchangeRefresh(id);
    }
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
    for (const auto& value : cryptoExchanges_) {
        if (value.toMap().value("id").toString() != id) continue;
        if (!repository_.deleteCryptoExchange(id)) return false;
        if (refreshingCryptoWalletIds_.contains(id)) { bybitProvider_.cancel(id); finishCryptoWalletRequest(id); finishCryptoRequest(); }
        exchangeCredentials_.remove(id); exchangeErrors_.remove(id); exchangeCredentialWarnings_.remove(id);
        (void)QtConcurrent::run([id] { ExchangeCredentials::remove(id); });
        cryptoExchanges_ = repository_.loadCryptoExchanges();
        if (selectedCryptoWalletId_ == id) {
            selectedCryptoWalletId_ = cryptoWallets_.isEmpty() ? (cryptoExchanges_.isEmpty() ? QString() : cryptoExchanges_.constFirst().toMap().value("id").toString()) : cryptoWallets_.constFirst().id();
            emit selectedCryptoWalletIdChanged();
        }
        emit cryptoWalletsChanged(); emit cryptoTransactionsChanged(); emit balanceChanged(); return true;
    }

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
    cryptoWalletNames_.remove(id);
    if (selectedCryptoWalletId_ == id) {
        selectedCryptoWalletId_ = cryptoWallets_.isEmpty()
            ? (cryptoExchanges_.isEmpty() ? QString() : cryptoExchanges_.constFirst().toMap().value("id").toString())
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
    setCryptoLastError(QString());
    refreshCryptoExchanges();
    if (cryptoWallets_.isEmpty()) {
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
    if (instrument.hasQuote()) {
        return;
    }
    investmentQuoteBusy_ = true;
    requestedSearchQuoteId_ = instrument.id();
    emit investmentSearchStateChanged();
    investmentProvider_.requestQuote(instrument);
}

QVariantMap FinanceController::addInvestmentPosition(const QString& accountId, int index,
    const QString& quantity, const QString& averagePrice)
{
    return saveInvestmentPosition({{"accountId",accountId},{"searchIndex",index},{"quantity",quantity},{"averagePrice",averagePrice}});
}
QVariantMap FinanceController::updateInvestmentPosition(const QString& id, const QString& accountId,
    int index, const QString& quantity, const QString& averagePrice)
{
    return saveInvestmentPosition({{"id",id},{"accountId",accountId},{"searchIndex",index},{"quantity",quantity},{"averagePrice",averagePrice}});
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
            if (age >= 0 && age < 30 * 1000) {
                continue;
            }
        }
        refreshingInvestmentIds_.insert(instrument.id());
        investmentProvider_.requestQuote(InvestmentMarketInstrument(
            instrument.id(), instrument.symbol(), instrument.isin(),
            instrument.name(), instrument.type(), instrument.primaryBoardId(), instrument.terms()));
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
    emit scheduledTransactionsChanged();
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
    recurringTransactions_ = repository_.loadRecurringTransactions();
    emit categoriesChanged();
    emit transactionsChanged();
    emit scheduledTransactionsChanged();
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

bool FinanceController::addProjectTransaction(
    const QString& projectId,
    const qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& accountId,
    const QString& type,
    const QDateTime& occurredAt
    )
{
    if (!hasProject(projectId)) {
        return false;
    }
    const QString normalizedType = type.trimmed().toLower();
    if (normalizedType != QStringLiteral("income") &&
        normalizedType != QStringLiteral("expense")) {
        return false;
    }
    const TransactionType transactionType =
        normalizedType == QStringLiteral("income")
            ? TransactionType::Income
            : TransactionType::Expense;
    return addTransaction(
        minorUnits,
        transactionType,
        description,
        categoryId,
        Currency::RUB,
        accountId,
        occurredAt,
        projectId);
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
    const QString originalProjectId = original.projectId();
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
        if (!originalProjectId.isEmpty()) {
            emit projectsChanged();
        }
        return true;
    }

    const Account* selectedAccount = nullptr;
    for (const Account& account : accounts_) {
        if (account.id() == accountId) {
            selectedAccount = &account;
            break;
        }
    }
    if (!selectedAccount ||
        (originalProjectId.isEmpty() &&
         selectedAccount->assetType() != selectedAsset_)) {
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
        description,
        originalProjectId,
        original.recipient()
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
    if (!originalProjectId.isEmpty()) {
        emit projectsChanged();
    }
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

bool FinanceController::deleteHistoryRows(const QVariantList& rows)
{
    if (!repository_.isOpen() || !repository_.deleteHistoryRows(rows)) {
        qWarning() << "Failed to delete selected operations:" << repository_.lastError();
        return false;
    }
    transactions_ = repository_.loadTransactions();
    investmentPositions_ = repository_.loadInvestmentPositions();
    cryptoTransactions_ = repository_.loadCryptoTransactions();
    cryptoExchanges_ = repository_.loadCryptoExchanges();
    summary_ = repository_.loadSummary();
    emit transactionsChanged();
    emit investmentPositionsChanged();
    emit cryptoTransactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
    emit projectsChanged();
    return true;
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
    emit projectsChanged();
    return true;
}

bool FinanceController::addTransaction(
    qint64 minorUnits,
    TransactionType type,
    const QString& description,
    const QString& categoryId,
    Currency currency,
    const QString& accountId,
    const QDateTime& occurredAt,
    const QString& projectId
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

    if (!selectedAccount ||
        (projectId.isEmpty() && selectedAccount->assetType() != selectedAsset_)) {
        return false;
    }

    if (!projectId.isEmpty() && !hasProject(projectId)) {
        return false;
    }

    const CategoryType requiredCategoryType =
        type == TransactionType::Income
            ? CategoryType::Income
            : CategoryType::Expense;
    const bool categoryIsValid = std::any_of(
        categories_.cbegin(), categories_.cend(),
        [this, &categoryId, requiredCategoryType](const Category& category) {
            return category.id() == categoryId &&
                category.type() == requiredCategoryType &&
                !archivedCategoryIds_.contains(categoryId);
        });
    if (!categoryIsValid) {
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
            description,
            projectId
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
    if (!projectId.isEmpty()) {
        emit projectsChanged();
    }
    return true;
}

const DepositSettings* FinanceController::depositSettingsForAccount(
    const QString& accountId
    ) const
{
    const auto settings = std::find_if(
        depositSettings_.cbegin(), depositSettings_.cend(),
        [&accountId](const DepositSettings& candidate)
        {
            return candidate.accountId() == accountId;
        });
    return settings == depositSettings_.cend() ? nullptr : &*settings;
}

qint64 FinanceController::accountBalanceMinor(const Account& account) const
{
    qint64 balance=account.initialBalanceMinor();
    for(const auto& transaction:transactions_){
        if(transaction.accountId()!=account.id())continue;
        if(dateFilterActive() && !TransactionDateFilter::isOnOrBefore(transaction,dateFilterTo_))continue;
        balance=saturatedCapitalAdd(balance,transaction.type()==TransactionType::Income?transaction.money().minorUnits():-transaction.money().minorUnits());
    }
    if(account.assetType()==AssetType::Investment){
        for(const auto& op:investmentOperations_){if(op.accountId!=account.id())continue;
            if(dateFilterActive() && op.occurredAtUtc.toLocalTime().date()>dateFilterTo_)continue;
            balance=saturatedCapitalAdd(balance,op.cashDeltaMinor);
        }
        balance=saturatedCapitalAdd(balance,investmentAccountValueMinor(account.id()));
    }
    return balance;
}

qint64 FinanceController::investmentAccountValueMinor(const QString& id) const
{
    qint64 total=0;for(const auto& p:investmentPositions_)if(p.accountId()==id){
        const auto value=investmentPositionValue(p);if(value.available)total=saturatedCapitalAdd(total,value.valueMinor);
    }return total;
}

InvestmentValuation FinanceController::investmentPositionValue(const InvestmentPosition& p) const
{
    const auto i=std::find_if(investmentInstruments_.cbegin(),investmentInstruments_.cend(),[&](const auto& i){return i.id()==p.instrumentId();});
    const auto a=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==p.accountId();});
    const auto q=std::find_if(investmentQuotes_.cbegin(),investmentQuotes_.cend(),[&](const auto& q){return q.instrumentId()==p.instrumentId();});
    if(i==investmentInstruments_.cend() || a==accounts_.cend())return {false,0,tr("Не найден счёт или инструмент")};
    auto t=q!=investmentQuotes_.cend() && !q->terms().engine.isEmpty()?q->terms():i->terms();
    if(t.engine.isEmpty() && i->marketCode()=="MOEX")t=moexDefaultTerms(i->type());
    if(t.currencyCode.isEmpty())t.currencyCode=currencyCode(i->currency());
    const long double source=rateProvider_.rateToRubMicros(t.currencyCode),target=rateProvider_.rateToRubMicros(currencyCode(a->currency()));
    qint64 fx=p.settings().manualFxRateMicros;
    if(fx<=0 && source>0 && target>0){const long double rate=source/target*1'000'000.0L;
        if(rate<std::numeric_limits<qint64>::max())fx=static_cast<qint64>(std::round(rate));}
    return valueInvestmentPosition(p,q==investmentQuotes_.cend()?nullptr:&*q,t,fx);
}

bool FinanceController::investmentValuationIncomplete() const
{
    for(const auto& p:investmentPositions_)if(!investmentPositionValue(p).available)return true;
    return false;
}

void FinanceController::reloadInvestments()
{
    investmentInstruments_=repository_.loadInvestmentInstruments();investmentPositions_=repository_.loadInvestmentPositions();
    investmentQuotes_=repository_.loadInvestmentQuotes();investmentOperations_=repository_.loadInvestmentOperations();
    emit investmentPositionsChanged();emit accountsChanged();emit transactionsChanged();emit balanceChanged();
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
        total = saturatedCapitalAdd(total, currencyConverter_.convert(
            Money(accountBalanceMinor(account), account.currency()),
            appCurrency_).minorUnits());
    }
    if (asset == AssetType::Crypto) {
        const qint64 trackedWallets = cryptoWalletsTotalMinor();
        if (trackedWallets > 0 && total >
                std::numeric_limits<qint64>::max() - trackedWallets) {
            return std::numeric_limits<qint64>::max();
        }
        total += trackedWallets;
        for (const auto& value : cryptoExchanges_) total = saturatedCapitalAdd(total,
            currencyConverter_.convert(Money(exchangeUsdMinor(value.toMap()), Currency::USD), appCurrency_).minorUnits());
    }
    return total;
}

qint64 FinanceController::cryptoAmountValueMinor(
    const CryptoWallet& wallet,
    const qint64 amountAtomic
    ) const
{
    return cryptoAmountValueMinorInCurrency(
        wallet, amountAtomic, appCurrency_);
}

qint64 FinanceController::cryptoAmountValueMinorInCurrency(
    const CryptoWallet& wallet,
    const qint64 amountAtomic,
    const Currency currency
    ) const
{
    const qint64 priceUsdMicros = cryptoPricesUsdMicros_.value(
        wallet.symbol());
    if (priceUsdMicros <= 0 || amountAtomic <= 0 ||
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
    const Int128 product = static_cast<Int128>(amountAtomic) *
        static_cast<Int128>(priceUsdMicros);
    const Int128 roundedUsdMinor = (product + divisor / 2) / divisor;
    const Int128 maximum = std::numeric_limits<qint64>::max();
    const qint64 usdMinor = roundedUsdMinor > maximum
        ? std::numeric_limits<qint64>::max()
        : static_cast<qint64>(roundedUsdMinor);
    return currencyConverter_.convert(
        Money(usdMinor, Currency::USD), currency).minorUnits();
}

qint64 FinanceController::cryptoWalletValueMinor(
    const CryptoWallet& wallet
    ) const
{
    return cryptoAmountValueMinor(wallet, wallet.balanceAtomic());
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

void FinanceController::scheduleRecurringMaterialization()
{
    if (recurringMaterializationScheduled_) {
        return;
    }
    recurringMaterializationScheduled_ = true;
    QTimer::singleShot(
        0,
        this,
        &FinanceController::materializeRecurringTransactions);
}

void FinanceController::materializeRecurringTransactions()
{
    recurringMaterializationScheduled_ = false;
    if (!repository_.isOpen()) {
        return;
    }

    const QDate today = QDate::currentDate();
    int totalInserted = 0;
    for (const RecurringTransaction& recurring :
         std::as_const(recurringTransactions_)) {
        const auto account = std::find_if(
            accounts_.cbegin(), accounts_.cend(),
            [&recurring](const Account& candidate)
            {
                return candidate.id() == recurring.accountId() &&
                    candidate.assetType() == AssetType::Fiat;
            });
        const CategoryType categoryType =
            recurring.transactionType() == TransactionType::Income
                ? CategoryType::Income
                : CategoryType::Expense;
        const bool categoryIsValid = std::any_of(
            categories_.cbegin(), categories_.cend(),
            [this, &recurring, categoryType](const Category& category)
            {
                return category.id() == recurring.categoryId() &&
                    category.type() == categoryType &&
                    !archivedCategoryIds_.contains(category.id());
            });
        if (account == accounts_.cend() || !categoryIsValid) {
            qWarning() << "Skipped invalid recurring transaction:"
                       << recurring.id();
            continue;
        }

        const Money accountAmount = currencyConverter_.convert(
            Money(recurring.amountMinor(), recurring.currency()),
            account->currency());
        if (accountAmount.minorUnits() <= 0) {
            qWarning() << "Skipped recurring transaction because its amount "
                          "could not be converted:"
                       << recurring.id();
            continue;
        }

        QDate from = recurring.generatedThrough().isValid()
            ? recurring.generatedThrough().addDays(1)
            : recurring.startsOn();
        from = std::max(from, recurring.startsOn());
        if (from > today) {
            continue;
        }

        const QVector<QDate> dates =
            RecurringScheduleCalculator::occurrences(
                recurring, from, today);
        QVector<FinanceRepository::RecurringOccurrence> occurrences;
        occurrences.reserve(dates.size());
        for (const QDate& date : dates) {
            const QString transactionId = QStringLiteral("recurring-")
                + recurring.id() + QLatin1Char('-')
                + date.toString(QStringLiteral("yyyyMMdd"));
            occurrences.append(FinanceRepository::RecurringOccurrence{
                date,
                Transaction(
                    transactionId,
                    recurring.accountId(),
                    recurring.categoryId(),
                    accountAmount,
                    recurring.transactionType(),
                    QDateTime(
                        date,
                        QTime(12, 0),
                        QTimeZone::systemTimeZone()),
                    recurring.name())
            });
        }

        int inserted = 0;
        if (!repository_.materializeRecurringOccurrences(
                recurring.id(), occurrences, today, &inserted)) {
            qWarning() << "Failed to materialize recurring transaction:"
                       << recurring.id() << repository_.lastError();
            continue;
        }
        totalInserted += inserted;
    }

    totalInserted += materializeDepositInterest();

    recurringTransactions_ = repository_.loadRecurringTransactions();
    emit scheduledTransactionsChanged();
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime nextCheck(
        now.date().addDays(1),
        QTime(0, 1),
        QTimeZone::systemTimeZone());
    recurringTimer_.start(static_cast<int>(std::max<qint64>(
        1'000, now.msecsTo(nextCheck))));
    if (totalInserted <= 0) {
        return;
    }
    categories_ = repository_.loadCategories();
    archivedCategoryIds_ = repository_.loadArchivedCategoryIds();
    emit categoriesChanged();
    transactions_ = repository_.loadTransactions();
    summary_ = repository_.loadSummary();
    emit transactionsChanged();
    emit balanceChanged();
    emit accountsChanged();
}

int FinanceController::materializeDepositInterest()
{
    const QDate today = QDate::currentDate();
    int totalInserted = 0;

    for (const DepositSettings& settings :
         std::as_const(depositSettings_)) {
        const auto account = std::find_if(
            accounts_.cbegin(), accounts_.cend(),
            [&settings](const Account& candidate)
            {
                return candidate.id() == settings.accountId() &&
                    candidate.assetType() == AssetType::Investment &&
                    candidate.type() == AccountType::Deposit;
            });
        if (account == accounts_.cend()) {
            qWarning() << "Skipped invalid deposit settings:"
                       << settings.accountId();
            continue;
        }

        QDate from = settings.generatedThrough().isValid()
            ? settings.generatedThrough().addDays(1)
            : settings.startsOn();
        from = std::max(from, settings.startsOn());
        if (from > today) {
            continue;
        }

        const QVector<QDate> dates =
            DepositInterestCalculator::occurrences(
                settings, from, today);
        QVector<FinanceRepository::DepositInterestOccurrence> occurrences;
        occurrences.reserve(dates.size());

        for (const QDate& date : dates) {
            const QDateTime payoutAt(
                date,
                QTime(0, 0),
                QTimeZone::systemTimeZone());
            qint64 balanceMinor = account->initialBalanceMinor();
            for (const Transaction& transaction :
                 std::as_const(transactions_)) {
                if (transaction.accountId() != account->id() ||
                    transaction.date() >= payoutAt) {
                    continue;
                }
                balanceMinor = transaction.type() == TransactionType::Income
                    ? saturatedCapitalAdd(
                        balanceMinor, transaction.money().minorUnits())
                    : saturatedCapitalSubtract(
                        balanceMinor, transaction.money().minorUnits());
            }
            for (const FinanceRepository::DepositInterestOccurrence& pending :
                 std::as_const(occurrences)) {
                balanceMinor = saturatedCapitalAdd(
                    balanceMinor,
                    pending.transaction.money().minorUnits());
            }

            const qint64 interestMinor =
                DepositInterestCalculator::interestMinor(
                    balanceMinor,
                    settings.annualRateBasisPoints(),
                    settings.payoutFrequency());
            if (interestMinor <= 0) {
                continue;
            }

            const QString transactionId = QStringLiteral("deposit-interest-")
                + settings.accountId() + QLatin1Char('-')
                + date.toString(QStringLiteral("yyyyMMdd"));
            occurrences.append(
                FinanceRepository::DepositInterestOccurrence{
                    date,
                    Transaction(
                        transactionId,
                        settings.accountId(),
                        QStringLiteral("deposit_interest"),
                        Money(interestMinor, account->currency()),
                        TransactionType::Income,
                        payoutAt,
                        tr("Проценты по вкладу"))});
        }

        int inserted = 0;
        if (!repository_.materializeDepositInterest(
                settings, occurrences, today, &inserted)) {
            qWarning() << "Failed to materialize deposit interest:"
                       << settings.accountId() << repository_.lastError();
            continue;
        }
        totalInserted += inserted;
    }

    depositSettings_ = repository_.loadDepositSettings();
    return totalInserted;
}

CapitalHistorySeries FinanceController::calculateCapitalHistory(
    const Currency currency,
    const bool applyDateFilter
    ) const
{
    qint64 openingMinor = 0;
    QVector<CapitalHistoryEvent> events;
    events.reserve(
        transactions_.size() +
        cryptoTransactions_.size() +
        investmentPositions_.size());

    for (const Account& account : std::as_const(accounts_)) {
        const qint64 initialMinor = currencyConverter_.convert(
            Money(account.initialBalanceMinor(), account.currency()),
            currency).minorUnits();
        openingMinor = saturatedCapitalAdd(openingMinor, initialMinor);
    }

    for(const auto& position:investmentPositions_){
        const auto account=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==position.accountId();});
        const auto value=investmentPositionValue(position);
        if(account==accounts_.cend() || !value.available)continue;
        const auto minor=currencyConverter_.convert(Money(value.valueMinor,account->currency()),currency).minorUnits();
        const auto date=position.createdAtUtc().toLocalTime().date();
        if(date.isValid())events.append({date,minor});else openingMinor=saturatedCapitalAdd(openingMinor,minor);
    }
    for(const auto& op:investmentOperations_){
        const auto account=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==op.accountId;});
        if(account!=accounts_.cend())events.append({op.occurredAtUtc.toLocalTime().date(),
            currencyConverter_.convert(Money(op.cashDeltaMinor,account->currency()),currency).minorUnits()});
    }

    for (const CryptoWallet& wallet : std::as_const(cryptoWallets_)) {
        qint64 knownDeltaMinor = 0;
        const auto sameWalletAddress = [&wallet](const QString& address)
        {
            return wallet.network() == QStringLiteral("ETHEREUM")
                ? address.compare(
                      wallet.address(), Qt::CaseInsensitive) == 0
                : address == wallet.address();
        };
        for (const CryptoTransaction& transaction :
             std::as_const(cryptoTransactions_)) {
            if (transaction.walletId() != wallet.id()) {
                continue;
            }
            const bool fromWallet = sameWalletAddress(
                transaction.fromAddress());
            const bool toWallet = sameWalletAddress(
                transaction.toAddress());
            const bool incoming = toWallet && !fromWallet;
            const bool outgoing = fromWallet && !toWallet;
            if (!incoming && !outgoing) {
                continue;
            }

            const QDate transactionDate =
                transaction.occurredAtUtc().toLocalTime().date();
            if (!transactionDate.isValid()) {
                continue;
            }

            qint64 deltaMinor = cryptoAmountValueMinorInCurrency(
                wallet, transaction.amountAtomic(), currency);
            if (outgoing) {
                deltaMinor = -deltaMinor;
            }
            knownDeltaMinor = saturatedCapitalAdd(
                knownDeltaMinor, deltaMinor);
            events.append({transactionDate, deltaMinor});
        }

        const qint64 residualOpeningMinor = saturatedCapitalSubtract(
            cryptoAmountValueMinorInCurrency(
                wallet, wallet.balanceAtomic(), currency),
            knownDeltaMinor);
        openingMinor = saturatedCapitalAdd(
            openingMinor, residualOpeningMinor);
    }

    for (const Transaction& transaction : std::as_const(transactions_)) {
        qint64 deltaMinor = currencyConverter_.convert(
            transaction.money(), currency).minorUnits();
        if (transaction.type() == TransactionType::Expense ||
            transaction.categoryId() == QStringLiteral("transfer-out")) {
            deltaMinor = -deltaMinor;
        }
        events.append({
            transaction.date().toLocalTime().date(),
            deltaMinor
        });
    }

    return CapitalHistoryCalculator::calculate(
        openingMinor,
        events,
        QDate::currentDate(),
        applyDateFilter && dateFilterActive() ? dateFilterFrom_ : QDate(),
        applyDateFilter && dateFilterActive() ? dateFilterTo_ : QDate());
}

void FinanceController::rebuildCapitalHistory()
{
    capitalHistorySeries_ = calculateCapitalHistory(appCurrency_, true);
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
    if (id == QStringLiteral("deposit_interest") &&
        name == QStringLiteral("Проценты по вкладу"))
        return tr("Проценты по вкладу");
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

QVariantMap FinanceController::transactionToVariant(
    const Transaction& transaction
    ) const
{
    QVariantMap item;
    item[QStringLiteral("id")] = transaction.id();
    item[QStringLiteral("accountId")] = transaction.accountId();
    item[QStringLiteral("categoryId")] = transaction.categoryId();
    item[QStringLiteral("categoryName")] = categoryName(
        transaction.categoryId());
    item[QStringLiteral("projectId")] = transaction.projectId();
    item[QStringLiteral("amount")] = transaction.money().minorUnits();
    item[QStringLiteral("displayAmount")] = currencyConverter_.convert(
        transaction.money(), appCurrency_).minorUnits();
    item[QStringLiteral("currency")] = currencyCode(
        transaction.money().currency());

    const bool transfer = isTransfer(transaction);
    item[QStringLiteral("type")] = transfer
        ? QStringLiteral("transfer")
        : transaction.type() == TransactionType::Income
            ? QStringLiteral("income")
            : QStringLiteral("expense");
    item[QStringLiteral("direction")] =
        transaction.categoryId() == QStringLiteral("transfer-in")
            ? QStringLiteral("in")
            : transaction.categoryId() == QStringLiteral("transfer-out")
                ? QStringLiteral("out")
                : QString();
    item[QStringLiteral("date")] = transaction.date().toString(Qt::ISODate);
    item[QStringLiteral("description")] =
        transactionDisplayDescription(transaction);
    item[QStringLiteral("rawDescription")] = transaction.description();
    item[QStringLiteral("recipientName")] = transaction.recipient().name;
    item[QStringLiteral("recipientKey")] = transaction.recipient().key;
    item[QStringLiteral("recipientSource")] = transaction.recipient().source;
    return item;
}

bool FinanceController::hasProject(const QString& id) const
{
    return std::any_of(
        projects_.cbegin(), projects_.cend(),
        [&id](const Project& project) { return project.id() == id; });
}

bool FinanceController::hasBudget(const QString& id) const
{
    return std::any_of(
        budgets_.cbegin(), budgets_.cend(),
        [&id](const Budget& budget) { return budget.id() == id; });
}

bool FinanceController::transactionMatchesBudget(
    const Transaction& transaction,
    const Budget& budget
    ) const
{
    if (transaction.type() != TransactionType::Expense ||
        isTransfer(transaction)) {
        return false;
    }
    if (!budget.allAccounts() &&
        !budget.accountIds().contains(transaction.accountId())) {
        return false;
    }
    if (budget.allCategories()) {
        return true;
    }
    return std::any_of(
        budget.categoryLimits().cbegin(), budget.categoryLimits().cend(),
        [&transaction](const BudgetCategoryLimit& limit)
        {
            return limit.categoryId == transaction.categoryId();
        });
}

void FinanceController::refreshBudgetMonthLimits()
{
    budgetMonthLimits_.clear();
    if (!selectedBudgetMonth_.isValid()) {
        return;
    }
    for (const Budget& budget : budgets_) {
        if (!repository_.ensureBudgetMonth(
                budget.id(), selectedBudgetMonth_,
                budget.defaultLimitMinor())) {
            qWarning() << "Failed to materialize budget month:"
                       << repository_.lastError();
        }
        budgetMonthLimits_.insert(
            budget.id(),
            repository_.loadBudgetLimit(
                budget.id(), selectedBudgetMonth_,
                budget.defaultLimitMinor()));
    }
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

std::optional<AccountType> FinanceController::accountTypeFromString(
    const QString& type)
{
    const QString value = type.trimmed().toLower();
    if (value == QStringLiteral("cash")) return AccountType::Cash;
    if (value == QStringLiteral("debit_card")) return AccountType::DebitCard;
    if (value == QStringLiteral("credit_card")) return AccountType::CreditCard;
    if (value == QStringLiteral("crypto_wallet")) return AccountType::CryptoWallet;
    if (value == QStringLiteral("brokerage")) return AccountType::Brokerage;
    if (value == QStringLiteral("deposit")) return AccountType::Deposit;
    if (value == QStringLiteral("other")) return AccountType::Other;
    return std::nullopt;
}

QString FinanceController::accountTypeToString(const AccountType type)
{
    switch (type) {
    case AccountType::Cash: return QStringLiteral("cash");
    case AccountType::DebitCard: return QStringLiteral("debit_card");
    case AccountType::CreditCard: return QStringLiteral("credit_card");
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

QVariantList FinanceController::goalSources() const
{
    QVariantList result;
    for (const auto& account : accounts_) {
        result.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("account:") + account.id()},
            {QStringLiteral("name"), accountDisplayName(account) + QStringLiteral(" · ") + currencyCode(account.currency())}});
    }
    for (const auto& wallet : cryptoWallets_) {
        result.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("wallet:") + wallet.id()},
            {QStringLiteral("name"), wallet.symbol() + QStringLiteral(" · ") + wallet.address()}});
    }
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap();
        result.append(QVariantMap{{"id", "exchange:" + account.value("id").toString()}, {"name", account.value("name").toString() + " · Bybit"}});
    }
    return result;
}

QVector<GoalAssetValue> FinanceController::goalAssetValues() const
{
    // Use all current balances, independent of the overview's selected asset/date.
    QVector<GoalAssetValue> values;
    const auto now = QDateTime::currentDateTimeUtc();
    const __int128_t maximum = std::numeric_limits<qint64>::max();
    const __int128_t minimum = std::numeric_limits<qint64>::min();
    for (const auto& account : accounts_) {
        const QString source = QStringLiteral("account:") + account.id();
        __int128_t balance = account.initialBalanceMinor();
        for (const auto& transaction : transactions_) {
            if (transaction.accountId() == account.id() && transaction.date() <= now) {
                const __int128_t amount = transaction.money().minorUnits();
                balance += transaction.type() == TransactionType::Income ? amount : -amount;
            }
        }
        for(const auto& op:investmentOperations_)if(op.accountId==account.id() && op.occurredAtUtc<=now)balance+=op.cashDeltaMinor;
        values.append({source, Money(static_cast<qint64>(std::clamp(balance, minimum, maximum)),
                                     account.currency()), balance >= minimum && balance <= maximum});
        if (account.assetType() != AssetType::Investment) continue;
        for (const auto& position : investmentPositions_) {
            if (position.accountId() != account.id()) continue;
            const auto instrument = std::find_if(investmentInstruments_.cbegin(), investmentInstruments_.cend(),
                [&position](const auto& item) { return item.id() == position.instrumentId(); });
            const auto quote = std::find_if(investmentQuotes_.cbegin(), investmentQuotes_.cend(),
                [&position](const auto& item) { return item.instrumentId() == position.instrumentId(); });
            const auto value=investmentPositionValue(position);
            values.append({source,Money(value.valueMinor,account.currency()),value.available});
        }
    }
    for (const auto& wallet : cryptoWallets_) {
        const qint64 price = cryptoPricesUsdMicros_.value(wallet.symbol());
        bool available = wallet.balanceFetchedAtUtc().isValid() &&
            (wallet.balanceAtomic() == 0 || price > 0) && wallet.decimals() >= 0 && wallet.decimals() <= 18;
        __int128_t usdMinor = 0;
        if (available && wallet.balanceAtomic() > 0) {
            __int128_t divisor = 10'000;
            for (int i = 0; i < wallet.decimals(); ++i) divisor *= 10;
            const __int128_t product = static_cast<__int128_t>(wallet.balanceAtomic()) * price;
            usdMinor = (product + divisor / 2) / divisor;
            if (usdMinor > maximum) { usdMinor = maximum; available = false; }
        }
        values.append({QStringLiteral("wallet:") + wallet.id(),
                       Money(static_cast<qint64>(usdMinor), Currency::USD), available});
    }
    for (const auto& value : cryptoExchanges_) {
        const auto account = value.toMap();
        values.append({"exchange:" + account.value("id").toString(), Money(exchangeUsdMinor(account), Currency::USD), account.value("fetchedAtMs").toLongLong() > 0});
    }
    return values;
}

QVariantList FinanceController::financialGoals() const
{
    QVariantList result;
    const auto assets = goalAssetValues();
    for (const auto& goal : financialGoals_) {
        const auto progress = FinancialGoalCalculator::calculate(goal, assets, rateProvider_, QDate::currentDate());
        result.append(QVariantMap{
            {QStringLiteral("id"), goal.id}, {QStringLiteral("name"), goal.name},
            {QStringLiteral("currency"), currencyCode(goal.currency)},
            {QStringLiteral("targetMinor"), goal.targetMinor},
            {QStringLiteral("deadline"), goal.deadline.toString(Qt::ISODate)},
            {QStringLiteral("allSources"), goal.allSources},
            {QStringLiteral("sourceIds"), goal.sourceIds},
            {QStringLiteral("currentMinor"), progress.currentMinor},
            {QStringLiteral("remainingMinor"), progress.remainingMinor},
            {QStringLiteral("ratio"), progress.ratio},
            {QStringLiteral("complete"), progress.complete},
            {QStringLiteral("achieved"), progress.achieved},
            {QStringLiteral("overdue"), progress.overdue}});
    }
    return result;
}

QVariantMap FinanceController::saveFinancialGoal(const QVariantMap& values)
{
    auto error = [](const QString& text) { return QVariantMap{
        {QStringLiteral("ok"), false}, {QStringLiteral("error"), text}}; };
    FinancialGoal goal;
    goal.id = values.value(QStringLiteral("id")).toString();
    const bool editing = !goal.id.isEmpty();
    const auto existing = std::find_if(financialGoals_.cbegin(), financialGoals_.cend(),
        [&goal](const auto& item) { return item.id == goal.id; });
    if (editing && existing == financialGoals_.cend()) return error(tr("Цель не найдена"));
    goal.name = values.value(QStringLiteral("name")).toString().trimmed();
    if (goal.name.isEmpty() || goal.name.size() > 80)
        return error(tr("Введите название длиной до 80 символов"));
    const auto code = values.value(QStringLiteral("currency")).toString();
    if (code != QStringLiteral("RUB") && code != QStringLiteral("USD") && code != QStringLiteral("EUR"))
        return error(tr("Выберите валюту цели"));
    goal.currency = currencyFromString(code);
    // Parse decimal text without floating-point rounding or silent truncation.
    QString amount = values.value(QStringLiteral("amountText")).toString().trimmed();
    amount.replace(QLatin1Char(','), QLatin1Char('.'));
    const QRegularExpression pattern(QStringLiteral("^([0-9]{1,12})(?:[.]([0-9]{1,2}))?$"));
    const auto match = pattern.match(amount);
    if (!match.hasMatch()) return error(tr("Введите положительную сумму, не более двух знаков после запятой"));
    goal.targetMinor = match.captured(1).toLongLong() * 100 +
        match.captured(2).leftJustified(2, QLatin1Char('0')).toLongLong();
    if (goal.targetMinor <= 0) return error(tr("Сумма цели должна быть больше нуля"));
    const auto deadline = values.value(QStringLiteral("deadline")).toString().trimmed();
    if (!deadline.isEmpty()) {
        const QString format = QStringLiteral("dd.MM.yyyy");
        goal.deadline = QDate::fromString(deadline, format);
        if (!goal.deadline.isValid() || goal.deadline.toString(format) != deadline)
            return error(tr("Введите дату в формате ДД.ММ.ГГГГ"));
    }
    goal.allSources = values.value(QStringLiteral("allSources"), true).toBool();
    if (!goal.allSources) {
        const auto sources = goalSources();
        for (const auto& value : values.value(QStringLiteral("sourceIds")).toList()) {
            const auto id = value.toString();
            const bool exists = std::any_of(sources.cbegin(), sources.cend(),
                [&id](const auto& item) { return item.toMap().value(QStringLiteral("id")).toString() == id; });
            // Keep selected archived sources visible as unavailable until user removes them.
            if (!exists && !(editing && existing->sourceIds.contains(id)))
                return error(tr("Один из выбранных счетов больше не доступен"));
            if (!goal.sourceIds.contains(id)) goal.sourceIds.append(id);
        }
        if (goal.sourceIds.isEmpty()) return error(tr("Выберите хотя бы один счёт или кошелёк"));
    }
    if (!editing) goal.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!repository_.saveFinancialGoal(goal, editing)) return error(tr("Не удалось сохранить цель"));
    financialGoals_ = repository_.loadFinancialGoals();
    emit financialGoalsChanged();
    return {{QStringLiteral("ok"), true}};
}

bool FinanceController::deleteFinancialGoal(const QString& id)
{
    if (!repository_.deleteFinancialGoal(id)) return false;
    financialGoals_ = repository_.loadFinancialGoals();
    emit financialGoalsChanged();
    return true;
}

void FinanceController::captureCapitalSnapshot(const bool overwriteToday)
{
    if (!repository_.isOpen()) return;
    if (accounts_.isEmpty() && cryptoWallets_.isEmpty() && cryptoExchanges_.isEmpty()) return;
    const QDate today = QDate::currentDate();
    if (!overwriteToday && lastCapitalSnapshotDate_ == today) return;
    qint64 total = 0;
    for (const auto& asset : goalAssetValues()) {
        if (!asset.available || rateProvider_.rateToUsd(asset.value.currency()) <= 0 ||
            rateProvider_.rateToUsd(appCurrency_) <= 0) return;
        total = saturatedCapitalAdd(total,
            currencyConverter_.convert(asset.value, appCurrency_).minorUnits());
    }
    if (!repository_.saveCapitalSnapshot(
            {today, appCurrency_, total})) {
        qWarning() << "Failed to save capital snapshot:" << repository_.lastError();
    } else {
        lastCapitalSnapshotDate_ = today;
    }
}

QVariantMap FinanceController::financialTrajectory()
{
    const QDate today = QDate::currentDate();
    const QDate end = QDate(today.year(), today.month(), 1).addDays(-1);
    const int months = (trajectorySettings_.analysisMonths == 3 ||
                        trajectorySettings_.analysisMonths == 12)
        ? trajectorySettings_.analysisMonths : 6;
    const QDate firstIncludedMonth = end.addMonths(-(months - 1));
    const QDate start(firstIncludedMonth.year(), firstIncludedMonth.month(), 1);
    qint64 income = 0;
    qint64 expense = 0;
    for (const auto& transaction : transactions_) {
        const QDate date = transaction.date().date();
        if (date < start || date > end || isTransfer(transaction)) continue;
        const qint64 amount = currencyConverter_.convert(
            transaction.money(), appCurrency_).minorUnits();
        if (transaction.type() == TransactionType::Income)
            income = saturatedCapitalAdd(income, amount);
        else
            expense = saturatedCapitalAdd(expense, amount);
    }
    const qint64 averageIncome = income / months;
    const qint64 averageExpense = expense / months;

    qint64 currentCapital = 0;
    qint64 investmentCapital = 0;
    bool complete = true;
    for (const auto& asset : goalAssetValues()) {
        complete = complete && asset.available &&
            rateProvider_.rateToUsd(asset.value.currency()) > 0;
        if (!asset.available) continue;
        const qint64 converted = currencyConverter_.convert(
            asset.value, appCurrency_).minorUnits();
        currentCapital = saturatedCapitalAdd(currentCapital, converted);
        if (asset.sourceId.startsWith(QStringLiteral("account:"))) {
            const QString accountId = asset.sourceId.mid(8);
            const auto account = std::find_if(accounts_.cbegin(), accounts_.cend(),
                [&accountId](const auto& item) { return item.id() == accountId; });
            if (account != accounts_.cend() && account->assetType() == AssetType::Investment)
                investmentCapital = saturatedCapitalAdd(investmentCapital, converted);
        }
    }

    FinancialTrajectoryInput input;
    input.currentCapitalMinor = currentCapital;
    input.investmentCapitalMinor = std::max<qint64>(0, investmentCapital);
    input.averageIncomeMinor = averageIncome;
    input.averageExpenseMinor = averageExpense;
    input.settings = trajectorySettings_;
    input.currentDate = today;
    const auto forecast = FinancialTrajectoryCalculator::calculate(input);

    QVariantList historyRows;
    for (const auto& snapshot : repository_.loadCapitalSnapshots()) {
        historyRows.append(QVariantMap{
            {QStringLiteral("date"), snapshot.date.toString(Qt::ISODate)},
            {QStringLiteral("valueMinor"), currencyConverter_.convert(
                Money(snapshot.totalMinor, snapshot.currency), appCurrency_).minorUnits()}});
    }
    QVariantList forecastRows;
    for (const auto& point : forecast) {
        forecastRows.append(QVariantMap{
            {QStringLiteral("date"), point.date.toString(Qt::ISODate)},
            {QStringLiteral("valueMinor"), point.nominalMinor},
            {QStringLiteral("realMinor"), point.realMinor}});
    }
    const qint64 saving = saturatedCapitalSubtract(averageIncome, averageExpense);
    return {
        {QStringLiteral("currency"), currencyCode(appCurrency_)},
        {QStringLiteral("complete"), complete},
        {QStringLiteral("currentMinor"), currentCapital},
        {QStringLiteral("averageIncomeMinor"), averageIncome},
        {QStringLiteral("averageExpenseMinor"), averageExpense},
        {QStringLiteral("averageSavingMinor"), saving},
        {QStringLiteral("savingRate"), averageIncome > 0
            ? static_cast<double>(saving) * 100.0 / averageIncome : 0.0},
        {QStringLiteral("history"), historyRows},
        {QStringLiteral("forecast"), forecastRows},
        {QStringLiteral("analysisMonths"), trajectorySettings_.analysisMonths},
        {QStringLiteral("horizonMonths"), trajectorySettings_.horizonMonths},
        {QStringLiteral("annualReturnPercent"), trajectorySettings_.annualReturnPercent},
        {QStringLiteral("annualInflationPercent"), trajectorySettings_.annualInflationPercent},
        {QStringLiteral("incomeChangePercent"), trajectorySettings_.incomeChangePercent},
        {QStringLiteral("expenseChangePercent"), trajectorySettings_.expenseChangePercent},
        {QStringLiteral("purchaseMinor"), trajectorySettings_.purchaseMinor},
        {QStringLiteral("purchaseMonth"), trajectorySettings_.purchaseMonth}};
}

QVariantMap FinanceController::saveFinancialTrajectorySettings(const QVariantMap& values)
{
    FinancialTrajectorySettings next = trajectorySettings_;
    next.analysisMonths = values.value(QStringLiteral("analysisMonths"), next.analysisMonths).toInt();
    next.horizonMonths = values.value(QStringLiteral("horizonMonths"), next.horizonMonths).toInt();
    next.annualReturnPercent = values.value(QStringLiteral("annualReturnPercent"), next.annualReturnPercent).toDouble();
    next.annualInflationPercent = values.value(QStringLiteral("annualInflationPercent"), next.annualInflationPercent).toDouble();
    next.incomeChangePercent = values.value(QStringLiteral("incomeChangePercent"), next.incomeChangePercent).toDouble();
    next.expenseChangePercent = values.value(QStringLiteral("expenseChangePercent"), next.expenseChangePercent).toDouble();
    next.purchaseMinor = values.value(QStringLiteral("purchaseMinor"), next.purchaseMinor).toLongLong();
    next.purchaseMonth = values.value(QStringLiteral("purchaseMonth"), next.purchaseMonth).toInt();
    const auto finite = [](double value) { return std::isfinite(value) && value > -100.0 && value <= 1000.0; };
    if ((next.analysisMonths != 3 && next.analysisMonths != 6 && next.analysisMonths != 12) ||
        (next.horizonMonths != 6 && next.horizonMonths != 12 && next.horizonMonths != 36 && next.horizonMonths != 60) ||
        !finite(next.annualReturnPercent) || !finite(next.annualInflationPercent) ||
        !finite(next.incomeChangePercent) || !finite(next.expenseChangePercent) ||
        next.purchaseMinor < 0 || next.purchaseMonth < 0 || next.purchaseMonth > next.horizonMonths) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), tr("Проверьте параметры прогноза")}};
    }
    if (!repository_.saveFinancialTrajectorySettings(next)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), tr("Не удалось сохранить параметры прогноза")}};
    }
    trajectorySettings_ = next;
    emit financialTrajectoryChanged();
    return {{QStringLiteral("ok"), true}};
}

namespace {
QVariantMap investmentTermsToVariant(const InvestmentTerms& t)
{
    return {{"pricing",t.pricing},{"quoteCurrency",t.currencyCode},{"quoteSource",t.quoteSource},{"quoteSourceLabel",
            t.quoteSource=="user_trade" ? QCoreApplication::translate("FinanceController","Цена вашей сделки") :
            t.quoteSource=="user_clearing" ? QCoreApplication::translate("FinanceController","Ваш расчёт") :
            t.quoteSource=="prevprice" ? QCoreApplication::translate("FinanceController","Предыдущее закрытие") :
            (t.quoteSource=="settleprice_clr" || t.quoteSource=="prevsettleprice") ? QCoreApplication::translate("FinanceController","Расчётная цена") :
            t.quoteSource=="last" ? QCoreApplication::translate("FinanceController","Последняя сделка") :
            QCoreApplication::translate("FinanceController","Цена биржи")},{"quantityUnit",t.quantityUnit},
        {"faceValueText",formatMicros(t.faceValueMicros)},{"accruedInterestText",t.accruedInterestMicros<0?QString():formatMicros(t.accruedInterestMicros)},
        {"priceStepText",formatMicros(t.priceStepMicros)},{"stepValueText",formatMicros(t.stepValueMicros)},
        {"multiplierText",formatMicros(t.multiplierMicros)},{"lotSizeText",formatMicros(t.lotSizeMicros)},
        {"settlementPriceText",formatMicros(t.settlementPriceMicros)},{"strikeText",formatMicros(t.strikeMicros)},
        {"maturity",t.maturity.toString(Qt::ISODate)},{"optionRight",t.optionRight},{"underlying",t.underlying},{"perpetual",t.perpetual}};
}

bool parseSignedInvestmentNumber(QString text, qint64& result, bool minor = false)
{
    text=text.trimmed();text.remove(QLatin1Char(' '));text.remove(QChar(0x00A0));text.replace(QChar(0x2212),QLatin1Char('-'));
    const bool negative=text.startsWith('-');if(negative || text.startsWith('+'))text.remove(0,1);
    static const QRegularExpression zero(QStringLiteral("^0+(?:[.,]0{1,6})?$"));
    if(zero.match(text).hasMatch()){result=0;return true;}
    qint64 n=0;if(!parsePositiveMicros(text,n))return false;
    if(minor){if(n%10'000!=0)return false;n/=10'000;}
    result=negative?-n:n;return true;
}
QDateTime parseInvestmentDate(const QString& text)
{
    auto date=QDateTime::fromString(text,QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    if(!date.isValid())date=QDateTime::fromString(text,QStringLiteral("dd.MM.yyyy HH:mm"));
    if(!date.isValid())date=QDateTime::fromString(text,Qt::ISODate);
    return date;
}
QString investmentOperationName(const QString& kind)
{
    if(kind=="buy")return QCoreApplication::translate("FinanceController","Покупка / увеличение позиции");
    if(kind=="sell")return QCoreApplication::translate("FinanceController","Продажа / уменьшение позиции");
    if(kind=="coupon")return QCoreApplication::translate("FinanceController","Купон");
    if(kind=="dividend")return QCoreApplication::translate("FinanceController","Дивиденды / выплата фонда");
    if(kind=="amortization")return QCoreApplication::translate("FinanceController","Частичное погашение номинала");
    if(kind=="fee")return QCoreApplication::translate("FinanceController","Комиссия");
    if(kind=="margin")return QCoreApplication::translate("FinanceController","Расчёт вариационной маржи");
    return QCoreApplication::translate("FinanceController","Погашение / исполнение контракта");
}
}

QVariantMap FinanceController::saveInvestmentPosition(const QVariantMap& v)
{
    const auto fail=[](const QString& e){return QVariantMap{{"ok",false},{"error",e}};};
    if(selectedAsset_!=AssetType::Investment)return fail(tr("Сначала выберите актив «Инвестиции»"));
    const auto id=v.value("id").toString(),accountId=v.value("accountId").toString();
    const bool editing=!id.isEmpty();const int index=v.value("searchIndex",-2).toInt();
    const auto account=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==accountId && a.type()==AccountType::Brokerage;});
    if(account==accounts_.cend())return fail(tr("Выберите брокерский счёт"));
    const auto current=std::find_if(investmentPositions_.cbegin(),investmentPositions_.cend(),[&](const auto& p){return p.id()==id;});
    if(editing && current==investmentPositions_.cend())return fail(tr("Позиция не найдена"));
    const InvestmentMarketInstrument* selected=nullptr;const InvestmentInstrument* previous=nullptr;
    if(editing){const auto i=std::find_if(investmentInstruments_.cbegin(),investmentInstruments_.cend(),[&](const auto& i){return i.id()==current->instrumentId();});
        if(i==investmentInstruments_.cend())return fail(tr("Инструмент не найден"));previous=&*i;}
    if(index>=0){if(index>=investmentSearchResults_.size())return fail(tr("Выберите инструмент"));selected=&investmentSearchResults_[index];}
    else if(index==-1 && !previous)return fail(tr("Выберите инструмент"));
    else if(index < -2)return fail(tr("Выберите инструмент"));
    QString instrumentId,symbol,isin,name,market,board;InvestmentInstrumentType type;
    InvestmentTerms terms;Currency valuationCurrency=Currency::RUB;std::optional<InvestmentQuote> quote;
    if(selected){instrumentId=selected->id();symbol=selected->symbol();isin=selected->isin();name=selected->name();type=selected->type();
        market="MOEX";board=selected->primaryBoardId();terms=selected->terms();
        valuationCurrency=currencyFromString(selected->currencyCode());
        if(selected->hasQuote())quote=InvestmentQuote(instrumentId,selected->priceMicros(),selected->quotedAtUtc(),terms);
    }else if(index==-1){instrumentId=previous->id();symbol=previous->symbol();isin=previous->isin();name=previous->name();type=previous->type();
        market=previous->marketCode();board=previous->primaryBoardId();terms=previous->terms();valuationCurrency=previous->currency();
        const auto q=std::find_if(investmentQuotes_.cbegin(),investmentQuotes_.cend(),[&](const auto& q){return q.instrumentId()==instrumentId;});
        if(q!=investmentQuotes_.cend()){quote=*q;if(!q->terms().engine.isEmpty())terms=q->terms();}
        if(terms.engine.isEmpty() && market=="MOEX")terms=moexDefaultTerms(type);
    }else{const int value=v.value("instrumentType",4).toInt();if(value<0 || value>10)return fail(tr("Неизвестный вид инструмента"));
        type=static_cast<InvestmentInstrumentType>(value);instrumentId="manual:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        symbol=v.value("symbol").toString().trimmed();name=v.value("name").toString().trimmed();isin=v.value("isin").toString().trimmed();
        if(symbol.isEmpty() || name.isEmpty())return fail(tr("Укажите название и обозначение инструмента"));
        terms=moexDefaultTerms(type);terms.engine.clear();terms.market.clear();terms.pricing=type==InvestmentInstrumentType::Future?QStringLiteral("future"):QStringLiteral("manual");
        terms.currencyCode=v.value("quoteCurrency",currencyCode(account->currency())).toString().trimmed().toUpper();
        valuationCurrency=account->currency();
        if(type==InvestmentInstrumentType::Option){
            const auto scheme=v.value("optionPricing",QStringLiteral("premium_option")).toString();
            if(scheme!="premium_option" && scheme!="margined_option")return fail(tr("Выберите схему расчётов опциона"));
            terms.pricing=scheme;terms.optionRight=v.value("optionRight",QStringLiteral("C")).toString();
            if(terms.optionRight!="C" && terms.optionRight!="P")return fail(tr("Выберите право покупки или продажи"));
            if(!v.value("strike").toString().isEmpty() && !parsePositiveMicros(v.value("strike").toString(),terms.strikeMicros))return fail(tr("Некорректная цена исполнения"));
        }
        terms.underlying=v.value("underlying").toString().trimmed();
        if(!v.value("maturity").toString().trimmed().isEmpty()){
            terms.maturity=QDate::fromString(v.value("maturity").toString(),QStringLiteral("dd.MM.yyyy"));
            if(!terms.maturity.isValid())return fail(tr("Укажите дату исполнения в формате дд.мм.гггг"));
        }
        for(const auto& existing:investmentInstruments_)if(existing.marketCode().isEmpty() && existing.type()==type &&
            existing.symbol().compare(symbol,Qt::CaseInsensitive)==0 && existing.currency()==valuationCurrency){instrumentId=existing.id();break;}
    }
    // A shared instrument's accounting currency is stable even across accounts.
    const auto existing=std::find_if(investmentInstruments_.cbegin(),investmentInstruments_.cend(),[&](const auto& i){return i.id()==instrumentId;});
    if(existing!=investmentInstruments_.cend())valuationCurrency=existing->currency();
    if(terms.currencyCode.isEmpty())terms.currencyCode=currencyCode(valuationCurrency);
    if(editing && repository_.hasInvestmentOperations(id) && (current->accountId()!=accountId || current->instrumentId()!=instrumentId))
        return fail(tr("У позиции есть операции: счёт и инструмент менять нельзя"));
    for(const auto& p:investmentPositions_)if(p.id()!=id && p.accountId()==accountId && p.instrumentId()==instrumentId)
        return fail(tr("Этот инструмент уже добавлен на выбранный счёт"));
    qint64 quantity=0,average=editing?current->averagePriceMicros():0;
    if(!parsePositiveMicros(v.value("quantity").toString(),quantity))return fail(tr("Введите количество больше нуля, до 6 знаков после запятой"));
    if((type==InvestmentInstrumentType::Future || type==InvestmentInstrumentType::Option || type==InvestmentInstrumentType::Bond ||
        type==InvestmentInstrumentType::Stock || type==InvestmentInstrumentType::PreferredStock || type==InvestmentInstrumentType::DepositaryReceipt)
        && quantity%1'000'000!=0)return fail(tr("Для этого инструмента количество должно быть целым"));
    const auto avg=v.value("averagePrice").toString().trimmed();
    if(!avg.isEmpty() && (!parseSignedInvestmentNumber(avg,average) || average<0))return fail(tr("Введите корректную среднюю цену"));
    if(avg.isEmpty() && !editing && quote)average=qMax<qint64>(0,quote->priceMicros());
    auto settings=editing?current->settings():InvestmentPositionSettings{};
    if(v.contains("direction"))settings.direction=v.value("direction").toInt();
    if(settings.direction!=1 && settings.direction!=-1)return fail(tr("Выберите направление позиции"));
    if(settings.direction==-1 && type!=InvestmentInstrumentType::Future && type!=InvestmentInstrumentType::Option)
        return fail(tr("Проданная позиция поддерживается для фьючерсов и опционов"));
    if(v.contains("manualValuation"))settings.manualValuation=v.value("manualValuation").toBool();
    if(index==-2)settings.manualValuation=true;
    if(settings.manualValuation){qint64 value=0;
        if(!parseSignedInvestmentNumber(v.value("manualValue",QString::number(settings.manualValueMinor/100.0,'f',2)).toString(),value,true))return fail(tr("Укажите стоимость всей позиции в валюте счёта, до 2 знаков после запятой"));
        if(value<0 && type!=InvestmentInstrumentType::Future && type!=InvestmentInstrumentType::Option)return fail(tr("Стоимость этого актива не может быть отрицательной"));
        settings.manualValueMinor=value;settings.valuedAtUtc=QDateTime::currentDateTimeUtc();
    }
    const auto parseSetting=[&](const char* key,qint64& target,bool minor,bool positive) {
        if(!v.contains(key) || v.value(key).toString().trimmed().isEmpty())return true;
        qint64 n=0;if(!parseSignedInvestmentNumber(v.value(key).toString(),n,minor) || (positive && n<0))return false;target=n;return true;
    };
    if(v.contains("fxRate") && v.value("fxRate").toString().trimmed().isEmpty())settings.manualFxRateMicros=0;
    if(!parseSetting("blockedMargin",settings.blockedMarginMinor,true,true) || !parseSetting("adjustment",settings.unsettledAdjustmentMinor,true,false)
        || !parseSetting("fxRate",settings.manualFxRateMicros,false,true))return fail(tr("Некорректное обеспечение, поправка или курс валюты"));
    const bool margined=terms.pricing=="future" || terms.pricing=="margined_option";
    if(margined && !settings.manualValuation){
        if(!parseSetting("referencePrice",settings.referencePriceMicros,false,false))return fail(tr("Некорректная цена последнего расчёта"));
        if(v.contains("referenceDate")){
            const auto date=parseInvestmentDate(v.value("referenceDate").toString());
            if(!date.isValid() || date>QDateTime::currentDateTime())return fail(tr("Укажите дату и время последнего расчёта"));
            settings.referenceAtUtc=date.toUTC();
        }
        if(!settings.referenceAtUtc.isValid())return fail(tr("Укажите цену, дату и время последнего расчёта или открытия позиции"));
    }
    // Ensure the result can be valued before saving an automatic position.
    const long double source=rateProvider_.rateToRubMicros(terms.currencyCode),target=rateProvider_.rateToRubMicros(currencyCode(account->currency()));
    qint64 fx=settings.manualFxRateMicros;if(fx<=0 && source>0 && target>0){const auto n=source/target*1'000'000.0L;
        if(n<std::numeric_limits<qint64>::max())fx=static_cast<qint64>(std::round(n));}
    const auto now=QDateTime::currentDateTimeUtc();
    InvestmentPosition position(editing?id:QUuid::createUuid().toString(QUuid::WithoutBraces),accountId,instrumentId,quantity,average,
        editing?current->createdAtUtc():now,now,settings);
    const auto value=valueInvestmentPosition(position,quote?&*quote:nullptr,terms,fx);
    if(!value.available)return fail(value.error);
    std::optional<InvestmentOperation> operation;
    if(!editing && v.value("purchase").toBool()){
        qint64 cash=0;if(!parseSignedInvestmentNumber(v.value("cashAmount").toString(),cash,true))return fail(tr("Укажите фактическое изменение денег, со знаком"));
        if((type!=InvestmentInstrumentType::Future && terms.pricing!="margined_option") &&
            ((settings.direction==1 && cash>=0) || (settings.direction==-1 && cash<=0)))return fail(tr("Покупка уменьшает деньги, продажа опциона увеличивает; укажите соответствующий знак"));
        operation=InvestmentOperation{QUuid::createUuid().toString(QUuid::WithoutBraces),accountId,position.id(),instrumentId,"buy",cash,quantity,now,
            tr("Открытие позиции: %1").arg(symbol)};
    }
    const InvestmentInstrument instrument(instrumentId,symbol,isin,name,type,valuationCurrency,market,board,terms);
    if(!repository_.saveInvestmentBundle(instrument,position,quote,editing,operation))return fail(tr("Не удалось сохранить позицию: %1").arg(repository_.lastError()));
    reloadInvestments();return {{"ok",true}};
}

QVariantMap FinanceController::recordInvestmentOperation(const QVariantMap& v)
{
    const auto fail=[](const QString& e){return QVariantMap{{"ok",false},{"error",e}};};
    const auto id=v.value("positionId").toString(),kind=v.value("kind").toString();
    const auto p=std::find_if(investmentPositions_.cbegin(),investmentPositions_.cend(),[&](const auto& p){return p.id()==id;});
    if(p==investmentPositions_.cend())return fail(tr("Позиция не найдена"));
    const auto i=std::find_if(investmentInstruments_.cbegin(),investmentInstruments_.cend(),[&](const auto& i){return i.id()==p->instrumentId();});
    if(i==investmentInstruments_.cend())return fail(tr("Инструмент не найден"));
    const QStringList kinds{"buy","sell","coupon","dividend","amortization","fee","margin","expiry"};
    if(!kinds.contains(kind))return fail(tr("Неизвестный вид операции"));
    auto date=parseInvestmentDate(v.value("date").toString());
    if(!date.isValid() || date>QDateTime::currentDateTime())return fail(tr("Укажите дату и время фактической операции"));date=date.toUTC();
    // Quantity/clearing mutations must follow the last recorded mutation.
    if(kind=="buy" || kind=="sell" || kind=="margin" || kind=="expiry")for(const auto& op:investmentOperations_)
        if(op.positionId==id && op.occurredAtUtc.toSecsSinceEpoch()>date.toSecsSinceEpoch() && (op.kind=="buy" || op.kind=="sell" || op.kind=="margin" || op.kind=="expiry"))
            return fail(tr("Изменение позиции нужно записывать после её последней операции"));
    qint64 cash=0;if(!parseSignedInvestmentNumber(v.value("cashAmount").toString(),cash,true))return fail(tr("Укажите фактическое изменение денег со знаком, до 2 знаков после запятой"));
    if((kind=="coupon" || kind=="dividend" || kind=="amortization") && cash<=0)return fail(tr("Выплата должна увеличивать денежный остаток"));
    if(kind=="fee" && cash>=0)return fail(tr("Комиссия должна уменьшать денежный остаток"));
    if((kind=="coupon" || kind=="amortization") && i->type()!=InvestmentInstrumentType::Bond)return fail(tr("Эта операция доступна для облигаций"));
    if(kind=="dividend" && i->type()!=InvestmentInstrumentType::Stock && i->type()!=InvestmentInstrumentType::PreferredStock &&
        i->type()!=InvestmentInstrumentType::DepositaryReceipt && i->type()!=InvestmentInstrumentType::Etf && i->type()!=InvestmentInstrumentType::Fund)
        return fail(tr("Эта выплата доступна для акций, расписок и фондов"));
    const auto q=std::find_if(investmentQuotes_.cbegin(),investmentQuotes_.cend(),[&](const auto& q){return q.instrumentId()==i->id();});
    auto terms=q!=investmentQuotes_.cend() && !q->terms().engine.isEmpty()?q->terms():i->terms();
    const bool margined=terms.pricing=="future" || terms.pricing=="margined_option" ||
        (i->type()==InvestmentInstrumentType::Future && p->settings().manualValuation);
    if(kind=="margin" && !margined)return fail(tr("Расчёт вариационной маржи доступен для маржируемых контрактов"));
    qint64 quantity=p->quantityMicros(),average=p->averagePriceMicros(),delta=0;auto settings=p->settings();bool close=false;
    std::optional<InvestmentQuote> adjustedQuote;
    if(kind=="coupon" || kind=="amortization"){
        if(settings.manualValuation){qint64 value=0;
            if(!parseSignedInvestmentNumber(v.value("remainingValue").toString(),value,true) || value<0)return fail(tr("Укажите стоимость облигаций после выплаты"));
            settings.manualValueMinor=value;settings.valuedAtUtc=date;
        }else{
            qint64 accrued=0;if(!parseSignedInvestmentNumber(v.value("accruedAfter").toString(),accrued) || accrued<0)
                return fail(tr("Укажите НКД после выплаты"));
            if(q==investmentQuotes_.cend())return fail(tr("Нужна котировка или ручная оценка облигаций"));
            terms.accruedInterestMicros=accrued;
            if(kind=="amortization"){
                qint64 face=0;if(!parsePositiveMicros(v.value("faceAfter").toString(),face) || face>=terms.faceValueMicros)
                    return fail(tr("Оставшийся номинал должен быть положительным и меньше прежнего; для полного погашения выберите исполнение"));
                terms.faceValueMicros=face;
            }
            adjustedQuote=InvestmentQuote(i->id(),q->priceMicros(),qMax(q->quotedAtUtc(),date),terms);
        }
    }
    if(kind=="buy" || kind=="sell"){
        qint64 n=0;if(!parsePositiveMicros(v.value("quantity").toString(),n))return fail(tr("Укажите количество операции"));
        if(i->type()!=InvestmentInstrumentType::Metal && i->type()!=InvestmentInstrumentType::Currency
            && i->type()!=InvestmentInstrumentType::Etf && i->type()!=InvestmentInstrumentType::Fund && n%1'000'000!=0)
            return fail(tr("Количество должно быть целым"));
        if(kind=="sell" && n>quantity)return fail(tr("Нельзя уменьшить позицию больше её количества"));
        delta=kind=="buy"?n:-n;
        if(delta>0 && quantity>std::numeric_limits<qint64>::max()-delta)return fail(tr("Количество слишком велико"));
        quantity+=delta;close=quantity==0;
        if(!margined && !settings.manualValuation){const int sign=(kind=="buy"?-1:1)*settings.direction;
            if(cash==0 || (cash>0?1:-1)!=sign)return fail(tr("Знак денег не соответствует направлению сделки"));}
        qint64 price=0;
        if(kind=="buy" || !v.value("price").toString().trimmed().isEmpty()){
            if(!parseSignedInvestmentNumber(v.value("price").toString(),price) || price<0)return fail(tr("Укажите цену сделки"));
            if(!settings.manualValuation && q!=investmentQuotes_.cend() && q->quotedAtUtc()<date){
                auto observed=terms;observed.quoteSource="user_trade";adjustedQuote=InvestmentQuote(i->id(),price,date,observed);
            }
        }
        if(kind=="buy"){
            const long double avg=(static_cast<long double>(p->quantityMicros())*average+static_cast<long double>(n)*price)/quantity;
            if(avg>=std::numeric_limits<qint64>::max())return fail(tr("Средняя цена вне диапазона"));average=static_cast<qint64>(std::round(avg));
            if(margined){
                // The unbooked result for existing contracts must survive an increase.
                const long double ref=(static_cast<long double>(p->quantityMicros())*settings.referencePriceMicros+static_cast<long double>(n)*price)/quantity;
                if(std::abs(ref)>=std::numeric_limits<qint64>::max())return fail(tr("Расчётная цена вне диапазона"));settings.referencePriceMicros=static_cast<qint64>(std::round(ref));
            }
        }
        if(kind=="sell" && !close){
            settings.blockedMarginMinor=static_cast<qint64>(std::round(static_cast<long double>(settings.blockedMarginMinor)*quantity/p->quantityMicros()));
            settings.unsettledAdjustmentMinor=static_cast<qint64>(std::round(static_cast<long double>(settings.unsettledAdjustmentMinor)*quantity/p->quantityMicros()));
        }
        if(settings.manualValuation && !close){qint64 value=0;if(!parseSignedInvestmentNumber(v.value("remainingValue").toString(),value,true))return fail(tr("Укажите новую ручную оценку оставшейся позиции"));settings.manualValueMinor=value;settings.valuedAtUtc=date;}
    }
    if(kind=="margin"){
        qint64 reference=0;if(!parseSignedInvestmentNumber(v.value("price").toString(),reference))return fail(tr("Укажите цену проведённого расчёта"));
        if(settings.referenceAtUtc.isValid() && date<=settings.referenceAtUtc)return fail(tr("Этот расчёт уже учтён; укажите следующий"));
        settings.referencePriceMicros=reference;settings.referenceAtUtc=date;settings.unsettledAdjustmentMinor=0;
        if(!settings.manualValuation && q!=investmentQuotes_.cend() && q->quotedAtUtc()<date){
            auto observed=terms;observed.quoteSource="user_clearing";adjustedQuote=InvestmentQuote(i->id(),reference,date,observed);
        }
        if(settings.manualValuation){settings.manualValueMinor=0;settings.valuedAtUtc=date;}
    }
    if(kind=="expiry"){close=true;delta=-quantity;}
    const InvestmentPosition updated(p->id(),p->accountId(),p->instrumentId(),close?p->quantityMicros():quantity,average,p->createdAtUtc(),QDateTime::currentDateTimeUtc(),settings);
    const InvestmentOperation op{QUuid::createUuid().toString(QUuid::WithoutBraces),p->accountId(),id,p->instrumentId(),kind,cash,delta,date,
        v.value("description").toString().trimmed().isEmpty()?investmentOperationName(kind)+QStringLiteral(" · ")+i->symbol():v.value("description").toString()};
    if(!repository_.bookInvestmentOperation(op,*p,updated,close,adjustedQuote))return fail(tr("Не удалось сохранить операцию: %1").arg(repository_.lastError()));
    reloadInvestments();if(kind=="coupon" || kind=="amortization")refreshInvestmentQuotes();return {{"ok",true}};
}

QVariantList FinanceController::investmentOperations() const
{
    QVariantList result;
    for(const auto& op:investmentOperations_){
        if(!selectedAccountId_.isEmpty() && selectedAccountId_!=op.accountId)continue;
        if(dateFilterActive() && (op.occurredAtUtc.toLocalTime().date()<dateFilterFrom_ || op.occurredAtUtc.toLocalTime().date()>dateFilterTo_))continue;
        const auto a=std::find_if(accounts_.cbegin(),accounts_.cend(),[&](const auto& a){return a.id()==op.accountId;});
        if(a==accounts_.cend())continue;
        result.append(QVariantMap{{"id",op.id},{"accountId",op.accountId},{"positionId",op.positionId},{"accountName",accountDisplayName(*a)},
            {"type","investment_cash"},{"kind",op.kind},{"amount",op.cashDeltaMinor},{"currency",currencyCode(a->currency())},
            {"categoryName",investmentOperationName(op.kind)},{"rawDescription",op.description},{"date",op.occurredAtUtc}});
    }return result;
}
