#include "../services/InvestmentValuation.h"
#include <QtTest>

class InvestmentValuationTest : public QObject {
    Q_OBJECT
private slots:
    void bondsIncludeAccrualAndRemainingNominal();
    void spotUnitsAndCurrencies();
    void optionsAreAssetsOrLiabilities();
    void clearingDoesNotCountProfitTwice();
    void missingInputsAndOverflowAreUnavailable();
    void manualLiabilityAndPrecisionSurviveSerialization();
};
static QDateTime timeAt() { return QDateTime::fromString("2026-10-09T08:00:00Z",Qt::ISODate); }
static InvestmentPosition position(qint64 quantity,InvestmentPositionSettings settings={}) {
    return {"position","broker","asset",quantity,120'000'000,{},{},settings};
}
void InvestmentValuationTest::bondsIncludeAccrualAndRemainingNominal() {
    InvestmentTerms t;t.pricing="bond";t.faceValueMicros=1'000'000'000;t.accruedInterestMicros=20'000'000;
    InvestmentQuote q("asset",95'000'000,timeAt(),t);
    auto v=valueInvestmentPosition(position(10'000'000),&q,t,1'000'000);
    QVERIFY(v.available);QCOMPARE(v.valueMinor,qint64(970'000));
    t.faceValueMicros=500'000'000;v=valueInvestmentPosition(position(10'000'000),&q,t,1'000'000);
    QCOMPARE(v.valueMinor,qint64(495'000));
    t.accruedInterestMicros=0;v=valueInvestmentPosition(position(10'000'000),&q,t,1'000'000);
    QCOMPARE(v.valueMinor,qint64(475'000));
}
void InvestmentValuationTest::spotUnitsAndCurrencies() {
    InvestmentTerms t;t.pricing="unit";InvestmentQuote q("asset",281'440'000,timeAt(),t);
    QCOMPARE(valueInvestmentPosition(position(5'000'000),&q,t,1'000'000).valueMinor,qint64(140'720));
    q=InvestmentQuote("asset",10'000'000'000,timeAt(),t);
    QCOMPARE(valueInvestmentPosition(position(2'500'000),&q,t,1'000'000).valueMinor,qint64(2'500'000)); // 2.5 grams.
    q=InvestmentQuote("asset",1'000'000,timeAt(),t);
    QCOMPARE(valueInvestmentPosition(position(1'000'000'000),&q,t,13'000'000).valueMinor,qint64(1'300'000));
    t.multiplierMicros=10'000; // Quote per 100 base units, quantity in actual units.
    q=InvestmentQuote("asset",90'000'000,timeAt(),t);
    QCOMPARE(valueInvestmentPosition(position(100'000'000),&q,t,1'000'000).valueMinor,qint64(9'000));
}
void InvestmentValuationTest::optionsAreAssetsOrLiabilities() {
    InvestmentTerms t;t.pricing="premium_option";t.multiplierMicros=100'000'000;
    InvestmentQuote q("asset",10'000'000,timeAt(),t);InvestmentPositionSettings s;
    auto v=valueInvestmentPosition(position(1'000'000,s),&q,t,1'000'000);
    QCOMPARE(qint64(900'000)+v.valueMinor,qint64(1'000'000)); // Premium already paid from cash.
    s.direction=-1;v=valueInvestmentPosition(position(1'000'000,s),&q,t,1'000'000);
    QCOMPARE(qint64(1'100'000)+v.valueMinor,qint64(1'000'000)); // Received premium offsets obligation.
    q=InvestmentQuote("asset",0,timeAt(),t);QCOMPARE(valueInvestmentPosition(position(1'000'000,s),&q,t,1'000'000).valueMinor,qint64(0));
}
void InvestmentValuationTest::clearingDoesNotCountProfitTwice() {
    InvestmentTerms t;t.pricing="future";t.priceStepMicros=1'000'000;t.stepValueMicros=10'000'000;
    InvestmentPositionSettings s;s.referencePriceMicros=100'000'000;s.referenceAtUtc=timeAt().addSecs(-3600);s.blockedMarginMinor=2'000'000;
    InvestmentQuote q("asset",110'000'000,timeAt(),t);
    auto v=valueInvestmentPosition(position(2'000'000,s),&q,t,1'000'000);
    QVERIFY(v.available);QCOMPARE(v.valueMinor,qint64(20'000));
    QCOMPARE(qint64(10'000'000)+v.valueMinor,qint64(10'020'000)); // Margin is part of cash.
    s.referencePriceMicros=q.priceMicros();s.referenceAtUtc=timeAt();
    v=valueInvestmentPosition(position(2'000'000,s),&q,t,1'000'000);
    QCOMPARE(v.valueMinor,qint64(0));QCOMPARE(qint64(10'020'000)+v.valueMinor,qint64(10'020'000));
    s.direction=-1;s.referencePriceMicros=100'000'000;s.referenceAtUtc=timeAt().addSecs(-3600);
    QCOMPARE(valueInvestmentPosition(position(2'000'000,s),&q,t,1'000'000).valueMinor,qint64(-20'000));
    t.pricing="margined_option";QCOMPARE(valueInvestmentPosition(position(2'000'000,s),&q,t,1'000'000).valueMinor,qint64(-20'000));
}
void InvestmentValuationTest::missingInputsAndOverflowAreUnavailable() {
    InvestmentTerms t;InvestmentQuote q("asset",100'000'000,timeAt(),t);
    QVERIFY(!valueInvestmentPosition(position(1'000'000),nullptr,t,1'000'000).available);
    QVERIFY(!valueInvestmentPosition(position(1'000'000),&q,t,0).available);
    t.pricing="bond";t.faceValueMicros=1'000'000'000;
    QVERIFY(!valueInvestmentPosition(position(1'000'000),&q,t,1'000'000).available); // Missing accrued value != zero.
    t.pricing="future";t.priceStepMicros=1'000'000;t.stepValueMicros=1'000'000;
    InvestmentPositionSettings s;s.referenceAtUtc=timeAt().addSecs(1);
    QVERIFY(!valueInvestmentPosition(position(1'000'000,s),&q,t,1'000'000).available);
    t.pricing="unit";q=InvestmentQuote("asset",std::numeric_limits<qint64>::max(),timeAt(),t);
    QVERIFY(!valueInvestmentPosition(position(std::numeric_limits<qint64>::max()),&q,t,1'000'000).available);
}
void InvestmentValuationTest::manualLiabilityAndPrecisionSurviveSerialization() {
    InvestmentPositionSettings s;s.manualValuation=true;s.manualValueMinor=-12345;s.referencePriceMicros=9'007'199'254'740'993LL;
    const auto copy=InvestmentPositionSettings::deserialize(s.serialize());QCOMPARE(copy.referencePriceMicros,s.referencePriceMicros);
    const auto v=valueInvestmentPosition(position(1'000'000,copy),nullptr,{},0);
    QVERIFY(v.available);QCOMPARE(v.valueMinor,qint64(-12345));
    InvestmentTerms t;t.faceValueMicros=9'007'199'254'740'993LL;QCOMPARE(InvestmentTerms::deserialize(t.serialize()).faceValueMicros,t.faceValueMicros);
}
QTEST_APPLESS_MAIN(InvestmentValuationTest)
#include "InvestmentValuationTest.moc"
