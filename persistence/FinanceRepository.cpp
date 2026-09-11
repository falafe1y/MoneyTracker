#include "FinanceRepository.h"

#include "../core/Currency.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

#include <cmath>

namespace
{
constexpr int currencyIndex(const Currency currency)
{
    return static_cast<int>(currency);
}

Currency currencyFromCode(const QString& code)
{
    if (code == QStringLiteral("USD")) {
        return Currency::USD;
    }
    if (code == QStringLiteral("EUR")) {
        return Currency::EUR;
    }
    return Currency::RUB;
}

AssetType assetTypeFromInt(const int value)
{
    if (value == 1) {
        return AssetType::Crypto;
    }
    if (value == 2) {
        return AssetType::Investment;
    }
    return AssetType::Fiat;
}

bool isTransferCategory(const QString& categoryId)
{
    return categoryId == QStringLiteral("transfer-in") ||
           categoryId == QStringLiteral("transfer-out");
}

struct TransferComponentIds
{
    QString outgoing;
    QString incoming;
    bool valid = false;
};

TransferComponentIds transferComponentIds(
    const QString& id,
    const QString& categoryId
    )
{
    QString transferId;
    if (categoryId == QStringLiteral("transfer-out") &&
        id.endsWith(QStringLiteral("-out"))) {
        transferId = id.left(id.size() - 4);
    } else if (categoryId == QStringLiteral("transfer-in") &&
               id.endsWith(QStringLiteral("-in"))) {
        transferId = id.left(id.size() - 3);
    }

    if (transferId.isEmpty()) {
        return {};
    }

    return {
        transferId + QStringLiteral("-out"),
        transferId + QStringLiteral("-in"),
        true
    };
}

void prepareTransactionInsert(QSqlQuery& query)
{
    query.prepare(QStringLiteral(
        "INSERT INTO transactions(id, account_id, category_id, type, "
        "amount_minor, occurred_at, description, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
}

bool insertTransactionRow(
    QSqlQuery& query,
    const Transaction& transaction,
    const qint64 createdAt
    )
{
    query.bindValue(0, transaction.id());
    query.bindValue(1, transaction.accountId());
    query.bindValue(2, transaction.categoryId());
    query.bindValue(3, transaction.type() == TransactionType::Income ? 0 : 1);
    query.bindValue(4, transaction.money().minorUnits());
    query.bindValue(5, transaction.date().toMSecsSinceEpoch());
    query.bindValue(6, transaction.description());
    query.bindValue(7, createdAt);
    return query.exec();
}

bool deleteOperationRows(
    QSqlDatabase& database,
    const QString& id,
    QString& error
    )
{
    QSqlQuery lookup(database);
    lookup.prepare(QStringLiteral(
        "SELECT category_id FROM transactions WHERE id = ?"));
    lookup.addBindValue(id);
    if (!lookup.exec()) {
        error = lookup.lastError().text();
        return false;
    }
    if (!lookup.next()) {
        error = QStringLiteral("Transaction was not found");
        return false;
    }

    const QString categoryId = lookup.value(0).toString();
    lookup.finish();
    const TransferComponentIds componentIds = transferComponentIds(id, categoryId);

    QSqlQuery deletion(database);
    if (isTransferCategory(categoryId) && componentIds.valid) {
        deletion.prepare(QStringLiteral(
            "DELETE FROM transactions "
            "WHERE (id = ? AND category_id = 'transfer-out') "
            "   OR (id = ? AND category_id = 'transfer-in')"));
        deletion.addBindValue(componentIds.outgoing);
        deletion.addBindValue(componentIds.incoming);
    } else {
        deletion.prepare(QStringLiteral(
            "DELETE FROM transactions WHERE id = ?"));
        deletion.addBindValue(id);
    }

    if (!deletion.exec() || deletion.numRowsAffected() < 1) {
        error = deletion.lastError().isValid()
            ? deletion.lastError().text()
            : QStringLiteral("Transaction was not found");
        return false;
    }
    return true;
}
}

FinanceRepository::FinanceRepository(const QString& databasePath)
    : connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , database_(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_))
{
    const QString resolvedDatabasePath = databasePath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/moneytracker.sqlite3")
        : databasePath;
    const QString directory = QFileInfo(resolvedDatabasePath).absolutePath();

    if (!QDir().mkpath(directory)) {
        setLastError(QStringLiteral("Cannot create application data directory"));
        return;
    }

    database_.setDatabaseName(resolvedDatabasePath);
    if (!database_.open()) {
        setLastError(database_.lastError().text());
        return;
    }

    QSqlQuery pragma(database_);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setLastError(pragma.lastError().text());
        database_.close();
        return;
    }

    if (!initializeSchema() || !migrateLegacySchema() || !seedDefaults()) {
        database_.close();
    }
}

FinanceRepository::~FinanceRepository()
{
    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
}

bool FinanceRepository::isOpen() const { return database_.isOpen(); }
QString FinanceRepository::lastError() const { return lastError_; }

