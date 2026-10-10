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
    void transfersPersistAndStatementsJoinWithoutDuplicates();
    void ordinaryCounterpartIsCombinedAtomically();
    void transferValidationAndCrossCurrencyAmounts();
    void repeatedMatchAndLinkFailureRollBack();
    void editingKeepsStatementIdentities();
    void cnyAccountImportAndSettingsSurviveRestart();
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
    FinanceRepository restored(dir.filePath("restored.db"));qint64 added=0;QString previous;QVERIFY2(restored.restoreDatabase(copy,&added,&previous),qPrintable(restored.lastError()));
    QCOMPARE(restored.loadBankRecipientRules().size(),1);QCOMPARE(restored.loadBankCategoryRules().front().field,QString("recipient"));
    QCOMPARE(restored.loadTransactions().front().recipient().name,QString("Пятёрочка"));
    QVERIFY(restored.restoreDatabase(copy,&added,&previous));QVERIFY(added>0);
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

namespace {
QVariantMap transferChoice(const QVariantMap& row, QString source="bank", QString target="second") {
    return {{"fingerprint",row.value("fingerprint")},{"transactionType","transfer"},{"confirmed",true},
        {"sourceAccountId",source},{"targetAccountId",target}};
}
QVariantMap rowFrom(const QVariantMap& preview) { return preview.value("operationRows").toList().front().toMap(); }
bool seedSecond(const QString& path) {
    FinanceRepository r(path);
    return r.insertAccount({"second","Второй банк",AssetType::Fiat,AccountType::DebitCard,Currency::RUB})
        && r.insertAccount({"usd","Доллары",AssetType::Fiat,AccountType::Cash,Currency::USD});
}
}
void BankImportControllerTest::transfersPersistAndStatementsJoinWithoutDuplicates() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv"),copy=dir.filePath("backup.db");
    QVERIFY(seed(path));QVERIFY(seedSecond(path));
    QVERIFY(csv(file,"01.10.2026;-100;Перевод между своими счетами;out;;;RUB;\n"));
    QString incomingRowKey;
    {
        FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
        const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());const auto row=rowFrom(preview);
        auto choice=transferChoice(row);choice["remember"]=true;choice["rememberRecipient"]=true; // Ignored for transfers.
        const auto choices=choicesFor(preview,choice);
        const auto resolved=c.resolveBankImportRows(preview.value("operationRows").toList(),choices);
        QVERIFY2(resolved.value("ok").toBool(),qPrintable(resolved.value("error").toString()));
        QCOMPARE(rowFrom(resolved).value("type").toString(),QString("transfer"));QVERIFY(!rowFrom(resolved).value("needsReview").toBool());
        auto result=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("imported").toInt(),1);QCOMPARE(result.value("transfers").toInt(),1);
        QCOMPARE(c.bankCategoryRules().value("items").toList().size(),0);QCOMPARE(c.bankRecipientRules().value("items").toList().size(),0);
        result=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}});
        QVERIFY(result.value("ok").toBool());QCOMPARE(result.value("imported").toInt(),0);QCOMPARE(result.value("skipped").toInt(),1);
        QCOMPARE(c.previewBankImport(QUrl::fromLocalFile(file),profile()).value("duplicateCount").toInt(),1);
        auto secondProfile=profile();secondProfile["id"]="second-profile";secondProfile["name"]="Второй банк";secondProfile["accountId"]="second";
        QVERIFY(c.saveBankCsvProfile(secondProfile).value("ok").toBool());
        QVERIFY(csv(file,"02.10.2026;100;Зачисление перевода;in;;;RUB;\n"));
        const auto secondPreview=c.previewBankImport(QUrl::fromLocalFile(file),secondProfile);const auto incomingRow=rowFrom(secondPreview);
        incomingRowKey=incomingRow.value("rowKey").toString();auto incomingChoice=transferChoice(incomingRow);
        auto details=c.bankImportTransferDetails(incomingRow,incomingChoice);QVERIFY(!details.value("ok").toBool());
        QCOMPARE(details.value("matches").toList().size(),1);const auto match=details.value("matches").toList().front().toMap();
        QCOMPARE(match.value("kind").toString(),QString("transfer"));incomingChoice["transferMatchId"]=match.value("value");
        result=c.importBankCsv(QUrl::fromLocalFile(file),"second-profile",{{"choices",choicesFor(secondPreview,incomingChoice)}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(result.value("imported").toInt(),1);
        QCOMPARE(c.previewBankImport(QUrl::fromLocalFile(file),secondProfile).value("duplicateCount").toInt(),1);
    }
    FinanceRepository r(path);const auto txs=r.loadTransactions();QCOMPARE(txs.size(),2);qint64 bank=0,second=0;
    for(const auto& t:txs) {QVERIFY(t.categoryId().startsWith("transfer-"));if(t.accountId()=="bank")bank-=t.money().minorUnits();else second+=t.money().minorUnits();}
    QCOMPARE(bank,qint64(-10000));QCOMPARE(second,qint64(10000));const auto summary=r.loadSummary();
    QCOMPARE(summary.income[0],qint64(0));QCOMPARE(summary.expense[0],qint64(0));QCOMPARE(summary.balance[0],qint64(0));
    QCOMPARE(r.loadBankImportLinks().size(),2);QVERIFY(r.loadBankImportLinks().contains(incomingRowKey));
    QVERIFY(r.backupDatabase(copy));FinanceRepository restored(dir.filePath("restored.db"));QVERIFY(restored.restoreDatabase(copy));
    QCOMPARE(restored.loadBankImportLinks().size(),2);QCOMPARE(restored.loadTransactions().size(),2);
    FinanceController reopened(nullptr,path);auto secondProfile=profile();secondProfile["id"]="second-profile";secondProfile["name"]="Второй банк";secondProfile["accountId"]="second";
    QCOMPARE(reopened.previewBankImport(QUrl::fromLocalFile(file),secondProfile).value("duplicateCount").toInt(),1);
    const auto pair=txs.front().id();QVERIFY(reopened.deleteTransaction(pair));QVERIFY(r.loadBankImportLinks().isEmpty());
}
void BankImportControllerTest::ordinaryCounterpartIsCombinedAtomically() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));QVERIFY(seedSecond(path));
    FinanceController c(nullptr,path);auto secondProfile=profile();secondProfile["id"]="second-profile";secondProfile["name"]="Второй банк";secondProfile["accountId"]="second";
    QVERIFY(c.saveBankCsvProfile(secondProfile).value("ok").toBool());QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    QVERIFY(csv(file,"02.10.2026;100;Перевод с другого счёта;in;;;RUB;\n"));
    QVERIFY(c.importBankCsv(QUrl::fromLocalFile(file),"second-profile",{{"requireReview",false}}).value("ok").toBool());
    FinanceRepository r(path);const auto original=r.loadTransactions().front();QCOMPARE(original.categoryId(),QString("salary"));
    QVERIFY(csv(file,"01.10.2026;-100;Перевод на другой счёт;out;;;RUB;\n"));
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());const auto row=rowFrom(preview);auto choice=transferChoice(row);
    auto details=c.bankImportTransferDetails(row,choice);QCOMPARE(details.value("matches").toList().size(),1);
    const auto match=details.value("matches").toList().front().toMap();QCOMPARE(match.value("kind").toString(),QString("operation"));
    choice["transferMatchId"]=match.value("value");const auto result=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,choice)}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(r.loadTransactions().size(),2);
    QCOMPARE(r.loadSummary().income[0],qint64(0));QCOMPARE(r.loadSummary().expense[0],qint64(0));
    QVERIFY(r.loadBankImportLinks().contains(original.id()));
    for(const auto& tx:r.loadTransactions())if(tx.accountId()=="second") {
        QCOMPARE(tx.description(),original.description());QCOMPARE(tx.date(),original.date());
    }
    QVERIFY(csv(file,"02.10.2026;100;Перевод с другого счёта;in;;;RUB;\n"));
    QCOMPARE(c.previewBankImport(QUrl::fromLocalFile(file),secondProfile).value("duplicateCount").toInt(),1);
}
void BankImportControllerTest::transferValidationAndCrossCurrencyAmounts() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));QVERIFY(seedSecond(path));
    FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    QVERIFY(csv(file,"01.10.2026;-100;Конвертация между своими счетами;out;;;RUB;\n"));
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());const auto row=rowFrom(preview);
    auto choice=transferChoice(row);
    for(const auto& invalid:QList<QVariantMap>{transferChoice(row,"bank","bank"),transferChoice(row,"second","bank"),
        transferChoice(row,"bank","missing"),transferChoice(row,"bank","usd")}) {
        QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,invalid)},{"requireReview",false}}).value("ok").toBool());
    }
    choice["confirmed"]=false;
    QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,choice)},{"requireReview",false}}).value("ok").toBool());
    choice=transferChoice(row,"bank","usd");
    for(const auto& amount:QStringList{"-1","0","12.345","1e2","2abc"}) {
        choice["peerAmount"]=amount;
        QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,choice)}}).value("ok").toBool());
    }
    choice["peerAmount"]="1,23";
    auto result=c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,choice)}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    FinanceRepository r(path);for(const auto& t:r.loadTransactions()) {
        QCOMPARE(t.money().minorUnits(),t.accountId()=="bank"?qint64(10000):qint64(123));
        QCOMPARE(t.money().currency(),t.accountId()=="bank"?Currency::RUB:Currency::USD);
    }
    auto usdProfile=profile();usdProfile["id"]="usd-profile";usdProfile["name"]="Доллары";usdProfile["accountId"]="usd";
    QVERIFY(c.saveBankCsvProfile(usdProfile).value("ok").toBool());
    QVERIFY(csv(file,"03.10.2026;2.50;Конвертация;in;;;USD;\n"));
    const auto incoming=c.previewBankImport(QUrl::fromLocalFile(file),usdProfile);choice=transferChoice(rowFrom(incoming),"bank","usd");choice["peerAmount"]="200";
    result=c.importBankCsv(QUrl::fromLocalFile(file),"usd-profile",{{"choices",choicesFor(incoming,choice)}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(r.loadTransactions().size(),4);
    QVERIFY(csv(file,"04.10.2026;-50;PYATEROCHKA;purchase;;;RUB;\n"));
    const auto normal=c.previewBankImport(QUrl::fromLocalFile(file),profile());choice=transferChoice(rowFrom(normal));choice["transactionType"]="income";
    QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(normal,choice)}}).value("ok").toBool());
    QCOMPARE(r.loadTransactions().size(),4);
}
void BankImportControllerTest::repeatedMatchAndLinkFailureRollBack() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));QVERIFY(seedSecond(path));
    FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    QVERIFY(c.addTransfer(10000,"Ручной перевод","bank","second",QDateTime(QDate(2026,10,1),QTime(12,0))));
    QVERIFY(csv(file,"01.10.2026;-100;Перевод;one;;;RUB;\n01.10.2026;-100;Перевод;two;;;RUB;\n"));
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());QVariantMap choices;
    for(const auto& value:preview.value("operationRows").toList()) {
        const auto row=value.toMap();auto choice=transferChoice(row);const auto details=c.bankImportTransferDetails(row,choice);
        choice["transferMatchId"]=details.value("matches").toList().front().toMap().value("value");choices[row.value("rowKey").toString()]=choice;
    }
    QVERIFY(!c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choices}}).value("ok").toBool());
    FinanceRepository r(path);QCOMPARE(r.loadTransactions().size(),2);QVERIFY(r.loadBankImportLinks().isEmpty());
    const Transaction invalid("bad","bank","groceries",Money(1,Currency::RUB),TransactionType::Expense,QDateTime::currentDateTimeUtc(),"Test");
    QVERIFY(!r.insertTransactions({invalid},{},{},{{"alias","missing-transfer"}}));QCOMPARE(r.loadTransactions().size(),2);QVERIFY(r.loadBankImportLinks().isEmpty());
    QVERIFY(r.clearAllUserData());QVERIFY(r.loadBankImportLinks().isEmpty());
}

