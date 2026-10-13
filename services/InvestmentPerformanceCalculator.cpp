#include "InvestmentPerformanceCalculator.h"
#include "InvestmentValuation.h"
#include "InvestmentRiskCalculator.h"
#include "../core/InvestmentQuote.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QHash>
#include <QTimeZone>
#include <QLocale>
#include <cmath>
#include <limits>
#include <algorithm>

namespace {
QString tr(const char* text){return QCoreApplication::translate("InvestmentPerformance",text);}
std::optional<qint64> checked(__int128_t n){
    if(n>std::numeric_limits<qint64>::max() || n<std::numeric_limits<qint64>::min())return {};
    return static_cast<qint64>(n);
}
QVariant value(const std::optional<qint64>& n){return n?QVariant(*n):QVariant();}
QVariant real(const std::optional<long double>& n){return n && std::isfinite(*n)?QVariant(double(*n)):QVariant();}
std::optional<qint64> integer(const QVariant& v){return v.isNull()?std::nullopt:std::optional<qint64>(v.toLongLong());}
QJsonObject json(const QVariant& v){return QJsonDocument::fromJson(v.toString().toUtf8()).object();}
qint64 millis(const QVariantMap& row,const char* key){return row.value(QString::fromLatin1(key)).toLongLong();}
QDate day(qint64 at){return QDateTime::fromMSecsSinceEpoch(at,QTimeZone::UTC).toLocalTime().date();}
struct Point {QDate date; qint64 at=0; QMap<QString,QVariantMap> rows; std::optional<qint64> total;};
struct Event {qint64 at=0, amount=0;QString account,currency;bool income=false;QString source;};

class Calculation {
public:
    explicit Calculation(const InvestmentPerformanceInput& source):in(source){
        for(const auto& a:in.accounts)if(in.accountId.isEmpty() || in.accountId==a.id)accounts.insert(a.id,a);
        for(const auto& v:in.positionHistory){const auto row=v.toMap();
            histories[row.value("position_id").toString()].append(row);
            if(!row.value("operation_id").isNull())byOperation.insert(row.value("operation_id").toString(),row);
        }
        for(auto& history:histories)std::sort(history.begin(),history.end(),[](const auto& a,const auto& b){return millis(a,"sequence")<millis(b,"sequence");});
    }
    QVariantMap run();
private:
    const InvestmentPerformanceInput& in;
    QMap<QString,PerformanceAccount> accounts;
    QHash<QString,QVector<QVariantMap>> histories;
    QHash<QString,QVariantMap> byOperation;
    QStringList warnings;
    mutable QHash<QString,std::optional<qint64>> rateCache;
    QMap<QDate,Point> points;
    QVector<Event> flows,incomes;
    bool flowsComplete=true;
    QVector<QPair<qint64,QString>> unknownFlows;
    void warn(const QString& reason){if(!warnings.contains(reason))warnings.append(reason);}
    std::optional<qint64> rate(const QString& currency,const QDate& date,const QString& preferred={})const;
    std::optional<qint64> convert(qint64 money,const QString& currency,const QDate& date,const QString& preferred={})const;
    QVariantMap stateAt(const QString& id,qint64 observed)const;
    bool margined(const QVariantMap& row)const;
    std::optional<qint64> importedValue(const QVariantMap& h)const;
    void buildPoints();void buildFlows();
    std::optional<qint64> sumPoint(const Point& p,const QDate& conversionDate,bool validate=true);
    std::optional<qint64> unrealized(const Point& p,const QDate& conversionDate,bool& complete);
    QVariantMap assetInfo(const QVariantMap& row)const;
    QVariantMap operationAsset(const InvestmentOperation& op)const;
    QVariantMap allocation(const Point* closing);
    QVariantMap contributions(const Point* opening,const Point* closing,const QDate& baseDate,
                              const std::optional<qint64>& gain,const std::optional<qint64>& fx);
    QVariantMap payments(qint64 endAt,const QDate& to);
    QVariantMap dataQuality(const Point* opening,const Point* closing,const QDate& to,const QVariantMap& report);
};

std::optional<qint64> Calculation::rate(const QString& code,const QDate& date,const QString& preferred)const {
    if(code=="RUB")return qint64(1'000'000);
    const auto key=code+"|"+date.toString(Qt::ISODate)+"|"+preferred;
    if(rateCache.contains(key))return rateCache.value(key);
    QDate best; qint64 observed=-1;int bestPriority=-1;std::optional<qint64> answer;
    for(const auto& entry:in.rates){const auto r=entry.toMap();const auto d=QDate::fromString(r.value("rate_date").toString(),Qt::ISODate);
        const auto source=r.value("source").toString();
        if(r.value("currency").toString()!=code || !d.isValid() || d>date || source=="estimated")continue;
        const int priority=!preferred.isEmpty() && source==preferred?1:0;
        if(d>best || (d==best && (priority>bestPriority || (priority==bestPriority && millis(r,"observed_at")>observed)))){
            best=d;bestPriority=priority;observed=millis(r,"observed_at");answer=integer(r.value("rub_per_unit_micros"));
        }
    }
    if(answer && *answer<=0)answer.reset();
    rateCache.insert(key,answer);return answer;
}
std::optional<qint64> Calculation::convert(qint64 money,const QString& currency,const QDate& date,const QString& preferred)const {
    if(currency==in.currency || money==0)return money;
    const auto from=rate(currency,date,preferred),to=rate(in.currency,date,preferred);if(!from || !to)return {};
    const __int128_t n=__int128_t(money)* *from;
    return checked(n<0?-((-n+ *to/2)/ *to):(n+ *to/2)/ *to);
}
QVariantMap Calculation::stateAt(const QString& id,qint64 observed)const {
    QVariantMap found;for(const auto& row:histories.value(id))if(millis(row,"recorded_at")<=observed
        && (found.isEmpty() || millis(row,"sequence")>millis(found,"sequence")))found=row;
    return found;
}
bool Calculation::margined(const QVariantMap& row)const {
    auto details=json(row.value("details_json"));const auto id=row.value("source_id").toString().mid(9);
    const auto state=json(stateAt(id,millis(row,"observed_at")).value("state_json"));
    const auto terms=details.value("terms").toObject().isEmpty()?state.value("terms").toObject():details.value("terms").toObject();
    return terms.value("pricing").toString()=="future" || terms.value("pricing").toString()=="margined_option"
        || (state.value("type").toInt(-1)==9 && details.value("settings").toObject().value("manual").toBool());
}
std::optional<qint64> Calculation::importedValue(const QVariantMap& h)const {
    const auto s=json(h.value("state_json"));const auto settings=InvestmentPositionSettings::deserialize(QString::fromUtf8(QJsonDocument(s.value("settings").toObject()).toJson(QJsonDocument::Compact)));
    const auto terms=InvestmentTerms::fromJson(s.value("terms").toObject());
    if(settings.manualValuation)return settings.valuedAtUtc.isValid() && std::abs(settings.valuedAtUtc.toMSecsSinceEpoch()-millis(h,"recorded_at"))<=72LL*3600*1000?std::optional<qint64>(settings.manualValueMinor):std::nullopt;
    QVariantMap quote;
    for(const auto& entry:in.prices){const auto p=entry.toMap();if(p.value("asset_id")==h.value("instrument_id") && millis(p,"quoted_at")<=millis(h,"recorded_at")
        && (quote.isEmpty() || millis(p,"quoted_at")>millis(quote,"quoted_at")))quote=p;}
    if(quote.isEmpty() || millis(h,"recorded_at")-millis(quote,"quoted_at")>72LL*3600*1000)return {};
    const auto currency=h.value("currency").toString();const auto from=rate(terms.currencyCode,day(millis(h,"recorded_at"))),to=rate(currency,day(millis(h,"recorded_at")));
    qint64 fx=settings.manualFxRateMicros;
    if(fx<=0){if(terms.currencyCode==currency)fx=1'000'000;else if(from && to){const auto n=checked((__int128_t(*from)*1'000'000+ *to/2)/ *to);if(n)fx=*n;}}
    InvestmentPosition position(h.value("position_id").toString(),h.value("account_id").toString(),h.value("instrument_id").toString(),
        s.value("quantityMicros").toString().toLongLong(),s.value("averagePriceMicros").toString().toLongLong(),{},{},settings);
    InvestmentQuote q(position.instrumentId(),quote.value("price_micros").toLongLong(),QDateTime::fromMSecsSinceEpoch(millis(quote,"quoted_at"),QTimeZone::UTC),terms);
    const auto result=valueInvestmentPosition(position,&q,terms,fx);return result.available?std::optional<qint64>(result.valueMinor):std::nullopt;
}
void Calculation::buildPoints(){
    for(const auto& v:in.valuations){const auto r=v.toMap();if(!accounts.contains(r.value("account_id").toString()))continue;
        const auto date=QDate::fromString(r.value("snapshot_date").toString(),Qt::ISODate);if(!date.isValid())continue;
        auto& p=points[date];p.date=date;p.at=qMax(p.at,millis(r,"observed_at"));p.rows.insert(r.value("source_id").toString(),r);
    }

}
std::optional<qint64> Calculation::sumPoint(const Point& p,const QDate& conversionDate,bool validate){
    __int128_t total=0;
    for(const auto& a:accounts)if(a.createdAt<=p.at && !p.rows.contains("cash:"+a.id)){
        if(validate)warn(tr("В дневной оценке отсутствует денежный остаток счёта."));return {};
    }
    for(auto it=histories.cbegin();it!=histories.cend();++it){
        const auto h=stateAt(it.key(),p.at);
        if(!h.isEmpty() && accounts.contains(h.value("account_id").toString()) && h.value("quantity_micros").toLongLong()>0 && !p.rows.contains("position:"+it.key())){
            if(validate)warn(tr("В дневной оценке отсутствует открытая позиция."));return {};
        }
    }
    for(const auto& row:p.rows){
        if(millis(row,"observed_at")!=p.at){if(validate)warn(tr("Оценки счетов записаны на разное время."));return {};}
        if(validate){
            const auto issues=row.value("issues").toString().split(';',Qt::SkipEmptyParts);
            const bool needsFx=row.value("currency").toString()!=in.currency;
            for(const auto& issue:issues)if(issue=="history_changed" || issue=="missing_valuation" || issue=="missing_price" || issue=="value_overflow"
                || issue=="stale_valuation" || issue=="unknown_valuation_date" || issue=="missing_quote_fx" || (needsFx && (issue=="missing_fx" || issue=="estimated_fx" || issue=="stale_fx"))){
                warn(issue=="history_changed"?tr("Прошлые оценки требуют пересчёта после исправления истории."):tr("Для части периода нет достоверных цен или курсов."));return {};
            }
            const auto details=json(row.value("details_json")),settings=details.value("settings").toObject();
            const auto quoteCurrency=details.value("quoteCurrency").toString();
            if(!settings.value("manual").toBool() && !quoteCurrency.isEmpty() && quoteCurrency!=row.value("currency").toString()
                && settings.value("fx").toString().toLongLong()<=0 && !rate(quoteCurrency,p.date)){
                warn(tr("Неизвестен исторический курс валюты котировки."));return {};
            }
        }
        const auto native=integer(row.value("value_minor"));if(!native){if(validate)warn(tr("В дневной оценке есть неизвестная стоимость актива."));return {};}
        const auto money=convert(*native,row.value("currency").toString(),conversionDate,json(row.value("details_json")).value("fxSource").toString());
        if(!money){if(validate)warn(tr("Не найден исторический курс для выбранной валюты отчёта."));return {};}total+=*money;
    }
    return checked(total);
}
void Calculation::buildFlows(){
    QHash<QString,const Transaction*> txs;for(const auto& tx:in.transactions)txs.insert(tx.id(),&tx);
    for(const auto& tx:in.transactions){if(!accounts.contains(tx.accountId()))continue;
        Event e{tx.date().toMSecsSinceEpoch(),tx.type()==TransactionType::Income?tx.money().minorUnits():-tx.money().minorUnits(),tx.accountId(),currencyCode(tx.money().currency()),tx.categoryId()=="deposit_interest",{}};
        if(tx.categoryId()=="transfer-in" || tx.categoryId()=="transfer-out"){
            const bool incoming=tx.categoryId()=="transfer-in";const auto suffix=incoming?QStringLiteral("-in"):QStringLiteral("-out");
            const auto other=tx.id().endsWith(suffix)?txs.value(tx.id().left(tx.id().size()-suffix.size())+(incoming?"-out":"-in"),nullptr):nullptr;
            if(!other){unknownFlows.append({e.at,tr("У перевода отсутствует связанная операция.")});}
            else if(accounts.contains(other->accountId()))continue;
        }
        (e.income?incomes:flows).append(e);
    }
    for(const auto& a:accounts)if(a.initialMinor!=0)flows.append({a.createdAt,a.initialMinor,a.id,a.currency,false,{}});
    for(auto it=histories.cbegin();it!=histories.cend();++it){
        QVariantMap previous;
        for(const auto& h:it.value()){
            const auto kind=h.value("event").toString();const bool linked=!h.value("operation_id").isNull();
            if(accounts.contains(h.value("account_id").toString()) && !linked && (kind=="opening" || kind=="archive" || kind=="delete")){
                const auto amount=importedValue(h);
                const auto signedAmount=amount?((kind=="opening")?amount:checked(-__int128_t(*amount))):std::nullopt;
                if(signedAmount)flows.append({millis(h,"recorded_at"),*signedAmount,h.value("account_id").toString(),h.value("currency").toString(),false,"position:"+it.key()});
                else unknownFlows.append({millis(h,"recorded_at"),tr("Неизвестна стоимость переноса или удаления позиции.")});
            }else if(!linked && kind=="correction" && !previous.isEmpty()
                && (accounts.contains(h.value("account_id").toString()) || accounts.contains(previous.value("account_id").toString()))
                && (h.value("quantity_micros")!=previous.value("quantity_micros") || h.value("instrument_id")!=previous.value("instrument_id")
                    || h.value("account_id")!=previous.value("account_id")
                    || json(h.value("state_json")).value("settings").toObject().value("direction").toInt(1)!=json(previous.value("state_json")).value("settings").toObject().value("direction").toInt(1))){
                unknownFlows.append({millis(h,"recorded_at"),tr("Позиция перенесена или исправлена без связанной денежной операции.")});
            }
            previous=h;
        }
    }
}
std::optional<qint64> Calculation::unrealized(const Point& p,const QDate& conversionDate,bool& complete){
    __int128_t sum=0;complete=true;
    for(const auto& row:p.rows){if(!row.value("source_id").toString().startsWith("position:"))continue;
        const auto amount=integer(row.value("value_minor")),basis=integer(row.value("cost_basis_minor"));
        const auto state=json(stateAt(row.value("source_id").toString().mid(9),p.at).value("state_json"));
        const bool derivative=state.value("type").toInt(-1)==10 || state.value("type").toInt(-1)==9;
        if(!amount || (!margined(row) && (!basis || derivative))){complete=false;continue;}
        const auto net=checked(__int128_t(*amount)-(margined(row)?0:*basis));
        const auto converted=net?convert(*net,row.value("currency").toString(),conversionDate):std::nullopt;
        if(!converted){complete=false;continue;}sum+=*converted;
    }
    return checked(sum);
}

QVariantMap Calculation::assetInfo(const QVariantMap& row)const {
    const auto source=row.value("source_id").toString(),account=row.value("account_id").toString();
    if(source.startsWith("cash:"))return {{"id",source},{"label",tr("Деньги · ")+accounts.value(account).name},{"type",tr("Деньги")}};
    const auto h=stateAt(source.mid(9),millis(row,"observed_at"));
    const auto state=json(h.value("state_json"));
    auto id=row.value("instrument_id").toString();if(id.isEmpty())id=h.value("instrument_id").toString();
    QString label=state.value("symbol").toString();int type=state.value("type").toInt(-1);
    for(const auto& item:in.instruments){const auto instrument=item.toMap();if(instrument.value("id").toString()!=id)continue;
        if(label.isEmpty())label=instrument.value("symbol").toString();
        if(label.isEmpty())label=instrument.value("name").toString();
        if(type<0)type=instrument.value("type").toInt();
        break;
    }
    if(label.isEmpty())label=id.isEmpty()?tr("Позиция · ")+source.mid(9):id;
    const QStringList types{tr("Акции"),tr("Биржевые фонды"),tr("Облигации"),tr("Фонды"),tr("Другие активы"),
        tr("Привилегированные акции"),tr("Депозитарные расписки"),tr("Металлы"),tr("Валюта"),tr("Фьючерсы"),tr("Опционы")};
    return {{"id",id.isEmpty()?source:"instrument:"+id},{"label",label},{"typeCode",type},{"type",type>=0 && type<types.size()?types[type]:tr("Тип не указан")}};
}
QVariantMap Calculation::operationAsset(const InvestmentOperation& op)const {
    const auto h=byOperation.value(op.id);
    auto instrument=op.instrumentId;if(instrument.isEmpty())instrument=h.value("instrument_id").toString();
    auto position=op.positionId;if(position.isEmpty())position=h.value("position_id").toString();
    if(position.isEmpty() && instrument.isEmpty())return {{"id","unassigned:"+op.accountId},
        {"label",tr("Операции без привязки · ")+accounts.value(op.accountId).name},{"type",tr("Без привязки")}};
    return assetInfo({{"source_id","position:"+position},{"account_id",op.accountId},{"instrument_id",instrument},
                      {"observed_at",qMax(op.occurredAtUtc.toMSecsSinceEpoch(),millis(h,"recorded_at"))}});
}

QVariantMap Calculation::allocation(const Point* closing){
    QVariantMap result{{"available",false},{"pieAvailable",false},{"items",QVariantList()},{"adjustments",QVariantList()}};
    if(!closing)return result;
    struct Holding {QVariantMap info;__int128_t amount=0;bool complete=true;};
    QMap<QString,Holding> holdings,adjustments;
    QMap<QString,__int128_t> types,accountTotals,currencies;
    __int128_t capital=0,adjustment=0;bool positive=true;
    for(const auto& row:closing->rows){const auto info=assetInfo(row);const auto currency=row.value("currency").toString();
        const auto native=integer(row.value("value_minor"));
        std::optional<qint64> money;
        if(native)money=convert(*native,currency,closing->date,json(row.value("details_json")).value("fxSource").toString());
        const bool margin=row.value("source_id").toString().startsWith("position:") && margined(row);
        auto& item=(margin?adjustments:holdings)[info.value("id").toString()];item.info=info;
        if(!money){item.complete=false;continue;}
        item.amount+=*money;
        if(margin){adjustment+=*money;continue;}
        capital+=*money;positive=positive && *money>=0;
        types[info.value("type").toString()]+=*money;
        accountTotals[row.value("account_id").toString()]+=*money;
        currencies[currency]+=*money;
    }
    const auto denominator=checked(capital);const bool complete=bool(closing->total) && bool(denominator) && bool(checked(adjustment));
    const bool pie=complete && positive && *denominator>0;
    const auto rows=[&](const QMap<QString,Holding>& source,bool percentages){QVariantList list;
        for(const auto& item:source){auto row=item.info;const auto n=item.complete?checked(item.amount):std::nullopt;
            row["amountMinor"]=value(n);row["percent"]=percentages && n?QVariant(double(static_cast<long double>(*n)/ *denominator*100)):QVariant();
            list.append(row);}
        std::sort(list.begin(),list.end(),[](const auto& a,const auto& b){return a.toMap().value("amountMinor").toLongLong()>b.toMap().value("amountMinor").toLongLong();});return list;};
    const auto groups=[&](const QMap<QString,__int128_t>& source,bool accountNames=false){QVariantList list;for(auto it=source.cbegin();it!=source.cend();++it){const auto n=checked(it.value());
        list.append(QVariantMap{{"id",it.key()},{"label",accountNames?accounts.value(it.key()).name:it.key()},{"amountMinor",complete?value(n):QVariant()},
            {"percent",pie && n?QVariant(double(static_cast<long double>(*n)/ *denominator*100)):QVariant()}});}return list;};
    result["available"]=complete;result["pieAvailable"]=pie;result["date"]=closing->date.toString(Qt::ISODate);
    result["capitalMinor"]=complete?value(denominator):QVariant();result["adjustmentMinor"]=complete?value(checked(adjustment)):QVariant();
    result["totalMinor"]=value(closing->total);result["items"]=rows(holdings,pie);result["adjustments"]=rows(adjustments,false);
    result["types"]=groups(types);result["accounts"]=groups(accountTotals,true);result["currencies"]=groups(currencies);return result;
}

QVariantMap Calculation::contributions(const Point* opening,const Point* closing,const QDate& baseDate,
                                      const std::optional<qint64>& gain,const std::optional<qint64>& fx){
    QVariantMap result{{"available",false},{"items",QVariantList()}};
    if(!opening || !closing || !opening->total || !closing->total || closing->at<=opening->at || !flowsComplete || !gain || !fx)return result;
    struct Contribution {QVariantMap info;__int128_t amount=0;};QMap<QString,Contribution> assets;bool complete=true;
    const auto add=[&](const QVariantMap& info,const std::optional<qint64>& amount,int sign=1){
        auto& item=assets[info.value("id").toString()];item.info=info;if(amount)item.amount+=__int128_t(*amount)*sign;else complete=false;};
    for(const auto* point:{opening,closing})for(const auto& row:point->rows){
        if(!row.value("source_id").toString().startsWith("position:"))continue;
        const auto n=integer(row.value("value_minor"));add(assetInfo(row),n?convert(*n,row.value("currency").toString(),baseDate):std::nullopt,point==opening?-1:1);
    }
    for(const auto& op:in.operations)if(accounts.contains(op.accountId) && op.occurredAtUtc.toMSecsSinceEpoch()>opening->at && op.occurredAtUtc.toMSecsSinceEpoch()<=closing->at)
        add(operationAsset(op),convert(op.cashDeltaMinor,accounts.value(op.accountId).currency,baseDate));
    // Transfers of positions are external flows, just like cash deposits; they are not profit.
    for(const auto& e:flows)if(!e.source.isEmpty() && e.at>opening->at && e.at<=closing->at){
        const auto h=stateAt(e.source.mid(9),e.at);
        add(assetInfo({{"source_id",e.source},{"account_id",e.account},{"instrument_id",h.value("instrument_id")},{"observed_at",e.at}}),convert(e.amount,e.currency,baseDate),-1);
    }
    for(const auto& e:incomes)if(e.at>opening->at && e.at<=closing->at)
        add(assetInfo({{"source_id","cash:"+e.account},{"account_id",e.account}}),convert(e.amount,e.currency,baseDate));
    if(!complete)return result;
    QVariantList list;__int128_t sum=0;
    for(const auto& item:assets){const auto n=checked(item.amount);if(!n)return result;auto row=item.info;row["amountMinor"]=*n;list.append(row);sum+=*n;}
    std::sort(list.begin(),list.end(),[](const auto& a,const auto& b){return std::abs(a.toMap().value("amountMinor").toDouble())>std::abs(b.toMap().value("amountMinor").toDouble());});
    const auto remainder=checked(__int128_t(*gain)- *fx-sum);if(!remainder)return result;
    result["available"]=true;result["items"]=list;result["fxMinor"]=*fx;result["unexplainedMinor"]=*remainder;
    result["totalMinor"]=*gain;return result;
}

QVariantMap Calculation::payments(qint64 endAt,const QDate& to){
    struct Summary {__int128_t net=0,gross=0,fees=0,tax=0;bool netComplete=true,grossComplete=true,feesComplete=true,taxComplete=true;int count=0;QString label;};
    Summary total;QMap<QString,Summary> kinds,assets,months;QVariantList events;
    const auto add=[&](Summary& s,const std::optional<qint64>& n,const std::optional<qint64>& gross,
                       const std::optional<qint64>& fee,const std::optional<qint64>& tax){
        ++s.count;if(n)s.net+=*n;else s.netComplete=false;if(gross)s.gross+=*gross;else s.grossComplete=false;
        if(fee)s.fees+=*fee;else s.feesComplete=false;if(tax)s.tax+=*tax;else s.taxComplete=false;};
    const auto record=[&](QString id,QString kind,const QVariantMap& asset,const QString& account,const QString& currency,
                          qint64 at,qint64 native,std::optional<qint64> fee,std::optional<qint64> tax){
        const auto date=day(at);if(at>endAt || date>to || (in.from.isValid() && date<in.from))return;
        const auto n=convert(native,currency,date),f=fee?convert(*fee,currency,date):std::nullopt,t=tax?convert(*tax,currency,date):std::nullopt;
        const auto nativeGross=fee && tax?checked(__int128_t(native)+ *fee+ *tax):std::nullopt;
        const auto g=nativeGross?convert(*nativeGross,currency,date):std::nullopt;
        const auto key=asset.value("id").toString(),month=date.toString("yyyy-MM");
        auto& byKind=kinds[kind];byKind.label=kind=="dividend"?tr("Дивиденды"):kind=="coupon"?tr("Купоны"):tr("Проценты по вкладам");
        auto& byAsset=assets[key];byAsset.label=asset.value("label").toString();months[month].label=month;
        for(auto* summary:{&total,&byKind,&byAsset,&months[month]})add(*summary,n,g,f,t);
        events.append(QVariantMap{{"id",id},{"date",date.toString(Qt::ISODate)},{"at",at},{"kind",byKind.label},{"asset",byAsset.label},
            {"account",accounts.value(account).name},{"currency",currency},{"nativeNetMinor",native},{"netMinor",value(n)},
            {"grossMinor",value(g)},{"feesMinor",value(f)},{"taxMinor",value(t)}});
    };
    for(const auto& op:in.operations)if(accounts.contains(op.accountId) && (op.kind=="dividend" || op.kind=="coupon"))
        record(op.id,op.kind,operationAsset(op),op.accountId,accounts.value(op.accountId).currency,op.occurredAtUtc.toMSecsSinceEpoch(),op.cashDeltaMinor,op.feeMinor,op.taxMinor);
    for(const auto& tx:in.transactions)if(accounts.contains(tx.accountId()) && tx.categoryId()=="deposit_interest")
        record(tx.id(),"interest",assetInfo({{"source_id","cash:"+tx.accountId()},{"account_id",tx.accountId()}}),tx.accountId(),currencyCode(tx.money().currency()),tx.date().toMSecsSinceEpoch(),
            tx.type()==TransactionType::Income?tx.money().minorUnits():-tx.money().minorUnits(),{},{});
    const auto summary=[](const Summary& s){return QVariantMap{{"label",s.label},{"count",s.count},
        {"netMinor",s.netComplete?value(checked(s.net)):QVariant()},{"knownNetMinor",value(checked(s.net))},
        {"grossMinor",s.grossComplete?value(checked(s.gross)):QVariant()},{"feesMinor",s.feesComplete?value(checked(s.fees)):QVariant()},
        {"taxMinor",s.taxComplete?value(checked(s.tax)):QVariant()}};};
    const auto list=[&](const QMap<QString,Summary>& source){QVariantList rows;for(auto it=source.cbegin();it!=source.cend();++it){auto row=summary(it.value());row["id"]=it.key();rows.append(row);}return rows;};
    std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.toMap().value("at").toLongLong()>b.toMap().value("at").toLongLong();});
    auto result=summary(total);result["kinds"]=list(kinds);result["assets"]=list(assets);result["months"]=list(months);result["events"]=events;return result;
}

