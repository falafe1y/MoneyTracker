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
    void parsesDirectionAndCurrencyColumns();
    void readsXlsxStatement();
};

namespace
{
void append16(QByteArray& bytes, const quint16 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

void append32(QByteArray& bytes, const quint32 value)
{
    append16(bytes, static_cast<quint16>(value & 0xffff));
    append16(bytes, static_cast<quint16>(value >> 16));
}

QByteArray storedZip(const QVector<QPair<QByteArray, QByteArray>>& files)
{
    QByteArray archive;
    QVector<quint32> offsets;
    for (const auto& file : files) {
        offsets.append(static_cast<quint32>(archive.size()));
        append32(archive, 0x04034b50); append16(archive, 20); append16(archive, 0);
        append16(archive, 0); append16(archive, 0); append16(archive, 0);
        append32(archive, 0); append32(archive, file.second.size());
        append32(archive, file.second.size()); append16(archive, file.first.size());
        append16(archive, 0); archive += file.first; archive += file.second;
    }
    const quint32 directoryOffset = static_cast<quint32>(archive.size());
    for (qsizetype index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        append32(archive, 0x02014b50); append16(archive, 20); append16(archive, 20);
        append16(archive, 0); append16(archive, 0); append16(archive, 0); append16(archive, 0);
        append32(archive, 0); append32(archive, file.second.size());
        append32(archive, file.second.size()); append16(archive, file.first.size());
        append16(archive, 0); append16(archive, 0); append16(archive, 0);
        append16(archive, 0); append32(archive, 0); append32(archive, offsets[index]);
        archive += file.first;
    }
    const quint32 directorySize = static_cast<quint32>(archive.size()) - directoryOffset;
    append32(archive, 0x06054b50); append16(archive, 0); append16(archive, 0);
    append16(archive, files.size()); append16(archive, files.size());
    append32(archive, directorySize); append32(archive, directoryOffset); append16(archive, 0);
    return archive;
}
}

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

void BankCsvImporterTest::parsesDirectionAndCurrencyColumns()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("direction.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("date;amount;direction;currency\n"
               "17.09.2026;1000;debit;RUR\n"
               "18.09.2026;2500;credit;643\n");
    file.close();
    BankCsvProfile profile;
    profile.dateColumn = 0; profile.amountColumn = 1;
    profile.directionColumn = 2; profile.currencyColumn = 3;
    const auto result = BankCsvImporter::parse(path, profile);
    QCOMPARE(result.operations.size(), 2);
    QCOMPARE(result.operations[0].signedMinor, qint64(-100'000));
    QCOMPARE(result.operations[0].currencyCode, QStringLiteral("RUB"));
    QCOMPARE(result.operations[1].signedMinor, qint64(250'000));
    QCOMPARE(result.operations[1].currencyCode, QStringLiteral("RUB"));
}

void BankCsvImporterTest::readsXlsxStatement()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("statement.xlsx"));
    const QByteArray worksheet =
        "<?xml version=\"1.0\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>"
        "<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Date</t></is></c><c r=\"B1\" t=\"inlineStr\"><is><t>Amount</t></is></c></row>"
        "<row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>19.09.2026</t></is></c><c r=\"B2\"><v>-123.45</v></c></row>"
        "</sheetData></worksheet>";
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(storedZip({{QByteArray("xl/worksheets/sheet1.xml"), worksheet}}));
    file.close();
    BankCsvProfile profile;
    profile.dateColumn = 0; profile.amountColumn = 1;
    const auto result = BankCsvImporter::parse(path, profile);
    QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join(';')));
    QCOMPARE(result.operations.size(), 1);
    QCOMPARE(result.operations.first().signedMinor, qint64(-12'345));
    QCOMPARE(result.operations.first().occurredAt.date(), QDate(2026, 9, 19));
}

QTEST_APPLESS_MAIN(BankCsvImporterTest)

#include "BankCsvImporterTest.moc"
