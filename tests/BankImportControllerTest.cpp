#include "../interface/FinanceController.h"
#include <QFile>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QtTest>

class BankImportControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void correctionsPersistAndDuplicateIdentityStaysStable();
    void staleChoicesAndRecipientChangesAreRejected();
    void unknownDescriptionIsPreservedSeparately();
    void rollbackBackupAndRestoreKeepRecipients();
    void migratesAndRestoresOlderSchema();
};
namespace {
bool seed(const QString& path) {
    FinanceRepository r(path);
    return r.isOpen() && r.saveAutomaticCurrencyRates(false) &&
        r.insertAccount({"bank","Банк",AssetType::Fiat,AccountType::DebitCard,Currency::RUB});
}
QVariantMap profile() {
    return {{"id","bank-profile"},{"name","Банк"},{"accountId","bank"},{"incomeCategoryId","salary"},
        {"expenseCategoryId","other_expense"},{"dateColumn",0},{"amountColumn",1},{"descriptionColumn",2},
        {"idColumn",3},{"recipientColumn",4},{"recipientIdColumn",5},{"currencyColumn",6},{"categoryColumn",7},
        {"delimiter","semicolon"},{"encoding","utf8"},{"amountMode","signed"},{"dateFormat","auto"},{"positiveMeansIncome",true}};
}
bool csv(const QString& path,const QString& body) {
    QFile file(path);return file.open(QIODevice::WriteOnly) &&
        file.write(("Дата;Сумма;Описание;Номер операции;Получатель;ИНН получателя;Валюта;Категория\n"+body).toUtf8())>0;
}
QVariantMap firstChoice(const QVariantMap& preview) {
    const auto row=preview.value("operationRows").toList().front().toMap();
    return {{"fingerprint",row.value("fingerprint")},{"recipientName","Магазин у дома"},{"recipientConfirmed",true},
        {"rememberRecipient",true},{"recipientPattern","shop alpha"},{"recipientField","description"},{"recipientMode","contains"},
        {"categoryId","groceries"},{"confirmed",true},{"remember",true},{"pattern","Магазин у дома"},
        {"categoryField","recipient"},{"matchMode","exact"}};
}
QVariantMap choicesFor(const QVariantMap& preview,const QVariantMap& choice) {
    return {{preview.value("operationRows").toList().front().toMap().value("rowKey").toString(),choice}};
}
}
void BankImportControllerTest::correctionsPersistAndDuplicateIdentityStaysStable() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));
    QVERIFY(csv(file,"01.10.2026;-100;POS SHOP ALPHA 11;one;;;RUB;\n02.10.2026;-200;POS SHOP ALPHA 22;two;;;RUB;\n03.10.2026;-50;PYATEROCHKA;three;;;RUB;\n"));
    {
        FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
        const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());QVERIFY(preview.value("ok").toBool());QCOMPARE(preview.value("reviewCount").toInt(),2);
        const auto choices=choicesFor(preview,firstChoice(preview));
        const auto resolved=c.resolveBankImportRows(preview.value("operationRows").toList(),choices);QVERIFY(resolved.value("ok").toBool());
        QCOMPARE(resolved.value("operationRows").toList()[1].toMap().value("merchant").toString(),QString("Магазин у дома"));
        const auto result=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(result.value("imported").toInt(),3);
        QCOMPARE(c.bankRecipientRules().value("items").toList().size(),1);QCOMPARE(c.bankCategoryRules().value("items").toList().size(),1);
        const auto repeated=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}});
        QVERIFY(repeated.value("ok").toBool());QCOMPARE(repeated.value("imported").toInt(),0);QCOMPARE(repeated.value("skipped").toInt(),3);
    }
    FinanceRepository r(path);const auto transactions=r.loadTransactions();QCOMPARE(transactions.size(),3);
    int learned=0;for(const auto& tx:transactions)if(tx.description().contains("ALPHA")) {
        ++learned;QCOMPARE(tx.recipient().name,QString("Магазин у дома"));QCOMPARE(tx.categoryId(),QString("groceries"));QVERIFY(tx.description().startsWith("POS SHOP ALPHA"));
    }QCOMPARE(learned,2);
    FinanceController reopened(nullptr,path);
    QVERIFY(csv(file,"04.10.2026;-40;SHOP ALPHA 33;four;;;RUB;\n"));
    const auto later=reopened.previewBankImport(QUrl::fromLocalFile(file),profile());QCOMPARE(later.value("reviewCount").toInt(),0);
    QCOMPARE(later.value("operationRows").toList().front().toMap().value("merchant").toString(),QString("Магазин у дома"));
    QVERIFY(reopened.importBankCsv(QUrl::fromLocalFile(file),"bank-profile").value("ok").toBool());
    QVERIFY(reopened.deleteBankRecipientRule("shop alpha","description","contains","expense").value("ok").toBool());
    QVERIFY(csv(file,"05.10.2026;-40;SHOP ALPHA 44;five;;;RUB;\n"));
    QVERIFY(reopened.previewBankImport(QUrl::fromLocalFile(file),profile()).value("operationRows").toList().front().toMap().value("merchant").toString().isEmpty());
}
void BankImportControllerTest::staleChoicesAndRecipientChangesAreRejected() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));
    QVERIFY(csv(file,"01.10.2026;-100;POS SHOP ALPHA 11;one;ИП Иванов;123;RUB;\n"));
    FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());auto choice=firstChoice(preview);choice["rememberRecipient"]=false;choice["remember"]=false;
    const auto choices=choicesFor(preview,choice);
    QVERIFY(csv(file,"01.10.2026;-100;POS SHOP ALPHA 11;one;ИП Петров;456;RUB;\n"));
    const auto fresh=c.previewBankImport(QUrl::fromLocalFile(file),profile());
    QCOMPARE(preview.value("operationRows").toList().front().toMap().value("rowKey"),fresh.value("operationRows").toList().front().toMap().value("rowKey"));
    QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}}).value("ok").toBool());
    FinanceRepository r(path);QVERIFY(r.loadTransactions().isEmpty());QVERIFY(r.loadBankRecipientRules().isEmpty());
    QVariantMap extra{{"removed-row",QVariantMap{{"fingerprint","stale"}}}};
    QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",extra},{"requireReview",false}}).value("ok").toBool());
}
void BankImportControllerTest::unknownDescriptionIsPreservedSeparately() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));
    const auto text=QString("Покупка неизвестная карта 9999 номер 2345");
    QVERIFY(csv(file,"01.10.2026;-100;"+text+";one;;;RUB;Продукты\n"));
    FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());QCOMPARE(preview.value("reviewCount").toInt(),0);
    QVERIFY(preview.value("operationRows").toList().front().toMap().value("merchant").toString().isEmpty());
    QVERIFY(c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile").value("ok").toBool());
    FinanceRepository r(path);const auto tx=r.loadTransactions().front();QCOMPARE(tx.description(),text);QVERIFY(tx.recipient().name.isEmpty());
    c.setSelectedAccountId("bank");c.clearDateFilter();
    const auto items=c.transactions();QVERIFY(!items.isEmpty());QVERIFY(items.front().toMap().value("recipientName").toString().isEmpty());
}
void BankImportControllerTest::rollbackBackupAndRestoreKeepRecipients() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");QVERIFY(seed(path));
    FinanceRepository r(path);const Transaction t("good","bank","groceries",Money(100,Currency::RUB),TransactionType::Expense,
        QDateTime::currentDateTimeUtc(),"Полное исходное описание",{}, {"Пятёрочка","dict:pyaterochka","dictionary"});
    const BankCategoryRule category{"пятерочка","exact","groceries",CategoryType::Expense,"recipient"};
    const BankRecipientRule alias{"shop alpha","description","contains",CategoryType::Expense,"Пятёрочка"};
    auto invalid=alias;invalid.field="missing";
    QVERIFY(!r.insertTransactions({t},{category},{invalid}));QVERIFY(r.loadTransactions().isEmpty());QVERIFY(r.loadBankCategoryRules().isEmpty());
    QVERIFY2(r.insertTransactions({t},{category},{alias}),qPrintable(r.lastError()));
    const auto copy=dir.filePath("backup.db");QVERIFY(r.backupDatabase(copy));
    FinanceRepository restored(dir.filePath("restored.db"));qint64 added=0,skipped=0;QVERIFY2(restored.restoreDatabase(copy,&added,&skipped),qPrintable(restored.lastError()));
    QCOMPARE(restored.loadBankRecipientRules().size(),1);QCOMPARE(restored.loadBankCategoryRules().front().field,QString("recipient"));
    QCOMPARE(restored.loadTransactions().front().recipient().name,QString("Пятёрочка"));
    QVERIFY(restored.restoreDatabase(copy,&added,&skipped));QCOMPARE(added,qint64(0));
    auto renamed=t;QVERIFY(restored.updateTransaction(renamed));QCOMPARE(restored.loadTransactions().front().recipient().name,QString("Пятёрочка"));
    QVERIFY(restored.clearAllUserData());QVERIFY(restored.loadBankRecipientRules().isEmpty());
}
void BankImportControllerTest::migratesAndRestoresOlderSchema() {
    QTemporaryDir dir;const auto old=dir.filePath("old.db");QVERIFY(seed(old));
    {FinanceRepository r(old);QVERIFY(r.insertTransactions({Transaction("legacy","bank","groceries",Money(100,Currency::RUB),TransactionType::Expense,QDateTime::currentDateTimeUtc(),"Старое описание")},
        {{"пятерочка","exact","groceries",CategoryType::Expense}}));}
    const QString connection="bank-old-schema";
    {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(old);QVERIFY(db.open());QSqlQuery q(db);
        QVERIFY(q.exec("DROP TABLE bank_recipient_rules"));QVERIFY(q.exec("ALTER TABLE transactions DROP COLUMN recipient_json"));
        QVERIFY(q.exec("ALTER TABLE bank_category_rules DROP COLUMN match_field"));db.close();}
    QSqlDatabase::removeDatabase(connection);
    FinanceRepository restored(dir.filePath("current.db"));qint64 added=0;QVERIFY2(restored.restoreDatabase(old,&added),qPrintable(restored.lastError()));
    QCOMPARE(restored.loadTransactions().front().description(),QString("Старое описание"));QVERIFY(restored.loadTransactions().front().recipient().name.isEmpty());
    QCOMPARE(restored.loadBankCategoryRules().front().field,QString("legacy"));
    FinanceRepository migrated(old);QVERIFY2(migrated.isOpen(),qPrintable(migrated.lastError()));
    QCOMPARE(migrated.loadTransactions().front().description(),QString("Старое описание"));QVERIFY(migrated.loadTransactions().front().recipient().name.isEmpty());
    QCOMPARE(migrated.loadBankCategoryRules().front().field,QString("legacy"));QVERIFY(migrated.loadBankRecipientRules().isEmpty());
}
QTEST_GUILESS_MAIN(BankImportControllerTest)
#include "BankImportControllerTest.moc"