QVector<Transaction> FinanceRepository::loadTransactions()
{
    QVector<Transaction> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT t.id, t.account_id, t.category_id, t.amount_minor, "
            "t.type, t.occurred_at, t.description, a.currency "
            "FROM transactions t JOIN accounts a ON a.id = t.account_id "
            "ORDER BY t.occurred_at DESC, t.created_at DESC"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.append(Transaction(
            query.value(0).toString(), query.value(1).toString(),
            query.value(2).toString(),
            Money(query.value(3).toLongLong(),
                  currencyFromCode(query.value(7).toString())),
            query.value(4).toInt() == 0
                ? TransactionType::Income : TransactionType::Expense,
            QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong(), Qt::UTC),
            query.value(6).toString()));
    }
    return result;
}

QVector<Category> FinanceRepository::loadCategories()
{
    QVector<Category> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT id, name, type FROM categories "
            "ORDER BY type, name COLLATE NOCASE"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.append(Category(
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toInt() == 0
                ? CategoryType::Income
                : CategoryType::Expense));
    }
    return result;
}

QVector<Account> FinanceRepository::loadAccounts()
{
    QVector<Account> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT id, name, asset_type, account_type, currency, "
            "       initial_balance_minor, credit_limit_minor "
            "FROM accounts WHERE is_archived = 0 "
            "ORDER BY asset_type, created_at, name COLLATE NOCASE"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.append(Account(
            query.value(0).toString(),
            query.value(1).toString(),
            assetTypeFromInt(query.value(2).toInt()),
            static_cast<AccountType>(query.value(3).toInt()),
            currencyFromCode(query.value(4).toString()),
            query.value(5).toLongLong(),
            query.value(6).toLongLong()));
    }
    return result;
}

QVector<CryptoWallet> FinanceRepository::loadCryptoWallets()
{
    QVector<CryptoWallet> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT id, address, network, symbol, decimals, balance_atomic, "
            "       balance_fetched_at, history_fetched_at "
            "FROM crypto_wallets WHERE is_archived = 0 "
            "ORDER BY created_at, symbol, address"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        const QVariant fetchedAt = query.value(6);
        result.append(CryptoWallet(
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toInt(),
            query.value(5).toLongLong(),
            fetchedAt.isNull()
                ? QDateTime()
                : QDateTime::fromMSecsSinceEpoch(
                      fetchedAt.toLongLong(), Qt::UTC),
            query.value(7).isNull()
                ? QDateTime()
                : QDateTime::fromMSecsSinceEpoch(
                      query.value(7).toLongLong(), Qt::UTC)));
    }
    return result;
}

QVector<CryptoTransaction> FinanceRepository::loadCryptoTransactions()
{
    QVector<CryptoTransaction> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT ct.wallet_id, ct.transaction_id, ct.from_address, "
            "       ct.to_address, ct.amount_atomic, ct.occurred_at "
            "FROM crypto_transactions ct "
            "JOIN crypto_wallets cw ON cw.id = ct.wallet_id "
            "WHERE cw.is_archived = 0 "
            "ORDER BY ct.occurred_at DESC, ct.transaction_id"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.append(CryptoTransaction(
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toLongLong(),
            QDateTime::fromMSecsSinceEpoch(
                query.value(5).toLongLong(), Qt::UTC)));
    }
    return result;
}

FinanceRepository::CryptoPriceSnapshot FinanceRepository::loadCryptoPrice(
    const QString& symbol
    ) const
{
    CryptoPriceSnapshot result;
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT price_usd_micros, fetched_at "
        "FROM crypto_prices WHERE symbol = ?"));
    query.addBindValue(symbol.trimmed().toUpper());
    if (query.exec() && query.next()) {
        const qint64 price = query.value(0).toLongLong();
        if (price > 0) {
            result.priceUsdMicros = price;
        }
        if (!query.value(1).isNull()) {
            result.fetchedAtUtc = QDateTime::fromMSecsSinceEpoch(
                query.value(1).toLongLong(), Qt::UTC);
        }
    }
    return result;
}

QDateTime FinanceRepository::loadCryptoRefreshAttemptUtc() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings "
            "WHERE key = 'crypto_refresh_attempt_utc'")) &&
        query.next()) {
        bool ok = false;
        const qint64 milliseconds = query.value(0).toString().toLongLong(&ok);
        if (ok && milliseconds > 0) {
            return QDateTime::fromMSecsSinceEpoch(milliseconds, Qt::UTC);
        }
    }
    return {};
}

QSet<QString> FinanceRepository::loadArchivedCategoryIds()
{
    QSet<QString> result;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT id FROM categories WHERE is_archived = 1"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.insert(query.value(0).toString());
    }
    return result;
}

