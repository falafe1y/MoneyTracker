#include "../services/BybitProvider.h"
#include "../persistence/FinanceRepository.h"
#include <QtTest>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <cstring>

class Reply : public QNetworkReply {
public:
    Reply(const QNetworkRequest& request, const QJsonObject& object, QObject* parent)
        : QNetworkReply(parent), data_(QJsonDocument(object).toJson()) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        QTimer::singleShot(0, this, [this] { setFinished(true); emit readyRead(); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return data_.size() - offset_ + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* destination, qint64 maxSize) override {
        const qint64 count = std::min(maxSize, qint64(data_.size() - offset_));
        if (!count) return -1;
        std::memcpy(destination, data_.constData() + offset_, size_t(count)); offset_ += count; return count;
    }
private:
    QByteArray data_; qint64 offset_ = 0;
};
class Network : public QNetworkAccessManager {
public:
    bool readOnly = true, apiError = false, repeatingCursor = false, validSignatures = true;
    int authenticatedRequests = 0, utaPages = 0, fundingPages = 0;
    bool encodedCursorSeen = false;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice*) override {
        if (operation != GetOperation) validSignatures = false;
        const QString path = request.url().path();
        QJsonObject result;
        if (path == "/v5/market/time") result = {{"timeSecond", QString::number(QDateTime::currentSecsSinceEpoch())}};
        else {
            ++authenticatedRequests;
            validSignatures &= request.rawHeader("X-BAPI-SIGN") == BybitProvider::signature(
                request.rawHeader("X-BAPI-TIMESTAMP"), "test-key", "test-secret", request.url().query(QUrl::FullyEncoded).toUtf8());
            if (path == "/v5/user/query-api") result = {{"readOnly", readOnly ? 1 : 0}};
            else if (path == "/v5/asset/asset-overview") result = QJsonDocument::fromJson(R"({"list":[
                {"accountType":"FundingAccount","totalEquity":"120.25","coinDetail":[{"coin":"USDT","equity":"120.250000"}]},
                {"accountType":"UnifiedTradingAccount","categories":[{"category":"crypto","equity":"50","coinDetail":[{"coin":"BTC","equity":"0.000500000"}]}]}
            ]})").object();
            else if (path == "/v5/account/transaction-log") {
                ++utaPages;
                const auto query = QUrlQuery(request.url());
                const QString cursor = query.queryItemValue("cursor", QUrl::FullyDecoded);
                if (!cursor.isEmpty()) encodedCursorSeen |= cursor == "next:+%/";
                QJsonArray rows{QJsonObject{{"id", "id-1"}, {"currency", "USDT"}, {"change", "-0.000035610000"},
                    {"type", "TRADE"}, {"side", "Buy"}, {"transactionTime", "1700000000000"}, {"symbol", "BTCUSDT"}}};
                if (!cursor.isEmpty()) rows.append(QJsonObject{{"id", "id-2"}, {"currency", "BTC"}, {"change", "0"},
                    {"type", "SETTLEMENT"}, {"transactionTime", "1700000001000"}});
                result = {{"list", rows}, {"nextPageCursor", cursor.isEmpty() || repeatingCursor ? "next:+%/" : ""}};
            } else if (path == "/v5/asset/fundinghistory") {
                ++fundingPages;
                result = {{"list", QJsonArray{QJsonObject{{"currcCursor", "id-1"}, {"currency", "USDT"}, {"txnAmt", "1.500000"},
                    {"ioDirection", "I"}, {"createTime", "1700000000"}, {"descriptionEn", "Deposit"}}}}, {"nextPageCursor", ""}};
            } else validSignatures = false;
        }
        return new Reply(request, {{"retCode", apiError && path != "/v5/market/time" ? 10003 : 0}, {"result", result}}, this);
    }
};
class BybitProviderTest : public QObject {
    Q_OBJECT
private slots:
    void signatureMatchesReference() {
        QCOMPARE(BybitProvider::signature("1700000000000", "test-key", "test-secret", "accountType=UNIFIED&cursor=a%2Bb"),
                 QByteArray("632f2b39d6a8c0b89cfa0b4fa0a144e4a5c12ea2481de3e71934ad94d407fb3a"));
    }
    void followsPagesAndSeparatesAccounts() {
        Network network; BybitProvider provider(nullptr, &network);
        QSignalSpy snapshots(&provider, &BybitProvider::snapshotReady), finished(&provider, &BybitProvider::finished);
        QVERIFY(provider.refresh("a", "test-key", "test-secret", QDateTime::currentMSecsSinceEpoch() - 86400000));
        QVERIFY(!provider.refresh("a", "test-key", "test-secret", 0));
        QVERIFY(finished.count() == 1 || finished.wait(5000)); QCOMPARE(finished.count(), 1);
        QCOMPARE(finished[0][1].toString(), QString()); QCOMPARE(snapshots.count(), 1);
        const auto holdings = snapshots[0][1].toList(); QCOMPARE(holdings.size(), 4);
        QCOMPARE(holdings[1].toMap().value("amountText").toString(), QString("120.25"));
        QCOMPARE(holdings[3].toMap().value("amountText").toString(), QString("0.0005"));
        const auto rows = snapshots[0][2].toList(); QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[0].toMap().value("amountText").toString(), QString("0.00003561"));
        QCOMPARE(rows[1].toMap().value("direction").toString(), QString("neutral"));
        QCOMPARE(rows[2].toMap().value("transactionId").toString(), QString("fund:id-1"));
        QVERIFY(network.validSignatures); QVERIFY(network.encodedCursorSeen);
        QCOMPARE(network.utaPages, 2); QCOMPARE(network.fundingPages, 1);
    }
    void loadsThirtyDaysInSevenDayWindows() {
        Network network; BybitProvider provider(nullptr, &network);
        QSignalSpy finished(&provider, &BybitProvider::finished);
        QVERIFY(provider.refresh("a", "test-key", "test-secret", 0));
        QVERIFY(finished.count() == 1 || finished.wait(5000)); QCOMPARE(finished.count(), 1);
        QCOMPARE(finished[0][1].toString(), QString());
        QCOMPARE(network.utaPages, 10); QCOMPARE(network.fundingPages, 5);
    }
    void rejectsUnsafeKeyAndFailedResponses() {
        for (int mode = 0; mode < 3; ++mode) {
            Network network; network.readOnly = mode != 0; network.apiError = mode == 1; network.repeatingCursor = mode == 2;
            BybitProvider provider(nullptr, &network);
            QSignalSpy snapshots(&provider, &BybitProvider::snapshotReady), finished(&provider, &BybitProvider::finished);
            QVERIFY(provider.refresh("a", "test-key", "test-secret", QDateTime::currentMSecsSinceEpoch() - 86400000));
            QVERIFY(finished.count() == 1 || finished.wait(5000)); QCOMPARE(finished.count(), 1); QVERIFY(!finished[0][1].toString().isEmpty()); QCOMPARE(snapshots.count(), 0);
            if (mode < 2) QCOMPARE(network.authenticatedRequests, 1);
        }
    }
    void cachesMergesAndClearsWithoutSecrets() {
        QTemporaryDir directory; const auto path = directory.filePath("finance.sqlite3");
        QVariantList oldRows{QVariantMap{{"transactionId", "uta:old"}, {"occurredAtMs", qint64(1700000000000)}, {"amountText", "1"}}};
        {
            FinanceRepository repository(path); QVERIFY2(repository.isOpen(), qPrintable(repository.lastError()));
            QVERIFY(repository.saveCryptoExchange({{"id", "a"}, {"name", "Мой Bybit"}}));
            QVERIFY(repository.saveCryptoExchangeSnapshot("a", {}, oldRows, 1700000000000));
            QVariantList updated{QVariantMap{{"transactionId", "uta:old"}, {"occurredAtMs", qint64(1700000000000)}, {"amountText", "2"}},
                                 QVariantMap{{"transactionId", "fund:new"}, {"occurredAtMs", qint64(1700000001000)}, {"amountText", "3"}}};
            QVERIFY(repository.saveCryptoExchangeSnapshot("a", {}, updated, 1700000002000));
            QVERIFY(repository.saveCryptoExchangeSnapshot("a", {}, updated, 1700000002000));
            QCOMPARE(repository.loadCryptoExchanges()[0].toMap().value("history").toList().size(), 2);
            QVERIFY(!repository.saveCryptoExchangeSnapshot("a", {}, {QVariantMap{{"transactionId", "broken"}}}, 1700000003000));
        }
        {
            FinanceRepository repository(path); const auto rows = repository.loadCryptoExchanges(); QCOMPARE(rows.size(), 1);
            QCOMPARE(rows[0].toMap().value("name").toString(), QString("Мой Bybit"));
            QCOMPARE(rows[0].toMap().value("history").toList().size(), 2);
            QVERIFY(repository.deleteHistoryRows({QVariantMap{{"isExchange", true}, {"walletId", "a"}, {"transactionId", "uta:old"}}}));
            QCOMPARE(repository.loadCryptoExchanges()[0].toMap().value("history").toList().size(), 1);
            QVERIFY(repository.clearAllUserData()); QVERIFY(repository.loadCryptoExchanges().isEmpty());
        }
    }
};
QTEST_GUILESS_MAIN(BybitProviderTest)
#include "BybitProviderTest.moc"
