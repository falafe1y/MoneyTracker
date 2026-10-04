#include "BybitProvider.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <algorithm>

namespace {
QString decimal(QString value) {
    if (value.isEmpty()) return QStringLiteral("0");
    if (value.contains('.')) {
        while (value.endsWith('0')) value.chop(1);
        if (value.endsWith('.')) value.chop(1);
    }
    return value == QStringLiteral("-0") ? QStringLiteral("0") : value;
}
bool validDecimal(const QString& value) {
    static const QRegularExpression format(QStringLiteral("^[+-]?[0-9]+(?:\\.[0-9]+)?$"));
    return format.match(value).hasMatch();
}
bool nonzero(const QString& value) {
    return value.contains(QRegularExpression(QStringLiteral("[1-9]")));
}
QString operationName(const QString& type, const QString& side) {
    if (type == "TRADE") return side == "Buy" ? QObject::tr("Покупка") : side == "Sell" ? QObject::tr("Продажа") : QObject::tr("Сделка");
    if (type == "TRANSFER_IN") return QObject::tr("Перевод на счёт");
    if (type == "TRANSFER_OUT") return QObject::tr("Перевод со счёта");
    if (type == "SETTLEMENT") return QObject::tr("Расчёт по позиции");
    if (type == "INTEREST") return QObject::tr("Проценты");
    if (type == "BONUS") return QObject::tr("Бонус");
    if (type == "FEE_REFUND") return QObject::tr("Возврат комиссии");
    if (type == "CURRENCY_BUY") return QObject::tr("Покупка валюты");
    if (type == "CURRENCY_SELL") return QObject::tr("Продажа валюты");
    if (type == "LIQUIDATION") return QObject::tr("Ликвидация");
    return QObject::tr("Операция") + " · " + type;
}
}
struct BybitProvider::Sync {
    QString id;
    QByteArray key, secret;
    QVariantList holdings, operations;
    QSet<QString> operationIds, cursors;
    qint64 fromMs = 0, untilMs = 0, windowFrom = 0, windowTo = 0, offsetMs = 0;
    QString cursor;
    bool funding = false, canceled = false;
    int pages = 0;
    QElapsedTimer elapsed;
};
BybitProvider::BybitProvider(QObject* parent, QNetworkAccessManager* manager)
    : QObject(parent), network_(manager ? manager : &defaultNetwork_) {}
