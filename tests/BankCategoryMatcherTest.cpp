#include "../services/BankCategoryMatcher.h"
#include <QtTest>

class BankCategoryMatcherTest final : public QObject
{
    Q_OBJECT
private slots:
    void dictionaryAndAmbiguousMerchants();
    void rulesRespectDirectionAndArchives();
    void specificRulesAndConflicts();
    void transfersAndRefundsRequireReview();
    void unknownMerchantsKeepIdentifyingNumbers();
};

namespace {
QVector<Category> categories()
{
    return {{QStringLiteral("food"), QStringLiteral("Продукты"), CategoryType::Expense},
            {QStringLiteral("other"), QStringLiteral("Другое"), CategoryType::Expense},
            {QStringLiteral("salary"), QStringLiteral("Зарплата"), CategoryType::Income}};
}
BankCsvOperation operation(const QString& description, qint64 amount = -100)
{
    BankCsvOperation result; result.description = description; result.signedMinor = amount; return result;
}
}

void BankCategoryMatcherTest::dictionaryAndAmbiguousMerchants()
{
    const auto result = BankCategoryMatcher::suggest(operation(QStringLiteral("POS PYATEROCHKA 1234 MOSCOW")), categories(), {}, {}, QStringLiteral("other"));
    QCOMPARE(result.merchant, QStringLiteral("Пятёрочка"));
    QCOMPARE(result.categoryId, QStringLiteral("food"));
    QVERIFY(!result.needsReview);
    for (const auto& name : {QStringLiteral("OZON 123"), QStringLiteral("YANDEX GO"), QStringLiteral("MAGNITOGORSK")}) {
        const auto uncertain = BankCategoryMatcher::suggest(operation(name), categories(), {}, {}, QStringLiteral("other"));
        QCOMPARE(uncertain.categoryId, QStringLiteral("other")); QVERIFY(uncertain.needsReview);
    }
    auto ambiguousCategories = categories();
    ambiguousCategories.append({QStringLiteral("meal"), QStringLiteral("Еда"), CategoryType::Expense});
    QCOMPARE(BankCategoryMatcher::suggest(operation(QStringLiteral("Пятёрочка")), ambiguousCategories, {}, {}, QStringLiteral("other")).categoryId, QStringLiteral("other"));
}

void BankCategoryMatcherTest::rulesRespectDirectionAndArchives()
{
    const QVector<BankCategoryRule> rules{{QStringLiteral("пятерочка"), QStringLiteral("exact"), QStringLiteral("other"), CategoryType::Expense}};
    const auto result = BankCategoryMatcher::suggest(operation(QStringLiteral("PYATEROCHKA 998")), categories(), {}, rules, QStringLiteral("food"));
    QCOMPARE(result.categoryId, QStringLiteral("other")); QVERIFY(!result.needsReview);
    const auto income = BankCategoryMatcher::suggest(operation(QStringLiteral("PYATEROCHKA"), 100), categories(), {}, rules, QStringLiteral("salary"));
    QCOMPARE(income.categoryId, QStringLiteral("salary")); QVERIFY(income.needsReview);
    const auto archived = BankCategoryMatcher::suggest(operation(QStringLiteral("PYATEROCHKA")), categories(), {QStringLiteral("other")}, rules, QStringLiteral("food"));
    QCOMPARE(archived.categoryId, QStringLiteral("food")); QVERIFY(!archived.needsReview);
}

void BankCategoryMatcherTest::specificRulesAndConflicts()
{
    QVector<BankCategoryRule> rules{
        {QStringLiteral("магазин"), QStringLiteral("contains"), QStringLiteral("other"), CategoryType::Expense},
        {QStringLiteral("магазин у дома"), QStringLiteral("contains"), QStringLiteral("food"), CategoryType::Expense}};
    const auto result = BankCategoryMatcher::suggest(operation(QStringLiteral("Покупка магазин у дома 25")), categories(), {}, rules, QStringLiteral("other"));
    QCOMPARE(result.categoryId, QStringLiteral("food")); QVERIFY(!result.needsReview);
    rules.append({QStringLiteral("магазин у дома"), QStringLiteral("contains"), QStringLiteral("other"), CategoryType::Expense});
    const auto conflict = BankCategoryMatcher::suggest(operation(QStringLiteral("магазин у дома 25")), categories(), {}, rules, QStringLiteral("other"));
    QVERIFY(conflict.needsReview); QVERIFY(conflict.reason.contains(QStringLiteral("Конфликт")));
}

void BankCategoryMatcherTest::transfersAndRefundsRequireReview()
{
    for (const auto& text : {QStringLiteral("Перевод СБП PYATEROCHKA"), QStringLiteral("Возврат PYATEROCHKA"), QStringLiteral("Cash withdrawal")}) {
        auto op = operation(text); op.categoryName = QStringLiteral("Продукты");
        const auto result = BankCategoryMatcher::suggest(op, categories(), {}, {}, QStringLiteral("other"));
        QVERIFY(result.needsReview); QCOMPARE(result.categoryId, QStringLiteral("other"));
        QVERIFY(BankCategoryMatcher::isSpecialOperation(text));
    }
    for (const auto& text : {QStringLiteral("METRO CASH CARRY"), QStringLiteral("Оплата СБП PYATEROCHKA")}) {
        QVERIFY(!BankCategoryMatcher::isSpecialOperation(text));
        const auto purchase = BankCategoryMatcher::suggest(operation(text), categories(), {}, {}, QStringLiteral("other"));
        QCOMPARE(purchase.categoryId, QStringLiteral("food")); QVERIFY(!purchase.needsReview);
    }
    auto op = operation(QStringLiteral("Покупка")); op.categoryName = QStringLiteral("Продукты");
    const auto bank = BankCategoryMatcher::suggest(op, categories(), {}, {}, QStringLiteral("other"));
    QCOMPARE(bank.categoryId, QStringLiteral("food")); QVERIFY(!bank.needsReview);
}

void BankCategoryMatcherTest::unknownMerchantsKeepIdentifyingNumbers()
{
    QVERIFY(BankCategoryMatcher::merchant(QStringLiteral("ИП Иванов 123")).isEmpty());
    QVERIFY(BankCategoryMatcher::normalize(QStringLiteral("ИП Иванов 123")) !=
            BankCategoryMatcher::normalize(QStringLiteral("ИП Иванов 456")));
    QCOMPARE(BankCategoryMatcher::normalize(QStringLiteral(" ПЯТЁРОЧКА*123 ")), QStringLiteral("пятерочка 123"));
}

QTEST_GUILESS_MAIN(BankCategoryMatcherTest)
#include "BankCategoryMatcherTest.moc"
