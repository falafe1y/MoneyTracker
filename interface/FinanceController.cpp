#include "FinanceController.h"

#include <QDebug>
#include <QUuid>

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
}

FinanceController::FinanceController(QObject* parent)
    : QObject(parent)
    , currencyConverter_(rateProvider_)
    , balanceCalculator_(currencyConverter_)
{
    if (!repository_.isOpen()) {
        qWarning() << "Failed to open finance database:"
                   << repository_.lastError();
        return;
    }

    appCurrency_ = currencyFromString(repository_.loadAppCurrency());
    selectedAsset_ = assetTypeFromString(repository_.loadSelectedAsset());
    transactions_ = repository_.loadTransactions();
    categories_ = repository_.loadCategories();
    accounts_ = repository_.loadAccounts();
    archivedCategoryIds_ = repository_.loadArchivedCategoryIds();
    summary_ = repository_.loadSummary();
}

qint64 FinanceController::balanceMinorUnits() const
{
    return convertedTotal(summary_.balance);
}

QString FinanceController::balanceCurrency() const
{
    return currencyCode(appCurrency_);
}

qint64 FinanceController::incomeMinorUnits() const
{
    return convertedTotal(summary_.income);
}

qint64 FinanceController::expenseMinorUnits() const
{
    return convertedTotal(summary_.expense);
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
}

QVariantList FinanceController::transactions() const
{
    QVariantList result;

    for (const Transaction& transaction : transactions_) {
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

        item["description"] = transaction.description();

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
        item["label"] = category.name();
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
        item["name"] = account.name();
        item["type"] = accountTypeToString(account.type());
        item["currency"] = currencyCode(account.currency());
        item["initialBalanceMinor"] = account.initialBalanceMinor();
        item["balanceMinor"] = accountBalanceMinor(account);
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
        item["name"] = account.name();
        item["asset"] = assetTypeToString(account.assetType());
        item["assetTitle"] = account.assetType() == AssetType::Fiat
            ? QStringLiteral("Фиат")
            : account.assetType() == AssetType::Crypto
                ? QStringLiteral("Крипта")
                : QStringLiteral("Инвестиции");
        item["currency"] = currencyCode(account.currency());
        item["displayName"] = item["assetTitle"].toString()
            + QStringLiteral(" · ") + account.name()
            + QStringLiteral(" · ") + currencyCode(account.currency());
        item["balanceMinor"] = accountBalanceMinor(account);
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
    if (!repository_.saveSelectedAsset(assetTypeToString(newAsset))) {
        qWarning() << "Failed to save selected asset:"
                   << repository_.lastError();
    }

    emit selectedAssetChanged();
    emit selectedAccountIdChanged();
    emit accountsChanged();
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
}

bool FinanceController::addAccount(
    const QString& name,
    const QString& type,
    const QString& currency,
    const qint64 initialBalanceMinor
    )
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 60) {
        return false;
    }

    for (const Account& existing : accounts_) {
        if (existing.assetType() == selectedAsset_ &&
            existing.name().compare(normalizedName, Qt::CaseInsensitive) == 0) {
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

    const Currency accountCurrency = currencyFromString(currency);
    const Account account(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        normalizedName,
        selectedAsset_,
        accountType,
        accountCurrency,
        initialBalanceMinor);

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
            existing.name().compare(normalizedName, Qt::CaseInsensitive) == 0) {
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

    const CategoryType type = categories_[categoryIndex].type();
    for (const Category& category : categories_) {
        if (category.id() != id &&
            !archivedCategoryIds_.contains(category.id()) &&
            category.type() == type &&
            category.name().compare(normalizedName, Qt::CaseInsensitive) == 0) {
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
            return category.name();
        }
    }
    return QStringLiteral("Без категории");
}

qint64 FinanceController::convertTransaction(
    int transactionIndex,
    const QString& targetCurrency
    ) const
{
    if (transactionIndex < 0 ||
        transactionIndex >= transactions_.size()) {
        return 0;
    }

    const Currency target = currencyFromString(
        targetCurrency
        );

    const Money converted = currencyConverter_.convert(
        transactions_[transactionIndex].money(),
        target
        );

    return converted.minorUnits();
}

bool FinanceController::addIncome(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency,
    const QString& accountId
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Income,
        description,
        categoryId,
        currencyFromString(currency),
        accountId
        );
}

bool FinanceController::addExpense(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency,
    const QString& accountId
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Expense,
        description,
        categoryId,
        currencyFromString(currency),
        accountId
        );
}

bool FinanceController::addTransfer(
    const qint64 sourceMinorUnits,
    const QString& description,
    const QString& sourceAccountId,
    const QString& targetAccountId
    )
{
    if (sourceMinorUnits <= 0 || sourceAccountId.isEmpty() ||
        targetAccountId.isEmpty() || sourceAccountId == targetAccountId) {
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
    const QDateTime occurredAt = QDateTime::currentDateTime();
    const qint64 targetMinorUnits = currencyConverter_.convert(
        Money(sourceMinorUnits, source->currency()), target->currency()).minorUnits();
    if (targetMinorUnits <= 0) {
        return false;
    }

    const QString normalizedDescription = description.trimmed().isEmpty()
        ? QStringLiteral("Перевод: %1 → %2").arg(source->name(), target->name())
        : description.trimmed();
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
    const QString& type
    )
{
    return updateOperation(
        id,
        minorUnits,
        description,
        categoryId,
        accountId,
        type,
        QString()
        );
}

bool FinanceController::updateOperation(
    const QString& id,
    const qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& accountId,
    const QString& type,
    const QString& targetAccountId
    )
{
    if (id.isEmpty() || minorUnits <= 0) {
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
        const QString normalizedDescription = description.trimmed().isEmpty()
            ? QStringLiteral("Перевод: %1 → %2").arg(source->name(), target->name())
            : description.trimmed();
        const Transaction outgoing(
            resolvedTransferId + QStringLiteral("-out"),
            source->id(),
            QStringLiteral("transfer-out"),
            Money(minorUnits, source->currency()),
            TransactionType::Expense,
            original.date(),
            normalizedDescription);
        const Transaction incoming(
            resolvedTransferId + QStringLiteral("-in"),
            target->id(),
            QStringLiteral("transfer-in"),
            Money(targetMinorUnits, target->currency()),
            TransactionType::Income,
            original.date(),
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
        original.date(),
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
    const QString& accountId
    )
{
    if (minorUnits <= 0) {
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
            QDateTime::currentDateTime(),
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
        balance += transaction.type() == TransactionType::Income
            ? transaction.money().minorUnits()
            : -transaction.money().minorUnits();
    }
    return balance;
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
    return total;
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
