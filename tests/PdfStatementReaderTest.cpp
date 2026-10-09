#include "../services/PdfStatementReader.h"
#include "../services/BankCsvImporter.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QGuiApplication>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

namespace {
using Reader = PdfStatementReader;
void add(Reader::Page& page, QString text, double x, double y)
{
    for (const auto& word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        page.append({word, QRectF(x, y, word.size() * 5., 10)});
        x += word.size() * 5. + 4;
    }
}
Reader::Page alfaHeader()
{
    Reader::Page page;
    add(page, QStringLiteral("АО Альфа-Банк"), 20, 10);
    add(page, QStringLiteral("Валюта счета: RUR"), 20, 30);
    add(page, QStringLiteral("Дата проводки"), 20, 70);
    add(page, QStringLiteral("Код операции"), 100, 70);
    add(page, QStringLiteral("Описание"), 200, 70);
    add(page, QStringLiteral("Сумма"), 500, 70);
    return page;
}
}

class PdfStatementReaderTest : public QObject {
    Q_OBJECT
private slots:
    void alfaMultilineAndPageContinuation() {
        auto page = alfaHeader();
        add(page, "01.10.2026", 20, 100);
        add(page, "CRD_001", 100, 100);
        add(page, QStringLiteral("Оплата магазина"), 200, 100);
        add(page, "-1 250,45 RUR", 460, 100);
        add(page, QStringLiteral("Пятёрочка"), 200, 116);
        Reader::Page second;
        add(second, QStringLiteral("Продолжение описания"), 200, 10);
        add(second, "02.10.2026", 20, 30);
        add(second, "TX002", 100, 30);
        add(second, QStringLiteral("Зарплата"), 200, 30);
        add(second, "50 000,00 RUR", 460, 30);
        add(second, QStringLiteral("Итого 48 749,55 RUR"), 20, 60);
        const auto result = Reader::parsePages({page, second});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.rows.size(), 3);
        QCOMPARE(result.rows[1][1], QStringLiteral("-1 250,45"));
        QVERIFY(result.rows[1][2].contains(QStringLiteral("Пятёрочка")));
        QVERIFY(result.rows[1][2].contains(QStringLiteral("Продолжение описания")));
        QCOMPARE(result.rows[1][3], QStringLiteral("CRD_001"));
        QCOMPARE(result.rows[2][1], QStringLiteral("+50 000,00"));
        QCOMPARE(result.rows[2][4], QStringLiteral("RUB"));
    }
    void sberSeparateColumns() {
        Reader::Page page;
        add(page, QStringLiteral("ПАО Сбербанк"), 20, 10);
        add(page, QStringLiteral("Валюта счета: RUB"), 20, 30);
        add(page, QStringLiteral("Дата"), 20, 70);
        add(page, QStringLiteral("Описание"), 150, 70);
        add(page, QStringLiteral("Приход"), 400, 70);
        add(page, QStringLiteral("Расход"), 520, 70);
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Перевод"), 150, 100);
        add(page, "700,00", 400, 100);
        add(page, "02.10.2026", 20, 130);
        add(page, QStringLiteral("Покупка"), 150, 130);
        add(page, "0,00", 400, 130);
        add(page, "350,00", 520, 130);
        const auto result = Reader::parsePages({page});
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.rows[1][1], QStringLiteral("+700,00"));
        QCOMPARE(result.rows[2][1], QStringLiteral("-350,00"));
    }
    void alfaIgnoresOtherBankInMultilineDescription() {
        auto page = alfaHeader();
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Перевод из ПАО Сбербанк"), 200, 100);
        add(page, "700,00 RUR", 460, 100);
        Reader::Page second;
        add(second, QStringLiteral("через Сбербанк Онлайн"), 200, 10);
        add(second, "02.10.2026", 20, 40);
        add(second, QStringLiteral("Оплата"), 200, 40);
        add(second, "-100,00 RUR", 460, 40);
        const auto result = Reader::parsePages({page, second});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.encoding, QStringLiteral("PDF · Альфа-Банк"));
        QCOMPARE(result.rows.size(), 3);
        QCOMPARE(result.rows[1][1], QStringLiteral("+700,00"));
        QVERIFY(result.rows[1][2].contains(QStringLiteral("Сбербанк Онлайн")));
    }
    void sberIgnoresAlfaInDescription() {
        auto page = alfaHeader();
        page[1].text = QStringLiteral("Сбербанк");
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Перевод в АО Альфа-Банк"), 200, 100);
        add(page, "-700,00 RUB", 460, 100);
        const auto result = Reader::parsePages({page});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.encoding, QStringLiteral("PDF · Сбер"));
        QCOMPARE(result.rows[1][1], QStringLiteral("-700,00"));
    }
    void issuerOnlyInSignature() {
        auto page = alfaHeader();
        page.remove(0, 2); // The actual Alfa statement has no bank name in its preamble.
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Перевод из ПАО Сбербанк"), 200, 100);
        add(page, "700,00 RUR", 460, 100);
        add(page, QStringLiteral("(подпись сотрудника АО «АЛЬФА-БАНК»)"), 20, 750);
        const auto result = Reader::parsePages({page});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.encoding, QStringLiteral("PDF · Альфа-Банк"));
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows[1][1], QStringLiteral("+700,00"));
    }
    void counterpartyDoesNotIdentifyUnknownIssuer() {
        auto page = alfaHeader();
        page[1].text = QStringLiteral("Другой-Банк");
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Перевод из ПАО Сбербанк"), 200, 100);
        add(page, "700,00 RUB", 460, 100);
        const auto result = Reader::parsePages({page});
        QVERIFY(result.error.contains(QStringLiteral("Банк не определён")));
        QCOMPARE(result.rows.size(), 1);
    }
    void conflictingIssuersStillFail() {
        auto page = alfaHeader();
        add(page, QStringLiteral("ПАО Сбербанк"), 250, 10);
        add(page, "01.10.2026", 20, 100);
        add(page, "700,00 RUB", 460, 100);
        QVERIFY(Reader::parsePages({page}).error.contains(QStringLiteral("Банк не определён")));
    }
    void datesAtEndOfDescriptionAreNotAmounts() {
        for (const auto &date : {QStringLiteral("22.02.26"), QStringLiteral("22.02.2026")}) {
            auto page = alfaHeader();
            add(page, "22.01.2026", 20, 100);
            add(page, QStringLiteral("Комиссия за период до"), 200, 100);
            add(page, date, 440, 100); // A long description reaches the amount search region.
            add(page, "-99,00 RUR", 520, 100);
            const auto result = Reader::parsePages({page});
            QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
            QCOMPARE(result.rows.size(), 2);
            QCOMPARE(result.rows[1][1], QStringLiteral("-99,00"));
        }
    }
    void unrecognizedBankFails() {
        auto page = alfaHeader();
        page[1].text = QStringLiteral("Другой-Банк");
        QVERIFY(!Reader::parsePages({page}).error.isEmpty());
    }
    void ambiguousAmountsAndCurrencyAreRejected() {
        auto page = alfaHeader();
        add(page, "01.10.2026", 20, 100);
        add(page, QStringLiteral("Покупка"), 200, 100);
        add(page, "10,00 20,00 RUB", 440, 100);
        const auto result = Reader::parsePages({page});
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.rows.size(), 2);
        QVERIFY(result.rows[1][1].isEmpty());
        Reader::Page sber = alfaHeader();
        sber[1].text = QStringLiteral("Сбербанк");
        add(sber, "01.10.2026", 20, 100);
        add(sber, QStringLiteral("Покупка"), 200, 100);
        add(sber, "100,00 RUB", 470, 100);
        const auto unsignedResult = Reader::parsePages({sber});
        QVERIFY(unsignedResult.error.isEmpty());
        QVERIFY(unsignedResult.rows[1][1].isEmpty());
    }
    void doesNotImportBalancesOrPendingOperations() {
        auto page = alfaHeader();
        add(page, "01.10.2026", 20, 100);
        add(page, "-125,00 RUR", 460, 100);
        add(page, QStringLiteral("HOLD Неподтвержденная операция 01.10.2026"), 20, 130);
        add(page, "-900,00 RUR", 460, 146);
        add(page, QStringLiteral("Исходящий остаток 99 999,00"), 20, 180);
        const auto result = Reader::parsePages({page});
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows[1][1], QStringLiteral("-125,00"));
    }
    void actualPdfUsesCanonicalMappingAndStableFingerprints() {
        QTemporaryDir dir;
        const QString path = dir.filePath("alfa.pdf");
        {
            QPdfWriter writer(path); writer.setResolution(72);
            QPainter painter(&writer); painter.setFont(QFont("DejaVu Sans", 8));
            painter.drawText(20, 20, QStringLiteral("АО Альфа-Банк"));
            painter.drawText(20, 40, QStringLiteral("Валюта счета: RUB"));
            painter.drawText(20, 80, QStringLiteral("Дата проводки"));
            painter.drawText(100, 80, QStringLiteral("Код операции"));
            painter.drawText(200, 80, QStringLiteral("Описание"));
            painter.drawText(500, 80, QStringLiteral("Сумма"));
            painter.drawText(20, 110, "01.10.2026");
            painter.drawText(100, 110, "CRD_123");
            painter.drawText(200, 110, QStringLiteral("Перевод из Сбербанка"));
            painter.drawText(460, 110, "-1 250,45 RUB");
            painter.end();
        }
        BankCsvProfile profile; // No column mappings: PDF ignores CSV settings.
        auto result = BankCsvImporter::parse(path, profile);
        QCOMPARE(result.operations.size(), 1);
        QCOMPARE(result.operations[0].signedMinor, qint64(-125045));
        QCOMPARE(result.operations[0].currencyCode, QStringLiteral("RUB"));
        QCOMPARE(result.operations[0].externalId, QStringLiteral("CRD_123"));
        QCOMPARE(result.operations[0].fingerprint, BankCsvImporter::parse(path, profile).operations[0].fingerprint);
#ifndef Q_OS_WIN
        // Once decoded, the statement can be previewed/imported without launching Poppler again.
        if (!QFileInfo(QDir(QCoreApplication::applicationDirPath()).filePath("pdftotext")).isExecutable()) {
            const bool hadPath = qEnvironmentVariableIsSet("PATH");
            const QByteArray previousPath = qgetenv("PATH");
            const auto restorePath = qScopeGuard([hadPath, previousPath] {
                if (hadPath) qputenv("PATH", previousPath); else qunsetenv("PATH");
            });
            qputenv("PATH", QByteArray());
            const auto cached = BankCsvImporter::readTable(path, profile);
            QVERIFY2(cached.error.isEmpty(), qPrintable(cached.error));
            QCOMPARE(cached.rows.size(), 2);
            // A changed file must never return the previously decoded operations.
            QFile replacement(path);
            QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
            replacement.write("changed PDF"); replacement.close();
            QVERIFY(!BankCsvImporter::readTable(path, profile).error.isEmpty());
        }
#endif

    }
    void scannedPdfAndInvalidFileFail() {
        QTemporaryDir dir;
        const QString path = dir.filePath("scan.pdf");
        { QPdfWriter writer(path); QPainter painter(&writer); painter.fillRect(0, 0, 100, 100, Qt::black); }
        QVERIFY(Reader::read(path).error.contains(QStringLiteral("текстового слоя")));
        QFile invalid(dir.filePath("invalid.pdf")); QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not a PDF"); invalid.close();
        QVERIFY(!Reader::read(invalid.fileName()).error.isEmpty());
    }
};
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PdfStatementReaderTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "PdfStatementReaderTest.moc"
