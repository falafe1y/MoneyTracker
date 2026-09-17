#include "../services/BankCsvImporter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class BankCsvImporterTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesCommonMoneyFormats();
    void parsesSignedAmountStatement();
    void parsesSeparateIncomeAndExpenseColumns();
    void reportsBadRowsWithoutDroppingValidRows();
};

void BankCsvImporterTest::parsesCommonMoneyFormats()
{
    qint64 amount = 0;
    QVERIFY(BankCsvImporter::parseAmountMinor(
        QStringLiteral("1 234,56 ₽"), amount));
    QCOMPARE(amount, qint64(123'456));
    QVERIFY(BankCsvImporter::parseAmountMinor(
        QStringLiteral("(123.45)"), amount));
    QCOMPARE(amount, qint64(-12'345));
    QVERIFY(BankCsvImporter::parseAmountMinor(
        QStringLiteral("1.234,56"), amount));
    QCOMPARE(amount, qint64(123'456));
    QVERIFY(BankCsvImporter::parseAmountMinor(
        QStringLiteral("1,234.56"), amount));
    QCOMPARE(amount, qint64(123'456));
    QVERIFY(BankCsvImporter::parseAmountMinor(
        QStringLiteral("123,45-"), amount));
    QCOMPARE(amount, qint64(-12'345));
}

void BankCsvImporterTest::parsesSignedAmountStatement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("signed.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "Date;Amount;Description;Operation ID\n"
        "16.09.2026;-1 234,56;Groceries;bank-1\n"
        "20.09.2026;50 000,00;Salary;bank-2\n");
    file.close();

    BankCsvProfile profile;
    profile.dateColumn = 0;
    profile.amountColumn = 1;
    profile.descriptionColumn = 2;
    profile.idColumn = 3;
    const BankCsvParseResult result = BankCsvImporter::parse(path, profile);

    QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join(';')));
    QCOMPARE(result.operations.size(), 2);
    QCOMPARE(result.operations.at(0).signedMinor, qint64(-123'456));
    QCOMPARE(result.operations.at(0).description, QStringLiteral("Groceries"));
    QCOMPARE(result.operations.at(1).signedMinor, qint64(5'000'000));
    QCOMPARE(result.operations.at(1).occurredAt.date(), QDate(2026, 9, 20));
}

void BankCsvImporterTest::parsesSeparateIncomeAndExpenseColumns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("separate.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "date,income,expense,details\n"
        "17.09.2026,,900.50,Internet\n"
        "18.09.2026,1000.00,,Refund\n");
    file.close();

    BankCsvProfile profile;
    profile.delimiter = QStringLiteral("comma");
    profile.amountMode = QStringLiteral("separate");
    profile.dateColumn = 0;
    profile.incomeColumn = 1;
    profile.expenseColumn = 2;
    profile.descriptionColumn = 3;
    const BankCsvParseResult result = BankCsvImporter::parse(path, profile);

    QCOMPARE(result.operations.size(), 2);
    QCOMPARE(result.operations.at(0).signedMinor, qint64(-90'050));
    QCOMPARE(result.operations.at(1).signedMinor, qint64(100'000));
}

void BankCsvImporterTest::reportsBadRowsWithoutDroppingValidRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("mixed.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        "date;amount\n"
        "not-a-date;-100\n"
        "17.09.2026;-200\n");
    file.close();

    BankCsvProfile profile;
    profile.dateColumn = 0;
    profile.amountColumn = 1;
    const BankCsvParseResult result = BankCsvImporter::parse(path, profile);

    QCOMPARE(result.rejected, 1);
    QCOMPARE(result.operations.size(), 1);
    QVERIFY(!result.errors.isEmpty());
}

QTEST_APPLESS_MAIN(BankCsvImporterTest)

#include "BankCsvImporterTest.moc"
