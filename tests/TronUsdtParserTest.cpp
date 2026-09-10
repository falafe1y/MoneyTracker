#include "../services/TronUsdtParser.h"

#include <QtTest>

class TronUsdtParserTest : public QObject
{
    Q_OBJECT

private slots:
    void validatesMainnetAddressAndBuildsAbiParameter();
    void rejectsInvalidChecksum();
    void parsesBalanceResponse();
    void rejectsFailedBalanceResponse();
    void parsesPriceResponse();
};

void TronUsdtParserTest::validatesMainnetAddressAndBuildsAbiParameter()
{
    const QString address =
        QStringLiteral("TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t");
    QVERIFY(isValidTronAddress(address));
    QCOMPARE(
        tronUsdtBalanceParameter(address),
        QStringLiteral(
            "000000000000000000000000"
            "a614f803b6fd780986a42c78ec9c7f77e6ded13c"));
}

void TronUsdtParserTest::rejectsInvalidChecksum()
{
    QVERIFY(!isValidTronAddress(QStringLiteral(
        "TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj61")));
    QVERIFY(!isValidTronAddress(QStringLiteral("0x1234")));
}

void TronUsdtParserTest::parsesBalanceResponse()
{
    const QByteArray response = R"({
        "result":{"result":true},
        "constant_result":[
            "33b03b2c0"
        ]
    })";
    qint64 balanceAtomic = 0;
    QString error;
    QVERIFY2(parseTronUsdtBalanceResponse(
                 response, balanceAtomic, &error), qPrintable(error));
    QCOMPARE(balanceAtomic, qint64(13'875'000'000));
}

void TronUsdtParserTest::rejectsFailedBalanceResponse()
{
    qint64 balanceAtomic = 0;
    QVERIFY(!parseTronUsdtBalanceResponse(
        QByteArrayLiteral(R"({"result":{"result":false}})"),
        balanceAtomic));
}

void TronUsdtParserTest::parsesPriceResponse()
{
    qint64 priceUsdMicros = 0;
    QString error;
    QVERIFY2(parseCoinGeckoUsdtPriceResponse(
                 QByteArrayLiteral(
                     R"({"tether":{"usd":0.999731,"last_updated_at":1}})"),
                 priceUsdMicros,
                 &error),
             qPrintable(error));
    QCOMPARE(priceUsdMicros, qint64(999'731));
}

QTEST_APPLESS_MAIN(TronUsdtParserTest)

#include "TronUsdtParserTest.moc"