void BankImportControllerTest::editingKeepsStatementIdentities() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db"),file=dir.filePath("bank.csv");QVERIFY(seed(path));QVERIFY(seedSecond(path));
    FinanceController c(nullptr,path);QVERIFY(c.saveBankCsvProfile(profile()).value("ok").toBool());
    QVERIFY(csv(file,"01.10.2026;-100;Перевод между счетами;one;;;RUB;\n"));
    const auto preview=c.previewBankImport(QUrl::fromLocalFile(file),profile());const auto row=rowFrom(preview);
    QVERIFY(c.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"choices",choicesFor(preview,transferChoice(row))}}).value("ok").toBool());
    FinanceRepository r(path);auto transactions=r.loadTransactions();QString currentId;
    const auto date=QDateTime(QDate(2026,10,1),QTime(12,0));
    for(const auto& tx:transactions)if(tx.accountId()=="bank")currentId=tx.id();
    QVERIFY(r.replaceTransactionWithTransfer(currentId,
        Transaction("edited-out","bank","transfer-out",Money(10000,Currency::RUB),TransactionType::Expense,date,"Исправленный перевод"),
        Transaction("edited-in","second","transfer-in",Money(10000,Currency::RUB),TransactionType::Income,date,"Исправленный перевод")));
    QCOMPARE(r.loadBankImportLinks().value(row.value("rowKey").toString()),QString("edited-out"));
    FinanceController reopened(nullptr,path);QCOMPARE(reopened.previewBankImport(QUrl::fromLocalFile(file),profile()).value("duplicateCount").toInt(),1);
    QVERIFY(r.replaceTransaction("edited-out",Transaction("ordinary","bank","other_expense",Money(10000,Currency::RUB),
        TransactionType::Expense,date,"Изменено на расход")));
    QCOMPARE(r.loadBankImportLinks().value(row.value("rowKey").toString()),QString("ordinary"));
    FinanceController ordinary(nullptr,path);QCOMPARE(ordinary.previewBankImport(QUrl::fromLocalFile(file),profile()).value("duplicateCount").toInt(),1);
    QVERIFY(r.deleteTransaction("ordinary"));QVERIFY(r.loadBankImportLinks().isEmpty());
}

