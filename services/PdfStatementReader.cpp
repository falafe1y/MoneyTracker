#include "PdfStatementReader.h"

#include <QFileInfo>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
struct Line { QVector<PdfStatementReader::Word> words; double y = 0; };
QVector<Line> lines(PdfStatementReader::Page page)
{
    std::sort(page.begin(), page.end(), [](const auto& a, const auto& b) {
        return a.bounds.center().y() < b.bounds.center().y();
    });
    QVector<Line> result;
    for (const auto& word : page) {
        if (result.isEmpty() || std::abs(result.last().y - word.bounds.center().y()) > 3)
            result.append(Line{{}, word.bounds.center().y()});
        result.last().words.append(word);
    }
    for (auto& line : result)
        std::sort(line.words.begin(), line.words.end(), [](const auto& a, const auto& b) {
            return a.bounds.left() < b.bounds.left();
        });
    return result;
}
QString text(const Line& line, double left = -1, double right = 1e9)
{
    QStringList parts;
    for (const auto& word : line.words)
        if (word.bounds.center().x() >= left && word.bounds.center().x() < right)
            parts.append(word.text);
    return parts.join(QLatin1Char(' ')).simplified();
}
QString currency(QString text)
{
    text = text.toUpper();
    const QRegularExpression re(QStringLiteral("(?:\\b(RUB|RUR|USD|EUR|CNY)\\b|₽|РУБ(?:Л|\\.|\\b)|€|\\$)"));
    const auto m = re.match(text);
    if (!m.hasMatch()) return {};
    const QString code = m.captured(1);
    if (code == QStringLiteral("RUR") || code == QStringLiteral("RUB") || m.captured().startsWith(QStringLiteral("РУБ")) || m.captured() == QStringLiteral("₽")) return QStringLiteral("RUB");
    if (m.captured() == QStringLiteral("€")) return QStringLiteral("EUR");
    if (m.captured() == QStringLiteral("$")) return QStringLiteral("USD");
    return code;
}
struct Header { double date = -1, description = -1, amount = -1, income = -1, expense = -1, id = -1; };
struct Record { QString date, description, id, amount, currency; int amountCount = 0; bool pending = false; };
const QRegularExpression dateRe(QStringLiteral("^(\\d{2}\\.\\d{2}\\.(?:\\d{4}|\\d{2}))(?:\\s|$)"));
const QRegularExpression moneyRe(QStringLiteral("(?<![\\d.,])([+\\-−]?\\s*(?:\\d{1,3}(?:[ \\x{00a0}\\x{202f}]\\d{3})+|\\d+)[.,]\\d{2})(?![\\d.,])"));
}

CsvCodec::ReadResult PdfStatementReader::read(const QString& path)
{
    CsvCodec::ReadResult failure;
    failure.encoding = QStringLiteral("PDF");
    if (QFileInfo(path).size() > 32 * 1024 * 1024) {
        failure.error = QStringLiteral("PDF больше 32 МБ. Сформируйте выписку за меньший период.");
        return failure;
    }
    QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
        QStringLiteral("pdftotext.exe")
#else
        QStringLiteral("pdftotext")
#endif
    );
    if (!QFileInfo(executable).isExecutable())
        executable = QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
    if (executable.isEmpty()) {
        failure.error = QStringLiteral("Для чтения PDF требуется pdftotext из Poppler. Установите его или поместите pdftotext рядом с приложением.");
        return failure;
    }
    QProcess process;
    process.start(executable, {QStringLiteral("-bbox-layout"), QStringLiteral("-enc"),
                              QStringLiteral("UTF-8"), QFileInfo(path).absoluteFilePath(), QStringLiteral("-")});
    if (!process.waitForStarted(5000)) {
        failure.error = QStringLiteral("Не удалось запустить pdftotext. Проверьте его установку.");
        return failure;
    }
    QByteArray output;
    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(100);
        output += process.readAllStandardOutput();
        if (output.size() > 32 * 1024 * 1024 || timer.elapsed() > 30000) {
            process.kill();
            process.waitForFinished(1000);
            failure.error = QStringLiteral("Превышен размер или время обработки PDF. Сформируйте выписку за меньший период.");
            return failure;
        }
    }
    output += process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        failure.error = QStringLiteral("Не удалось прочитать PDF. Проверьте файл и снимите защиту паролем, если она установлена.");
        return failure;
    }
    QXmlStreamReader xml(output);
    QVector<Page> pages;
    qsizetype characters = 0;
    qsizetype pageCharacters = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == QStringLiteral("page")) {
            if (pages.size() >= 200) {
                failure.error = QStringLiteral("В PDF больше 200 страниц. Разделите выписку на несколько периодов.");
                return failure;
            }
            pages.append(Page{});
            pageCharacters = 0;
        } else if (xml.name() == QStringLiteral("word") && !pages.isEmpty()) {
            const auto attrs = xml.attributes();
            bool valid[4];
            const double x1 = attrs.value(QStringLiteral("xMin")).toDouble(&valid[0]);
            const double y1 = attrs.value(QStringLiteral("yMin")).toDouble(&valid[1]);
            const double x2 = attrs.value(QStringLiteral("xMax")).toDouble(&valid[2]);
            const double y2 = attrs.value(QStringLiteral("yMax")).toDouble(&valid[3]);
            const QString word = xml.readElementText();
            characters += word.size();
            pageCharacters += word.size();
            if (!valid[0] || !valid[1] || !valid[2] || !valid[3] ||
                !std::isfinite(x1) || !std::isfinite(x2) || !std::isfinite(y1) || !std::isfinite(y2) ||
                x2 < x1 || y2 < y1 || characters > 1000000 || pageCharacters > 100000) {
                failure.error = QStringLiteral("Некорректный или слишком большой текстовый слой PDF.");
                return failure;
            }
            pages.last().append(Word{word, QRectF(x1, y1, x2-x1, y2-y1)});
        }
    }
    if (xml.hasError() || pages.isEmpty()) {
        failure.error = QStringLiteral("Не удалось разобрать текстовый слой PDF.");
        return failure;
    }
    for (int i = 0; i < pages.size(); ++i) {
        if (pages[i].isEmpty()) {
            failure.error = QStringLiteral("На странице %1 нет текстового слоя. Сканы и фотографии выписок не поддерживаются.").arg(i + 1);
            return failure;
        }
    }
    return parsePages(pages);
}

