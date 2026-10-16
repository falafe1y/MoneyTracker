#include "../services/InvestmentReturn.h"
#include <QtTest>

namespace {
constexpr qint64 kStart = 1735689600000LL; // 2025-01-01 UTC
constexpr qint64 kDay = 86400000LL;

QVariantMap item(const QString& id, const QString& group, qint64 value, const QVariant& rate)
{
    return {{"id", id}, {"group", group}, {"title", id}, {"valueMinor", value}, {"annualPercent", rate}};
}

QVariantMap groupAt(const QVariantMap& summary, int index)
{
    return summary.value("groups").toList().at(index).toMap();
}
} // namespace

class AverageReturnTest : public QObject {
    Q_OBJECT
private slots:
    void oneYearPurchaseGrowingByHalfIsFiftyPercent()
    {
        const auto result = annualReturnOf({{kStart, -100000}}, kStart + 365 * kDay, 150000);
        QVERIFY(result.percent.has_value());
        QVERIFY(qAbs(*result.percent - 50.0) < 0.1);
    }

    void shortHistoryIsNotAnnualized()
    {
        const auto result = annualReturnOf({{kStart, -100000}}, kStart + 10 * kDay, 110000);
        QVERIFY(!result.percent.has_value());
        QVERIFY(result.reason.contains(QStringLiteral("30 дней")));
    }

    void lostMoneyIsMinusHundred()
    {
        const auto result = annualReturnOf({{kStart, -100000}}, kStart + 400 * kDay, 0);
        QVERIFY(result.percent.has_value());
        QCOMPARE(*result.percent, -100.0);
    }

    void withoutInvestmentThereIsNoRate()
    {
        const auto result = annualReturnOf({{kStart, 5000}}, kStart + 400 * kDay, 0);
        QVERIFY(!result.percent.has_value());
        QVERIFY(result.reason.contains(QStringLiteral("нет вложений")));
    }

    void twoPurchasesAreWeightedByTheirTime()
    {
        // 100 000 in at the start, 100 000 in a year later, 240 000 out after two years: about 12.8% a year
        const auto result = annualReturnOf({{kStart, -100000}, {kStart + 365 * kDay, -100000}},
                                           kStart + 730 * kDay, 240000);
        QVERIFY(result.percent.has_value());
        QVERIFY(qAbs(*result.percent - 12.80) < 0.05);
    }

    void averageIsWeightedByTheMoneyInEachInstrument()
    {
        const QVariantList items{item("deposit", "investments", 200000, 13.0),
                                 item("bonds", "investments", 20000, 20.0),
                                 item("btc", "crypto", 50000, QVariant())};
        const QVariantMap merged = averageReturnSummaryOf(items, {}, true);
        QVERIFY(qAbs(groupAt(merged, 0).value("annualPercent").toDouble() - 13.636) < 0.01);
        QCOMPARE(groupAt(merged, 0).value("counted").toInt(), 2);
        QCOMPARE(groupAt(merged, 0).value("shown").toInt(), 3);

        // Leaving the deposit out leaves the bonds alone
        const QVariantMap without = averageReturnSummaryOf(items, QStringList{"deposit"}, true);
        QCOMPARE(groupAt(without, 0).value("annualPercent").toDouble(), 20.0);

        // Without merging, crypto has no rate and gets its own pie
        const QVariantMap separate = averageReturnSummaryOf(items, {}, false);
        QVERIFY(qAbs(groupAt(separate, 0).value("annualPercent").toDouble() - 13.636) < 0.01);
        QVERIFY(groupAt(separate, 1).value("annualPercent").isNull());
        const QVariantList slices = groupAt(separate, 1).value("slices").toList();
        QCOMPARE(int(slices.size()), 1);
        QCOMPARE(slices.first().toMap().value("share").toDouble(), 1.0);
    }
};

QTEST_GUILESS_MAIN(AverageReturnTest)
#include "AverageReturnTest.moc"
