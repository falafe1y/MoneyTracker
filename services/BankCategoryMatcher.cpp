#include "BankCategoryMatcher.h"
#include <QRegularExpression>

namespace {
bool matches(const QString& text, const QString& expression)
{
    // Unicode token boundaries prevent e.g. MAGNIT from matching MAGNITOGORSK.
    return QRegularExpression(QStringLiteral("(?:^|[^\\p{L}\\p{N}])(?:%1)(?=$|[^\\p{L}\\p{N}])")
        .arg(expression), QRegularExpression::UseUnicodePropertiesOption).match(text).hasMatch();
}
}

QString BankCategoryMatcher::normalize(const QString& text)
{
    return BankRecipientMatcher::normalize(text);
}

QString BankCategoryMatcher::merchant(const QString& description)
{
    BankCsvOperation operation;operation.description=description;operation.signedMinor=-1;
    const auto result=BankRecipientMatcher::identify(operation);
    return result.status=="known" ? result.recipient.name : QString();
}

bool BankCategoryMatcher::isSpecialOperation(const QString& description)
{
    const auto text = normalize(description);
    if (matches(text, QStringLiteral(
        "перевод|перевода|переводы|transfer|возврат|возврата|refund|reversal|снятие|внесение|"
        "cash withdrawal|cash withdraw|withdrawal|cash out|cashout|atm|выдача наличных"))) return true;
    // A merchant purchase through SBP is still an ordinary expense.
    return matches(text, QStringLiteral("сбп|sbp"))
        && !matches(text, QStringLiteral("оплата|покупка|payment|purchase|pos"));
}

BankCategorySuggestion BankCategoryMatcher::suggest(
    const BankCsvOperation& operation, const QVector<Category>& categories,
    const QSet<QString>& archived, const QVector<BankCategoryRule>& rules,
    const QString& fallbackCategoryId, const BankRecipientMatch* recipient)
{
    BankCategorySuggestion result;
    const auto resolved=recipient ? *recipient : BankRecipientMatcher::identify(operation);
    result.merchant = resolved.recipient.name;
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
            const QString target=rule.field=="recipient" ? merchantKey : descriptor;
            const bool match = mode == QStringLiteral("exact")
                ? (rule.field=="legacy" ? pattern==merchantKey || pattern==descriptor : pattern==target)
                : matches(target, QRegularExpression::escape(pattern));
            if (!match) continue;
            const qsizetype score = (mode == QStringLiteral("exact") ? 0 : pattern.size()) + (rule.field=="legacy"?0:1000);
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
            QStringList aliases = resolved.categoryNames;
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
                result.categoryId = selected;
                result.needsReview = resolved.status != "known";
                result.reason = QStringLiteral("Категория по локальному справочнику");
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
