#pragma once

#include "Currency.h"

#include <QString>

#include <utility>

enum class InvestmentInstrumentType {
    Stock,
    Etf,
    Bond,
    Fund,
    Other
};

class InvestmentInstrument
{
public:
    InvestmentInstrument(
        QString id,
        QString symbol,
        QString isin,
        QString name,
        InvestmentInstrumentType type,
        Currency currency
        )
        : id_(std::move(id))
        , symbol_(std::move(symbol))
        , isin_(std::move(isin))
        , name_(std::move(name))
        , type_(type)
        , currency_(currency)
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& symbol() const noexcept { return symbol_; }
    const QString& isin() const noexcept { return isin_; }
    const QString& name() const noexcept { return name_; }
    InvestmentInstrumentType type() const noexcept { return type_; }
    Currency currency() const noexcept { return currency_; }

private:
    QString id_;
    QString symbol_;
    QString isin_;
    QString name_;
    InvestmentInstrumentType type_ = InvestmentInstrumentType::Other;
    Currency currency_ = Currency::RUB;
};
