#pragma once
#include "../core/InvestmentPosition.h"
#include "../core/InvestmentQuote.h"

struct InvestmentValuation
{
    bool available = false;
    qint64 valueMinor = 0;
    QString error;
};

// fxRateMicros converts the quote's monetary currency to the account currency.
// A null quote never turns an unknown price into a zero-valued asset.
InvestmentValuation valueInvestmentPosition(const InvestmentPosition& position,
    const InvestmentQuote* quote, const InvestmentTerms& terms, qint64 fxRateMicros);
