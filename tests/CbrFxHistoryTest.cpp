#include "../services/CbrFxHistory.h"
#include <QtTest>

class CbrFxHistoryTest : public QObject {
    Q_OBJECT
private slots:
    void urlAsksForDollarRatesOfThePeriod()
    {
        const QString url = cbrUsdHistoryUrl(QDate(2024, 10, 16), QDate(2026, 10, 16));
        QVERIFY(url.startsWith(QStringLiteral("https://www.cbr.ru/scripts/XML_dynamic.asp?")));
        QVERIFY(url.contains(QStringLiteral("date_req1=16/10/2024&date_req2=16/10/2026&VAL_NM_RQ=R01235")));
    }

    void recordsBecomeDollarRatesInMicros()
    {
        const QByteArray xml =
            "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n"
            "<ValCurs ID=\"R01235\" DateRange1=\"15.10.2026\" DateRange2=\"16.10.2026\" name=\"ForeignCurrency\">\n"
            "<Record Date=\"15.10.2026\" Id=\"R01235\"><Nominal>1</Nominal><Value>96,4231</Value><VunitRate>96,4231</VunitRate></Record>\n"
            "<Record Date=\"16.10.2026\" Id=\"R01235\"><Nominal>1</Nominal><Value>97,0500</Value><VunitRate>97,0500</VunitRate></Record>\n"
            "</ValCurs>\n";
        const QDateTime observed = QDateTime(QDate(2026, 10, 16), QTime(9, 0), QTimeZone::UTC);
        const auto rates = parseCbrUsdHistory(xml, observed);
        QCOMPARE(rates.size(), 2);
        QCOMPARE(rates.at(0).currency, QStringLiteral("USD"));
        QCOMPARE(rates.at(0).source, QStringLiteral("CBR"));
        QCOMPARE(rates.at(0).rateDate, QDate(2026, 10, 15));
        QCOMPARE(rates.at(0).rubPerUnitMicros, qint64(96'423'100));
        QCOMPARE(rates.at(1).rateDate, QDate(2026, 10, 16));
        QCOMPARE(rates.at(1).rubPerUnitMicros, qint64(97'050'000));
        QCOMPARE(rates.at(1).observedAtUtc, observed);
    }

    void nominalScalesTheRate()
    {
        // Ten units of the currency cost 9 643,10 roubles: one unit is 964,31
        const QByteArray xml =
            "<ValCurs><Record Date=\"01.10.2026\" Id=\"R01235\"><Nominal>10</Nominal><Value>9643,10</Value></Record></ValCurs>";
        const auto rates = parseCbrUsdHistory(xml, QDateTime::currentDateTimeUtc());
        QCOMPARE(rates.size(), 1);
        QCOMPARE(rates.at(0).rubPerUnitMicros, qint64(964'310'000));
    }

    void brokenRecordsAreSkipped()
    {
        const QByteArray xml =
            "<ValCurs>"
            "<Record Date=\"not-a-date\"><Nominal>1</Nominal><Value>90,0</Value></Record>"
            "<Record Date=\"02.10.2026\"><Nominal>1</Nominal><Value>0</Value></Record>"
            "</ValCurs>";
        QVERIFY(parseCbrUsdHistory(xml, QDateTime::currentDateTimeUtc()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(CbrFxHistoryTest)
#include "CbrFxHistoryTest.moc"
