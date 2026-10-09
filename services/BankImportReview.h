#pragma once
#include "BankCategoryMatcher.h"
#include <QVariantMap>

struct BankImportReviewResult {
    QVariantList rows;
    QVector<BankCategoryRule> categoryRules;
    QVector<BankRecipientRule> recipientRules;
    QString error;
};
class BankImportReview final {
public:
    static BankImportReviewResult resolve(const QVariantList& rows, const QVariantMap& choices,
        const QVector<Category>& categories, const QSet<QString>& archived,
        const QVector<BankCategoryRule>& categoryRules, const QVector<BankRecipientRule>& recipientRules);
};
