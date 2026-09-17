#include "../services/CsvCodec.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class CsvCodecTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsUnicodeSeparatorsQuotesAndNewlines();
    void detectsCommaDelimiter();
    void detectsWindows1251();
    void rejectsUnclosedQuotedField();
};

void CsvCodecTest::roundTripsUnicodeSeparatorsQuotesAndNewlines()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("operations.csv"));
    const QVector<QStringList> source{
        {QStringLiteral("type"), QStringLiteral("description")},
        {QStringLiteral("expense"), QStringLiteral("Кофе; булочка")},
        {QStringLiteral("income"), QStringLiteral("Строка \"один\"\nстрока два")}};

    QString error;
    QVERIFY2(CsvCodec::writeFile(path, source, error), qPrintable(error));
    QFile raw(path);
    QVERIFY(raw.open(QIODevice::ReadOnly));
    QVERIFY(raw.read(3) == QByteArray("\xEF\xBB\xBF", 3));

    const CsvCodec::ReadResult result = CsvCodec::readFile(path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.rows, source);
}

void CsvCodecTest::detectsCommaDelimiter()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("bank.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("date,amount,description\n17.09.2026,-123.45,\"Coffee, shop\"\n");
    file.close();

    const CsvCodec::ReadResult result = CsvCodec::readFile(path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.delimiter, QLatin1Char(','));
    QCOMPARE(result.rows.size(), 2);
    QCOMPARE(result.rows.at(1).at(2), QStringLiteral("Coffee, shop"));
}

void CsvCodecTest::detectsWindows1251()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("bank-1251.csv"));
    QByteArray bytes("date;description\n17.09.2026;");
    bytes.append(char(0xCA));
    bytes.append(char(0xEE));
    bytes.append(char(0xF4));
    bytes.append(char(0xE5));
    bytes.append('\n');
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
    file.close();

    const CsvCodec::ReadResult result = CsvCodec::readFile(path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.encoding, QStringLiteral("Windows-1251"));
    QCOMPARE(result.rows.at(1).at(1), QStringLiteral("Кофе"));
}

void CsvCodecTest::rejectsUnclosedQuotedField()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("broken.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("\"broken"), 7);
    file.close();

    const CsvCodec::ReadResult result = CsvCodec::readFile(path);
    QVERIFY(!result.error.isEmpty());
}

QTEST_MAIN(CsvCodecTest)
#include "CsvCodecTest.moc"
