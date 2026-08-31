#include "Currency.h"

QString currencyCode(const Currency currency)
{
    switch (currency) {
    case Currency::RUB:
        return QStringLiteral("RUB");
    case Currency::USD:
        return QStringLiteral("USD");
    case Currency::EUR:
        return QStringLiteral("EUR");
    }

    return {};
}

QString currencySymbol(const Currency currency)
{
    switch (currency) {
    case Currency::RUB:
        return QStringLiteral("₽");
    case Currency::USD:
        return QStringLiteral("$");
    case Currency::EUR:
        return QStringLiteral("€");
    }

    return {};
}

int currencyFractionDigits(const Currency currency)
{
    switch (currency) {
    case Currency::RUB:
    case Currency::USD:
    case Currency::EUR:
        return 2;
    }

    return 2;
}
