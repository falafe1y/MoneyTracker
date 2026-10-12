#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QDate>
#include <QVariant>

class InvestmentBenchmarkProvider final : public QObject {
    Q_OBJECT
public:
    explicit InvestmentBenchmarkProvider(QObject* parent=nullptr);
    void cancel();
    void load(QVariantMap descriptor, QDate from, QDate to);
signals:
    void succeeded(const QVariantMap& descriptor,const QVariantList& prices,const QDate& from,const QDate& to);
    void failed(const QString& error);
private:
    void page(QVariantMap descriptor,QDate from,QDate to,int start,quint64 generation,QVariantList collected);
    QNetworkAccessManager network_;
    quint64 generation_=0;
};
