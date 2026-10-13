#include "FinanceController.h"
#include "../services/InvestmentBenchmark.h"
#include <QRegularExpression>

QVariantList FinanceController::benchmarkIndices() const {return investmentBenchmarkIndices();}
QVariantList FinanceController::benchmarkSearchResults() const {
    QVariantList out;for(int i=0;i<benchmarkSearchResults_.size();++i){const auto& f=benchmarkSearchResults_[i];out.append(QVariantMap{{"index",i},{"symbol",f.symbol()},{"name",f.name()}});}return out;
}
void FinanceController::initializeBenchmark(){
    connect(this,&FinanceController::performanceChanged,this,&FinanceController::rebuildBenchmarkComparison);
    connect(&benchmarkSearchProvider_,&MoexInvestmentProvider::searchSucceeded,this,[this](const auto& instruments){
        benchmarkSearchResults_.clear();for(const auto& f:instruments)if(f.type()==InvestmentInstrumentType::Fund || f.type()==InvestmentInstrumentType::Etf)benchmarkSearchResults_.append(f);
        benchmarkSearchBusy_=false;benchmarkSearchError_=benchmarkSearchResults_.isEmpty()?tr("Биржевые фонды по запросу не найдены."):QString();emit benchmarkChanged();
    });
    connect(&benchmarkSearchProvider_,&MoexInvestmentProvider::searchFailed,this,[this](const QString& error){benchmarkSearchBusy_=false;benchmarkSearchError_=error;emit benchmarkChanged();});
    connect(&benchmarkHistoryProvider_,&InvestmentBenchmarkProvider::failed,this,[this](const QString& error){benchmarkBusy_=false;benchmarkError_=error;emit benchmarkChanged();});
    connect(&benchmarkHistoryProvider_,&InvestmentBenchmarkProvider::succeeded,this,[this](const QVariantMap& descriptor,const QVariantList& prices,const QDate& from,const QDate& to){
        benchmarkBusy_=false;if(descriptor.value("key")!=benchmarkSelection_.value("key"))return;
        if(!repository_.saveBenchmarkHistory(descriptor,prices,from,to)){benchmarkError_=repository_.lastError();emit benchmarkChanged();return;}
        benchmarkSelection_=descriptor;benchmarkError_.clear();rebuildBenchmarkComparison();
    });
    rebuildBenchmarkComparison();
}
void FinanceController::cancelBenchmarkRequests(){
    benchmarkHistoryProvider_.cancel();benchmarkSearchProvider_.cancelAll();benchmarkBusy_=false;benchmarkSearchBusy_=false;
    benchmarkRequest_.clear();benchmarkSearchResults_.clear();benchmarkComparison_.clear();benchmarkError_.clear();benchmarkSearchError_.clear();emit benchmarkChanged();
}
QVariantList FinanceController::benchmarkPortfolioDates() const {
    QVariantList dates;const auto today=QDate::currentDate();
    for(const auto& p:investmentPerformance_.value("performanceSeries").toList())if(QDate::fromString(p.toMap().value("date").toString(),Qt::ISODate)<today)dates.append(p);
    return dates;
}
void FinanceController::rebuildBenchmarkComparison(){
    if(!valuationHistoryReady_ || !repository_.isOpen())return;
    auto descriptor=repository_.loadBenchmarkSelection();
    if(descriptor.value("kind")=="index"){
        QVariantMap valid;for(const auto& v:benchmarkIndices())if(v.toMap().value("key")==descriptor.value("key"))valid=v.toMap();descriptor=valid;
    }else if(!descriptor.isEmpty() && (descriptor.value("kind")!="fund" || !QRegularExpression("^[A-Z0-9_.-]{1,32}$").match(descriptor.value("symbol").toString()).hasMatch()
        || descriptor.value("key").toString()!="fund:"+descriptor.value("symbol").toString() || (descriptor.value("type").toInt()!=int(InvestmentInstrumentType::Etf) && descriptor.value("type").toInt()!=int(InvestmentInstrumentType::Fund))))descriptor.clear();
    const auto portfolio=benchmarkPortfolioDates();benchmarkSelection_=descriptor;
    const auto historyPeriod=benchmarkHistoryPeriod();
    const bool validPeriod=historyPeriod.first.isValid() && historyPeriod.second.isValid() && historyPeriod.first<=historyPeriod.second;
    const auto request=descriptor.value("key").toString()+"|"+historyPeriod.first.toString(Qt::ISODate)+"|"+historyPeriod.second.toString(Qt::ISODate);
    if(benchmarkBusy_ && (descriptor.isEmpty() || !validPeriod || request!=benchmarkRequest_)){benchmarkHistoryProvider_.cancel();benchmarkBusy_=false;benchmarkRequest_.clear();}
    benchmarkComparison_={{"available",false},{"points",QVariantList()},{"reason",tr("Выберите индекс или биржевой фонд.")}};
    if(!descriptor.isEmpty()){
        if(portfolio.size()<2)benchmarkComparison_["reason"]=tr("Для сравнения нужны две завершённые дневные оценки и непрерывная история. Текущий день не включён.");
        else {
            const auto from=QDate::fromString(portfolio.first().toMap().value("date").toString(),Qt::ISODate),to=QDate::fromString(portfolio.last().toMap().value("date").toString(),Qt::ISODate);
            const auto key=descriptor.value("key").toString();
            if(!benchmarkBusy_ && request!=benchmarkRequest_)benchmarkError_.clear();
            if(benchmarkBusy_ && request!=benchmarkRequest_){benchmarkHistoryProvider_.cancel();benchmarkBusy_=false;benchmarkRequest_.clear();}
            if(repository_.benchmarkHistoryCovered(key,from,to)){
                QVariantList rates;for(const auto& code:{"RUB","USD","EUR","CNY"})rates.append(repository_.loadFxHistory(code));
                benchmarkComparison_=compareInvestmentBenchmark(portfolio,repository_.loadBenchmarkPrices(key),rates,currencyCode(appCurrency_));
                if(!benchmarkBusy_)benchmarkRequest_=request;
            }else{
                benchmarkComparison_["reason"]=tr("Для этого периода ещё не загружена полная история ориентира.");
                if(!restoringDatabase_ && request!=benchmarkRequest_){benchmarkRequest_=request;benchmarkBusy_=true;benchmarkError_.clear();benchmarkRequestedFrom_=from.addDays(-31);benchmarkRequestedTo_=to;benchmarkHistoryProvider_.load(descriptor,benchmarkRequestedFrom_,to);}
            }
        }
    }
    benchmarkComparison_["canRefresh"]=!descriptor.isEmpty() && validPeriod;
    benchmarkComparison_["historyFrom"]=historyPeriod.first.toString(Qt::ISODate);
    benchmarkComparison_["historyTo"]=historyPeriod.second.toString(Qt::ISODate);
    benchmarkComparison_["historyLoaded"]=!descriptor.isEmpty() && validPeriod && repository_.benchmarkHistoryCovered(descriptor.value("key").toString(),historyPeriod.first,historyPeriod.second);
    benchmarkComparison_["historyFallback"]=dateFilterActive() && dateFilterFrom_>qMin(dateFilterTo_,QDate::currentDate().addDays(-1));
    if(!validPeriod && !descriptor.isEmpty())benchmarkComparison_["reason"]=tr("Для загрузки истории выберите период, в котором есть завершённые дни. Текущий день не включён.");
    emit benchmarkChanged();
}
void FinanceController::selectBenchmarkIndex(const QString& key){
    QVariantMap chosen;if(!key.isEmpty()){for(const auto& v:benchmarkIndices())if(v.toMap().value("key").toString()==key)chosen=v.toMap();if(chosen.isEmpty())return;}
    if(!repository_.saveBenchmarkSelection(chosen)){benchmarkError_=repository_.lastError();emit benchmarkChanged();return;}
    benchmarkHistoryProvider_.cancel();benchmarkBusy_=false;benchmarkRequest_.clear();benchmarkError_.clear();rebuildBenchmarkComparison();
}
void FinanceController::searchBenchmarkFunds(const QString& query){
    benchmarkSearchProvider_.cancelAll();benchmarkSearchResults_.clear();benchmarkSearchError_.clear();const auto text=query.trimmed();
    if(text.size()<2){benchmarkSearchBusy_=false;benchmarkSearchError_=tr("Введите хотя бы два символа названия или обозначения фонда.");emit benchmarkChanged();return;}
    benchmarkSearchBusy_=true;emit benchmarkChanged();benchmarkSearchProvider_.searchFunds(text);
}
void FinanceController::selectBenchmarkFund(int index){
    if(index<0 || index>=benchmarkSearchResults_.size())return;const auto& f=benchmarkSearchResults_[index];
    const QVariantMap descriptor{{"key","fund:"+f.symbol()},{"kind","fund"},{"symbol",f.symbol()},{"name",f.name()},{"type",int(f.type())},{"totalReturn",false}};
    if(!repository_.saveBenchmarkSelection(descriptor)){benchmarkError_=repository_.lastError();emit benchmarkChanged();return;}
    benchmarkHistoryProvider_.cancel();benchmarkBusy_=false;benchmarkRequest_.clear();benchmarkError_.clear();rebuildBenchmarkComparison();
}
QPair<QDate,QDate> FinanceController::benchmarkHistoryPeriod() const {
    const auto yesterday=QDate::currentDate().addDays(-1);
    const auto dates=benchmarkPortfolioDates();
    if(dates.size()>=2)return {QDate::fromString(dates.first().toMap().value("date").toString(),Qt::ISODate),QDate::fromString(dates.last().toMap().value("date").toString(),Qt::ISODate)};
    if(dateFilterActive()){
        const auto end=qMin(dateFilterTo_,yesterday);
        if(dateFilterFrom_<=end)return {dateFilterFrom_,end};
        return {yesterday.addYears(-1),yesterday};
    }
    const auto beginning=QDate::fromString(investmentPerformance_.value("from").toString(),Qt::ISODate);
    return {beginning.isValid() && beginning<=yesterday?beginning:yesterday.addYears(-1),yesterday};
}
void FinanceController::refreshBenchmark(){
    const auto period=benchmarkHistoryPeriod();
    if(benchmarkSelection_.isEmpty() || !period.first.isValid() || !period.second.isValid() || period.first>period.second || benchmarkBusy_)return;
    benchmarkRequestedFrom_=period.first.addDays(-31);benchmarkRequestedTo_=period.second;
    benchmarkBusy_=true;benchmarkError_.clear();benchmarkRequest_=benchmarkSelection_.value("key").toString()+"|"+period.first.toString(Qt::ISODate)+"|"+period.second.toString(Qt::ISODate);emit benchmarkChanged();
    benchmarkHistoryProvider_.load(benchmarkSelection_,benchmarkRequestedFrom_,benchmarkRequestedTo_);
}
