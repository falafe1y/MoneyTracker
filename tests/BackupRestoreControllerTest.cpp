#include "../interface/FinanceController.h"
#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class BackupRestoreControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void migratesLegacyNotesOnce();
    void backupFlushesPendingNotes();
    void replacementRefreshesSettingsAndNotes();
    void failedRestorePreservesNotes();
    void oldBackupClearsNotesWithoutReimport();
};
static bool seed(const QString& path) {
    FinanceRepository r(path);
    return r.isOpen() && r.clearAllUserData() && r.saveAutomaticCurrencyRates(false);
}
static bool writeLegacy(const QString& path, const QString& text) {
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(text.toUtf8()) == text.toUtf8().size();
}
void BackupRestoreControllerTest::migratesLegacyNotesOnce() {
    QTemporaryDir dir; const auto path=dir.filePath("data.db");
    // A new database has no document yet: this is the migration marker.
    { FinanceRepository r(path); QVERIFY(r.saveAutomaticCurrencyRates(false)); }
    const auto legacy=dir.filePath("notes.txt");
    const QString text=QStringLiteral("Заметки\nПривет 👋 'цитата'");
    QVERIFY(writeLegacy(legacy,text));
    { FinanceController c(nullptr,path); QCOMPARE(c.notesText(),text); QVERIFY(!QFile::exists(legacy));
      c.setNotesText(text+"\nНовая строка"); QVERIFY(c.saveNotes()); }
    QVERIFY(writeLegacy(legacy,"obsolete"));
    { FinanceController c(nullptr,path); QCOMPARE(c.notesText(),text+"\nНовая строка"); }
}
void BackupRestoreControllerTest::backupFlushesPendingNotes() {
    QTemporaryDir dir; const auto path=dir.filePath("data.db"); QVERIFY(seed(path));
    FinanceController c(nullptr,path);
    const QString text=QStringLiteral("Доходы\n")+QString(100000,QChar(0x044F));
    c.setNotesText(text);
    const auto backup=dir.filePath("copy.db");
    const auto result=c.backupDatabase(QUrl::fromLocalFile(backup));
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    FinanceRepository copy(backup); QString saved; QVERIFY(copy.loadNotes(saved)); QCOMPARE(saved,text);
}
void BackupRestoreControllerTest::replacementRefreshesSettingsAndNotes() {
    QTemporaryDir dir; const auto path=dir.filePath("data.db"), source=dir.filePath("source.db"), backup=dir.filePath("copy.db");
    QVERIFY(seed(path)); QVERIFY(seed(source));
    { FinanceRepository r(source); QVERIFY(r.saveNotes("Из копии")); QVERIFY(r.saveAppCurrency("EUR"));
      QVERIFY(r.saveAnalyticsCurrency("CNY")); QVERIFY(r.saveUiLanguage("en")); QVERIFY(r.saveManualCurrencyRates(1,95,105,14));
      QVERIFY(r.insertAccount({"restored","Из копии",AssetType::Fiat,AccountType::Cash,Currency::RUB})); QVERIFY(r.backupDatabase(backup)); }
    FinanceController c(nullptr,path); c.setNotesText("Несохранённый текущий текст");
    const auto result=c.restoreDatabase(QUrl::fromLocalFile(backup));
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.notesText(),QStringLiteral("Из копии")); QCOMPARE(c.appCurrency(),QStringLiteral("EUR"));
    QCOMPARE(c.analyticsCurrency(),QStringLiteral("CNY")); QCOMPARE(c.uiLanguage(),QStringLiteral("en")); QCOMPARE(c.manualUsdToRubRate(),95.0);
    const auto previous=result.value("previousBackupPath").toString(); QVERIFY(QFile::exists(previous));
    { FinanceRepository r(previous); QString saved; QVERIFY(r.loadNotes(saved)); QCOMPARE(saved,QStringLiteral("Несохранённый текущий текст")); }
    QTest::qWait(850);
    { FinanceRepository r(path); QString saved; QVERIFY(r.loadNotes(saved)); QCOMPARE(saved,QStringLiteral("Из копии")); }
}
void BackupRestoreControllerTest::failedRestorePreservesNotes() {
    QTemporaryDir dir; const auto path=dir.filePath("data.db"); QVERIFY(seed(path));
    FinanceController c(nullptr,path); c.setNotesText("Текущий текст");
    QVERIFY(!c.restoreDatabase(QUrl::fromLocalFile(dir.filePath("missing.db"))).value("ok").toBool());
    QCOMPARE(c.notesText(),QStringLiteral("Текущий текст"));
    FinanceRepository r(path); QString text; QVERIFY(r.loadNotes(text)); QCOMPARE(text,c.notesText());
}
void BackupRestoreControllerTest::oldBackupClearsNotesWithoutReimport() {
    QTemporaryDir dir; const auto path=dir.filePath("data.db"), source=dir.filePath("source.db"), backup=dir.filePath("copy.db");
    QVERIFY(seed(path)); QVERIFY(seed(source));
    { FinanceRepository r(source); QVERIFY(r.backupDatabase(backup)); }
    const QString connection="old-backup-notes";
    { auto db=QSqlDatabase::addDatabase("QSQLITE",connection); db.setDatabaseName(backup); QVERIFY(db.open());
      QSqlQuery query(db); QVERIFY(query.exec("DROP TABLE notes")); }
    QSqlDatabase::removeDatabase(connection);
    { FinanceController c(nullptr,path); c.setNotesText("Текущий текст"); QVERIFY(writeLegacy(dir.filePath("notes.txt"),"obsolete"));
      const auto result=c.restoreDatabase(QUrl::fromLocalFile(backup)); QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString())); QVERIFY(c.notesText().isEmpty()); }
    FinanceController reopened(nullptr,path); QVERIFY(reopened.notesText().isEmpty());
}
QTEST_GUILESS_MAIN(BackupRestoreControllerTest)
#include "BackupRestoreControllerTest.moc"
