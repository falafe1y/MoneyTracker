#include "BankImportReview.h"

#include <algorithm>

namespace {
BankCsvOperation operationFor(const QVariantMap& row)
{
    BankCsvOperation operation;
    operation.signedMinor = row.value("signedMinor").toLongLong();
    operation.description = row.value("description").toString();
    operation.rawRecipient = row.value("rawRecipient").toString();
    operation.recipientId = row.value("recipientId").toString();
    operation.categoryName = row.value("bankCategory").toString();
    return operation;
}

QString recipientRuleKey(const BankRecipientRule& rule)
{
    return QString::number(static_cast<int>(rule.type)) + QChar(0x1f) + rule.field
        + QChar(0x1f) + rule.matchMode + QChar(0x1f) + rule.pattern;
}

QString categoryRuleKey(const BankCategoryRule& rule)
{
    // Match the existing database key, including for legacy category rules.
    return QString::number(static_cast<int>(rule.type)) + QChar(0x1f)
        + rule.matchMode + QChar(0x1f) + rule.pattern;
}

bool validCategory(const QString& id, CategoryType type,
    const QVector<Category>& categories, const QSet<QString>& archived)
{
    return id != "transfer-in" && id != "transfer-out" && !archived.contains(id)
        && std::any_of(categories.cbegin(), categories.cend(), [&](const auto& category) {
            return category.id() == id && category.type() == type;
        });
}
}

