#pragma once

#include "Currency.h"
#include "InvestmentTerms.h"

#include <QString>

#include <utility>

enum class InvestmentInstrumentType {
    Stock,
    Etf,
    Bond,
    Fund,
    Other,
    PreferredStock,
    DepositaryReceipt,
    Metal,
    Currency,
    Future,
    Option
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
        Currency currency,
        QString marketCode = {},
        QString primaryBoardId = {},
        InvestmentTerms terms = {}
        )
        : id_(std::move(id))
        , symbol_(std::move(symbol))
        , isin_(std::move(isin))
        , name_(std::move(name))
        , type_(type)
        , currency_(currency)
        , marketCode_(marketCode.isNull()
              ? QStringLiteral("")
              : std::move(marketCode))
        , primaryBoardId_(primaryBoardId.isNull()
              ? QStringLiteral("")
              : std::move(primaryBoardId))
        , terms_(std::move(terms))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& symbol() const noexcept { return symbol_; }
    const QString& isin() const noexcept { return isin_; }
    const QString& name() const noexcept { return name_; }
    InvestmentInstrumentType type() const noexcept { return type_; }
    Currency currency() const noexcept { return currency_; }
    const QString& marketCode() const noexcept { return marketCode_; }
    const QString& primaryBoardId() const noexcept { return primaryBoardId_; }

    const InvestmentTerms& terms() const noexcept { return terms_; }

private:
    InvestmentTerms terms_;
    QString id_;
    QString symbol_;
    QString isin_;
    QString name_;
    InvestmentInstrumentType type_ = InvestmentInstrumentType::Other;
    Currency currency_ = Currency::RUB;
    QString marketCode_;
    QString primaryBoardId_;
};
