#include "FinanceRepository.h"

#include "../core/Currency.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

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
            "       initial_balance_minor "
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
            query.value(5).toLongLong()));
    }
    return result;
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
    query.prepare(QStringLiteral(
        "INSERT INTO transactions(id, account_id, category_id, type, "
        "amount_minor, occurred_at, description, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(transaction.id());
    query.addBindValue(transaction.accountId());
    query.addBindValue(transaction.categoryId());
    query.addBindValue(transaction.type() == TransactionType::Income ? 0 : 1);
    query.addBindValue(transaction.money().minorUnits());
    query.addBindValue(transaction.date().toMSecsSinceEpoch());
    query.addBindValue(transaction.description());
    query.addBindValue(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

    if (!query.exec() || !database_.commit()) {
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
    query.prepare(QStringLiteral(
        "INSERT INTO transactions(id, account_id, category_id, type, "
        "amount_minor, occurred_at, description, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    const qint64 createdAt = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

    const auto insert = [&](const Transaction& transaction) {
        query.bindValue(0, transaction.id());
        query.bindValue(1, transaction.accountId());
        query.bindValue(2, transaction.categoryId());
        query.bindValue(3, transaction.type() == TransactionType::Income ? 0 : 1);
        query.bindValue(4, transaction.money().minorUnits());
        query.bindValue(5, transaction.date().toMSecsSinceEpoch());
        query.bindValue(6, transaction.description());
        query.bindValue(7, createdAt);
        return query.exec();
    };

    if (!insert(outgoing) || !insert(incoming) || !database_.commit()) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : database_.lastError().text());
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

bool FinanceRepository::deleteTransaction(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "DELETE FROM transactions WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Transaction was not found"));
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
        "initial_balance_minor, is_archived, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, 0, ?)"));
    query.addBindValue(account.id());
    query.addBindValue(account.name());
    query.addBindValue(static_cast<int>(account.assetType()));
    query.addBindValue(static_cast<int>(account.type()));
    query.addBindValue(currencyCode(account.currency()));
    query.addBindValue(account.initialBalanceMinor());
    query.addBindValue(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());

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

bool FinanceRepository::initializeSchema()
{
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS accounts ("
                       "id TEXT PRIMARY KEY, name TEXT NOT NULL, "
                       "asset_type INTEGER NOT NULL CHECK(asset_type IN (0,1,2)), "
                       "account_type INTEGER NOT NULL CHECK(account_type IN (0,1,2,3,4,5,6,7)), "
                       "currency TEXT NOT NULL CHECK(currency IN ('RUB','USD','EUR')), "
                       "initial_balance_minor INTEGER NOT NULL DEFAULT 0, "
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
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_occurred_at "
                       "ON transactions(occurred_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_account_date "
                       "ON transactions(account_id, occurred_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_category "
                       "ON transactions(category_id)")
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
    while (columns.next()) {
        const QString name = columns.value(1).toString();
        hasLegacyGroupType = hasLegacyGroupType ||
            name == QStringLiteral("group_type");
        hasAssetType = hasAssetType ||
            name == QStringLiteral("asset_type");
    }

    if (!hasLegacyGroupType || hasAssetType) {
        return true;
    }

    QSqlQuery migration(database_);
    if (!migration.exec(QStringLiteral(
            "ALTER TABLE accounts RENAME COLUMN group_type TO asset_type"))) {
        setLastError(migration.lastError().text());
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
