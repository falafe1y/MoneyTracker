#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>

namespace {
// The fiat total of the overview, in kopecks
qint64 fiatTotal(FinanceController& controller)
{
    for (const QVariant& value : controller.assetSummaries()) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("code")).toString() == QStringLiteral("fiat")) {
            return item.value(QStringLiteral("balanceMinor")).toLongLong();
        }
    }
    return 0;
}

QVariantMap accountNamed(FinanceController& controller, const QString& name)
{
    for (const QVariant& value : controller.accounts()) {
        if (value.toMap().value(QStringLiteral("rawName")).toString() == name) {
            return value.toMap();
        }
    }
    return {};
}
} // namespace

class CreditDebtFlagControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void debtOfACardIsSubtractedOnlyWhileTheCardCountsIt()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("data.db");
        {
            FinanceRepository r(path);
            QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.saveManualCurrencyRates(1, 100, 120));
        }
        QString cardId;
        {
            FinanceController c(nullptr, path);
            c.setSelectedAsset("fiat");
            QVERIFY(c.addAccount("Наличка", "cash", "RUB", 600000, 0));
            QVERIFY(c.addAccount("Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000));

            // A new card counts its debt: 6 000 ₽ minus 93 927,83 ₽
            QCOMPARE(fiatTotal(c), qint64(-8792783));
            QVERIFY(accountNamed(c, "Сбер | Кредитка").value("countsDebt").toBool());

            cardId = accountNamed(c, "Сбер | Кредитка").value("id").toString();
            QVERIFY(c.updateAccount(cardId, "Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000,
                                    0.0, QString(), 1, false));
            // Not counted: the debt does not reduce the fiat total, and the card still shows its debt
            QCOMPARE(fiatTotal(c), qint64(600000));
            QVERIFY(!accountNamed(c, "Сбер | Кредитка").value("countsDebt").toBool());
            QCOMPARE(accountNamed(c, "Сбер | Кредитка").value("debtMinor").toLongLong(), qint64(9392783));

            // Counted again
            QVERIFY(c.updateAccount(cardId, "Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000,
                                    0.0, QString(), 1, true));
            QCOMPARE(fiatTotal(c), qint64(-8792783));
        }
        {
            // The choice is kept after a restart
            FinanceController c(nullptr, path);
            QCOMPARE(fiatTotal(c), qint64(-8792783));
            QVERIFY(c.updateAccount(cardId, "Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000,
                                    0.0, QString(), 1, false));
        }
        {
            FinanceController c(nullptr, path);
            QCOMPARE(fiatTotal(c), qint64(600000));
            QVERIFY(!accountNamed(c, "Сбер | Кредитка").value("countsDebt").toBool());
        }
    }

    void debtOfAnotherCardStaysCountedWhenOneCardIsNot()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("data.db");
        {
            FinanceRepository r(path);
            QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.saveManualCurrencyRates(1, 100, 120));
        }
        FinanceController c(nullptr, path);
        c.setSelectedAsset("fiat");
        QVERIFY(c.addAccount("Наличка", "cash", "RUB", 600000, 0));
        QVERIFY(c.addAccount("Альфа | Кредитка", "credit_card", "RUB", -100000, 1000000, 0.0, QString(), 1, false));
        QVERIFY(c.addAccount("Сбер | Кредитка", "credit_card", "RUB", -200000, 1000000));
        // Only the card that counts its debt reduces the total: 6 000 - 2 000 ₽
        QCOMPARE(fiatTotal(c), qint64(400000));
    }

    void cardExpensesDoNotMoveTheTotalsWhileTheDebtIsNotCounted()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("data.db");
        {
            FinanceRepository r(path);
            QVERIFY(r.saveAutomaticCurrencyRates(false));
            QVERIFY(r.saveManualCurrencyRates(1, 100, 120));
        }
        FinanceController c(nullptr, path);
        c.setSelectedAsset("fiat");
        QVERIFY(c.addAccount("Наличка", "cash", "RUB", 600000, 0));
        QVERIFY(c.addAccount("Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000, 0.0, QString(), 1, false));
        const QString cardId = accountNamed(c, "Сбер | Кредитка").value("id").toString();
        QVERIFY(c.addExpense(100000, QStringLiteral("Покупка"), QStringLiteral("other_expense"),
                             QStringLiteral("RUB"), cardId, QDateTime::currentDateTime()));

        // The debt grows, but the card is not counted: the fiat total and the capital history stay at the cash
        QCOMPARE(fiatTotal(c), qint64(600000));
        const QVariantList historyBefore = c.capitalHistoryRub();
        QVERIFY(!historyBefore.isEmpty());
        QCOMPARE(historyBefore.last().toMap().value(QStringLiteral("totalMinor")).toLongLong(), qint64(600000));

        // Counted again: the debt, with the new expense, comes off both the total and the history
        QVERIFY(c.updateAccount(cardId, "Сбер | Кредитка", "credit_card", "RUB", -9392783, 15000000,
                                0.0, QString(), 1, true));
        QCOMPARE(fiatTotal(c), qint64(600000 - 9392783 - 100000));
        const QVariantList historyAfter = c.capitalHistoryRub();
        QCOMPARE(historyAfter.last().toMap().value(QStringLiteral("totalMinor")).toLongLong(),
                 qint64(600000 - 9392783 - 100000));
    }
};

QTEST_GUILESS_MAIN(CreditDebtFlagControllerTest)
#include "CreditDebtFlagControllerTest.moc"