BankImportReviewResult BankImportReview::resolve(const QVariantList& input,
    const QVariantMap& choices, const QVector<Category>& categories,
    const QSet<QString>& archived, const QVector<BankCategoryRule>& storedCategories,
    const QVector<BankRecipientRule>& storedRecipients)
{
    BankImportReviewResult result;
    auto recipientRules = storedRecipients;
    auto categoryRules = storedCategories;
    QHash<QString, QString> recipientConflicts, categoryConflicts;

    // Gather recipient corrections first, so one correction applies to this entire import.
    for (const auto& value : input) {
        const auto row = value.toMap();
        const auto choice = choices.value(row.value("rowKey").toString()).toMap();
        const auto operation = operationFor(row);
        if (!choice.isEmpty() && choice.value("fingerprint").toString() != row.value("fingerprint").toString()) {
            result.error = QStringLiteral("Выписка изменилась. Обновите предварительный просмотр.");
            return result;
        }
        if (choice.contains("recipientName") && choice.value("recipientName").toString().trimmed().size() > 160) {
            result.error = QStringLiteral("Имя получателя должно быть не длиннее 160 символов");
            return result;
        }
        if (!choice.value("rememberRecipient").toBool()) continue;
        const auto base = BankRecipientMatcher::identify(operation);
        BankRecipientRule rule{
            BankRecipientMatcher::normalize(choice.value("recipientPattern").toString()),
            choice.value("recipientField", "description").toString(),
            choice.value("recipientMode", "contains").toString(),
            operation.signedMinor > 0 ? CategoryType::Income : CategoryType::Expense,
            choice.value("recipientName").toString().trimmed()};
        if (rule.pattern.isEmpty() || rule.pattern.size() > 240 || rule.name.isEmpty()
            || !choice.value("recipientConfirmed").toBool()
            || !BankRecipientMatcher::ruleMatches(operation, base, rule)) {
            result.error = QStringLiteral("Укажите правило получателя, которое совпадает с примером операции");
            return result;
        }
        const auto key = recipientRuleKey(rule);
        if (recipientConflicts.contains(key)
            && BankRecipientMatcher::normalize(recipientConflicts.value(key)) != BankRecipientMatcher::normalize(rule.name)) {
            result.error = QStringLiteral("Для одного правила выбраны разные получатели. Оставьте одно правило, а исключения не запоминайте.");
            return result;
        }
        if (!recipientConflicts.contains(key)) result.recipientRules.append(rule);
        recipientConflicts.insert(key, rule.name);
        recipientRules.erase(std::remove_if(recipientRules.begin(), recipientRules.end(), [&](const auto& previous) {
            return recipientRuleKey(previous) == key;
        }), recipientRules.end());
        recipientRules.append(rule);
    }

    // Resolve recipients before categories. Explicit row choices remain one-off exceptions.
    QVector<BankRecipientMatch> recipients;
    recipients.reserve(input.size());
    for (const auto& value : input) {
        const auto row = value.toMap();
        const auto choice = choices.value(row.value("rowKey").toString()).toMap();
        const auto operation = operationFor(row);
        auto recipient = BankRecipientMatcher::identify(operation, recipientRules);
        if (choice.contains("recipientName") && choice.value("recipientConfirmed").toBool()) {
            const auto name = choice.value("recipientName").toString().trimmed();
            if (name != recipient.recipient.name || recipient.status != "known") {
                recipient.recipient = {name, BankRecipientMatcher::keyForName(name),
                    name.isEmpty() ? QString() : QStringLiteral("manual")};
                recipient.status = name.isEmpty() ? "unknown" : "known";
                recipient.reason = name.isEmpty() ? QStringLiteral("Импорт без имени получателя") : QStringLiteral("Ваше исправление");
                auto named = operation;
                named.description.clear();
                named.rawRecipient = name;
                recipient.categoryNames = BankRecipientMatcher::identify(named).categoryNames;
            }
        }
        recipients.append(recipient);
        if (!choice.value("remember").toBool()) continue;
        const auto type = operation.signedMinor > 0 ? CategoryType::Income : CategoryType::Expense;
        const auto mode = choice.value("matchMode", "exact").toString();
        BankCategoryRule rule{
            BankRecipientMatcher::normalize(choice.value("pattern", recipient.recipient.name).toString()),
            mode, choice.value("rememberedCategoryId", choice.value("categoryId")).toString(), type,
            choice.value("categoryField", mode == "exact" ? "recipient" : "description").toString()};
        if (rule.pattern.isEmpty() || rule.pattern.size() > 240 || !choice.value("confirmed").toBool()
            || !validCategory(rule.categoryId, type, categories, archived)
            || (rule.field != "recipient" && rule.field != "description" && rule.field != "legacy")) {
            result.error = QStringLiteral("Проверьте правило категории");
            return result;
        }
        const auto check = BankCategoryMatcher::suggest(operation, categories, archived, {rule}, {}, &recipient);
        if (check.categoryId != rule.categoryId || !check.reason.startsWith(QStringLiteral("Ваше правило"))) {
            result.error = QStringLiteral("Правило категории не соответствует операции");
            return result;
        }
        const auto key = categoryRuleKey(rule);
        if (categoryConflicts.contains(key) && categoryConflicts.value(key) != rule.categoryId) {
            result.error = QStringLiteral("Для одного правила выбраны разные категории. Не запоминайте разовые исключения.");
            return result;
        }
        if (!categoryConflicts.contains(key)) result.categoryRules.append(rule);
        categoryConflicts.insert(key, rule.categoryId);
        categoryRules.erase(std::remove_if(categoryRules.begin(), categoryRules.end(), [&](const auto& previous) {
            return categoryRuleKey(previous) == key;
        }), categoryRules.end());
        categoryRules.append(rule);
    }

    for (int n = 0; n < input.size(); ++n) {
        auto row = input[n].toMap();
        const auto choice = choices.value(row.value("rowKey").toString()).toMap();
        const auto operation = operationFor(row);
        const auto& recipient = recipients[n];
        const auto suggestion = BankCategoryMatcher::suggest(operation, categories, archived,
            categoryRules, row.value("fallbackCategoryId").toString(), &recipient);
        QString categoryId = suggestion.categoryId;
        if (choice.contains("categoryId")) categoryId = choice.value("categoryId").toString();
        const bool confirmed = choice.value("confirmed").toBool();
        const bool special = BankCategoryMatcher::isSpecialOperation(operation.description);
        const auto type = operation.signedMinor > 0 ? CategoryType::Income : CategoryType::Expense;
        row["bankRecipient"] = BankRecipientMatcher::bankRecipientName(operation);
        row["recognizedRecipient"] = recipient.baseRecipientName;
        row["merchant"] = recipient.recipient.name;
        row["recipientKey"] = recipient.recipient.key;
        row["recipientSource"] = recipient.recipient.source;
        row["recipientStatus"] = recipient.status;
        row["recipientReason"] = recipient.reason;
        row["recipientPattern"] = recipient.rulePattern;
        row["recipientField"] = recipient.ruleField;
        row["recipientMode"] = recipient.ruleMode;
        row["categoryId"] = categoryId;
        row["confirmed"] = confirmed;
        row["reason"] = confirmed ? QStringLiteral("Выбор подтверждён") : suggestion.reason;
        row["needsReview"] = !validCategory(categoryId, type, categories, archived)
            || (!confirmed && (suggestion.needsReview || recipient.status == "candidate" || recipient.status == "ambiguous"));
        row["special"] = special;
        // Unknown names, opposite directions/currencies and special operations never share a group.
        row["groupKey"] = recipient.recipient.key.isEmpty() || recipient.status != "known" || special
            ? "row:" + row.value("rowKey").toString()
            : row.value("type").toString() + QChar(0x1f) + row.value("currency").toString() + QChar(0x1f) + recipient.recipient.key;
        result.rows.append(row);
    }
    return result;
}
