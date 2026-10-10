#include "../core/Money.h"
#include "../services/CurrencyConverter.h"
#include "../services/TestCurrencyRateProvider.h"

#include <QtTest>

class CurrencyConverterTest : public QObject
{
    Q_OBJECT

private slots:
    void convertsRubToUsd();
    void convertsUsdToRub();
    void convertsEurToRub();
    void sameCurrencyDoesNotChange();
    void convertsCnyInBothDirections();
};

void CurrencyConverterTest::convertsRubToUsd()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);

    const Money rub(10'000, Currency::RUB); // 100 RUB

    const Money usd =
        converter.convert(rub, Currency::USD);

    QCOMPARE(usd.currency(), Currency::USD);
    QCOMPARE(usd.minorUnits(), qint64(110));
}

void CurrencyConverterTest::convertsUsdToRub()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);

    const Money usd(100'00, Currency::USD); // 100 USD

    const Money rub =
        converter.convert(usd, Currency::RUB);

    QCOMPARE(rub.currency(), Currency::RUB);
    QCOMPARE(rub.minorUnits(), qint64(909'091));
}

void CurrencyConverterTest::convertsEurToRub()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);

    const Money eur(100'00, Currency::EUR); // 100 EUR

    const Money rub =
        converter.convert(eur, Currency::RUB);

    QCOMPARE(rub.currency(), Currency::RUB);
    QCOMPARE(rub.minorUnits(), qint64(1'063'636));
}

void CurrencyConverterTest::sameCurrencyDoesNotChange()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);

    const Money original(123'45, Currency::RUB);

    const Money converted =
        converter.convert(original, Currency::RUB);

    QCOMPARE(converted.currency(), Currency::RUB);
    QCOMPARE(converted.minorUnits(), original.minorUnits());
}

void CurrencyConverterTest::convertsCnyInBothDirections()
{
    TestCurrencyRateProvider provider;
    CurrencyConverter converter(provider);
    const Money cny(10000,Currency::CNY);
    const auto rub=converter.convert(cny,Currency::RUB);
    QCOMPARE(rub.minorUnits(),qint64(125000));
    QCOMPARE(converter.convert(rub,Currency::CNY).minorUnits(),cny.minorUnits());
    QCOMPARE(converter.convert(cny,Currency::USD).minorUnits(),qint64(1375));
    QCOMPARE(converter.convert(cny,Currency::CNY).minorUnits(),cny.minorUnits());
    QCOMPARE(currencyCode(Currency::CNY),QString("CNY"));
    QCOMPARE(currencySymbol(Currency::CNY),QString("¥"));
}

QTEST_MAIN(CurrencyConverterTest)

#include "CurrencyConverterTest.moc"