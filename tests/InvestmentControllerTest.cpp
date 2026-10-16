#include "../interface/FinanceController.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>

class InvestmentControllerTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void clearingKeepsCapitalAndSurvivesRestart();
    void bondCouponAndTradesPreserveCapital();
    void shortOptionIsAnObligation();
    void foreignCurrencyAndMissingRateAreExplicit();
    void manuallyAddsEveryAssetClass();
    void transferToDepositAndBackKeepsBothHalves();
    void tradesAreTransferHalvesNotIncome();
    void proceedsOfSaleTransferToFiat();
    void brokerTransfersCannotTouchBlockedMargin();
    void purchaseKeepsChosenDate();
    void purchaseBeyondFreeMoneyIsRefused();
    void couponAndFeeAreIncomeAndExpenseOfTheBrokerAccount();
    void negativeFiatBalanceIsShownAsIs();
    void saleWritesCommissionAndTaxAsExpenses();
    void brokerChargeIsExpenseWithoutPosition();
    void transferCannotExceedSourceBalance();
    void buyCommissionIsSeparateRowOnExistingPosition();
    void purchaseNeedsFreeMoneyUntilFunded();
    void manualPositionCannotCreateMoney();
};
static qint64 balance(FinanceController& c) {
    c.setSelectedAsset("investment");for(const auto& item:c.accounts())if(item.toMap().value("id").toString()=="broker")return item.toMap().value("balanceMinor").toLongLong();
    return -1;
}
static bool seed(FinanceRepository& r) {
    return r.isOpen() && r.saveAutomaticCurrencyRates(false) && r.saveManualCurrencyRates(1,100,120) &&
        r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,10'000'000});
}
void InvestmentControllerTest::clearingKeepsCapitalAndSurvivesRestart() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="futures";t.market="forts";t.pricing="future";t.currencyCode="RUB";t.priceStepMicros=1'000'000;t.stepValueMicros=10'000'000;
     InvestmentPositionSettings s;s.referencePriceMicros=100'000'000;s.referenceAtUtc=at.addSecs(-3600);s.blockedMarginMinor=2'000'000;
     QVERIFY(r.saveInvestmentBundle({"future","SiZ6","","Фьючерс",InvestmentInstrumentType::Future,Currency::RUB,"MOEX","RFUD",t},
       {"position","broker","future",2'000'000,100'000'000,{},{},s},InvestmentQuote("future",110'000'000,at,t),false));}
    {FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'020'000));
     const auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","margin"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","200"},{"price","110"}});
     QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'020'000));
     QVERIFY(!c.investmentValuationIncomplete());QCOMPARE(c.investmentOperations().size(),1);}
    FinanceController restored(nullptr,path);QCOMPARE(balance(restored),qint64(10'020'000));
    QCOMPARE(restored.investmentPositions().at(0).toMap().value("marketValueMinor").toLongLong(),qint64(0));
}
void InvestmentControllerTest::bondCouponAndTradesPreserveCapital() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="stock";t.market="bonds";t.pricing="bond";t.currencyCode="RUB";t.faceValueMicros=1'000'000'000;t.accruedInterestMicros=20'000'000;
     QVERIFY(r.saveInvestmentBundle({"bond","BOND","","Облигация",InvestmentInstrumentType::Bond,Currency::RUB,"MOEX","TQCB",t},
       {"position","broker","bond",10'000'000,95'000'000},InvestmentQuote("bond",95'000'000,at,t),false));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'970'000));
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","coupon"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","200"},{"accruedAfter","0"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","buy"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","-4750"},{"quantity","5"},{"price","95"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","4750"},{"quantity","5"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'970'000));
}
void InvestmentControllerTest::shortOptionIsAnObligation() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="futures";t.market="options";t.pricing="premium_option";t.currencyCode="RUB";t.multiplierMicros=100'000'000;
     InvestmentPositionSettings s;s.direction=-1;
     InvestmentOperation opening{"opening","broker","position","option","buy",100'000,1'000'000,at,"Продажа опциона"};
     QVERIFY(r.saveInvestmentBundle({"option","OPT","","Опцион",InvestmentInstrumentType::Option,Currency::RUB,"MOEX","ROPD",t},
       {"position","broker","option",1'000'000,10'000'000,{},{},s},InvestmentQuote("option",10'000'000,at,t),false,opening));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'000'000));
    const auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)}, {"cashAmount","-1000"},{"quantity","1"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'000'000));QVERIFY(c.investmentPositions().isEmpty());
}
void InvestmentControllerTest::foreignCurrencyAndMissingRateAreExplicit() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");
    {FinanceRepository r(path);QVERIFY(seed(r));InvestmentTerms t;t.engine="stock";t.market="shares";t.currencyCode="USD";
     QVERIFY(r.saveInvestmentBundle({"foreign","USDSTOCK","","Акция",InvestmentInstrumentType::Stock,Currency::USD,"MOEX","TQTF",t},
       {"foreign-position","broker","foreign",10'000'000,1'000'000},InvestmentQuote("foreign",1'000'000,QDateTime::currentDateTimeUtc(),t),false));
     t.currencyCode="XYZ";QVERIFY(r.saveInvestmentBundle({"unknown","UNKNOWN","","Акция",InvestmentInstrumentType::Stock,Currency::RUB,"MOEX","TQTF",t},
       {"unknown-position","broker","unknown",1'000'000,1'000'000},InvestmentQuote("unknown",1'000'000,QDateTime::currentDateTimeUtc(),t),false));}
    FinanceController c(nullptr,path);QCOMPARE(balance(c),qint64(10'100'000));QVERIFY(c.investmentValuationIncomplete());
    const auto result=c.saveInvestmentPosition({{"id","unknown-position"},{"accountId","broker"},{"searchIndex",-1},{"quantity","1"},
        {"manualValuation",true},{"manualValue","500"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));QCOMPARE(balance(c),qint64(10'150'000));QVERIFY(!c.investmentValuationIncomplete());
}
void InvestmentControllerTest::manuallyAddsEveryAssetClass() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");{FinanceRepository r(path);QVERIFY(seed(r));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    for(int type=0;type<=10;++type){const auto result=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},
        {"instrumentType",type},{"name",QStringLiteral("Инструмент %1").arg(type)},{"symbol",QStringLiteral("ASSET%1").arg(type)},
        {"quantity","1"},{"manualValuation",true},{"manualValue","100"}});
        QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));}
    QCOMPARE(c.investmentPositions().size(),11);QCOMPARE(balance(c),qint64(10'110'000));
}
static qint64 accountBalance(FinanceController& c,const QString& id) {
    for(const auto& item:c.allAccounts())if(item.toMap().value("id").toString()==id)return item.toMap().value("balanceMinor").toLongLong();
    return -1;
}
static QVariantMap transferRow(FinanceController& c,const QString& accountId,const QString& direction) {
    for(const auto& item:c.transactions()){const auto row=item.toMap();
        if(row.value("type").toString()=="transfer" && row.value("accountId").toString()==accountId && row.value("direction").toString()==direction)return row;}
    return {};
}
static QVariantMap investmentHalf(const QVariantList& rows,const QString& kind,const QString& direction) {
    for(const auto& item:rows){const auto row=item.toMap();
        if(row.value("type").toString()=="transfer" && row.value("kind").toString()==kind && row.value("direction").toString()==direction)return row;}
    return {};
}
static void seedBondPosition(FinanceRepository& r,const QDateTime& at,qint64 blockedMargin=0) {
    // No accrued interest: trades at the clean price then move the cash and the holding by the same amount
    InvestmentTerms t;t.engine="stock";t.market="bonds";t.pricing="bond";t.currencyCode="RUB";t.faceValueMicros=1'000'000'000;t.accruedInterestMicros=0;
    InvestmentPositionSettings settings;settings.blockedMarginMinor=blockedMargin;
    QVERIFY(r.saveInvestmentBundle({"bond","BOND","","Облигация",InvestmentInstrumentType::Bond,Currency::RUB,"MOEX","TQCB",t},
        InvestmentPosition("position","broker","bond",10'000'000,95'000'000,at,at,settings),InvestmentQuote("bond",95'000'000,at,t),false));
}
void InvestmentControllerTest::transferToDepositAndBackKeepsBothHalves() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto now=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,500'000}));
     QVERIFY(r.insertAccount({"deposit","Вклад",AssetType::Investment,AccountType::Deposit,Currency::RUB,0}));}
    FinanceController c(nullptr,path);
    QVERIFY(c.addTransfer(300'000,QString(),"cash","deposit",now));
    QCOMPARE(accountBalance(c,"cash"),qint64(200'000));QCOMPARE(accountBalance(c,"deposit"),qint64(300'000));
    QVERIFY(c.addTransfer(100'000,QString(),"deposit","cash",now));
    QCOMPARE(accountBalance(c,"cash"),qint64(300'000));QCOMPARE(accountBalance(c,"deposit"),qint64(200'000));
    // Each side shows its own half: the outgoing row names the target, the incoming row names the source
    QCOMPARE(transferRow(c,"cash","out").value("transferLabel").toString(),QStringLiteral("Перевод НА Вклад"));
    QCOMPARE(transferRow(c,"cash","in").value("transferLabel").toString(),QStringLiteral("Перевод С Вклад"));
    QCOMPARE(transferRow(c,"deposit","in").value("transferLabel").toString(),QStringLiteral("Перевод С Сбер"));
    QCOMPARE(transferRow(c,"deposit","out").value("transferLabel").toString(),QStringLiteral("Перевод НА Сбер"));
}
void InvestmentControllerTest::tradesAreTransferHalvesNotIncome() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at);}
    FinanceController c(nullptr,path);const qint64 base=balance(c);
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","buy"},{"date",at.toString(Qt::ISODate)},{"cashAmount","-4750"},{"quantity","5"},{"price","95"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    auto rows=c.investmentOperations();QCOMPARE(rows.size(),2);
    const auto buyCash=investmentHalf(rows,"buy","out"),buyHolding=investmentHalf(rows,"buy","in");
    QVERIFY(!buyCash.isEmpty());QVERIFY(!buyHolding.isEmpty());
    QVERIFY(buyCash.value("investmentTransfer").toBool());
    QCOMPARE(buyCash.value("amount").toLongLong(),qint64(475'000));QCOMPARE(buyHolding.value("amount").toLongLong(),qint64(475'000));
    QCOMPARE(buyCash.value("transferLabel").toString(),QStringLiteral("Перевод НА BOND ×5"));
    QCOMPARE(buyHolding.value("transferLabel").toString(),QStringLiteral("Перевод С денежные средства"));
    QCOMPARE(balance(c),base);QCOMPARE(c.incomeMinorUnits(),qint64(0));QCOMPARE(c.expenseMinorUnits(),qint64(0));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)},{"cashAmount","4750"},{"quantity","5"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    rows=c.investmentOperations();QCOMPARE(rows.size(),4);
    const auto sellCash=investmentHalf(rows,"sell","in"),sellHolding=investmentHalf(rows,"sell","out");
    QCOMPARE(sellCash.value("transferLabel").toString(),QStringLiteral("Перевод С BOND ×5"));
    QCOMPARE(sellHolding.value("transferLabel").toString(),QStringLiteral("Перевод НА денежные средства"));
    // Every trade nets to zero across its two halves, and the account balance does not change
    qint64 net=0;for(const auto& item:rows){const auto row=item.toMap();
        const qint64 amount=row.value("amount").toLongLong();net+=row.value("direction").toString()=="in"?amount:-amount;}
    QCOMPARE(net,qint64(0));QCOMPARE(balance(c),base);
    QCOMPARE(c.incomeMinorUnits(),qint64(0));QCOMPARE(c.expenseMinorUnits(),qint64(0));
    // A trade half is not an operation of its own: it cannot be deleted on its own
    QVERIFY(!c.deleteHistoryRows(QVariantList{QVariant(buyCash)}));
    QCOMPARE(c.investmentOperations().size(),4);
}
void InvestmentControllerTest::proceedsOfSaleTransferToFiat() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at);
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,0}));}
    FinanceController c(nullptr,path);const qint64 base=balance(c);
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)},{"cashAmount","4750"},{"quantity","5"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    // The sale changes the form of value inside the brokerage account, not its total
    QCOMPARE(balance(c),base);
    // The proceeds stay on the brokerage cash until they are transferred to fiat
    QVERIFY(c.addTransfer(475'000,QString(),"broker","cash",at));
    QCOMPARE(accountBalance(c,"broker"),base-475'000);QCOMPARE(accountBalance(c,"cash"),qint64(475'000));
    QCOMPARE(transferRow(c,"broker","out").value("transferLabel").toString(),QStringLiteral("Перевод НА Сбер"));
    QCOMPARE(transferRow(c,"cash","in").value("transferLabel").toString(),QStringLiteral("Перевод С Брокер"));
}
static QVariantMap brokerage(FinanceController& c,const QString& id) {
    for(const auto& item:c.investmentAccounts())if(item.toMap().value("id").toString()==id)return item.toMap();
    return {};
}
void InvestmentControllerTest::brokerTransfersCannotTouchBlockedMargin() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at,200'000);
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,0}));}
    FinanceController c(nullptr,path);
    // 100 000 RUB of cash, 2 000 RUB of it blocked by the bond position: only 98 000 RUB may leave the account
    const qint64 free=brokerage(c,"broker").value("freeNowMinor").toLongLong();
    QCOMPARE(free,qint64(9'800'000));QCOMPARE(brokerage(c,"broker").value("blockedMarginMinor").toLongLong(),qint64(200'000));
    QVERIFY(!c.addTransfer(free+1,QString(),"broker","cash",at));
    QVERIFY(c.addTransfer(free,QString(),"broker","cash",at));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(0));
    QCOMPARE(accountBalance(c,"cash"),free);
    // Money coming into the brokerage account is not limited, and it becomes free money there
    QVERIFY(c.addTransfer(50'000,QString(),"cash","broker",at));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(50'000));
}
void InvestmentControllerTest::purchaseKeepsChosenDate() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto when=QDateTime::currentDateTimeUtc().addDays(-3);
    {FinanceRepository r(path);QVERIFY(seed(r));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    const auto result=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
        {"name",QStringLiteral("Доля")},{"symbol",QStringLiteral("SHARE")},{"quantity","2"},{"averagePrice","50"},
        {"manualValuation",true},{"manualValue","100"},{"purchase",true},{"cashAmount","-100"},{"fee","1"},
        {"date",when.toString(Qt::ISODate)},{"description",QStringLiteral("Покупка доли")}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    // Two halves of the purchase and the row of its commission
    const auto rows=c.investmentOperations();QCOMPARE(rows.size(),3);
    const auto cash=investmentHalf(rows,"buy","out");
    QCOMPARE(cash.value("date").toDateTime().toSecsSinceEpoch(),when.toSecsSinceEpoch());
    QCOMPARE(cash.value("rawDescription").toString(),QStringLiteral("Покупка доли"));
    // The purchase moves money into the holding; its commission leaves the account
    QCOMPARE(balance(c),qint64(10'000'000-100));
}
void InvestmentControllerTest::purchaseBeyondFreeMoneyIsRefused() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");
    {FinanceRepository r(path);QVERIFY(seed(r));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    // The brokerage account holds 100 000 RUB of free money: a purchase above that is refused
    const auto refused=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
        {"name",QStringLiteral("Доля")},{"symbol",QStringLiteral("SHARE")},{"quantity","1"},{"averagePrice","100001"},
        {"manualValuation",true},{"manualValue","100001"},{"purchase",true},{"cashAmount","-100001"}});
    QVERIFY(!refused.value("ok").toBool());
    QVERIFY(refused.value("error").toString().contains(QStringLiteral("Свободных денег")));
    QCOMPARE(c.investmentPositions().size(),0);
    // Exactly the free money is enough, and the account is left with no free money
    const auto exact=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
        {"name",QStringLiteral("Доля")},{"symbol",QStringLiteral("SHARE")},{"quantity","1"},{"averagePrice","100000"},
        {"manualValuation",true},{"manualValue","100000"},{"purchase",true},{"cashAmount","-100000"}});
    QVERIFY2(exact.value("ok").toBool(),qPrintable(exact.value("error").toString()));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(0));
    QCOMPARE(balance(c),qint64(10'000'000));
}
void InvestmentControllerTest::couponAndFeeAreIncomeAndExpenseOfTheBrokerAccount() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at);}
    FinanceController c(nullptr,path);
    const qint64 income=c.incomeMinorUnits(),expense=c.expenseMinorUnits();
    // A coupon is income recorded on the account where it arrived; a broker fee is an expense of that account
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","coupon"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","1200"},{"accruedAfter","0"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.incomeMinorUnits()-income,qint64(120'000));
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","fee"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","-50"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.expenseMinorUnits()-expense,qint64(5'000));
    QCOMPARE(c.incomeMinorUnits()-income,qint64(120'000));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(10'000'000+120'000-5'000));
    // Repaying part of the face value returns the holder's own capital: it is not income
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","amortization"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","300"},{"accruedAfter","0"},{"faceAfter","500"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.incomeMinorUnits()-income,qint64(120'000));
}
void InvestmentControllerTest::negativeFiatBalanceIsShownAsIs() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");
    {FinanceRepository r(path);QVERIFY(seed(r));
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,0}));}
    FinanceController c(nullptr,path);
    // No warnings and no refusals on ordinary accounts: the balance simply goes below zero
    QVERIFY(c.addExpense(500'000,QStringLiteral("Покупка"),QStringLiteral("other_expense"),QStringLiteral("RUB"),
        QStringLiteral("cash"),QDateTime::currentDateTimeUtc()));
    QCOMPARE(accountBalance(c,"cash"),qint64(-500'000));
}
void InvestmentControllerTest::saleWritesCommissionAndTaxAsExpenses() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at);}
    FinanceController c(nullptr,path);const qint64 base=balance(c);
    const qint64 income=c.incomeMinorUnits(),expense=c.expenseMinorUnits();
    // The sale keeps the gross amount (5 bonds at 95% of 1000 RUB); commission and tax are expenses of the same position
    auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","sell"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","4750"},{"quantity","5"},{"fee","20"},{"tax","100"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(balance(c),base-12'000);
    QCOMPARE(c.expenseMinorUnits()-expense,qint64(12'000));
    QCOMPARE(c.incomeMinorUnits(),income);
    // A tax recorded on its own is an expense too and does not touch the holding
    result=c.recordInvestmentOperation({{"positionId","position"},{"kind","tax"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","-50"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.expenseMinorUnits()-expense,qint64(17'000));
    QCOMPARE(balance(c),base-17'000);
}
void InvestmentControllerTest::brokerChargeIsExpenseWithoutPosition() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,50'000}));}
    FinanceController c(nullptr,path);const qint64 base=balance(c);const qint64 expense=c.expenseMinorUnits();
    // A tax withheld by the broker is an expense of the account, recorded without any position
    auto result=c.recordBrokerCharge({{"accountId","broker"},{"kind","tax"},{"amount","250"},{"date",at.toString(Qt::ISODate)}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QCOMPARE(c.expenseMinorUnits()-expense,qint64(25'000));
    QCOMPARE(balance(c),base-25'000);
    // A charge is paid from the free money of this account; without enough of it the account goes below zero
    result=c.recordBrokerCharge({{"accountId","broker"},{"kind","fee"},{"amount","99999999"},{"date",at.toString(Qt::ISODate)}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    QVERIFY(brokerage(c,"broker").value("freeNowMinor").toLongLong()<0);
    // Money can come into the account below zero, but cannot leave it
    QVERIFY(c.addTransfer(30'000,QString(),"cash","broker",at));
    QVERIFY(!c.addTransfer(1'000,QString(),"broker","cash",at));
    QCOMPARE(accountBalance(c,"cash"),qint64(20'000));
}
void InvestmentControllerTest::transferCannotExceedSourceBalance() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,50'000}));}
    FinanceController c(nullptr,path);
    // Money is not created by transfers: more than the balance is refused, and an empty account pays nothing
    QVERIFY(!c.addTransfer(50'001,QString(),"cash","broker",at));
    QVERIFY(c.addTransfer(50'000,QString(),"cash","broker",at));
    QCOMPARE(accountBalance(c,"cash"),qint64(0));
    QVERIFY(!c.addTransfer(1,QString(),"cash","broker",at));
}
void InvestmentControllerTest::buyCommissionIsSeparateRowOnExistingPosition() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));seedBondPosition(r,at);}
    FinanceController c(nullptr,path);const qint64 base=balance(c);const qint64 expense=c.expenseMinorUnits();
    // The purchase stays gross; its commission is a row of its own that leaves the account
    const auto result=c.recordInvestmentOperation({{"positionId","position"},{"kind","buy"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","-4750"},{"quantity","5"},{"price","95"},{"fee","2"}});
    QVERIFY2(result.value("ok").toBool(),qPrintable(result.value("error").toString()));
    const auto rows=c.investmentOperations();QCOMPARE(rows.size(),3);
    QCOMPARE(investmentHalf(rows,"buy","out").value("amount").toLongLong(),qint64(475'000));
    QCOMPARE(c.expenseMinorUnits()-expense,qint64(200));
    QCOMPARE(balance(c),base-200);
    // A purchase has no tax of its own: a tax is refused rather than hidden in the purchase
    const auto taxed=c.recordInvestmentOperation({{"positionId","position"},{"kind","buy"},{"date",at.toString(Qt::ISODate)},
        {"cashAmount","-4750"},{"quantity","5"},{"price","95"},{"tax","1"}});
    QVERIFY(!taxed.value("ok").toBool());
}
void InvestmentControllerTest::purchaseNeedsFreeMoneyUntilFunded() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(r.saveAutomaticCurrencyRates(false) && r.saveManualCurrencyRates(1,100,120));
     // A brokerage account with no money, and a fiat account with 1 500 RUB to fund it
     QVERIFY(r.insertAccount({"broker","Брокер",AssetType::Investment,AccountType::Brokerage,Currency::RUB,0}));
     QVERIFY(r.insertAccount({"cash","Сбер",AssetType::Fiat,AccountType::DebitCard,Currency::RUB,150'000}));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    const auto buy=[&](const QString& cash,const QString& fee) {
        return c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
            {"name",QStringLiteral("Акция")},{"symbol",QStringLiteral("SBER")},{"quantity","10"},{"averagePrice","100"},
            {"manualValuation",true},{"manualValue","1000"},{"purchase",true},{"cashAmount",cash},{"fee",fee},
            {"date",at.toString(Qt::ISODate)}});
    };
    // Without money on the brokerage account the purchase is refused, as a transfer would be
    QVERIFY(!buy("-1000","").value("ok").toBool());
    QCOMPARE(c.investmentPositions().size(),0);
    // The fiat account funds the amount first; the commission then takes the free money below zero
    QVERIFY(c.addTransfer(100'000,QString(),"cash","broker",at));
    const auto bought=buy("-1000","2");
    QVERIFY2(bought.value("ok").toBool(),qPrintable(bought.value("error").toString()));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(-200));
    // A negative brokerage account takes no transfers out, but money can still come in
    QVERIFY(!c.addTransfer(1,QString(),"broker","cash",at));
    QCOMPARE(accountBalance(c,"cash"),qint64(50'000));
    QVERIFY(c.addTransfer(10'000,QString(),"cash","broker",at));
    QCOMPARE(brokerage(c,"broker").value("freeNowMinor").toLongLong(),qint64(9'800));
}
void InvestmentControllerTest::manualPositionCannotCreateMoney() {
    QTemporaryDir dir;const auto path=dir.filePath("data.db");const auto at=QDateTime::currentDateTimeUtc().addSecs(-5);
    {FinanceRepository r(path);QVERIFY(seed(r));}
    FinanceController c(nullptr,path);c.setSelectedAsset("investment");
    const auto made=c.saveInvestmentPosition({{"accountId","broker"},{"searchIndex",-2},{"instrumentType",0},
        {"name",QStringLiteral("Акция")},{"symbol",QStringLiteral("MANUAL")},{"quantity","1"},{"manualValuation",true},{"manualValue","110"}});
    QVERIFY2(made.value("ok").toBool(),qPrintable(made.value("error").toString()));
    const auto id=c.investmentPositions().at(0).toMap().value("id").toString();
    const qint64 base=balance(c);
    // A manual position has no quote to check the money against: a purchase may not bring money in, a sale may not take it out
    auto result=c.recordInvestmentOperation({{"positionId",id},{"kind","buy"},{"date",at.toString(Qt::ISODate)},{"quantity","1"},
        {"cashAmount","100"},{"price","100"},{"remainingValue","110"}});
    QVERIFY(!result.value("ok").toBool());
    result=c.recordInvestmentOperation({{"positionId",id},{"kind","sell"},{"date",at.toString(Qt::ISODate)},{"quantity","1"},
        {"cashAmount","-100"},{"price","100"}});
    QVERIFY(!result.value("ok").toBool());
    QCOMPARE(balance(c),base);
}
QTEST_GUILESS_MAIN(InvestmentControllerTest)
#include "InvestmentControllerTest.moc"
