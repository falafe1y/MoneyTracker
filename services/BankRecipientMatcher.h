#pragma once
#include "BankCsvImporter.h"
#include "../core/Category.h"
#include "../core/TransactionRecipient.h"

struct BankRecipientRule {
    QString pattern;
    QString field = QStringLiteral("description");
    QString matchMode = QStringLiteral("contains");
    CategoryType type = CategoryType::Expense;
    QString name;
};

struct BankRecipientMatch {
    TransactionRecipient recipient;
    QString status = QStringLiteral("unknown");
    QString reason;
    QStringList categoryNames;
    QString rulePattern;
    QString ruleField = QStringLiteral("description");
    QString ruleMode = QStringLiteral("contains");
};

class BankRecipientMatcher final {
public:
    static QString normalize(const QString& text);
    static BankRecipientMatch identify(const BankCsvOperation& operation,
                                      const QVector<BankRecipientRule>& rules = {});
    static bool ruleMatches(const BankCsvOperation& operation, const BankRecipientMatch& base,
                            const BankRecipientRule& rule);
    static QString keyForName(const QString& name);
    static int dictionarySize();
};