QVariantMap Calculation::dataQuality(const Point* opening,const Point* closing,const QDate& to,const QVariantMap& report){
    struct Problem {QVariantMap row;QSet<QString> dates;int count=0;};
    QMap<QString,Problem> problems;
    QSet<QDate> errorDates;
    const auto add=[&](const QString& code,const QString& severity,const QString& label,const QString& impact,const QString& action,
                       const QString& account=QString(),const QString& asset=QString(),const QDate& date=QDate(),const QString& extra=QString()){
        const auto key=code+"|"+account+"|"+asset+"|"+extra;auto& p=problems[key];
        p.row={{"code",code},{"severity",severity},{"label",label},{"impact",impact},{"action",action},
            {"accountId",account},{"account",accounts.value(account).name},{"asset",asset}};
        ++p.count;if(date.isValid())p.dates.insert(date.toString(Qt::ISODate));
        if(date.isValid() && severity=="error")errorDates.insert(date);
    };
    const auto snapshotIssue=[&](QString code,const QString& account,const QString& asset,const QDate& date,const QString& currency){
        if((code=="missing_fx" || code=="estimated_fx" || code=="stale_fx") && currency==in.currency)return;
        QString label,impact,action,severity="error";
        if(code=="cost_unknown"){
            severity="warning";label=tr("Неизвестна стоимость приобретения");impact=tr("Абсолютная прибыль открытой позиции недоступна. Доходность между достоверными оценками может быть доступна.");
            action=tr("Проверьте историю покупок и исходную стоимость позиции.");
        }else if(code=="history_changed"){
            label=tr("История исправлена после записи оценки");impact=tr("Эта дневная оценка не используется как достоверная для доходности.");
            action=tr("Проверьте исправленные операции. Ранее записанные оценки не восстанавливаются автоматически.");
        }else if(code=="missing_valuation" || code=="missing_price"){
            label=tr("Неизвестна стоимость или цена актива");impact=tr("Нельзя полностью оценить капитал и доходность на эту дату.");
            action=tr("Обновите котировку или задайте ручную оценку с её датой. Это обновит текущую оценку, а не пропущенную прошлую.");
        }else if(code=="stale_valuation" || code=="unknown_valuation_date"){
            label=code=="stale_valuation"?tr("Оценка актива устарела"):tr("Неизвестна дата оценки");impact=tr("Стоимость не считается достоверной для выбранной даты.");
            action=tr("Обновите текущую котировку или дату ручной оценки. Проверьте проблемные прошлые даты отдельно.");
        }else if(code=="missing_fx" || code=="missing_quote_fx" || code=="missing_report_fx"){
            label=tr("Не найден исторический курс валюты");impact=tr("Невозможно достоверно перевести сумму в валюту оценки или отчёта.");
            action=tr("Проверьте валюты и сохранённые курсы на дату записи. Обновление сегодняшнего курса не заменяет прошлый.");
        }else if(code=="estimated_fx" || code=="stale_fx"){
            label=code=="estimated_fx"?tr("Использован приблизительный курс"):tr("Валютный курс устарел");impact=tr("Перевод в валюту отчёта не считается достоверным.");
            action=tr("Проверьте сохранённый курс и его дату.");
        }else if(code=="value_overflow"){
            label=tr("Сумма выходит за допустимый диапазон");impact=tr("Стоимость или итог не вычисляются.");action=tr("Проверьте количество, цену, валютный курс и денежные суммы.");
        }else{
            severity="warning";label=tr("Дополнительное ограничение оценки");impact=tr("Для записи сохранён признак качества: ")+code;
            action=tr("Проверьте исходную оценку и пояснения к расчёту.");
        }
        add(code,severity,label,impact,action,account,asset,date);
    };
    const auto first=opening?opening->date:(in.from.isValid()?in.from.addDays(-1):to);
    int checkedRows=0;
    if(!opening)add("missing_opening","error",tr("Нет начальной дневной оценки"),tr("Недоступны доходность и вклад активов за период."),tr("Начальная оценка должна быть записана перед началом выбранного периода. При отсутствии истории выберите более поздний период."),{},{},in.from.isValid()?in.from.addDays(-1):QDate());
    if(!closing)add("missing_closing","error",tr("Нет конечной дневной оценки"),tr("Недоступен полный результат на конец периода."),tr("Оценки записываются при работающем приложении. Выберите дату с сохранённой оценкой."),{},{},to);
    if(opening && closing && closing->at<=opening->at)add("short_history","info",tr("Недостаточно дневных оценок"),tr("Одна оценка показывает стоимость, но не изменение капитала во времени."),tr("Нужны как минимум две оценки на разные даты; для волатильности — три. Приложение должно работать в дни записи оценок."));
    QDate gap;
    const auto flushGap=[&](const QDate& last){
        if(!gap.isValid())return;
        const auto count=gap.daysTo(last)+1;
        add("missing_days","error",tr("Пропущены дневные оценки"),tr("Непрерывная доходность, риск и сравнение за этот период недоступны."),
            tr("Выберите непрерывный период с оценками. История не достраивается текущими ценами."),{},{},gap,gap.toString(Qt::ISODate));
        auto& p=problems["missing_days|||"+gap.toString(Qt::ISODate)];p.count=int(count);p.row["dateTo"]=last.toString(Qt::ISODate);
        gap={};
    };
    // Walk existing dates rather than every calendar day, including long empty intervals.
    QDate expected=first;
    for(auto it=points.cbegin();it!=points.cend();++it){const auto& p=it.value();if(p.date<first || p.date>to)continue;
        if(expected<p.date){gap=expected;flushGap(p.date.addDays(-1));}expected=p.date.addDays(1);
        for(const auto& a:accounts)if(a.createdAt<=p.at && !p.rows.contains("cash:"+a.id))
            add("missing_cash","error",tr("Не записан денежный остаток счёта"),tr("Дневная оценка капитала неполная."),tr("Проверьте состав счёта и сохранённую историю оценок."),a.id,tr("Деньги"),p.date);
        for(auto h=histories.cbegin();h!=histories.cend();++h){const auto state=stateAt(h.key(),p.at);
            if(!state.isEmpty() && accounts.contains(state.value("account_id").toString()) && state.value("quantity_micros").toLongLong()>0 && !p.rows.contains("position:"+h.key())){
                const auto row=QVariantMap{{"source_id","position:"+h.key()},{"account_id",state.value("account_id")},{"instrument_id",state.value("instrument_id")},{"observed_at",p.at}};
                add("missing_position","error",tr("Не записана открытая позиция"),tr("Дневная оценка капитала неполная."),tr("Проверьте позицию и её историю на указанную дату."),state.value("account_id").toString(),assetInfo(row).value("label").toString(),p.date);
            }
        }
        for(const auto& row:p.rows){++checkedRows;
            const auto account=row.value("account_id").toString(),asset=assetInfo(row).value("label").toString(),currency=row.value("currency").toString();
            auto issues=row.value("issues").toString().split(';',Qt::SkipEmptyParts);issues.removeDuplicates();
            if(row.value("value_minor").isNull() && !issues.contains("missing_valuation") && !issues.contains("missing_price"))issues.append("missing_valuation");
            const bool position=row.value("source_id").toString().startsWith("position:");
            const auto state=json(stateAt(row.value("source_id").toString().mid(9),p.at).value("state_json"));
            const int type=state.value("type").toInt(assetInfo(row).value("typeCode",-1).toInt());
            if(position && margined(row))issues.removeAll("cost_unknown");
            if(position && !margined(row) && (type==9 || type==10)){
                issues.removeAll("cost_unknown");
                add("unsupported_basis","info",tr("Для контракта неприменим обычный учёт стоимости"),tr("Абсолютная прибыль по стоимости приобретения не рассчитывается; изменение достоверной оценки и денежные расчёты учитываются отдельно."),tr("Проверьте тип контракта и записанные денежные расчёты."),account,asset,p.date);
            }else if(position && !margined(row) && row.value("cost_basis_minor").isNull() && !issues.contains("cost_unknown"))issues.append("cost_unknown");
            if(millis(row,"observed_at")!=p.at)add("different_times","error",tr("Оценки записаны на разное время"),tr("Из этих строк нельзя получить единый достоверный капитал."),tr("Проверьте дневную оценку всех выбранных счетов."),account,asset,p.date);
            const auto n=integer(row.value("value_minor"));
            if(n && !convert(*n,currency,p.date,json(row.value("details_json")).value("fxSource").toString()) && !issues.contains("missing_fx"))issues.append("missing_report_fx");
            const auto details=json(row.value("details_json")),settings=details.value("settings").toObject();const auto quoteCurrency=details.value("quoteCurrency").toString();
            if(!settings.value("manual").toBool() && !quoteCurrency.isEmpty() && quoteCurrency!=currency && settings.value("fx").toString().toLongLong()<=0 && !rate(quoteCurrency,p.date) && !issues.contains("missing_quote_fx"))issues.append("missing_quote_fx");
            for(const auto& issue:issues)snapshotIssue(issue,account,asset,p.date,currency);
        }
        if(!p.total && !errorDates.contains(p.date))add("invalid_total","error",tr("Не удалось вычислить достоверный капитал"),tr("Доходность на эту дату недоступна."),tr("Проверьте цены, курсы, валюту котировки и диапазон сумм."),{},{},p.date);
    }
    if(expected<=to){gap=expected;flushGap(to);}
    const auto endAt=closing?closing->at:QDateTime(to.addDays(1),QTime(0,0),QTimeZone::systemTimeZone()).toMSecsSinceEpoch()-1;
    const auto included=[&](qint64 at){return at<=endAt && day(at)<=to && (!in.from.isValid() || day(at)>=in.from);};
    for(const auto& op:in.operations)if(accounts.contains(op.accountId) && included(op.occurredAtUtc.toMSecsSinceEpoch())){
        const auto asset=operationAsset(op).value("label").toString();const auto d=op.occurredAtUtc.toLocalTime().date();
        if(!op.feeMinor && op.kind!="fee")add("unknown_fee","warning",tr("Комиссия не записана отдельно"),tr("Чистое денежное движение известно, но полный отдельный итог комиссий и сумма выплаты до расходов недоступны."),tr("При записи операции укажите известную комиссию отдельно, а её отсутствие — явным нулём. Старые записи сверяйте с выпиской."),op.accountId,asset,d);
        if(!op.taxMinor)add("unknown_tax","warning",tr("Удержание не записано отдельно"),tr("Чистое денежное движение известно, но полный отдельный итог удержаний и сумма выплаты до расходов недоступны."),tr("При записи операции укажите известное удержание отдельно, а его отсутствие — явным нулём. Старые записи сверяйте с выпиской."),op.accountId,asset,d);
        if(!convert(op.cashDeltaMinor,accounts.value(op.accountId).currency,d))snapshotIssue("missing_report_fx",op.accountId,asset,d,accounts.value(op.accountId).currency);
        if(op.kind=="sell" || op.kind=="expiry" || op.kind=="amortization"){
            const auto h=byOperation.value(op.id);const auto state=json(h.value("state_json"));const auto pricing=state.value("terms").toObject().value("pricing").toString();
            const bool margin=pricing=="future" || pricing=="margined_option" || (state.value("type").toInt(-1)==9 && state.value("settings").toObject().value("manual").toBool());
            if(!margin && h.value("realized_net_minor").isNull())add("unknown_sale_cost","warning",tr("Неизвестна прибыль закрытой части позиции"),tr("Полный итог прибыли после продаж недоступен. Вклад между оценками может быть известен."),tr("Проверьте покупки, стоимость проданной части и связь продажи с историей позиции."),op.accountId,asset,d);
        }
    }
    for(const auto& flow:unknownFlows)if(included(flow.first) && (!opening || flow.first>opening->at))
        add("unknown_flow","error",flow.second,tr("Нельзя отделить изменение капитала от пополнений и выводов."),tr("Проверьте связанные переводы, переносы и исправления позиции."),{},{},day(flow.first),flow.second);
    for(const auto* events:{&flows,&incomes})for(const auto& e:*events)if(included(e.at) && (!opening || e.at>opening->at) && !convert(e.amount,e.currency,day(e.at)))
        snapshotIssue("missing_report_fx",e.account,tr("Денежный поток"),day(e.at),e.currency);
    for(const auto& tx:in.transactions)if(accounts.contains(tx.accountId()) && tx.categoryId()=="deposit_interest" && included(tx.date().toMSecsSinceEpoch()))
        add("deposit_net_only","info",tr("Проценты по вкладу записаны чистой суммой"),tr("Полученная сумма известна, но расходы и сумма до удержаний не записаны отдельно."),tr("Сверьте чистую сумму с выпиской. Этот вид записи не содержит отдельных полей расходов."),tx.accountId(),tr("Проценты по вкладу"),tx.date().toLocalTime().date());
    if(opening && closing && closing->at>opening->at && errorDates.isEmpty() && !report.value("gainMinor").isNull() && report.value("periodPercent").isNull())
        add("return_base","info",tr("Нет подходящей базы для процентной доходности"),tr("Денежный результат может быть известен, но цепочку процентной доходности построить не удалось."),tr("Проверьте наличие положительного капитала и дневные пополнения и выводы. Пустые дни не создают доходность."));
    const auto risk=report.value("risk").toMap();
    if(risk.value("available").toBool() && risk.value("returnDays").toInt()<2)
        add("short_risk_history","info",tr("Для волатильности ещё мало наблюдений"),tr("Доходность и просадка могут быть доступны, но волатильность пока не вычисляется."),tr("Нужны минимум два дня доходности с положительной базой — как минимум три дневные оценки."));
    for(const auto& component:report.value("breakdown").toList()){const auto row=component.toMap();if(row.value("code")!="unexplained" || row.value("amountMinor").isNull())continue;
        const auto difference=row.value("amountMinor").toLongLong();if(std::abs(static_cast<long double>(difference))>qMax<long double>(5,std::abs(report.value("gainMinor").toDouble())*0.000001L))
            add("unexplained_result","warning",tr("Разложение не объясняет весь результат"),tr("Сумма отдельных составляющих отличается от результата периода; разница оставлена отдельной строкой."),tr("Проверьте историю позиций, записанные выплаты и расходы."));
    }
    QVariantList items;int errors=0,notices=0,notes=0;
    for(auto& p:problems){auto dates=p.dates.values();std::sort(dates.begin(),dates.end());p.row["dates"]=dates;p.row["count"]=p.count;
        p.row["dateFrom"]=dates.isEmpty()?QString():dates.first();if(!p.row.contains("dateTo"))p.row["dateTo"]=dates.isEmpty()?QString():dates.last();
        const auto severity=p.row.value("severity").toString();if(severity=="error")++errors;else if(severity=="warning")++notices;else ++notes;items.append(p.row);
    }
    const auto rank=[](QString s){return s=="error"?0:s=="warning"?1:2;};
    std::stable_sort(items.begin(),items.end(),[&](const auto& a,const auto& b){return rank(a.toMap().value("severity").toString())<rank(b.toMap().value("severity").toString());});
    return {{"checkedRows",checkedRows},{"errors",errors},{"warnings",notices},{"notes",notes},{"items",items},{"from",first.toString(Qt::ISODate)},{"to",to.toString(Qt::ISODate)}};
}

