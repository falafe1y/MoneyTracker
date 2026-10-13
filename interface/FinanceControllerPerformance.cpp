#include "FinanceController.h"
#include "../services/InvestmentPerformanceCalculator.h"
#include <QSet>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

void FinanceController::rebuildInvestmentPerformance()
{
    if(!valuationHistoryReady_ || !repository_.isOpen())return;
    InvestmentPerformanceInput input;
    performanceAccounts_.clear();
    for(const auto& value:repository_.loadPerformanceAccounts()){
        const auto row=value.toMap();
        input.accounts.append({row.value("id").toString(),row.value("name").toString(),row.value("currency").toString(),row.value("created_at").toLongLong(),row.value("initial_balance_minor").toLongLong()});
        performanceAccounts_.append(QVariantMap{{"id",row.value("id")},{"name",row.value("name")}});
    }
    if(!performanceAccountId_.isEmpty() && std::none_of(input.accounts.cbegin(),input.accounts.cend(),[this](const auto& a){return a.id==performanceAccountId_;}))performanceAccountId_.clear();
    input.accountId=performanceAccountId_;input.currency=currencyCode(appCurrency_);
    input.from=dateFilterActive()?dateFilterFrom_:QDate();input.to=dateFilterActive()?dateFilterTo_:QDate();input.now=QDateTime::currentDateTimeUtc();
    input.transactions=transactions_;input.operations=investmentOperations_;
    for(const auto& instrument:investmentInstruments_)
        input.instruments.append(QVariantMap{{"id",instrument.id()},{"symbol",instrument.symbol()},
                                            {"name",instrument.name()},{"type",int(instrument.type())}});
    input.valuations=repository_.loadDailyValuations(AssetDailyValuation::Investment, input.from.isValid()?input.from.addDays(-1):QDate(),input.to);
    input.positionHistory=repository_.loadAllPositionHistory();
    QSet<QString> instruments,currencies;currencies.insert(input.currency);
    for(const auto& a:input.accounts)currencies.insert(a.currency);
    for(const auto& value:input.positionHistory){const auto h=value.toMap();instruments.insert(h.value("instrument_id").toString());}
    for(const auto& id:instruments)input.prices.append(repository_.loadPriceHistory(AssetDailyValuation::Investment,id));
    for(const auto& value:input.prices)currencies.insert(value.toMap().value("currency").toString());
    for(const auto& currency:currencies)input.rates.append(repository_.loadFxHistory(currency));
    performanceInflation_=repository_.loadPerformanceInflation();
    bool valid=false;const auto inflation=performanceInflation_.toDouble(&valid);
    if(valid && std::isfinite(inflation) && inflation>-100)input.annualInflationPercent=inflation;
    performanceDeposit_=repository_.loadPerformanceDeposit();
    QString depositRate=performanceDeposit_.value("rate").toString();
    static const QRegularExpression depositPattern(QStringLiteral("^[0-9]+\\.[0-9]{4}$"));
    const auto mode=performanceDeposit_.value("capitalization",QStringLiteral("monthly")).toString();
    if(depositPattern.match(depositRate).hasMatch() && (mode=="none" || mode=="monthly" || mode=="daily")){
        depositRate.remove('.');bool ok=false;const auto units=depositRate.toLongLong(&ok);
        if(ok && units>=0 && units<=10'000'000)input.depositAnnualRateUnits=units;
    }
    input.depositCapitalization=mode;
    investmentPerformance_=InvestmentPerformanceCalculator::calculate(input);
    emit performanceChanged();
}
void FinanceController::setPerformanceAccount(const QString& id)
{
    if(!id.isEmpty() && std::none_of(performanceAccounts_.cbegin(),performanceAccounts_.cend(),[&](const auto& a){return a.toMap().value("id").toString()==id;}))return;
    performanceAccountId_=id;rebuildInvestmentPerformance();
}
bool FinanceController::setPerformanceInflation(const QString& percent)
{
    QString text=percent.trimmed();text.replace(',', '.');
    if(!text.isEmpty()){
        static const QRegularExpression pattern(QStringLiteral("^[+-]?[0-9]+(?:\\.[0-9]{1,4})?$"));
        bool valid=false;const auto n=text.toDouble(&valid);
        if(!pattern.match(text).hasMatch() || !valid || !std::isfinite(n) || n<=-100 || n>1000)return false;
        text=QString::number(n,'f',4);
    }
    if(!repository_.savePerformanceInflation(text))return false;
    rebuildInvestmentPerformance();return true;
}

bool FinanceController::setPerformanceDeposit(const QString& percent,const QString& capitalization)
{
    if(capitalization!="none" && capitalization!="monthly" && capitalization!="daily")return false;
    QString text=percent.trimmed();text.replace(',', '.');
    if(!text.isEmpty()){
        static const QRegularExpression pattern(QStringLiteral("^[0-9]+(?:\\.[0-9]{1,4})?$"));
        bool ok=false;const auto n=text.toDouble(&ok);
        if(!pattern.match(text).hasMatch() || !ok || !std::isfinite(n) || n<0 || n>1000)return false;
        text=QString::number(n,'f',4);
    }
    if(!repository_.savePerformanceDeposit({{"rate",text},{"capitalization",capitalization}}))return false;
    rebuildInvestmentPerformance();return true;
}
