#include "FinanceController.h"

#include <QDebug>
#include <QUuid>

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
    transactions_ = repository_.loadTransactions();
    categories_ = repository_.loadCategories();
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

        item["currency"] = currencyCode(
            transaction.money().currency()
            );

        item["type"] =
            transaction.type() == TransactionType::Income
                ? "income"
                : "expense";

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
    const QString& currency
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Income,
        description,
        categoryId,
        currencyFromString(currency)
        );
}

bool FinanceController::addExpense(
    qint64 minorUnits,
    const QString& description,
    const QString& categoryId,
    const QString& currency
    )
{
    return addTransaction(
        minorUnits,
        TransactionType::Expense,
        description,
        categoryId,
        currencyFromString(currency)
        );
}

bool FinanceController::addTransaction(
    qint64 minorUnits,
    TransactionType type,
    const QString& description,
    const QString& categoryId,
    Currency currency
    )
{
    if (minorUnits <= 0) {
        return false;
    }

    const Transaction transaction(
            QUuid::createUuid().toString(
                QUuid::WithoutBraces
                ),
            QStringLiteral("household-") +
                currencyCode(currency).toLower(),
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
    return true;
}

int FinanceController::currencyIndex(const Currency currency)
{
    return static_cast<int>(currency);
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
