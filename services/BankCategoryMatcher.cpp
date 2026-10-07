#include "BankCategoryMatcher.h"
#include <QRegularExpression>

namespace {
struct MerchantEntry { QString name; QString expression; QStringList categories; };
const QVector<MerchantEntry>& merchants()
{
    static const QVector<MerchantEntry> entries{
        {QStringLiteral("Пятёрочка"), QStringLiteral("пятерочка|pyaterochka|pyateroch|5ka"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("Перекрёсток"), QStringLiteral("перекресток|perekrestok|perekrest"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("Магнит"), QStringLiteral("магнит|magnit"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("Лента"), QStringLiteral("лента|lenta"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("ВкусВилл"), QStringLiteral("вкусвилл|vkusvill"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("Ашан"), QStringLiteral("ашан|auchan"), {QStringLiteral("Продукты"), QStringLiteral("Еда"), QStringLiteral("Продукты питания")}},
        {QStringLiteral("Вкусно — и точка"), QStringLiteral("вкусно и точка|vkusno i tochka"), {QStringLiteral("Кафе и рестораны"), QStringLiteral("Рестораны"), QStringLiteral("Общепит")}},
        {QStringLiteral("Бургер Кинг"), QStringLiteral("бургер кинг|burger king|burgerking"), {QStringLiteral("Кафе и рестораны"), QStringLiteral("Рестораны"), QStringLiteral("Общепит")}},
        {QStringLiteral("Додо Пицца"), QStringLiteral("додо|dodo"), {QStringLiteral("Кафе и рестораны"), QStringLiteral("Рестораны"), QStringLiteral("Общепит")}},
        {QStringLiteral("Лукойл"), QStringLiteral("лукойл|lukoil|lukoyl"), {QStringLiteral("Топливо"), QStringLiteral("Бензин"), QStringLiteral("Автомобиль")}},
        {QStringLiteral("Газпромнефть"), QStringLiteral("газпромнефть|gazpromneft"), {QStringLiteral("Топливо"), QStringLiteral("Бензин"), QStringLiteral("Автомобиль")}},
        {QStringLiteral("Роснефть"), QStringLiteral("роснефть|rosneft"), {QStringLiteral("Топливо"), QStringLiteral("Бензин"), QStringLiteral("Автомобиль")}},
        {QStringLiteral("Ozon"), QStringLiteral("ozon|озон"), {}},
        {QStringLiteral("Wildberries"), QStringLiteral("wildberries|вайлдберриз"), {}},
        {QStringLiteral("Яндекс"), QStringLiteral("яндекс|yandex|yandeks"), {}}
    };
    return entries;
}
bool matches(const QString& text, const QString& expression)
{
    // Unicode token boundaries prevent e.g. MAGNIT from matching MAGNITOGORSK.
    return QRegularExpression(QStringLiteral("(?:^|[^\\p{L}\\p{N}])(?:%1)(?=$|[^\\p{L}\\p{N}])")
        .arg(expression), QRegularExpression::UseUnicodePropertiesOption).match(text).hasMatch();
}
}

QString BankCategoryMatcher::normalize(const QString& text)
{
    QString result = text.normalized(QString::NormalizationForm_KC).toCaseFolded();
    result.replace(QChar(0x0451), QChar(0x0435));
    result.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    return result.simplified();
}

QString BankCategoryMatcher::merchant(const QString& description)
{
    const QString normalized = normalize(description);
    QString found;
    for (const auto& entry : merchants()) {
        if (matches(normalized, entry.expression)) {
            if (!found.isEmpty()) return normalized; // Multiple merchants: do not merge.
            found = entry.name;
        }
    }
    // Unknown merchants keep their complete descriptor, including identifying numbers.
    return found.isEmpty() ? normalized : found;
}

bool BankCategoryMatcher::isSpecialOperation(const QString& description)
{
    return matches(normalize(description), QStringLiteral(
        "перевод|перевода|переводы|transfer|сбп|sbp|возврат|возврата|refund|reversal|снятие|cash|внесение"));
}

BankCategorySuggestion BankCategoryMatcher::suggest(
    const BankCsvOperation& operation, const QVector<Category>& categories,
    const QSet<QString>& archived, const QVector<BankCategoryRule>& rules,
    const QString& fallbackCategoryId)
{
    BankCategorySuggestion result;
    result.merchant = merchant(operation.description);
    const auto type = operation.signedMinor > 0 ? CategoryType::Income : CategoryType::Expense;
    const auto valid = [&](const QString& id) {
        for (const auto& category : categories)
            if (id != QStringLiteral("transfer-in") && id != QStringLiteral("transfer-out") &&
                category.id() == id && category.type() == type && !archived.contains(id)) return true;
        return false;
    };
    const QString descriptor = normalize(operation.description);
    const QString merchantKey = normalize(result.merchant);
    // Exact merchant rules have priority; equally specific conflicts require review.
    for (const QString& mode : {QStringLiteral("exact"), QStringLiteral("contains")}) {
        QString selected;
        bool conflict = false;
        qsizetype specificity = -1;
        for (const auto& rule : rules) {
            if (rule.type != type || rule.matchMode != mode || !valid(rule.categoryId)) continue;
            const QString pattern = normalize(rule.pattern);
            if (pattern.isEmpty()) continue;
            const bool match = mode == QStringLiteral("exact")
                ? (pattern == merchantKey || pattern == descriptor)
                : matches(descriptor, QRegularExpression::escape(pattern));
            if (!match) continue;
            const qsizetype score = mode == QStringLiteral("exact") ? 0 : pattern.size();
            if (score > specificity) { selected = rule.categoryId; specificity = score; conflict = false; }
            else if (score == specificity && selected != rule.categoryId) conflict = true;
        }
        if (conflict) { result.reason = QStringLiteral("Конфликт правил — проверьте категорию"); break; }
        if (!selected.isEmpty()) {
            result.categoryId = selected;
            result.reason = QStringLiteral("Ваше правило");
            result.needsReview = isSpecialOperation(operation.description);
            if (result.needsReview) result.reason += QStringLiteral(" · Проверьте тип операции");
            return result;
        }
    }
    if (result.reason.isEmpty() && !isSpecialOperation(operation.description)) {
        // Bank-provided category names are used only when the match is unambiguous.
        if (!operation.categoryName.isEmpty()) {
            QString selected;
            int count = 0;
            for (const auto& category : categories)
                if (valid(category.id()) &&
                    normalize(category.name()) == normalize(operation.categoryName)) {
                    selected = category.id(); ++count;
                }
            if (count == 1) {
                result.categoryId = selected; result.reason = QStringLiteral("Категория из выписки");
                result.needsReview = false; return result;
            }
        }
        if (type == CategoryType::Expense) {
            QStringList aliases;
            for (const auto& entry : merchants())
                if (entry.name == result.merchant) aliases = entry.categories;
            if (aliases.isEmpty() && matches(descriptor, QStringLiteral("аптека|apteka")))
                aliases = {QStringLiteral("Здоровье"), QStringLiteral("Аптека"), QStringLiteral("Лекарства"), QStringLiteral("Медицина")};
            QString selected;
            int count = 0;
            for (const auto& category : categories) {
                if (!valid(category.id())) continue;
                for (const auto& alias : aliases)
                    if (normalize(category.name()) == normalize(alias)) { selected = category.id(); ++count; break; }
            }
            if (count == 1) {
                result.categoryId = selected; result.reason = QStringLiteral("Справочник магазинов — проверьте предложение");
                // Grocery stores can also sell non-food goods: dictionary suggestions need review.
                return result;
            }
        }
    }
    if (valid(fallbackCategoryId)) result.categoryId = fallbackCategoryId;
    if (result.reason.isEmpty()) result.reason = isSpecialOperation(operation.description)
        ? QStringLiteral("Возможный перевод, возврат или снятие — проверьте тип операции")
        : QStringLiteral("Категория из профиля — проверьте выбор");
    return result;
}
