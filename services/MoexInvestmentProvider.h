#pragma once

#include "../core/InvestmentMarketInstrument.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>

class MoexInvestmentProvider final : public QObject
{
    Q_OBJECT

public:
    explicit MoexInvestmentProvider(QObject* parent = nullptr);

    void search(const QString& query);
    void requestQuote(const InvestmentMarketInstrument& instrument);

signals:
    void searchSucceeded(const QVector<InvestmentMarketInstrument>& instruments);
    void searchFailed(const QString& message);
    void quoteSucceeded(
        const QString& instrumentId,
        qint64 priceMicros,
        const QString& currencyCode,
        const QDateTime& quotedAtUtc
        );
    void quoteFailed(const QString& instrumentId, const QString& message);

private:
    QNetworkAccessManager network_;
};