CsvCodec::ReadResult PdfStatementReader::parsePages(const QVector<Page>& pages)
{
    CsvCodec::ReadResult result;
    result.encoding = QStringLiteral("PDF");
    result.rows.append({QStringLiteral("Дата операции"), QStringLiteral("Сумма"),
                        QStringLiteral("Описание"), QStringLiteral("Идентификатор операции"),
                        QStringLiteral("Валюта счета")});
    QString documentText;
    QString issuerText;
    // Bank names in transfers and multiline descriptions identify counterparties,
    // not the statement issuer. Read the first-page preamble and signature captions.
    const QRegularExpression signatureRe(QStringLiteral(
        "^\\(?\\s*(?:(?:подпись|ф\\.?\\s*и\\.?\\s*о\\.?)\\s+сотрудника|уполномоченн(?:ое|ый)\\s+лицо)"),
        QRegularExpression::CaseInsensitiveOption);
    for (qsizetype pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        bool preamble = pageIndex == 0;
        for (const auto& line : lines(pages[pageIndex])) {
            const QString full = text(line);
            documentText += full + QLatin1Char('\n');
            if (dateRe.match(full).hasMatch()) preamble = false;
            if (preamble || signatureRe.match(full).hasMatch())
                issuerText += full + QLatin1Char('\n');
        }
    }
    const bool alfa = issuerText.contains(QRegularExpression(QStringLiteral("альфа[ -]?банк"), QRegularExpression::CaseInsensitiveOption));
    const bool sber = issuerText.contains(QStringLiteral("сбер"), Qt::CaseInsensitive);
    if (alfa == sber) {
        result.error = QStringLiteral("Банк не определён однозначно. Поддерживаются выписки Альфа-Банка и Сбера.");
        return result;
    }
    result.encoding = alfa ? QStringLiteral("PDF · Альфа-Банк") : QStringLiteral("PDF · Сбер");
    QString accountCurrency;
    const QRegularExpression currencyHeader(QStringLiteral("валюта\\s+(?:сч[её]та|карты)\\s*[:—-]?\\s*([^\\n]+)"), QRegularExpression::CaseInsensitiveOption);
    const auto cm = currencyHeader.match(documentText);
    if (cm.hasMatch()) accountCurrency = currency(cm.captured(1));
    Header previous;
    Record record;
    bool anyHeader = false;
    const auto finish = [&] {
        if (record.date.isEmpty()) return;
        if (record.pending) { record = {}; return; }
        if (record.currency.isEmpty()) record.currency = accountCurrency;
        // Invalid records stay in the normalized table and are counted as rejected.
        if (record.amountCount != 1 || record.currency.isEmpty()) record.amount.clear();
        result.rows.append({record.date, record.amount, record.description.simplified(), record.id, record.currency});
        record = {};
    };
    for (const auto& page : pages) {
        const auto pageLines = lines(page);
        Header header = previous;
        bool active = header.date >= 0 && !record.date.isEmpty();
        for (int i = 0; i < pageLines.size(); ++i) {
            const auto& line = pageLines[i];
            const QString full = text(line);
            const bool looksHeader = full.contains(QStringLiteral("дата"), Qt::CaseInsensitive)
                && !dateRe.match(full).hasMatch();
            if (looksHeader) {
                Header candidate;
                double endY = line.y;
                for (int j = i; j < pageLines.size() && pageLines[j].y - line.y < 36; ++j) {
                    if (dateRe.match(text(pageLines[j])).hasMatch()) break;
                    for (const auto& w : pageLines[j].words) {
                        QString lower = w.text.toLower();
                        const double x = w.bounds.left();
                        if (lower.startsWith(QStringLiteral("дата")) && candidate.date < 0) candidate.date = x;
                        if (lower.startsWith(QStringLiteral("описан")) || lower.startsWith(QStringLiteral("назначен")) || lower.startsWith(QStringLiteral("содержан"))) candidate.description = x;
                        if (lower.startsWith(QStringLiteral("сумма")) && candidate.amount < 0) candidate.amount = x;
                        if (lower.startsWith(QStringLiteral("приход")) || lower.startsWith(QStringLiteral("зачислен")) || lower.startsWith(QStringLiteral("поступлен")) || lower == QStringLiteral("кредит")) candidate.income = x;
                        if (lower.startsWith(QStringLiteral("расход")) || lower.startsWith(QStringLiteral("списан")) || lower == QStringLiteral("дебет")) candidate.expense = x;
                        if (lower == QStringLiteral("код") || lower.startsWith(QStringLiteral("идентификатор"))) candidate.id = x;
                    }
                    endY = pageLines[j].y;
                }
                if (candidate.date >= 0 && candidate.description >= 0 &&
                    (candidate.amount >= 0 || (candidate.income >= 0 && candidate.expense >= 0))) {
                    header = previous = candidate;
                    active = anyHeader = true;
                    // Header continuation lines are consumed before processing transactions.
                    while (i + 1 < pageLines.size() && pageLines[i + 1].y <= endY) ++i;
                    continue;
                }
            }
            // Some statements repeat only transactions on following pages.
            const auto dm = dateRe.match(text(line, header.date - 4, header.description));
            if (!active && header.date >= 0 && dm.hasMatch()) active = true;
            if (!active) continue;
            const QRegularExpression footer(QStringLiteral("^(?:итого|всего|оборот|исходящий остаток|входящий остаток|подпись|уполномоченное|неподтвержд[её]н|HOLD|страница|сведения об|конец выписки)"), QRegularExpression::CaseInsensitiveOption);
            if (footer.match(full).hasMatch()) {
                if (!full.startsWith(QStringLiteral("Страница"), Qt::CaseInsensitive)) { finish(); active = false; }
                continue;
            }
            if (dm.hasMatch()) {
                finish();
                record.date = dm.captured(1);
                if (header.id >= 0) record.id = text(line, header.id - 3, header.description - 3);
            }
            if (record.date.isEmpty()) continue;
            if (full.contains(QRegularExpression(QStringLiteral("неподтвержд[её]н|в обработке|ожидает подтверждения"), QRegularExpression::CaseInsensitiveOption))) record.pending = true;
            const bool separate = header.income >= 0 && header.expense >= 0;
            const double amountX = separate ? std::min(header.income, header.expense) : header.amount;
            const double amountLeft = std::max(header.description + 60, amountX - 65);
            QString amountText = text(line, amountLeft);
            QStringList amounts;
            if (separate) {
                const double split = (header.income + header.expense) / 2;
                for (const bool income : {true, false}) {
                    const bool onLeft = income ? header.income < header.expense : header.expense < header.income;
                    auto matches = moneyRe.globalMatch(text(line, onLeft ? amountLeft : split, onLeft ? split : 1e9));
                    while (matches.hasNext()) {
                        QString value = matches.next().captured(1).trimmed();
                        QString digits = value; digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
                        if (digits.contains(QRegularExpression(QStringLiteral("[1-9]")))) {
                            value.remove(QRegularExpression(QStringLiteral("^[+\\-−]\\s*")));
                            value.prepend(income ? QLatin1Char('+') : QLatin1Char('-'));
                            amounts.append(value);
                        }
                    }
                }
            } else {
                auto matches = moneyRe.globalMatch(amountText);
                while (matches.hasNext()) amounts.append(matches.next().captured(1).trimmed());
            }
            if (!amounts.isEmpty()) {
                const QString c = currency(amountText);
                if (!c.isEmpty()) {
                    if (!record.currency.isEmpty() && record.currency != c) record.amountCount += 2;
                    record.currency = c;
                }
                if (amounts.size() == 1) {
                    QString value = amounts.first();
                    if (!separate && !value.startsWith(QLatin1Char('+')) && !value.startsWith(QLatin1Char('-')) && !value.startsWith(QChar(0x2212))) {
                        if (alfa) value.prepend(QLatin1Char('+'));
                        else value.clear(); // Sber unsigned amounts need debit/credit columns.
                    }
                    record.amount = value;
                }
                record.amountCount += amounts.size();
            }
            const QString description = text(line, header.description - 3, amountLeft);
            if (!description.isEmpty()) record.description += description + QLatin1Char(' ');
        }
        // Keep a record open for a description continued on the next page.
    }
    finish();
    if (!anyHeader || result.rows.size() == 1)
        result.error = QStringLiteral("Таблица операций не распознана. Нужна текстовая выписка с датой, описанием и суммой со знаком либо столбцами прихода и расхода.");
    return result;
}
