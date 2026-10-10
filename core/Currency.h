#pragma once

#include <QString>

enum class Currency {
    RUB,
    USD,
    EUR,
    CNY
};

inline constexpr int kCurrencyCount = 4;
inline constexpr double kDefaultCnyToRubRate = 12.5;

QString currencyCode(Currency currency);
QString currencySymbol(Currency currency);
int currencyFractionDigits(Currency currency);