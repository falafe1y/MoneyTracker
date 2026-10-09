#pragma once

#include "BankCsvImporter.h"
#include "BankRecipientMatcher.h"
#include "../core/Category.h"
#include <QSet>

struct BankCategoryRule
{
    QString pattern;
    QString matchMode = QStringLiteral("exact");
    QString categoryId;
    CategoryType type = CategoryType::Expense;
    QString field = QStringLiteral("legacy");
};

struct BankCategorySuggestion
{
    QString merchant;
    QString categoryId;
    QString reason;
    bool needsReview = true;
};

class BankCategoryMatcher final
{
public:
    static QString normalize(const QString& text);
    static QString merchant(const QString& description);
    static bool isSpecialOperation(const QString& description);
    static BankCategorySuggestion suggest(
        const BankCsvOperation& operation,
        const QVector<Category>& categories,
        const QSet<QString>& archived,
        const QVector<BankCategoryRule>& rules,
        const QString& fallbackCategoryId,
        const BankRecipientMatch* recipient = nullptr);
};
