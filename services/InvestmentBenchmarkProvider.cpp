#include "InvestmentBenchmarkProvider.h"
#include "InvestmentBenchmark.h"
#include "MoexInvestmentParser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QMap>

namespace {
QNetworkRequest request(const QUrl& url){QNetworkRequest r(url);r.setHeader(QNetworkRequest::UserAgentHeader,QStringLiteral("Vexa/0.1"));r.setRawHeader("Accept","application/json");r.setTransferTimeout(15000);r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);return r;}
QString failure(QNetworkReply* r){const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();return r->error()!=QNetworkReply::NoError || status<200 || status>=300?QStringLiteral("Не удалось загрузить историю Мосбиржи: %1").arg(r->errorString()):QString();}
}
InvestmentBenchmarkProvider::InvestmentBenchmarkProvider(QObject* parent):QObject(parent){}
void InvestmentBenchmarkProvider::cancel(){++generation_;for(auto* r:network_.findChildren<QNetworkReply*>()){disconnect(r,nullptr,this,nullptr);r->abort();r->deleteLater();}}
void InvestmentBenchmarkProvider::load(QVariantMap descriptor,QDate from,QDate to){
    cancel();const auto generation=generation_;const auto symbol=descriptor.value("symbol").toString();
    if(!QRegularExpression("^[A-Z0-9_.-]{1,32}$").match(symbol).hasMatch() || !from.isValid() || !to.isValid() || from>to){emit failed(QStringLiteral("Некорректный ориентир или период."));return;}
    if(descriptor.value("kind")=="index"){page(descriptor,from,to,0,generation,{});return;}
    if(descriptor.value("kind")!="fund"){emit failed(QStringLiteral("Неизвестный вид ориентира."));return;}
    QUrl url("https://iss.moex.com/iss/securities/"+symbol+".json");QUrlQuery p;p.addQueryItem("iss.meta","off");p.addQueryItem("iss.only","description,boards");url.setQuery(p);
    auto* reply=network_.get(request(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,descriptor,from,to,generation]() mutable {
        const auto message=failure(reply);const auto payload=reply->readAll();reply->deleteLater();if(generation!=generation_)return;
        if(!message.isEmpty()){emit failed(message);return;}
        InvestmentMarketInstrument instrument("MOEX:"+descriptor.value("symbol").toString(),descriptor.value("symbol").toString(),{},descriptor.value("name").toString(),static_cast<InvestmentInstrumentType>(descriptor.value("type").toInt()),{});
        QString error;if(!parseMoexInstrumentDetails(payload,instrument,&error) || instrument.terms().engine!="stock" || instrument.terms().market!="shares"){emit failed(error.isEmpty()?QStringLiteral("Этот фонд не поддерживает сравнение по биржевой истории."):error);return;}
        const auto table=QJsonDocument::fromJson(payload).object().value("boards").toObject();QMap<QString,int> columns;
        const auto names=table.value("columns").toArray();for(int i=0;i<names.size();++i)columns.insert(names[i].toString(),i);
        const auto cell=[&](const QJsonArray& row,const char* name){int i=columns.value(name,-1);return i>=0 && i<row.size()?row[i]:QJsonValue();};
        int group=-1;for(const auto& v:table.value("data").toArray()){const auto row=v.toArray();if(cell(row,"boardid").toString()==instrument.primaryBoardId())group=cell(row,"board_group_id").toInt(-1);}
        QVariantList boards;for(const auto& v:table.value("data").toArray()){const auto row=v.toArray();if(group>=0 && cell(row,"board_group_id").toInt(-2)==group && cell(row,"engine").toString()=="stock" && cell(row,"market").toString()=="shares")
            boards.append(QVariantMap{{"id",cell(row,"boardid").toString()},{"from",cell(row,"history_from").toString()},{"to",cell(row,"history_till").toString()},{"primary",cell(row,"is_primary").toInt()==1}});}
        if(boards.isEmpty()){emit failed(QStringLiteral("Для фонда не найдены исторические режимы торгов."));return;}
        descriptor["boards"]=boards;
        for(const auto& v:table.value("data").toArray()){const auto row=v.toArray();if(cell(row,"boardid").toString()==instrument.primaryBoardId())descriptor["currency"]=cell(row,"currencyid").toString();}
        page(descriptor,from,to,0,generation,{});
    });
}
void InvestmentBenchmarkProvider::page(QVariantMap descriptor,QDate from,QDate to,int start,quint64 generation,QVariantList collected){
    const auto market=descriptor.value("kind")=="index"?QStringLiteral("index"):QStringLiteral("shares");
    QUrl url("https://iss.moex.com/iss/history/engines/stock/markets/"+market+"/securities/"+descriptor.value("symbol").toString()+".json");QUrlQuery p;
    p.addQueryItem("iss.meta","off");p.addQueryItem("iss.only","history,history.cursor");p.addQueryItem("history.columns","BOARDID,SECID,TRADEDATE,CLOSE,CURRENCYID,TRADINGSESSION");
    p.addQueryItem("from",from.toString(Qt::ISODate));p.addQueryItem("till",to.toString(Qt::ISODate));p.addQueryItem("start",QString::number(start));url.setQuery(p);
    auto* reply=network_.get(request(url));connect(reply,&QNetworkReply::finished,this,[this,reply,descriptor,from,to,start,generation,collected]() mutable {
        const auto message=failure(reply);const auto payload=reply->readAll();reply->deleteLater();if(generation!=generation_)return;
        if(!message.isEmpty()){emit failed(message);return;}
        if(payload.size()>4*1024*1024){emit failed(QStringLiteral("Ответ истории слишком велик."));return;}
        int next=-1;QString error;if(!parseBenchmarkPage(payload,descriptor,collected,next,error)){emit failed(error);return;}
        if(next>=0){if(next<=start || next>100000){emit failed(QStringLiteral("Не удалось завершить загрузку истории."));return;}page(descriptor,from,to,next,generation,collected);return;}
        QVariantList prices;if(!mergeBenchmarkRows(collected,prices,error)){emit failed(error);return;}
        for(const auto& v:prices){const auto d=QDate::fromString(v.toMap().value("price_date").toString(),Qt::ISODate);if(d<from || d>to){emit failed(QStringLiteral("Сервер вернул цену за пределами запрошенного периода."));return;}}
        emit succeeded(descriptor,prices,from,to);
    });
}