FinanceRepository::Summary FinanceRepository::loadSummary()
{
    Summary summary;

    QSqlQuery accounts(database_);
    if (!accounts.exec(QStringLiteral(
            "SELECT a.currency, "
            "       SUM(a.initial_balance_minor + COALESCE(t.net_amount, 0)) "
            "FROM accounts a "
            "LEFT JOIN ("
            "    SELECT account_id, "
            "           SUM(CASE "
            "                   WHEN type = 0 THEN amount_minor "
            "                   WHEN type = 1 THEN -amount_minor "
            "               END) AS net_amount "
            "    FROM transactions "
            "    GROUP BY account_id"
            ") t ON t.account_id = a.id "
            "WHERE a.is_archived = 0 "
            "GROUP BY a.currency"))) {
        setLastError(accounts.lastError().text());
        return summary;
    }

    while (accounts.next()) {
        const Currency currency = currencyFromCode(accounts.value(0).toString());
        summary.balance[currencyIndex(currency)] = accounts.value(1).toLongLong();
    }

    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT t.type, a.currency, SUM(t.amount_minor), t.category_id "
            "FROM transactions t JOIN accounts a ON a.id = t.account_id "
            "WHERE a.is_archived = 0 "
            "GROUP BY t.type, a.currency, t.category_id"))) {
        setLastError(query.lastError().text());
        return summary;
    }

    while (query.next()) {
        const QString categoryId = query.value(3).toString();
        if (categoryId == QStringLiteral("transfer-in") ||
            categoryId == QStringLiteral("transfer-out")) {
            continue;
        }
        const int type = query.value(0).toInt();
        const Currency currency = currencyFromCode(query.value(1).toString());
        auto& values = type == 0 ? summary.income : summary.expense;
        const qint64 amount = query.value(2).toLongLong();
        values[currencyIndex(currency)] += amount;
    }
    return summary;
}

bool FinanceRepository::insertTransaction(const Transaction& transaction)
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    prepareTransactionInsert(query);

    if (!insertTransactionRow(
            query,
            transaction,
            QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) ||
        !database_.commit()) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::insertTransfer(
    const Transaction& outgoing,
    const Transaction& incoming
    )
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    prepareTransactionInsert(query);
    const qint64 createdAt = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

    if (!insertTransactionRow(query, outgoing, createdAt) ||
        !insertTransactionRow(query, incoming, createdAt) ||
        !database_.commit()) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::insertTransactions(
    const QVector<Transaction>& transactions
    )
{
    if (transactions.isEmpty()) {
        return true;
    }
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    prepareTransactionInsert(query);
    const qint64 createdAt = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    for (const Transaction& transaction : transactions) {
        if (!insertTransactionRow(query, transaction, createdAt)) {
            setLastError(query.lastError().text());
            database_.rollback();
            return false;
        }
    }
    if (!database_.commit()) {
        setLastError(database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::updateTransaction(const Transaction& transaction)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE transactions "
        "SET account_id = ?, category_id = ?, type = ?, amount_minor = ?, "
        "    occurred_at = ?, description = ? "
        "WHERE id = ?"));
    query.addBindValue(transaction.accountId());
    query.addBindValue(transaction.categoryId());
    query.addBindValue(transaction.type() == TransactionType::Income ? 0 : 1);
    query.addBindValue(transaction.money().minorUnits());
    query.addBindValue(transaction.date().toMSecsSinceEpoch());
    query.addBindValue(transaction.description());
    query.addBindValue(transaction.id());

    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Transaction was not found"));
        return false;
    }
    return true;
}

bool FinanceRepository::replaceTransaction(
    const QString& currentId,
    const Transaction& replacement
    )
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QString error;
    QSqlQuery insertion(database_);
    prepareTransactionInsert(insertion);
    const bool succeeded =
        deleteOperationRows(database_, currentId, error) &&
        insertTransactionRow(
            insertion,
            replacement,
            QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!succeeded || !database_.commit()) {
        if (error.isEmpty()) {
            error = insertion.lastError().isValid()
                ? insertion.lastError().text()
                : database_.lastError().text();
        }
        setLastError(error);
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::replaceTransactionWithTransfer(
    const QString& currentId,
    const Transaction& outgoing,
    const Transaction& incoming
    )
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QString error;
    QSqlQuery insertion(database_);
    prepareTransactionInsert(insertion);
    const qint64 createdAt = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    const bool succeeded =
        deleteOperationRows(database_, currentId, error) &&
        insertTransactionRow(insertion, outgoing, createdAt) &&
        insertTransactionRow(insertion, incoming, createdAt);

    if (!succeeded || !database_.commit()) {
        if (error.isEmpty()) {
            error = insertion.lastError().isValid()
                ? insertion.lastError().text()
                : database_.lastError().text();
        }
        setLastError(error);
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::deleteTransaction(const QString& id)
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QString error;
    if (!deleteOperationRows(database_, id, error) || !database_.commit()) {
        setLastError(error.isEmpty() ? database_.lastError().text() : error);
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::insertCategory(const Category& category)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO categories(id, name, type, is_system, created_at) "
        "VALUES (?, ?, ?, 0, ?)"));
    query.addBindValue(category.id());
    query.addBindValue(category.name());
    query.addBindValue(category.type() == CategoryType::Income ? 0 : 1);
    query.addBindValue(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::insertAccount(const Account& account)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO accounts(id, name, asset_type, account_type, currency, "
        "initial_balance_minor, credit_limit_minor, is_archived, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?)"));
    query.addBindValue(account.id());
    query.addBindValue(account.name());
    query.addBindValue(static_cast<int>(account.assetType()));
    query.addBindValue(static_cast<int>(account.type()));
    query.addBindValue(currencyCode(account.currency()));
    query.addBindValue(account.initialBalanceMinor());
    query.addBindValue(account.creditLimitMinor());
    query.addBindValue(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::updateAccount(const Account& account)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE accounts "
        "SET name = ?, asset_type = ?, account_type = ?, currency = ?, "
        "    initial_balance_minor = ?, credit_limit_minor = ? "
        "WHERE id = ? AND is_archived = 0 "
        "  AND (currency = ? OR NOT EXISTS ("
        "      SELECT 1 FROM transactions WHERE account_id = ?"
        "  ))"));
    query.addBindValue(account.name());
    query.addBindValue(static_cast<int>(account.assetType()));
    query.addBindValue(static_cast<int>(account.type()));
    query.addBindValue(currencyCode(account.currency()));
    query.addBindValue(account.initialBalanceMinor());
    query.addBindValue(account.creditLimitMinor());
    query.addBindValue(account.id());
    query.addBindValue(currencyCode(account.currency()));
    query.addBindValue(account.id());

    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral(
                               "Account was not found or its currency cannot be changed"));
        return false;
    }
    return true;
}

