#include "CbrFxHistory.h"

#include <QXmlStreamReader>

#include <cmath>

QString cbrUsdHistoryUrl(QDate from, QDate to)
{
    return QStringLiteral("https://www.cbr.ru/scripts/XML_dynamic.asp?date_req1=%1&date_req2=%2&VAL_NM_RQ=R01235")
        .arg(from.toString(QStringLiteral("dd/MM/yyyy")), to.toString(QStringLiteral("dd/MM/yyyy")));
}

QVector<HistoricalFxRate> parseCbrUsdHistory(const QByteArray& xml, QDateTime observedAtUtc)
{
    QVector<HistoricalFxRate> rates;
    QXmlStreamReader reader(xml);
    QDate date;
    qint64 nominal = 0;
    double value = 0;
    QString field;
    bool inRecord = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("Record")) {
            date = QDate::fromString(reader.attributes().value(QStringLiteral("Date")).toString(), QStringLiteral("dd.MM.yyyy"));
            nominal = 0;
            value = 0;
            inRecord = true;
        } else if (reader.isStartElement() && inRecord) {
            field = reader.name().toString();
        } else if (reader.isCharacters() && inRecord) {
            QString text = reader.text().toString().trimmed();
            text.replace(QLatin1Char(','), QLatin1Char('.'));
            if (field == QStringLiteral("Nominal"))
                nominal = text.toLongLong();
            else if (field == QStringLiteral("Value"))
                value = text.toDouble();
        } else if (reader.isEndElement() && reader.name() == QStringLiteral("Record")) {
            // Value is the price of `nominal` dollars: convert it to one dollar in micros
            if (inRecord && date.isValid() && nominal > 0 && value > 0) {
                rates.append(HistoricalFxRate{QStringLiteral("USD"), QStringLiteral("CBR"), date, observedAtUtc,
                                              static_cast<qint64>(std::llround(value / static_cast<double>(nominal) * 1e6))});
            }
            inRecord = false;
        }
    }
    return rates;
}