QByteArray BybitProvider::signature(const QByteArray& timestamp, const QByteArray& key,
                                    const QByteArray& secret, const QByteArray& query) {
    return QMessageAuthenticationCode::hash(timestamp + key + "10000" + query,
                                            secret, QCryptographicHash::Sha256).toHex();
}
bool BybitProvider::refresh(const QString& id, const QString& key, const QString& secret, qint64 fromMs) {
    if (active_.contains(id) || key.isEmpty() || secret.isEmpty()) return false;
    auto sync = QSharedPointer<Sync>::create();
    sync->id = id; sync->key = key.toUtf8(); sync->secret = secret.toUtf8();
    sync->untilMs = QDateTime::currentMSecsSinceEpoch();
    sync->fromMs = std::max(fromMs, sync->untilMs - 30LL * 86400000);
    sync->elapsed.start(); active_.insert(id, sync);
    const qint64 sentAt = QDateTime::currentMSecsSinceEpoch();
    get(sync, "/v5/market/time", {}, [this, sync, sentAt](const QJsonObject& result) {
        bool ok = false;
        const qint64 seconds = result.value("timeSecond").toString().toLongLong(&ok);
        if (!ok) { finish(sync, tr("Bybit вернул некорректное время сервера")); return; }
        sync->offsetMs = seconds * 1000 - (sentAt + QDateTime::currentMSecsSinceEpoch()) / 2;
        get(sync, "/v5/user/query-api", {}, [this, sync](const QJsonObject& info) {
            if (info.value("readOnly").toInt(-1) != 1) {
                finish(sync, tr("Создайте в Bybit ключ API с доступом только на чтение")); return;
            }
            QUrlQuery query; query.addQueryItem("valuationCurrency", "USD");
            get(sync, "/v5/asset/asset-overview", query, [this, sync](const QJsonObject& result) {
                if (!result.value("list").isArray()) {
                    finish(sync, tr("Bybit вернул некорректный список остатков")); return;
                }
                sync->holdings = parseHoldings(result);
                for (const auto& value : sync->holdings) {
                    const auto row = value.toMap();
                    const QString amount = row.value(row.value("summary").toBool() ? "usdValue" : "amountText").toString();
                    if (!validDecimal(amount) || row.value("coin").toString().isEmpty()) {
                        finish(sync, tr("Не удалось прочитать остатки Bybit")); return;
                    }
                }
                sync->windowFrom = sync->fromMs;
                sync->windowTo = std::min(sync->untilMs, sync->windowFrom + 7LL * 86400000 - 1);
                history(sync);
            });
        });
    }, false);
    return true;
}
void BybitProvider::cancel(const QString& id) {
    if (auto sync = active_.take(id)) { sync->canceled = true; sync->secret.fill('\0'); }
}
void BybitProvider::finish(const QSharedPointer<Sync>& sync, const QString& error) {
    if (sync->canceled) return;
    sync->canceled = true;
    active_.remove(sync->id); sync->secret.fill('\0'); sync->key.clear();
    emit finished(sync->id, error);
}
void BybitProvider::get(const QSharedPointer<Sync>& sync, const QString& path,
                        const QUrlQuery& query, std::function<void(const QJsonObject&)> done,
                        bool authenticated) {
    if (sync->canceled) return;
    if (sync->elapsed.elapsed() > 120000) { finish(sync, tr("Загрузка Bybit заняла слишком много времени. Повторите обновление")); return; }
    QUrl url("https://api.bybit.com" + path); url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    if (authenticated) {
        const QByteArray timestamp = QByteArray::number(QDateTime::currentMSecsSinceEpoch() + sync->offsetMs);
        request.setRawHeader("X-BAPI-API-KEY", sync->key);
        request.setRawHeader("X-BAPI-TIMESTAMP", timestamp);
        request.setRawHeader("X-BAPI-RECV-WINDOW", "10000");
        request.setRawHeader("X-BAPI-SIGN", signature(timestamp, sync->key, sync->secret, url.query(QUrl::FullyEncoded).toUtf8()));
    }
    auto* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, sync, reply, done = std::move(done)] {
        const QByteArray bytes = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error(); reply->deleteLater();
        if (sync->canceled) return;
        if (networkError != QNetworkReply::NoError || status != 200) {
            finish(sync, tr("Не удалось получить данные Bybit (HTTP %1). Проверьте подключение и доступность биржи").arg(status)); return;
        }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        const auto root = doc.object();
        if (error.error != QJsonParseError::NoError || !root.contains("retCode")) {
            finish(sync, tr("Некорректный ответ Bybit")); return;
        }
        const int code = root.value("retCode").toInt(-1);
        if (code != 0) {
            // Do not expose the response: it may contain API key information.
            QString message = tr("Ошибка Bybit: %1").arg(code);
            if (code == 10003 || code == 10004 || code == 33004) message += tr(". Проверьте ключ и секретный ключ API");
            if (code == 10005) message += tr(". У ключа недостаточно прав на чтение остатков или истории");
            if (code == 10002) message += tr(". Время запроса не совпадает с временем сервера; повторите обновление");
            if (code == 10006) message += tr(". Превышен лимит запросов; повторите позднее");
            finish(sync, message); return;
        }
        done(root.value("result").toObject());
    });
}
QVariantList BybitProvider::parseHoldings(const QJsonObject& result) {
    QVariantList rows;
    for (const auto& entry : result.value("list").toArray()) {
        const auto account = entry.toObject();
        const QString section = account.value("accountType").toString();
        auto append = [&](const QJsonArray& coins, const QString& category, const QString& equity) {
            // A separate summary row preserves the exchange's own valuation without repricing coins.
            rows.append(QVariantMap{{"section", section}, {"category", category}, {"summary", true},
                                    {"usdValue", equity}, {"coin", "USD"}});
            for (const auto& value : coins) {
                const auto coin = value.toObject();
                const QString amount = decimal(coin.value("equity").toString());
                if (!nonzero(amount)) continue;
                rows.append(QVariantMap{{"section", section}, {"category", category}, {"summary", false},
                                        {"coin", coin.value("coin").toString()}, {"amountText", amount}});
            }
        };
        if (account.value("categories").isArray() && !account.value("categories").toArray().isEmpty()) {
            for (const auto& value : account.value("categories").toArray()) {
                const auto category = value.toObject();
                append(category.value("coinDetail").toArray(), category.value("category").toString(), category.value("equity").toString());
            }
        } else append(account.value("coinDetail").toArray(), {}, account.value("totalEquity").toString());
    }
    return rows;
}
QVariantMap BybitProvider::parseOperation(const QJsonObject& row, bool funding) {
    const QString rawAmount = row.value(funding ? "txnAmt" : "change").toString();
    if (!validDecimal(rawAmount)) return {};
    if (funding && row.value("ioDirection").toString() != "I" && row.value("ioDirection").toString() != "O") return {};
    QString amount = decimal(rawAmount);
    const bool negative = funding ? row.value("ioDirection").toString() == "O" : amount.startsWith('-');
    if (amount.startsWith('-') || amount.startsWith('+')) amount.remove(0, 1);
    const QString rawId = row.value(funding ? "currcCursor" : "id").toString();
    bool validTime = false;
    const qint64 time = row.value(funding ? "createTime" : "transactionTime").toString().toLongLong(&validTime) * (funding ? 1000 : 1);
    if (rawId.isEmpty() || !validTime || time <= 0 || row.value("currency").toString().isEmpty()) return {};
    QString description = funding ? row.value("descriptionEn").toString()
                                  : operationName(row.value("type").toString(), row.value("side").toString());
    if (funding && description.isEmpty()) description = row.value("showBusiTypeEn").toString();
    if (funding && description.isEmpty()) description = QObject::tr("Перевод");
    const QString pair = row.value("symbol").toString();
    if (!pair.isEmpty()) description += " · " + pair;
    return {{"transactionId", (funding ? "fund:" : "uta:") + rawId}, {"isExchange", true},
            {"direction", nonzero(amount) ? (negative ? "out" : "in") : "neutral"},
            {"amountText", amount}, {"symbol", row.value("currency").toString()}, {"occurredAtMs", time},
            {"counterparty", description}, {"section", funding ? "FundingAccount" : "UnifiedTradingAccount"},
            {"cashBalance", row.value(funding ? "afterAmt" : "cashBalance").toString()}};
}
void BybitProvider::history(const QSharedPointer<Sync>& sync) {
    QUrlQuery query;
    if (sync->funding) {
        query.addQueryItem("createTimeFrom", QString::number(sync->windowFrom / 1000));
        query.addQueryItem("createTimeTo", QString::number(sync->windowTo / 1000));
        query.addQueryItem("limit", "100");
    } else {
        query.addQueryItem("accountType", "UNIFIED");
        query.addQueryItem("startTime", QString::number(sync->windowFrom));
        query.addQueryItem("endTime", QString::number(sync->windowTo));
        query.addQueryItem("limit", "50");
    }
    if (!sync->cursor.isEmpty()) query.addQueryItem("cursor", sync->cursor);
    get(sync, sync->funding ? "/v5/asset/fundinghistory" : "/v5/account/transaction-log", query,
        [this, sync](const QJsonObject& result) {
        if (!result.value("list").isArray()) { finish(sync, tr("Bybit вернул некорректную историю")); return; }
        for (const auto& value : result.value("list").toArray()) {
            auto row = parseOperation(value.toObject(), sync->funding);
            if (row.isEmpty()) { finish(sync, tr("Не удалось прочитать строку истории Bybit")); return; }
            const QString id = row.value("transactionId").toString();
            if (sync->operationIds.contains(id)) continue;
            sync->operationIds.insert(id); sync->operations.append(row);
        }
        sync->cursor = result.value("nextPageCursor").toString();
        if (!sync->cursor.isEmpty()) {
            if (++sync->pages > 200 || sync->cursors.contains(sync->cursor)) {
                finish(sync, tr("История Bybit слишком велика или биржа повторяет страницу. Сохранённая история не изменена")); return;
            }
            sync->cursors.insert(sync->cursor);
        } else {
            sync->cursors.clear(); sync->pages = 0;
            if (sync->windowTo < sync->untilMs) {
                sync->windowFrom = sync->windowTo + 1;
                sync->windowTo = std::min(sync->untilMs, sync->windowFrom + 7LL * 86400000 - 1);
            } else if (!sync->funding) {
                sync->funding = true; sync->windowFrom = sync->fromMs;
                sync->windowTo = std::min(sync->untilMs, sync->windowFrom + 7LL * 86400000 - 1);
            } else {
                emit snapshotReady(sync->id, sync->holdings, sync->operations, sync->untilMs);
                finish(sync); return;
            }
        }
        QTimer::singleShot(120, this, [this, sync] { if (!sync->canceled) history(sync); });
    });
}
