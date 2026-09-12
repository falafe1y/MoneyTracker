#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

class InvestmentQuote
{
public:
    InvestmentQuote(
        QString instrumentId,
        qint64 priceMicros,
        QDateTime quotedAtUtc
        )
        : instrumentId_(std::move(instrumentId))
        , priceMicros_(priceMicros)
        , quotedAtUtc_(std::move(quotedAtUtc))
    {
    }

    const QString& instrumentId() const noexcept { return instrumentId_; }
    qint64 priceMicros() const noexcept { return priceMicros_; }
    const QDateTime& quotedAtUtc() const noexcept { return quotedAtUtc_; }

private:
    QString instrumentId_;
    qint64 priceMicros_ = 0;
    QDateTime quotedAtUtc_;
};
