#pragma once

#include "InvestmentTerms.h"

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
        QDateTime quotedAtUtc,
        InvestmentTerms terms = {}
        )
        : instrumentId_(std::move(instrumentId))
        , priceMicros_(priceMicros)
        , quotedAtUtc_(std::move(quotedAtUtc))
        , terms_(std::move(terms))
    {
    }

    const QString& instrumentId() const noexcept { return instrumentId_; }
    qint64 priceMicros() const noexcept { return priceMicros_; }
    const QDateTime& quotedAtUtc() const noexcept { return quotedAtUtc_; }

    const InvestmentTerms& terms() const noexcept { return terms_; }

private:
    InvestmentTerms terms_;
    QString instrumentId_;
    qint64 priceMicros_ = 0;
    QDateTime quotedAtUtc_;
};
