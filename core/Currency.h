#pragma once

#include <QString>

enum class Currency {
    RUB,
    USD,
    EUR
};

QString currencyCode(Currency currency);
QString currencySymbol(Currency currency);
int currencyFractionDigits(Currency currency);