bool FinanceRepository::deleteAccount(const QString& id)
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery lookup(database_);
    lookup.prepare(QStringLiteral(
        "SELECT id FROM transactions WHERE account_id = ?"));
    lookup.addBindValue(id);
    if (!lookup.exec()) {
        setLastError(lookup.lastError().text());
        database_.rollback();
        return false;
    }

    QStringList transactionIds;
    while (lookup.next()) {
        transactionIds.append(lookup.value(0).toString());
    }
    lookup.finish();

    QString error;
    QSqlQuery existence(database_);
    existence.prepare(QStringLiteral(
        "SELECT 1 FROM transactions WHERE id = ?"));
    for (const QString& transactionId : transactionIds) {
        existence.bindValue(0, transactionId);
        if (!existence.exec()) {
            setLastError(existence.lastError().text());
            database_.rollback();
            return false;
        }
        const bool stillExists = existence.next();
        existence.finish();
        if (!stillExists) {
            continue;
        }
        if (!deleteOperationRows(database_, transactionId, error)) {
            setLastError(error);
            database_.rollback();
            return false;
        }
    }

    QSqlQuery account(database_);
    account.prepare(QStringLiteral(
        "UPDATE accounts SET is_archived = 1 "
        "WHERE id = ? AND is_archived = 0"));
    account.addBindValue(id);
    if (!account.exec() || account.numRowsAffected() != 1 ||
        !database_.commit()) {
        setLastError(account.lastError().isValid()
                         ? account.lastError().text()
                         : database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::insertCryptoWallet(const CryptoWallet& wallet)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO crypto_wallets("
        "id, address, network, symbol, decimals, balance_atomic, "
        "balance_fetched_at, is_archived, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?)"));
    query.addBindValue(wallet.id());
    query.addBindValue(wallet.address());
    query.addBindValue(wallet.network());
    query.addBindValue(wallet.symbol());
    query.addBindValue(wallet.decimals());
    query.addBindValue(wallet.balanceAtomic());
    if (wallet.balanceFetchedAtUtc().isValid()) {
        query.addBindValue(
            wallet.balanceFetchedAtUtc().toUTC().toMSecsSinceEpoch());
    } else {
        query.addBindValue(QVariant());
    }
    query.addBindValue(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::updateCryptoWalletBalance(
    const QString& id,
    const qint64 balanceAtomic,
    const QDateTime& fetchedAtUtc
    )
{
    if (balanceAtomic < 0 || !fetchedAtUtc.isValid()) {
        setLastError(QStringLiteral("Invalid crypto-wallet balance snapshot"));
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE crypto_wallets "
        "SET balance_atomic = ?, balance_fetched_at = ? "
        "WHERE id = ? AND is_archived = 0"));
    query.addBindValue(balanceAtomic);
    query.addBindValue(fetchedAtUtc.toUTC().toMSecsSinceEpoch());
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Crypto wallet was not found"));
        return false;
    }
    return true;
}

bool FinanceRepository::deleteCryptoWallet(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE crypto_wallets SET is_archived = 1 "
        "WHERE id = ? AND is_archived = 0"));
    query.addBindValue(id);
    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Crypto wallet was not found"));
        return false;
    }
    return true;
}