QVariantMap Calculation::run(){
    QVariantMap result{{"currency",in.currency},{"warnings",QStringList()},{"accountCount",accounts.size()}};
    if(accounts.isEmpty()){
        result["warnings"]=QStringList{tr("Добавьте инвестиционный счёт.")};
        result["dataQuality"]=QVariantMap{{"checkedRows",0},{"errors",0},{"warnings",0},{"notes",1},
            {"items",QVariantList{QVariantMap{{"code","no_accounts"},{"severity","info"},{"label",tr("Нет инвестиционных счетов")},
                {"impact",tr("Инвестиционная аналитика пока недоступна.")},{"action",tr("Добавьте брокерский счёт или вклад.")},{"count",1}}}}};
        return result;
    }
    buildPoints();buildFlows();
    std::sort(flows.begin(),flows.end(),[](const auto& a,const auto& b){return a.at<b.at;});
    const auto today=in.now.toLocalTime().date();const auto to=in.to.isValid()?qMin(in.to,today):today;
    const Point* closing=points.contains(to)?&points[to]:nullptr;
    const Point* opening=nullptr;
    if(in.from.isValid()) {const auto before=in.from.addDays(-1);if(points.contains(before))opening=&points[before];}
    else if(!points.isEmpty() && points.firstKey()<=to)opening=&points.first();
    if(!closing)warn(tr("Нет дневной оценки на конец выбранного периода."));
    if(!opening)warn(tr("Нет дневной оценки перед началом выбранного периода."));
    const auto startAt=opening?opening->at:std::numeric_limits<qint64>::min();
    const auto endAt=closing?closing->at:QDateTime(to.addDays(1),QTime(0,0),QTimeZone::systemTimeZone()).toMSecsSinceEpoch()-1;
    const auto window=[&](qint64 at){return at<=endAt && (!in.from.isValid() || day(at)>=in.from) && day(at)<=to;};
    const auto interval=[&](qint64 at){return at>startAt && at<=endAt;};
    for(auto it=points.begin();it!=points.end();++it)if(it->date>= (opening?opening->date:(in.from.isValid()?in.from:to)) && it->date<=to)it->total=sumPoint(*it,it->date);
    for(const auto& e:unknownFlows)if(interval(e.first) && window(e.first)){flowsComplete=false;warn(e.second);}
    const QDate baseDate=opening?opening->date:to;
    bool soldComplete=true,chargesComplete=true,incomeComplete=true,openingComplete=true,closingComplete=true;
    bool openingBaseComplete=true,closingBaseComplete=true;
    __int128_t sold=0,soldBase=0,income=0,fee=0,tax=0,tradeCharges=0;
    // Realized profit cards follow the date filter even when daily return history is unavailable.
    for(const auto& op:in.operations){if(!accounts.contains(op.accountId) || !window(op.occurredAtUtc.toMSecsSinceEpoch()))continue;
        const auto h=byOperation.value(op.id);const auto state=json(h.value("state_json"));const auto terms=state.value("terms").toObject();
        const bool marginedOperation=terms.value("pricing").toString()=="future" || terms.value("pricing").toString()=="margined_option"
            || (state.value("type").toInt(-1)==9 && state.value("settings").toObject().value("manual").toBool());
        const bool payout=op.kind=="coupon" || op.kind=="dividend";
        const bool trading=op.kind=="buy" || op.kind=="sell" || op.kind=="expiry" || op.kind=="amortization" || op.kind=="margin";
        const auto currency=accounts.value(op.accountId).currency;
        const auto when=op.occurredAtUtc.toLocalTime().date();
        std::optional<qint64> pnl;
        if(trading && marginedOperation)pnl=op.cashDeltaMinor;
        else if(op.kind=="sell" || op.kind=="expiry" || op.kind=="amortization")pnl=integer(h.value("realized_net_minor"));
        if(trading && (marginedOperation || op.kind!="buy")){
            const auto converted=pnl?convert(*pnl,currency,when):std::nullopt;
            if(converted)sold+=*converted;else soldComplete=false;
            const auto fixed=pnl?convert(*pnl,currency,baseDate):std::nullopt;
            if(fixed && interval(op.occurredAtUtc.toMSecsSinceEpoch()))soldBase+=*fixed;
        }
        if(!interval(op.occurredAtUtc.toMSecsSinceEpoch()))continue;
        if(payout){const auto n=convert(op.cashDeltaMinor,currency,baseDate);if(n)income+=*n;else incomeComplete=false;}
        const auto addCharge=[&](const std::optional<qint64>& amount,__int128_t& total){
            if(!amount){chargesComplete=false;return std::optional<qint64>();}
            const auto converted=convert(*amount,currency,baseDate);if(converted)total+=*converted;else chargesComplete=false;return converted;
        };
        auto knownFee=op.feeMinor;
        if(op.kind=="fee" && !knownFee && op.cashDeltaMinor!=std::numeric_limits<qint64>::min())knownFee=checked(-__int128_t(op.cashDeltaMinor)-op.taxMinor.value_or(0));
        const auto f=addCharge(knownFee,fee),t=addCharge(op.taxMinor,tax);
        if(trading){if(f)tradeCharges+=*f;if(t)tradeCharges+=*t;}
        if(payout){if(f)income+=*f;if(t)income+=*t;}
    }
    for(const auto& e:incomes)if(interval(e.at)){const auto n=convert(e.amount,e.currency,baseDate);if(n)income+=*n;else incomeComplete=false;}
    result["realizedMinor"]=soldComplete?value(checked(sold)):QVariant();
    result["knownRealizedMinor"]=value(checked(sold));
    std::optional<qint64> endUnrealized,startUnrealized,endBaseUnrealized,startBaseUnrealized;
    if(closing){endUnrealized=unrealized(*closing,closing->date,closingComplete);endBaseUnrealized=unrealized(*closing,baseDate,closingBaseComplete);}
    if(opening){startUnrealized=unrealized(*opening,opening->date,openingComplete);startBaseUnrealized=unrealized(*opening,baseDate,openingBaseComplete);}
    result["unrealizedMinor"]=closing && closing->total && closingComplete?value(endUnrealized):QVariant();result["knownUnrealizedMinor"]=value(endUnrealized);
    if(!soldComplete)warn(tr("Для части продаж неизвестна стоимость приобретения."));
    if(!closingComplete || !openingComplete)warn(tr("Для части позиций неизвестна стоимость приобретения или её метод неприменим."));
    std::optional<qint64> gain,gainBase,netFlow,fx,unrealizedChange;
    std::optional<long double> period,annual,inflationAdjusted;
    InvestmentRisk risk;
    QVariantList performanceSeries;
    bool valuationsComplete=opening && closing && opening->total && closing->total && closing->at>opening->at;
    bool consecutive=valuationsComplete;
    if(valuationsComplete){
        __int128_t external=0,fixedExternal=0;
        for(const auto& e:flows)if(interval(e.at)){
            const auto n=convert(e.amount,e.currency,day(e.at)),fixed=convert(e.amount,e.currency,baseDate);
            if(!n || !fixed){flowsComplete=false;warn(tr("Не найден курс валюты денежного потока."));}
            else {external+=*n;fixedExternal+=*fixed;}
        }
        netFlow=checked(external);
        if(flowsComplete && netFlow)gain=checked(__int128_t(*closing->total)- *opening->total-external);
        const auto endAtFixed=sumPoint(*closing,baseDate,false);
        if(endAtFixed && flowsComplete)gainBase=checked(__int128_t(*endAtFixed)- *opening->total-fixedExternal);
        if(gain && gainBase)fx=checked(__int128_t(*gain)- *gainBase);
        long double factor=1;bool fundedDay=false;
        std::vector<InvestmentReturnDay> dailyReturns;
        QVariantList candidateSeries{QVariantMap{{"date",opening->date.toString(Qt::ISODate)},{"percent",0.0}}};
        auto previous=points.find(opening->date);
        for(auto it=std::next(previous);it!=points.end() && it->date<=closing->date;++it){
            if(previous->date.daysTo(it->date)!=1 || !previous->total || !it->total){consecutive=false;break;}
            __int128_t inflow=0,outflow=0;
            auto event=std::upper_bound(flows.cbegin(),flows.cend(),previous->at,[](qint64 at,const Event& e){return at<e.at;});
            for(;event!=flows.cend() && event->at<=it->at;++event){const auto n=convert(event->amount,event->currency,day(event->at));if(!n){consecutive=false;break;}
                if(*n>=0)inflow+=*n;else outflow-=__int128_t(*n);
            }
            const long double denominator=static_cast<long double>(*previous->total)+static_cast<long double>(inflow);
            const long double numerator=static_cast<long double>(*it->total)+static_cast<long double>(outflow);
            if(!consecutive || numerator<0){consecutive=false;break;}
            if(denominator==0 && numerator==0){dailyReturns.push_back({it->date.toString(Qt::ISODate).toStdString(),1,false});candidateSeries.append(QVariantMap{{"date",it->date.toString(Qt::ISODate)},{"percent",double((factor-1)*100)}});previous=it;continue;}
            if(denominator<=0){consecutive=false;break;}
            fundedDay=true;
            const auto dailyFactor=numerator/denominator;
            dailyReturns.push_back({it->date.toString(Qt::ISODate).toStdString(),dailyFactor,true});
            factor*=dailyFactor;candidateSeries.append(QVariantMap{{"date",it->date.toString(Qt::ISODate)},{"percent",double((factor-1)*100)}});previous=it;
        }
        if(consecutive && fundedDay && flowsComplete && std::isfinite(factor)){
            period=(factor-1)*100;performanceSeries=candidateSeries;
            risk=calculateInvestmentRisk(opening->date.toString(Qt::ISODate).toStdString(),dailyReturns);
            const long double days=(closing->at-opening->at)/86400000.0L;
            const long double yearly=std::pow(factor,365.25L/days)-1;
            if(std::isfinite(yearly))annual=yearly*100;
            if(in.annualInflationPercent && *in.annualInflationPercent>-100){const auto adjusted=factor/std::pow(1+ *in.annualInflationPercent/100.0L,days/365.25L)-1;
                if(std::isfinite(adjusted))inflationAdjusted=adjusted*100;}
        }else warn(tr("Для доходности нужна непрерывная история дневных оценок и денежных потоков."));
    }else warn(tr("Для доходности нужны достоверные оценки как минимум на две даты."));
    if(endUnrealized && startUnrealized && closingComplete && openingComplete)unrealizedChange=checked(__int128_t(*endUnrealized)- *startUnrealized);
    result["gainMinor"]=value(gain);result["externalFlowMinor"]=value(netFlow);result["periodPercent"]=real(period);
    result["annualPercent"]=real(annual);result["realPercent"]=real(inflationAdjusted);result["unrealizedChangeMinor"]=value(unrealizedChange);
    result["openingMinor"]=opening?value(opening->total):QVariant();result["closingMinor"]=closing?value(closing->total):QVariant();
    result["from"]=opening?opening->date.toString(Qt::ISODate):QString();result["to"]=closing?closing->date.toString(Qt::ISODate):QString();
    int count=0;for(const auto& p:points)if(p.date>=baseDate && p.date<=to)++count;
    result["observationCount"]=count;
    result["inflationPercent"]=in.annualInflationPercent?QVariant(*in.annualInflationPercent):QVariant();
    result["performanceSeries"]=performanceSeries;
    QVariantList drawdowns;
    for(const auto& p:risk.drawdowns)drawdowns.append(QVariantMap{{"date",QString::fromStdString(p.date)},{"percent",double(p.percent)}});
    result["risk"]=QVariantMap{
        {"available",risk.available},{"returnDays",risk.returnDays},
        {"annualVolatilityPercent",real(risk.annualVolatilityPercent)},
        {"maximumDrawdownPercent",risk.available?QVariant(double(risk.maximumDrawdownPercent)):QVariant()},
        {"currentDrawdownPercent",risk.available?QVariant(double(risk.currentDrawdownPercent)):QVariant()},
        {"bestDayPercent",real(risk.bestDayPercent)},{"worstDayPercent",real(risk.worstDayPercent)},
        {"bestDay",QString::fromStdString(risk.bestDay)},{"worstDay",QString::fromStdString(risk.worstDay)},
        {"peakDate",QString::fromStdString(risk.peakDate)},{"troughDate",QString::fromStdString(risk.troughDate)},
        {"recoveryDate",QString::fromStdString(risk.recoveryDate)},{"drawdowns",drawdowns}};
    QVariantList breakdown;__int128_t explained=0;
    const auto component=[&](const QString& code,const QString& label,const std::optional<qint64>& amount,bool complete){
        breakdown.append(QVariantMap{{"code",code},{"label",label},{"amountMinor",value(amount)},{"complete",complete}});if(amount)explained+=*amount;
    };
    std::optional<qint64> positionsResult;
    if(endBaseUnrealized && startBaseUnrealized)positionsResult=checked(soldBase+__int128_t(*endBaseUnrealized)- *startBaseUnrealized+tradeCharges);
    component("positions",tr("Продажи и переоценка позиций"),positionsResult,soldComplete && openingBaseComplete && closingBaseComplete && chargesComplete);
    component("income",tr("Дивиденды, купоны и проценты"),checked(income),incomeComplete && chargesComplete);
    component("fees",tr("Комиссии"),checked(-fee),chargesComplete);component("tax",tr("Удержания"),checked(-tax),chargesComplete);
    component("fx",tr("Валютная переоценка счетов"),fx,bool(fx));
    const auto residual=gain?checked(__int128_t(*gain)-explained):std::nullopt;
    component("unexplained",tr("Необъяснённая разница"),residual,bool(residual));
    if(residual && std::abs(static_cast<long double>(*residual))>qMax<long double>(5,std::abs(static_cast<long double>(gain.value_or(0)))*0.000001L))
        warn(tr("Разложение результата неполное: часть изменения капитала не объяснена записями."));
    if(!chargesComplete)warn(tr("Часть комиссий и удержаний не указана отдельно."));
    result["allocation"]=allocation(closing);
    result["contributions"]=contributions(opening,closing,baseDate,gain,fx);
    result["payments"]=payments(endAt,to);
    result["breakdown"]=breakdown;result["warnings"]=warnings;result["quality"]=warnings.isEmpty()?"complete":"partial";
    result["dataQuality"]=dataQuality(opening,closing,to,result);
    return result;
}
}
QVariantMap InvestmentPerformanceCalculator::calculate(const InvestmentPerformanceInput& input){return Calculation(input).run();}
