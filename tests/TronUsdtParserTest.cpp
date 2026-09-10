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
    void parsesConfirmedUsdtTransactions();
    void rejectsTransactionsForAnotherToken();
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

void TronUsdtParserTest::parsesConfirmedUsdtTransactions()
{
    const QByteArray response = R"({
        "data":[{
            "transaction_id":"abc123",
            "token_info":{
                "address":"TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t",
                "decimals":6,
                "symbol":"USDT"
            },
            "block_timestamp":1788100000000,
            "from":"TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t",
            "to":"TUoHaVjx7n5xz8LwPRDckgFrDWhMhuSuJM",
            "type":"Transfer",
            "value":"120650000"
        }],
        "success":true,
        "meta":{}
    })";
    QVector<CryptoTransaction> transactions;
    QString error;
    QVERIFY2(parseTronUsdtTransactionsResponse(
                 response,
                 QStringLiteral("wallet-1"),
                 transactions,
                 &error),
             qPrintable(error));
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.constFirst().walletId(), QStringLiteral("wallet-1"));
    QCOMPARE(transactions.constFirst().transactionId(), QStringLiteral("abc123"));
    QCOMPARE(transactions.constFirst().amountAtomic(), qint64(120'650'000));
    QCOMPARE(
        transactions.constFirst().occurredAtUtc().toMSecsSinceEpoch(),
        qint64(1'788'100'000'000));
}

void TronUsdtParserTest::rejectsTransactionsForAnotherToken()
{
    const QByteArray response = R"({
        "data":[{
            "transaction_id":"wrong-token",
            "token_info":{
                "address":"TUoHaVjx7n5xz8LwPRDckgFrDWhMhuSuJM",
                "decimals":6
            },
            "block_timestamp":1788100000000,
            "from":"TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t",
            "to":"TUoHaVjx7n5xz8LwPRDckgFrDWhMhuSuJM",
            "value":"1"
        }],
        "success":true
    })";
    QVector<CryptoTransaction> transactions;
    QVERIFY(!parseTronUsdtTransactionsResponse(
        response, QStringLiteral("wallet-1"), transactions));
    QVERIFY(transactions.isEmpty());
}

QTEST_APPLESS_MAIN(TronUsdtParserTest)

#include "TronUsdtParserTest.moc"
