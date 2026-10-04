#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <QVariantList>
#include <QJsonObject>
#include <QUrlQuery>
#include <QSharedPointer>
#include <functional>

class BybitProvider final : public QObject {
    Q_OBJECT
public:
    explicit BybitProvider(QObject* parent = nullptr, QNetworkAccessManager* manager = nullptr);
    bool refresh(const QString& id, const QString& apiKey, const QString& secret,
                 qint64 fromMs);
    void cancel(const QString& id);
    static QByteArray signature(const QByteArray& timestamp, const QByteArray& key,
                                const QByteArray& secret, const QByteArray& query);
    static QVariantList parseHoldings(const QJsonObject& result);
    static QVariantMap parseOperation(const QJsonObject& row, bool funding);
signals:
    void snapshotReady(const QString& id, const QVariantList& holdings,
                       const QVariantList& operations, qint64 fetchedAtMs);
    void finished(const QString& id, const QString& error);
private:
    struct Sync;
    void get(const QSharedPointer<Sync>& sync, const QString& path,
             const QUrlQuery& query, std::function<void(const QJsonObject&)> done,
             bool authenticated = true);
    void history(const QSharedPointer<Sync>& sync);
    void finish(const QSharedPointer<Sync>& sync, const QString& error = {});
    QNetworkAccessManager defaultNetwork_;
    QNetworkAccessManager* network_;
    QHash<QString, QSharedPointer<Sync>> active_;
};
