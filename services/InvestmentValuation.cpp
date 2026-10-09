#include "InvestmentValuation.h"
#include <cmath>
#include <limits>

InvestmentValuation valueInvestmentPosition(const InvestmentPosition& p,
    const InvestmentQuote* q, const InvestmentTerms& t, qint64 fx)
{
    const auto fail = [](const char* message) { return InvestmentValuation{false, 0, QString::fromUtf8(message)}; };
    const auto& s = p.settings();
    if (p.quantityMicros() <= 0 || (s.direction != 1 && s.direction != -1))
        return fail("Некорректное количество или направление позиции");
    if (s.manualValuation) return {true, s.manualValueMinor, {}};
    if (!q) return fail("Нет котировки; укажите ручную оценку");
    if (fx <= 0) return fail("Нет курса валюты; укажите курс или ручную оценку");
    const long double count = p.quantityMicros() / 1'000'000.0L;
    const long double price = q->priceMicros() / 1'000'000.0L;
    long double amount = 0;
    if (t.pricing == QStringLiteral("bond")) {
        if (price < 0 || t.faceValueMicros <= 0 || t.accruedInterestMicros < 0)
            return fail("Нет текущего номинала или НКД; укажите ручную оценку");
        amount = count * (t.faceValueMicros / 1'000'000.0L * price / 100.0L + t.accruedInterestMicros / 1'000'000.0L);
    } else if (t.pricing == QStringLiteral("future") || t.pricing == QStringLiteral("margined_option")) {
        if (!s.referenceAtUtc.isValid() || t.priceStepMicros <= 0 || t.stepValueMicros <= 0)
            return fail("Нужны цена последнего расчёта, шаг цены и стоимость шага");
        if (q->quotedAtUtc() < s.referenceAtUtc)
            return fail("Котировка старше последнего расчёта; обновите цену");
        amount = count * s.direction * (static_cast<long double>(q->priceMicros()) - s.referencePriceMicros)
            / t.priceStepMicros * t.stepValueMicros / 1'000'000.0L;
    } else if (t.pricing == QStringLiteral("premium_option")) {
        if (price < 0 || t.multiplierMicros <= 0) return fail("Нет размера опционного контракта");
        amount = count * s.direction * price * t.multiplierMicros / 1'000'000.0L;
    } else if (t.pricing == QStringLiteral("unit")) {
        if (price < 0 || t.multiplierMicros <= 0) return fail("Некорректная цена или единица котировки");
        amount = count * price * t.multiplierMicros / 1'000'000.0L;
    } else return fail("Для этого контракта нужна ручная оценка");
    const long double minor = amount * fx / 1'000'000.0L * 100.0L + s.unsettledAdjustmentMinor;
    if (!std::isfinite(minor) || minor >= static_cast<long double>(std::numeric_limits<qint64>::max())
        || minor <= static_cast<long double>(std::numeric_limits<qint64>::min()))
        return fail("Стоимость выходит за допустимый диапазон");
    return {true, static_cast<qint64>(std::round(minor)), {}};
}
