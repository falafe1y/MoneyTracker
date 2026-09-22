#include "../services/FinancialGoalCalculator.h"
#include <QtTest>

class GoalRates final : public CurrencyRateProvider
{
public:
    qint64 rub = 10'000'000; // 100 RUB/USD
    qint64 rateToUsd(Currency currency) const override
    {
        return currency == Currency::RUB ? rub : kCurrencyRateScale;
    }
};

class FinancialGoalCalculatorTest : public QObject
{
    Q_OBJECT
private slots:
    void exchangeRateAloneChangesAchievement()
    {
        GoalRates rates;
        FinancialGoal goal;
        goal.currency = Currency::USD;
        goal.targetMinor = 20'000'000; // 200,000 USD
        const QVector<GoalAssetValue> assets{
            {QStringLiteral("account:r"), Money(1'950'000'000, Currency::RUB)}};
        auto first = FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22));
        QCOMPARE(first.currentMinor, qint64(19'500'000));
        QVERIFY(!first.achieved);
        rates.rub = 11'000'000;
        auto second = FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22));
        QCOMPARE(second.currentMinor, qint64(21'450'000));
        QVERIFY(second.achieved);
        QCOMPARE(second.remainingMinor, qint64(0));
        rates.rub = 10'000'000;
        QVERIFY(!FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22)).achieved);
    }
    void scopeDebtAndIndependentGoals()
    {
        GoalRates rates;
        FinancialGoal all;
        all.currency = Currency::USD; all.targetMinor = 10'000;
        const QVector<GoalAssetValue> assets{
            {QStringLiteral("account:cash"), Money(12'000, Currency::USD)},
            {QStringLiteral("account:debt"), Money(-3'000, Currency::USD)},
            {QStringLiteral("wallet:btc"), Money(2'000, Currency::USD)}};
        auto result = FinancialGoalCalculator::calculate(all, assets, rates, QDate(2026,9,22));
        QCOMPARE(result.currentMinor, qint64(11'000));
        QVERIFY(result.achieved);
        FinancialGoal selected = all;
        selected.allSources = false;
        selected.sourceIds = {QStringLiteral("account:cash"), QStringLiteral("account:debt")};
        auto limited = FinancialGoalCalculator::calculate(selected, assets, rates, QDate(2026,9,22));
        QCOMPARE(limited.currentMinor, qint64(9'000));
        QVERIFY(!limited.achieved);
        QVERIFY(FinancialGoalCalculator::calculate(all, assets, rates, QDate(2026,9,22)).achieved);
    }
    void deadlineIsInclusiveAndDoesNotFreezeValue()
    {
        GoalRates rates;
        FinancialGoal goal;
        goal.currency = Currency::USD; goal.targetMinor = 100;
        auto check = [&](const QDate& today, qint64 value) {
            return FinancialGoalCalculator::calculate(goal,
                {{QStringLiteral("account:a"), Money(value, Currency::USD)}}, rates, today);
        };
        QVERIFY(!check(QDate(2030,1,1), 0).overdue); // Unlimited
        goal.deadline = QDate(2026,9,22);
        QVERIFY(!check(goal.deadline, 99).overdue);
        QVERIFY(check(goal.deadline.addDays(1), 99).overdue);
        QVERIFY(check(goal.deadline.addDays(1), 100).achieved);
        QVERIFY(!check(goal.deadline.addDays(1), 100).overdue);
    }
    void missingSourcesOrQuotesCannotClaimAchievement()
    {
        GoalRates rates;
        FinancialGoal goal;
        goal.currency = Currency::USD; goal.targetMinor = 100;
        const QVector<GoalAssetValue> assets{
            {QStringLiteral("account:a"), Money(1'000, Currency::USD)},
            {QStringLiteral("wallet:b"), Money(0, Currency::USD), false}};
        auto result = FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22));
        QVERIFY(!result.complete); QVERIFY(!result.achieved);
        goal.allSources = false; goal.sourceIds = {QStringLiteral("account:a")};
        QVERIFY(FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22)).achieved);
        goal.sourceIds.append(QStringLiteral("account:deleted"));
        QVERIFY(!FinancialGoalCalculator::calculate(goal, assets, rates, QDate(2026,9,22)).complete);
        goal.allSources = true; rates.rub = 0;
        QVERIFY(!FinancialGoalCalculator::calculate(goal,
            {{QStringLiteral("account:r"), Money(100, Currency::RUB)}}, rates, QDate(2026,9,22)).complete);
    }
};
QTEST_APPLESS_MAIN(FinancialGoalCalculatorTest)
#include "FinancialGoalCalculatorTest.moc"