bool FinanceRepository::replaceCryptoTransactions(
    const QString& walletId,
    const QVector<CryptoTransaction>& transactions,
    const QDateTime& fetchedAtUtc
    )
{
    if (walletId.isEmpty() || !fetchedAtUtc.isValid()) {
        setLastError(QStringLiteral("Invalid crypto transaction snapshot"));
        return false;
    }
    for (const CryptoTransaction& transaction : transactions) {
        if (transaction.walletId() != walletId ||
            transaction.transactionId().isEmpty() ||
            transaction.fromAddress().isEmpty() ||
            transaction.toAddress().isEmpty() ||
            transaction.amountAtomic() <= 0 ||
            !transaction.occurredAtUtc().isValid()) {
            setLastError(QStringLiteral("Invalid crypto transaction"));
            return false;
        }
    }

    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery deletion(database_);
    deletion.prepare(QStringLiteral(
        "DELETE FROM crypto_transactions WHERE wallet_id = ?"));
    deletion.addBindValue(walletId);
    if (!deletion.exec()) {
        setLastError(deletion.lastError().text());
        database_.rollback();
        return false;
    }

    QSqlQuery insertion(database_);
    insertion.prepare(QStringLiteral(
        "INSERT INTO crypto_transactions("
        "wallet_id, transaction_id, from_address, to_address, "
        "amount_atomic, occurred_at) VALUES (?, ?, ?, ?, ?, ?)"));
    for (const CryptoTransaction& transaction : transactions) {
        insertion.bindValue(0, transaction.walletId());
        insertion.bindValue(1, transaction.transactionId());
        insertion.bindValue(2, transaction.fromAddress());
        insertion.bindValue(3, transaction.toAddress());
        insertion.bindValue(4, transaction.amountAtomic());
        insertion.bindValue(
            5,
            transaction.occurredAtUtc().toUTC().toMSecsSinceEpoch());
        if (!insertion.exec()) {
            setLastError(insertion.lastError().text());
            database_.rollback();
            return false;
        }
    }

    QSqlQuery wallet(database_);
    wallet.prepare(QStringLiteral(
        "UPDATE crypto_wallets SET history_fetched_at = ? "
        "WHERE id = ? AND is_archived = 0"));
    wallet.addBindValue(fetchedAtUtc.toUTC().toMSecsSinceEpoch());
    wallet.addBindValue(walletId);
    if (!wallet.exec() || wallet.numRowsAffected() != 1 ||
        !database_.commit()) {
        setLastError(wallet.lastError().isValid()
                         ? wallet.lastError().text()
                         : database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::saveCryptoPrice(
    const QString& symbol,
    const qint64 priceUsdMicros,
    const QDateTime& fetchedAtUtc
    )
{
    const QString normalizedSymbol = symbol.trimmed().toUpper();
    if ((normalizedSymbol != QStringLiteral("USDT") &&
         normalizedSymbol != QStringLiteral("BTC") &&
         normalizedSymbol != QStringLiteral("ETH")) ||
        priceUsdMicros <= 0 || !fetchedAtUtc.isValid()) {
        setLastError(QStringLiteral("Invalid crypto price snapshot"));
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO crypto_prices(symbol, price_usd_micros, fetched_at) "
        "VALUES(?, ?, ?) "
        "ON CONFLICT(symbol) DO UPDATE SET "
        "price_usd_micros = excluded.price_usd_micros, "
        "fetched_at = excluded.fetched_at"));
    query.addBindValue(normalizedSymbol);
    query.addBindValue(priceUsdMicros);
    query.addBindValue(fetchedAtUtc.toUTC().toMSecsSinceEpoch());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::saveCryptoRefreshAttemptUtc(
    const QDateTime& attemptedAtUtc
    )
{
    if (!attemptedAtUtc.isValid()) {
        setLastError(QStringLiteral("Invalid crypto refresh timestamp"));
        return false;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) "
        "VALUES('crypto_refresh_attempt_utc', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(QString::number(
        attemptedAtUtc.toUTC().toMSecsSinceEpoch()));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::updateCategoryName(
    const QString& id,
    const QString& name
    )
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE categories SET name = ? "
        "WHERE id = ? AND is_archived = 0"));
    query.addBindValue(name);
    query.addBindValue(id);

    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Category was not found"));
        return false;
    }
    return true;
}

bool FinanceRepository::archiveCategory(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE categories SET is_archived = 1 "
        "WHERE id = ? AND is_archived = 0"));
    query.addBindValue(id);

    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Category was not found"));
        return false;
    }
    return true;
}

QString FinanceRepository::loadAppCurrency() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings WHERE key = 'app_currency'")) &&
        query.next()) {
        return query.value(0).toString();
    }
    return QStringLiteral("RUB");
}

bool FinanceRepository::saveAppCurrency(const QString& currency)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES('app_currency', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(currency);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

QString FinanceRepository::loadSelectedAsset() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings WHERE key = 'selected_asset'")) &&
        query.next()) {
        return query.value(0).toString();
    }
    return QStringLiteral("fiat");
}

bool FinanceRepository::saveSelectedAsset(const QString& asset)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES('selected_asset', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(asset);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

QString FinanceRepository::loadUiLanguage() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings WHERE key = 'ui_language'")) &&
        query.next()) {
        return query.value(0).toString();
    }
    return QStringLiteral("ru");
}

bool FinanceRepository::saveUiLanguage(const QString& language)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES('ui_language', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(language);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::loadAutomaticCurrencyRates() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings "
            "WHERE key = 'automatic_currency_rates'")) &&
        query.next()) {
        return query.value(0).toString() != QStringLiteral("0");
    }
    return true;
}

double FinanceRepository::loadManualUsdToRubRate() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings "
            "WHERE key = 'manual_usd_to_rub_rate'")) &&
        query.next()) {
        bool valid = false;
        const double value = query.value(0).toString().toDouble(&valid);
        if (valid && std::isfinite(value) && value > 0.0) {
            return value;
        }
    }
    return 90.909090909;
}

