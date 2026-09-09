#include "../services/CsvCodec.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class CsvCodecTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsUnicodeSeparatorsQuotesAndNewlines();
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