void BankImportControllerTest::cnyAccountImportAndSettingsSurviveRestart() {
    QTemporaryDir dir;
    const auto path=dir.filePath("data.db"),file=dir.filePath("cny.csv");
    QString accountId;
    {
        FinanceRepository repository(path);
        QVERIFY(repository.saveAutomaticCurrencyRates(false));
    }
    {
        FinanceController controller(nullptr,path);
        controller.setSelectedAsset("fiat");
        controller.clearDateFilter();
        QVERIFY(controller.saveManualCurrencyRates(1,100,120,13.25));
        QVERIFY(controller.addAccount("Юани","debit_card","CNY",10000,0));
        const auto account=controller.allAccounts().last().toMap();
        accountId=account.value("id").toString();
        QCOMPARE(account.value("currency").toString(),QString("CNY"));
        QCOMPARE(controller.balanceMinorUnits(),qint64(132500));
        auto values=profile();values["accountId"]=accountId;
        QVERIFY(controller.saveBankCsvProfile(values).value("ok").toBool());
        QVERIFY(csv(file,"01.10.2026;50;Зарплата;in;;;156;Зарплата\n02.10.2026;-10;PYATEROCHKA;out;;;CNY;Продукты\n"));
        const auto result=controller.importBankCsv(QUrl::fromLocalFile(file),"bank-profile",{{"requireReview",false}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
        QCOMPARE(result.value("imported").toInt(),2);
        QCOMPARE(controller.balanceMinorUnits(),qint64(185500));
        controller.setAppCurrency("CNY");
        QCOMPARE(controller.balanceMinorUnits(),qint64(14000));
        controller.setAnalyticsCurrency("CNY");
        QCOMPARE(controller.analyticsCurrency(),QString("CNY"));
        QVERIFY(controller.setDateFilter(QDateTime(QDate(2026,10,1),QTime(0,0)),QDateTime(QDate(2026,10,31),QTime(23,59))));
        QCOMPARE(controller.incomeMinorUnits(),qint64(5000));
        QCOMPARE(controller.expenseMinorUnits(),qint64(1000));
        QCOMPARE(controller.balanceMinorUnits(),qint64(14000));
        controller.clearDateFilter();
        QVERIFY(controller.saveManualCurrencyRates(1,100,120)); // Legacy overload preserves CNY.
        QCOMPARE(controller.manualCnyToRubRate(),13.25);
        QVERIFY(!controller.saveManualCurrencyRates(1,100,120,0));
        QCOMPARE(controller.manualCnyToRubRate(),13.25);
    }
    FinanceController reopened(nullptr,path);
    reopened.clearDateFilter();
    QCOMPARE(reopened.appCurrency(),QString("CNY"));
    QCOMPARE(reopened.analyticsCurrency(),QString("CNY"));
    QCOMPARE(reopened.manualCnyToRubRate(),13.25);
    QCOMPARE(reopened.balanceMinorUnits(),qint64(14000));
    FinanceRepository repository(path);
    const auto summary=repository.loadSummary();
    QCOMPARE(summary.income[static_cast<int>(Currency::CNY)],qint64(5000));
    QCOMPARE(summary.expense[static_cast<int>(Currency::CNY)],qint64(1000));
    for (const auto& transaction:repository.loadTransactions()) QCOMPARE(transaction.money().currency(),Currency::CNY);
}

QTEST_GUILESS_MAIN(BankImportControllerTest)
#include "BankImportControllerTest.moc"