double FinanceRepository::loadManualEurToRubRate() const
{
    QSqlQuery query(database_);
    if (query.exec(QStringLiteral(
            "SELECT value FROM settings "
            "WHERE key = 'manual_eur_to_rub_rate'")) &&
        query.next()) {
        bool valid = false;
        const double value = query.value(0).toString().toDouble(&valid);
        if (valid && std::isfinite(value) && value > 0.0) {
            return value;
        }
    }
    return 106.363636364;
}

bool FinanceRepository::saveAutomaticCurrencyRates(const bool enabled)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) "
        "VALUES('automatic_currency_rates', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(enabled ? QStringLiteral("1") : QStringLiteral("0"));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool FinanceRepository::saveManualCurrencyRates(
    const double rublesPerUsd,
    const double rublesPerEur
    )
{
    if (!std::isfinite(rublesPerUsd) || rublesPerUsd <= 0.0 ||
        !std::isfinite(rublesPerEur) || rublesPerEur <= 0.0) {
        setLastError(QStringLiteral("Currency rates must be positive numbers"));
        return false;
    }
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    const auto saveRate = [&query](const QString& key, const double value)
    {
        query.bindValue(0, key);
        query.bindValue(1, QString::number(value, 'g', 15));
        return query.exec();
    };
    if (!saveRate(QStringLiteral("manual_usd_to_rub_rate"), rublesPerUsd) ||
        !saveRate(QStringLiteral("manual_eur_to_rub_rate"), rublesPerEur)) {
        setLastError(query.lastError().text());
        database_.rollback();
        return false;
    }
    if (!database_.commit()) {
        setLastError(database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::initializeSchema()
{
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS accounts ("
                       "id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                       "asset_type INTEGER NOT NULL CHECK(asset_type IN (0,1,2)), "
                       "account_type INTEGER NOT NULL CHECK(account_type IN (0,1,2,3,4,5,6,7)), "
                       "currency TEXT NOT NULL CHECK(currency IN ('RUB','USD','EUR')), "
                       "initial_balance_minor INTEGER NOT NULL DEFAULT 0, "
                       "credit_limit_minor INTEGER NOT NULL DEFAULT 0 "
                       "CHECK(credit_limit_minor >= 0), "
                       "is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)), "
                       "created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS categories ("
                       "id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                       "type INTEGER NOT NULL CHECK(type IN (0,1)), "
                       "is_system INTEGER NOT NULL DEFAULT 0 CHECK(is_system IN (0,1)), "
                       "is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)), "
                       "created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS transactions ("
                       "id TEXT PRIMARY KEY, "
                       "account_id TEXT NOT NULL REFERENCES accounts(id), "
                       "category_id TEXT NOT NULL REFERENCES categories(id), "
                       "type INTEGER NOT NULL CHECK(type IN (0,1)), "
                       "amount_minor INTEGER NOT NULL CHECK(amount_minor > 0), "
                       "occurred_at INTEGER NOT NULL, description TEXT NOT NULL DEFAULT '', "
                       "created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings ("
                       "key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS crypto_wallets ("
                       "id TEXT PRIMARY KEY, address TEXT NOT NULL, "
                       "network TEXT NOT NULL, symbol TEXT NOT NULL, "
                       "decimals INTEGER NOT NULL, "
                       "balance_atomic INTEGER NOT NULL DEFAULT 0 "
                       "CHECK(balance_atomic >= 0), "
                       "balance_fetched_at INTEGER, "
                       "history_fetched_at INTEGER, "
                       "is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)), "
                       "created_at INTEGER NOT NULL, "
                       "CHECK((network = 'TRON' AND symbol = 'USDT' AND decimals = 6) OR "
                       "      (network = 'BITCOIN' AND symbol = 'BTC' AND decimals = 8) OR "
                       "      (network = 'ETHEREUM' AND symbol = 'ETH' AND decimals = 8)))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS crypto_prices ("
                       "symbol TEXT PRIMARY KEY "
                       "CHECK(symbol IN ('USDT','BTC','ETH')), "
                       "price_usd_micros INTEGER NOT NULL CHECK(price_usd_micros > 0), "
                       "fetched_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS crypto_transactions ("
                       "wallet_id TEXT NOT NULL REFERENCES crypto_wallets(id), "
                       "transaction_id TEXT NOT NULL, "
                       "from_address TEXT NOT NULL, "
                       "to_address TEXT NOT NULL, "
                       "amount_atomic INTEGER NOT NULL CHECK(amount_atomic > 0), "
                       "occurred_at INTEGER NOT NULL, "
                       "PRIMARY KEY(wallet_id, transaction_id))"),
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS "
                       "idx_crypto_wallets_active_address "
                       "ON crypto_wallets(network, address) "
                       "WHERE is_archived = 0"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_occurred_at "
                       "ON transactions(occurred_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_account_date "
                       "ON transactions(account_id, occurred_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_category "
                       "ON transactions(category_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS "
                       "idx_crypto_transactions_wallet_date "
                       "ON crypto_transactions(wallet_id, occurred_at DESC)")
    };

    for (const QString& statement : statements) {
        QSqlQuery query(database_);
        if (!query.exec(statement)) {
            setLastError(query.lastError().text());
            return false;
        }
    }
    return true;
}

bool FinanceRepository::migrateLegacySchema()
{
    QSqlQuery columns(database_);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(accounts)"))) {
        setLastError(columns.lastError().text());
        return false;
    }

    bool hasLegacyGroupType = false;
    bool hasAssetType = false;
    bool hasCreditLimit = false;
    while (columns.next()) {
        const QString name = columns.value(1).toString();
        hasLegacyGroupType = hasLegacyGroupType ||
            name == QStringLiteral("group_type");
        hasAssetType = hasAssetType ||
            name == QStringLiteral("asset_type");
        hasCreditLimit = hasCreditLimit ||
            name == QStringLiteral("credit_limit_minor");
    }

    QSqlQuery cryptoColumns(database_);
    if (!cryptoColumns.exec(QStringLiteral(
            "PRAGMA table_info(crypto_wallets)"))) {
        setLastError(cryptoColumns.lastError().text());
        return false;
    }
    bool hasCryptoHistoryFetchedAt = false;
    bool hasCryptoDecimals = false;
    while (cryptoColumns.next()) {
        hasCryptoHistoryFetchedAt = hasCryptoHistoryFetchedAt ||
            cryptoColumns.value(1).toString() ==
                QStringLiteral("history_fetched_at");
        hasCryptoDecimals = hasCryptoDecimals ||
            cryptoColumns.value(1).toString() == QStringLiteral("decimals");
    }

    const bool needsAssetTypeRename = hasLegacyGroupType && !hasAssetType;
    if (!needsAssetTypeRename && hasCreditLimit &&
        hasCryptoHistoryFetchedAt && hasCryptoDecimals) {
        return true;
    }
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery migration(database_);
    if (needsAssetTypeRename &&
        !migration.exec(QStringLiteral(
            "ALTER TABLE accounts RENAME COLUMN group_type TO asset_type"))) {
        setLastError(migration.lastError().text());
        database_.rollback();
        return false;
    }

    if (!hasCreditLimit) {
        if (!migration.exec(QStringLiteral(
                "ALTER TABLE accounts ADD COLUMN "
                "credit_limit_minor INTEGER NOT NULL DEFAULT 0 "
                "CHECK(credit_limit_minor >= 0)"))) {
            setLastError(migration.lastError().text());
            database_.rollback();
            return false;
        }

        // Before credit cards had their own semantics, a positive initial
        // balance was commonly used as the available credit. Preserve that
        // intent by turning it into a limit instead of counting bank money as
        // the user's asset.
        if (!migration.exec(QStringLiteral(
                "UPDATE accounts "
                "SET credit_limit_minor = initial_balance_minor, "
                "    initial_balance_minor = 0 "
                "WHERE account_type = %1 AND initial_balance_minor > 0")
                .arg(static_cast<int>(AccountType::CreditCard)))) {
            setLastError(migration.lastError().text());
            database_.rollback();
            return false;
        }
    }

    if (!hasCryptoHistoryFetchedAt &&
        !migration.exec(QStringLiteral(
            "ALTER TABLE crypto_wallets ADD COLUMN "
            "history_fetched_at INTEGER"))) {
        setLastError(migration.lastError().text());
        database_.rollback();
        return false;
    }

    if (!hasCryptoDecimals) {
        const QStringList cryptoMigrationStatements{
            QStringLiteral(
                "CREATE TABLE crypto_wallets_v2 ("
                "id TEXT PRIMARY KEY, address TEXT NOT NULL, "
                "network TEXT NOT NULL, symbol TEXT NOT NULL, "
                "decimals INTEGER NOT NULL, "
                "balance_atomic INTEGER NOT NULL DEFAULT 0 CHECK(balance_atomic >= 0), "
                "balance_fetched_at INTEGER, history_fetched_at INTEGER, "
                "is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)), "
                "created_at INTEGER NOT NULL, "
                "CHECK((network = 'TRON' AND symbol = 'USDT' AND decimals = 6) OR "
                "      (network = 'BITCOIN' AND symbol = 'BTC' AND decimals = 8) OR "
                "      (network = 'ETHEREUM' AND symbol = 'ETH' AND decimals = 8)))"),
            QStringLiteral(
                "INSERT INTO crypto_wallets_v2("
                "id,address,network,symbol,decimals,balance_atomic,"
                "balance_fetched_at,history_fetched_at,is_archived,created_at) "
                "SELECT id,address,network,symbol,6,balance_atomic,"
                "balance_fetched_at,history_fetched_at,is_archived,created_at "
                "FROM crypto_wallets"),
            QStringLiteral(
                "CREATE TABLE crypto_transactions_v2 ("
                "wallet_id TEXT NOT NULL REFERENCES crypto_wallets_v2(id), "
                "transaction_id TEXT NOT NULL, from_address TEXT NOT NULL, "
                "to_address TEXT NOT NULL, amount_atomic INTEGER NOT NULL "
                "CHECK(amount_atomic > 0), occurred_at INTEGER NOT NULL, "
                "PRIMARY KEY(wallet_id, transaction_id))"),
            QStringLiteral(
                "INSERT INTO crypto_transactions_v2 "
                "SELECT * FROM crypto_transactions"),
            QStringLiteral(
                "CREATE TABLE crypto_prices_v2 ("
                "symbol TEXT PRIMARY KEY CHECK(symbol IN ('USDT','BTC','ETH')), "
                "price_usd_micros INTEGER NOT NULL CHECK(price_usd_micros > 0), "
                "fetched_at INTEGER NOT NULL)"),
            QStringLiteral(
                "INSERT INTO crypto_prices_v2 SELECT * FROM crypto_prices"),
            QStringLiteral("DROP TABLE crypto_transactions"),
            QStringLiteral("DROP TABLE crypto_wallets"),
            QStringLiteral("DROP TABLE crypto_prices"),
            QStringLiteral("ALTER TABLE crypto_wallets_v2 RENAME TO crypto_wallets"),
            QStringLiteral("ALTER TABLE crypto_prices_v2 RENAME TO crypto_prices"),
            QStringLiteral("ALTER TABLE crypto_transactions_v2 RENAME TO crypto_transactions"),
            QStringLiteral(
                "CREATE UNIQUE INDEX idx_crypto_wallets_active_address "
                "ON crypto_wallets(network, address) WHERE is_archived = 0"),
            QStringLiteral(
                "CREATE INDEX idx_crypto_transactions_wallet_date "
                "ON crypto_transactions(wallet_id, occurred_at DESC)")};
        for (const QString& statement : cryptoMigrationStatements) {
            if (!migration.exec(statement)) {
                setLastError(migration.lastError().text());
                database_.rollback();
                return false;
            }
        }
    }

    if (!database_.commit()) {
        setLastError(database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

bool FinanceRepository::seedDefaults()
{
    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    QSqlQuery account(database_);
    account.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO accounts(id,name,asset_type,account_type,currency,created_at) "
        "VALUES (?, ?, 0, 4, ?, ?)"));
    const std::array<QString, 3> currencies{
        QStringLiteral("RUB"), QStringLiteral("USD"), QStringLiteral("EUR")};
    for (const QString& currency : currencies) {
        account.bindValue(0, QStringLiteral("household-") + currency.toLower());
        account.bindValue(1, QStringLiteral("Основной ") + currency);
        account.bindValue(2, currency);
        account.bindValue(3, now);
        if (!account.exec()) {
            setLastError(account.lastError().text());
            database_.rollback();
            return false;
        }
    }

    struct Seed { QString id; QString name; int type; };
    const Seed seeds[] = {
        {QStringLiteral("salary"), QStringLiteral("Зарплата"), 0},
        {QStringLiteral("freelance"), QStringLiteral("Фриланс"), 0},
        {QStringLiteral("gift"), QStringLiteral("Подарок"), 0},
        {QStringLiteral("investment"), QStringLiteral("Инвестиции"), 0},
        {QStringLiteral("other_income"), QStringLiteral("Другой доход"), 0},
        {QStringLiteral("groceries"), QStringLiteral("Продукты"), 1},
        {QStringLiteral("transport"), QStringLiteral("Транспорт"), 1},
        {QStringLiteral("housing"), QStringLiteral("Жилье"), 1},
        {QStringLiteral("health"), QStringLiteral("Здоровье"), 1},
        {QStringLiteral("entertainment"), QStringLiteral("Развлечения"), 1},
        {QStringLiteral("shopping"), QStringLiteral("Покупки"), 1},
        {QStringLiteral("other_expense"), QStringLiteral("Другое"), 1},
        {QStringLiteral("transfer-in"), QStringLiteral("Перевод"), 0},
        {QStringLiteral("transfer-out"), QStringLiteral("Перевод"), 1}};
    QSqlQuery category(database_);
    category.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO categories(id,name,type,is_system,created_at) "
        "VALUES (?, ?, ?, 1, ?)"));
    for (const Seed& seed : seeds) {
        category.bindValue(0, seed.id);
        category.bindValue(1, seed.name);
        category.bindValue(2, seed.type);
        category.bindValue(3, now);
        if (!category.exec()) {
            setLastError(category.lastError().text());
            database_.rollback();
            return false;
        }
    }

    QSqlQuery setting(database_);
    if (!setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) VALUES('app_currency','RUB')")) ||
        !setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) VALUES('selected_asset','fiat')")) ||
        !setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) VALUES('ui_language','ru')")) ||
        !setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) "
            "VALUES('automatic_currency_rates','1')")) ||
        !setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) "
            "VALUES('manual_usd_to_rub_rate','90.909090909')")) ||
        !setting.exec(QStringLiteral(
            "INSERT OR IGNORE INTO settings(key,value) "
            "VALUES('manual_eur_to_rub_rate','106.363636364')")) ||
        !database_.commit()) {
        setLastError(setting.lastError().isValid()
                         ? setting.lastError().text()
                         : database_.lastError().text());
        database_.rollback();
        return false;
    }
    return true;
}

void FinanceRepository::setLastError(const QString& error) { lastError_ = error; }
