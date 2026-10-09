#include "../services/BankImportReview.h"
#include <QtTest>

class BankImportReviewTest : public QObject {
    Q_OBJECT
private slots:
    void dictionaryAndBoundaries();
    void explicitFieldsAndAmbiguity();
    void personalAliasesAndConflicts();
    void pendingCorrectionsApplyToWholeImport();
    void groupsRespectDirectionAndExceptions();
    void staleAndInvalidRulesAreRejected();
};
namespace {
QVector<Category> categories() {
    return {{"food","Продукты",CategoryType::Expense},{"other","Другое",CategoryType::Expense},
        {"transport","Транспорт",CategoryType::Expense},{"salary","Зарплата",CategoryType::Income}};
}
BankCsvOperation op(QString text,qint64 minor=-100) {BankCsvOperation o;o.description=text;o.signedMinor=minor;return o;}
QVariantMap row(QString id,QString text,qint64 minor=-100) {
    return {{"rowKey",id},{"fingerprint","fp:"+id},{"description",text},{"signedMinor",minor},
        {"type",minor>0?"income":"expense"},{"currency","RUB"},{"fallbackCategoryId",minor>0?"salary":"other"}};
}
}
void BankImportReviewTest::dictionaryAndBoundaries() {
    QVERIFY(BankRecipientMatcher::dictionarySize()>=190);
    QCOMPARE(BankRecipientMatcher::identify(op("POS PYATEROCHKA1234 MOSCOW")).recipient.name,QString("Пятёрочка"));
    QCOMPARE(BankRecipientMatcher::identify(op("Google Pay MAGNIT 123")).recipient.name,QString("Магнит"));
    for(const auto& unknown:QStringList{"MAGNITOGORSK","Покупка 123 карта 9999","ИП Иванов 123"}) {
        const auto r=BankRecipientMatcher::identify(op(unknown));QVERIFY(r.recipient.name.isEmpty());QCOMPARE(r.status,QString("unknown"));
    }
    QCOMPARE(BankRecipientMatcher::identify(op("YANDEX GO TAXI")).recipient.name,QString("Яндекс Такси"));
    QCOMPARE(BankRecipientMatcher::identify(op("YANDEX EDA")).recipient.name,QString("Яндекс Еда"));
    QCOMPARE(BankRecipientMatcher::identify(op("YANDEX MARKET")).recipient.name,QString("Яндекс Маркет"));
    const auto food=BankImportReview::resolve({row("a","PYATEROCHKA")},{},categories(),{},{},{});
    QVERIFY(!food.rows[0].toMap().value("needsReview").toBool());
}
void BankImportReviewTest::explicitFieldsAndAmbiguity() {
    auto o=op("Оплата 777 карта 1234");o.rawRecipient="ИП Иванов 123";o.recipientId="1234567890";
    const auto r=BankRecipientMatcher::identify(o);QCOMPARE(r.recipient.name,o.rawRecipient);QCOMPARE(r.ruleField,QString("recipient_id"));
    QCOMPARE(BankRecipientMatcher::identify(op("Получатель: ИП Иванов; Номер операции: 456")).recipient.name,QString("ИП Иванов"));
    o.rawRecipient="Оплата товаров карта 1234";QVERIFY(BankRecipientMatcher::identify(o).recipient.name.isEmpty());
    const auto conflict=BankRecipientMatcher::identify(op("YANDEX TAXI MAGNIT"));QCOMPARE(conflict.status,QString("ambiguous"));QVERIFY(conflict.recipient.name.isEmpty());
    const auto contextual=BankRecipientMatcher::identify(op("доставка лента заказ 123"));QCOMPARE(contextual.status,QString("candidate"));
    QVERIFY(BankRecipientMatcher::ruleMatches(op("доставка лента заказ 123"),contextual,
        {contextual.rulePattern,contextual.ruleField,contextual.ruleMode,CategoryType::Expense,"Лента"}));
    o.description="Покупка неизвестная";o.rawRecipient=o.description;
    QVERIFY(BankRecipientMatcher::identify(o).recipient.name.isEmpty());
}
void BankImportReviewTest::personalAliasesAndConflicts() {
    QVector<BankRecipientRule> rules{{"shop 123","description","contains",CategoryType::Expense,"Пятёрочка"}};
    QCOMPARE(BankRecipientMatcher::identify(op("POS SHOP 123 MOSCOW"),rules).recipient.name,QString("Пятёрочка"));
    QVERIFY(BankRecipientMatcher::identify(op("POS SHOP 123 MOSCOW",100),rules).recipient.name.isEmpty());
    QVERIFY(BankRecipientMatcher::identify(op("POS SHOP 1234 MOSCOW"),rules).recipient.name.isEmpty());
    rules.append({"shop 123","description","contains",CategoryType::Expense,"Магнит"});
    QCOMPARE(BankRecipientMatcher::identify(op("SHOP 123"),rules).status,QString("ambiguous"));
    auto o=op("SHOP 123");o.recipientId="000111";
    rules.append({"000111","recipient_id","exact",CategoryType::Expense,"Пятёрочка"});
    QCOMPARE(BankRecipientMatcher::identify(o,rules).recipient.name,QString("Пятёрочка"));
    rules={{"пятерочка","recipient","exact",CategoryType::Expense,"Мой магазин"}};
    QCOMPARE(BankRecipientMatcher::identify(op("PYATEROCHKA 77"),rules).recipient.name,QString("Мой магазин"));
}
void BankImportReviewTest::pendingCorrectionsApplyToWholeImport() {
    const QVariantList rows{row("a","POS SHOP ALPHA 11"),row("b","POS SHOP ALPHA 22"),row("c","POS SHOP BETA")};
    QVariantMap choice{{"fingerprint","fp:a"},{"recipientName","Магазин у дома"},{"recipientConfirmed",true},
        {"rememberRecipient",true},{"recipientPattern","shop alpha"},{"recipientField","description"},{"recipientMode","contains"},
        {"confirmed",true},{"categoryId","food"},{"remember",true},{"pattern","Магазин у дома"},{"matchMode","exact"},{"categoryField","recipient"}};
    auto r=BankImportReview::resolve(rows,{{"a",choice}},categories(),{},{},{});QVERIFY2(r.error.isEmpty(),qPrintable(r.error));
    QCOMPARE(r.recipientRules.size(),1);QCOMPARE(r.categoryRules.size(),1);
    const auto a=r.rows[0].toMap(),b=r.rows[1].toMap(),c=r.rows[2].toMap();
    QCOMPARE(a.value("groupKey"),b.value("groupKey"));QCOMPARE(b.value("merchant").toString(),QString("Магазин у дома"));
    QCOMPARE(b.value("categoryId").toString(),QString("food"));QVERIFY(!b.value("needsReview").toBool());QVERIFY(c.value("merchant").toString().isEmpty());
    QVariantMap exception{{"fingerprint","fp:b"},{"categoryId","other"},{"confirmed",true}};
    r=BankImportReview::resolve(rows,{{"a",choice},{"b",exception}},categories(),{},{},{});
    QVERIFY(r.error.isEmpty());QCOMPARE(r.rows[1].toMap().value("categoryId").toString(),QString("other"));
    // An exception on the row which carries the pending rule must not change the group rule.
    choice["rememberedCategoryId"]="food";choice["categoryId"]="other";
    r=BankImportReview::resolve(rows,{{"a",choice}},categories(),{},{},{});
    QVERIFY(r.error.isEmpty());QCOMPARE(r.rows[0].toMap().value("categoryId").toString(),QString("other"));
    QCOMPARE(r.rows[1].toMap().value("categoryId").toString(),QString("food"));QCOMPARE(r.categoryRules.front().categoryId,QString("food"));
    const auto later=BankImportReview::resolve({row("z","SHOP ALPHA 44")},{},categories(),{},r.categoryRules,r.recipientRules);
    QCOMPARE(later.rows[0].toMap().value("categoryId").toString(),QString("food"));QVERIFY(!later.rows[0].toMap().value("needsReview").toBool());
}
void BankImportReviewTest::groupsRespectDirectionAndExceptions() {
    auto usd=row("usd","PYATEROCHKA");usd["currency"]="USD";
    const auto r=BankImportReview::resolve({row("a","PYATEROCHKA"),row("b","PYATEROCHKA"),row("income","PYATEROCHKA",100),usd,
        row("refund","Возврат PYATEROCHKA"),row("u1","Покупка неизвестная"),row("u2","Покупка неизвестная")},{},categories(),{},{},{});
    const auto key=r.rows[0].toMap().value("groupKey");QCOMPARE(key,r.rows[1].toMap().value("groupKey"));
    for(int i=2;i<r.rows.size();++i)QVERIFY(key!=r.rows[i].toMap().value("groupKey"));
    QVERIFY(r.rows[5].toMap().value("groupKey")!=r.rows[6].toMap().value("groupKey"));
    QVERIFY(r.rows[4].toMap().value("needsReview").toBool());
    auto bank=row("bank","Покупка неизвестная");bank["bankCategory"]="Продукты";
    const auto b=BankImportReview::resolve({bank},{},categories(),{},{},{});QVERIFY(!b.rows[0].toMap().value("needsReview").toBool());QVERIFY(b.rows[0].toMap().value("merchant").toString().isEmpty());
}
void BankImportReviewTest::staleAndInvalidRulesAreRejected() {
    const QVariantList rows{row("a","SHOP ALPHA")};
    QVERIFY(!BankImportReview::resolve(rows,{{"a",QVariantMap{{"fingerprint","old"}}}},categories(),{},{},{}).error.isEmpty());
    QVariantMap rule{{"fingerprint","fp:a"},{"recipientName","Магазин"},{"recipientConfirmed",true},{"rememberRecipient",true},
        {"recipientPattern","shop beta"},{"recipientField","description"},{"recipientMode","contains"}};
    QVERIFY(!BankImportReview::resolve(rows,{{"a",rule}},categories(),{},{},{}).error.isEmpty());
    rule["recipientPattern"]="shop alpha";rule["remember"]=true;rule["confirmed"]=true;rule["categoryId"]="salary";rule["pattern"]="Магазин";
    QVERIFY(!BankImportReview::resolve(rows,{{"a",rule}},categories(),{},{},{}).error.isEmpty());
}
QTEST_GUILESS_MAIN(BankImportReviewTest)
#include "BankImportReviewTest.moc"
