#pragma once

#include "InvestmentInstrument.h"

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

class InvestmentMarketInstrument
{
public:
    InvestmentMarketInstrument(
        QString id,
        QString symbol,
        QString isin,
        QString name,
        InvestmentInstrumentType type,
        QString primaryBoardId
        )
        : id_(std::move(id))
        , symbol_(std::move(symbol))
        , isin_(std::move(isin))
        , name_(std::move(name))
        , type_(type)
        , primaryBoardId_(std::move(primaryBoardId))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& symbol() const noexcept { return symbol_; }
    const QString& isin() const noexcept { return isin_; }
    const QString& name() const noexcept { return name_; }
    InvestmentInstrumentType type() const noexcept { return type_; }
    const QString& primaryBoardId() const noexcept { return primaryBoardId_; }
    qint64 priceMicros() const noexcept { return priceMicros_; }
    const QString& currencyCode() const noexcept { return currencyCode_; }
    const QDateTime& quotedAtUtc() const noexcept { return quotedAtUtc_; }

    void setQuote(
        const qint64 priceMicros,
        QString currencyCode,
        QDateTime quotedAtUtc
        )
    {
        priceMicros_ = priceMicros;
        currencyCode_ = std::move(currencyCode);
        quotedAtUtc_ = std::move(quotedAtUtc);
    }

private:
    QString id_;
    QString symbol_;
    QString isin_;
    QString name_;
    InvestmentInstrumentType type_ = InvestmentInstrumentType::Other;
    QString primaryBoardId_;
    qint64 priceMicros_ = 0;
    QString currencyCode_;
    QDateTime quotedAtUtc_;
};
