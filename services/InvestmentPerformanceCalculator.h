#pragma once
#include "../core/Transaction.h"
#include "../core/InvestmentOperation.h"
#include <QVariant>
#include <QSet>
#include <optional>

struct PerformanceAccount {
    QString id, name, currency;
    qint64 createdAt = 0, initialMinor = 0;
};
struct InvestmentPerformanceInput {
    QVector<PerformanceAccount> accounts;
    QVector<Transaction> transactions;
    QVector<InvestmentOperation> operations;
    QVariantList valuations, positionHistory, prices, rates;
    QString accountId, currency = QStringLiteral("RUB");
    QDate from, to;
    QDateTime now;
    std::optional<double> annualInflationPercent;
};

// Returns are linked from daily observations; cash flows are never investment income.
class InvestmentPerformanceCalculator {
public:
    static QVariantMap calculate(const InvestmentPerformanceInput& input);
};
