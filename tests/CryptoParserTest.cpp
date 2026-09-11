#include "../services/CryptoParser.h"

#include <QtTest>

class CryptoParserTest : public QObject
{
    Q_OBJECT

private slots:
    void validatesBitcoinMainnetAddresses();
    void rejectsInvalidBitcoinAddresses();
    void validatesEthereumMainnetAddresses();
    void parsesBitcoinBalanceAndTransactions();
    void parsesEthereumBalanceAndTransactions();
    void parsesAllCoinGeckoPrices();
};

void CryptoParserTest::validatesBitcoinMainnetAddresses()
{
    QVERIFY(isValidBitcoinAddress(QStringLiteral(
        "1BoatSLRHtKNngkdXEeobR76b53LETtpyT")));
    QVERIFY(isValidBitcoinAddress(QStringLiteral(
        "3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy")));
    QVERIFY(isValidBitcoinAddress(QStringLiteral(
        "bc1q98xg7qagqce24ft2alxwgk34m0xk5ya5qs9s8j")));
    QVERIFY(isValidBitcoinAddress(QStringLiteral(
        "bc1psthd7lpnxvugel99ahw3uemd4xwetkt35j3mrcqhucvd3ll2ufxshpnv9p")));
    QCOMPARE(
        normalizeBitcoinAddress(QStringLiteral(
            "BC1Q98XG7QAGQCE24FT2ALXWGK34M0XK5YA5QS9S8J")),
        QStringLiteral("bc1q98xg7qagqce24ft2alxwgk34m0xk5ya5qs9s8j"));
    QCOMPARE(
        normalizeBitcoinAddress(QStringLiteral(
            "1BoatSLRHtKNngkdXEeobR76b53LETtpyT")),
        QStringLiteral("1BoatSLRHtKNngkdXEeobR76b53LETtpyT"));
}

void CryptoParserTest::rejectsInvalidBitcoinAddresses()
{
    QVERIFY(!isValidBitcoinAddress(QStringLiteral(
        "1BoatSLRHtKNngkdXEeobR76b53LETtpy1")));
    QVERIFY(!isValidBitcoinAddress(QStringLiteral(
        "11BoatSLRHtKNngkdXEeobR76b53LETtpyT")));
    QVERIFY(!isValidBitcoinAddress(QStringLiteral(
        "tb1qfmvlgf4v6c6h3p65cvnl4j36tdcw0m9yvkqv76")));
    QVERIFY(!isValidBitcoinAddress(QStringLiteral(
        "bC1qw508d6qejxtdg4y5r3zarvary0c5xw7kygt080")));
}

void CryptoParserTest::validatesEthereumMainnetAddresses()
{
    QVERIFY(isValidEthereumAddress(QStringLiteral(
        "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045")));
    QCOMPARE(
        normalizeEthereumAddress(QStringLiteral(
            "0xd8dA6BF26964aF9D7eEd9e03E53415D37aA96045")),
        QStringLiteral("0xd8da6bf26964af9d7eed9e03e53415d37aa96045"));
    QVERIFY(!isValidEthereumAddress(QStringLiteral("0x1234")));
    QVERIFY(!isValidEthereumAddress(QStringLiteral(
        "0xg8dA6BF26964aF9D7eEd9e03E53415D37aA96045")));
}

void CryptoParserTest::parsesBitcoinBalanceAndTransactions()
{
    qint64 balance = 0;
    QString error;
    QVERIFY2(parseBitcoinBalanceResponse(
        QByteArrayLiteral(R"({
            "chain_stats":{"funded_txo_sum":175000000,"spent_txo_sum":25000000},
            "mempool_stats":{"funded_txo_sum":1000,"spent_txo_sum":0}
        })"), balance, &error), qPrintable(error));
    QCOMPARE(balance, qint64(150'000'000));

    const QString wallet = QStringLiteral(
        "1BoatSLRHtKNngkdXEeobR76b53LETtpyT");
    const QByteArray response = R"([{
        "txid":"04a8c6566baa29ddea91e9d259e9eff5ee9cb08369186394e30da9df5e0ebc15",
        "vin":[{"prevout":{"scriptpubkey_address":"1BoatSLRHtKNngkdXEeobR76b53LETtpyT","value":100000}}],
        "vout":[
            {"scriptpubkey_address":"bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kygt080","value":60000},
            {"scriptpubkey_address":"1BoatSLRHtKNngkdXEeobR76b53LETtpyT","value":39000}
        ],
        "status":{"confirmed":true,"block_time":1788100000}
    }])";
    QVector<CryptoTransaction> transactions;
    QVERIFY2(parseBitcoinTransactionsResponse(
        response, QStringLiteral("btc-wallet"), wallet, transactions, &error),
        qPrintable(error));
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.constFirst().amountAtomic(), qint64(61'000));
    QCOMPARE(transactions.constFirst().fromAddress(), wallet);
}

void CryptoParserTest::parsesEthereumBalanceAndTransactions()
{
    qint64 balance = 0;
    QString error;
    QVERIFY2(parseEthereumBalanceResponse(
        QByteArrayLiteral(
            R"({"message":"OK","result":"12345678901234567890","status":"1"})"),
        balance,
        &error),
        qPrintable(error));
    QCOMPARE(balance, qint64(1'234'567'890));

    const QString wallet = QStringLiteral(
        "0xd8da6bf26964af9d7eed9e03e53415d37aa96045");
    const QByteArray response = R"({
        "message":"OK",
        "status":"1",
        "result":[{
            "hash":"0x18fbf4798992552d03c80a474f2c5b42dfe67a1bbfcba6cacec93268f083cbab",
            "from":"0xfd8d904767176d9f2feb7305e4714dc89758dc48",
            "to":"0xd8da6bf26964af9d7eed9e03e53415d37aa96045",
            "value":"1500000000000000000",
            "timeStamp":"1788100000",
            "confirmations":"42",
            "isError":"0",
            "txreceipt_status":"1"
        }]
    })";
    QVector<CryptoTransaction> transactions;
    QVERIFY2(parseEthereumTransactionsResponse(
        response, QStringLiteral("eth-wallet"), wallet, transactions, &error),
        qPrintable(error));
    QCOMPARE(transactions.size(), 1);
    QCOMPARE(transactions.constFirst().amountAtomic(), qint64(150'000'000));
    QCOMPARE(transactions.constFirst().toAddress(), wallet);

    QVERIFY2(parseEthereumTransactionsResponse(
        QByteArrayLiteral(
            R"({"message":"No transactions found","result":[],"status":"0"})"),
        QStringLiteral("eth-wallet"),
        wallet,
        transactions,
        &error),
        qPrintable(error));
    QVERIFY(transactions.isEmpty());
}

void CryptoParserTest::parsesAllCoinGeckoPrices()
{
    QHash<QString, qint64> prices;
    QString error;
    QVERIFY2(parseCoinGeckoPricesResponse(
        QByteArrayLiteral(R"({
            "tether":{"usd":0.999731},
            "bitcoin":{"usd":112345.125},
            "ethereum":{"usd":4567.25}
        })"), prices, &error), qPrintable(error));
    QCOMPARE(prices.value(QStringLiteral("USDT")), qint64(999'731));
    QCOMPARE(prices.value(QStringLiteral("BTC")), qint64(112'345'125'000));
    QCOMPARE(prices.value(QStringLiteral("ETH")), qint64(4'567'250'000));
}

QTEST_APPLESS_MAIN(CryptoParserTest)

#include "CryptoParserTest.moc"
