#include "InvestmentBenchmark.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <cmath>
#include <limits>
#include <optional>

QVariantList investmentBenchmarkIndices() {
    return {QVariantMap{{"key","index:IMOEX"},{"kind","index"},{"symbol","IMOEX"},{"name",QStringLiteral("Индекс Мосбиржи · цены")},{"currency","RUB"},{"board","SNDX"},{"totalReturn",false}},
            QVariantMap{{"key","index:MCFTR"},{"kind","index"},{"symbol","MCFTR"},{"name",QStringLiteral("Индекс Мосбиржи · полная доходность брутто")},{"currency","RUB"},{"board","RTSI"},{"totalReturn",true}}};
}
namespace {
QString currencyCode(QString code) {
    code=code.toUpper();if(code=="SUR" || code=="RUR")code="RUB";
    return QStringList{"RUB","USD","EUR","CNY"}.contains(code)?code:QString();
}
}
bool parseBenchmarkPage(const QByteArray& payload,const QVariantMap& descriptor,QVariantList& rows,int& next,QString& error) {
    QJsonParseError parseError;const auto doc=QJsonDocument::fromJson(payload,&parseError);next=-1;
    if(parseError.error!=QJsonParseError::NoError || !doc.isObject()){error=QStringLiteral("Мосбиржа вернула некорректные данные истории.");return false;}
    const auto table=doc.object().value("history").toObject();QMap<QString,int> cols;
    const auto names=table.value("columns").toArray();for(int i=0;i<names.size();++i)cols.insert(names[i].toString().toUpper(),i);
    for(const auto& required:{"BOARDID","TRADEDATE","CLOSE","CURRENCYID"})if(!cols.contains(required)){error=QStringLiteral("В истории нет необходимых полей цены или валюты.");return false;}
    if(!table.value("data").isArray()){error=QStringLiteral("В истории отсутствует список записей.");return false;}
    const auto cell=[&](const QJsonArray& row,const char* name){const auto i=cols.value(name,-1);return i>=0 && i<row.size()?row[i]:QJsonValue();};
    for(const auto& value:table.value("data").toArray()){
        const auto row=value.toArray();const auto date=QDate::fromString(cell(row,"TRADEDATE").toString(),Qt::ISODate);if(!date.isValid()){error=QStringLiteral("В истории найдена некорректная дата.");return false;}
        if(cols.contains("SECID") && !descriptor.value("symbol").toString().isEmpty() && cell(row,"SECID").toString()!=descriptor.value("symbol").toString()){
            error=QStringLiteral("Сервер вернул историю другого инструмента.");return false;
        }
        const auto board=cell(row,"BOARDID").toString();int priority=-1;
        if(descriptor.value("kind")=="index") {if(board==descriptor.value("board"))priority=10;}
        else for(const auto& b:descriptor.value("boards").toList()){
            const auto item=b.toMap();const auto from=QDate::fromString(item.value("from").toString(),Qt::ISODate),to=QDate::fromString(item.value("to").toString(),Qt::ISODate);
            if(item.value("id")==board && (!from.isValid() || date>=from) && (!to.isValid() || date<=to))priority=qMax(priority,item.value("primary").toBool()?20:10);
        }
        if(priority<0)continue;
        const auto session=cell(row,"TRADINGSESSION").toVariant().toInt();priority=priority*10+(session==3?3:0);
        const auto close=cell(row,"CLOSE");const auto code=currencyCode(cell(row,"CURRENCYID").toString());
        const long double scaled=static_cast<long double>(close.toDouble())*1'000'000;
        QVariant price;if(close.isDouble() && std::isfinite(scaled) && scaled>=1 && scaled<std::numeric_limits<qint64>::max())price=qint64(std::llround(scaled));
        rows.append(QVariantMap{{"price_date",date.toString(Qt::ISODate)},{"price_micros",price},{"currency",code},{"board",board},{"priority",priority},
            {"issues",price.isNull()?QStringLiteral("missing_close"):(code.isEmpty()?QStringLiteral("missing_currency"):QString())}});
    }
    const auto cursor=doc.object().value("history.cursor").toObject();QMap<QString,int> cursorCols;
    const auto cn=cursor.value("columns").toArray();for(int i=0;i<cn.size();++i)cursorCols.insert(cn[i].toString().toUpper(),i);
    const auto data=cursor.value("data").toArray();
    if(!cursorCols.contains("INDEX") || !cursorCols.contains("TOTAL") || !cursorCols.contains("PAGESIZE") || data.isEmpty()){error=QStringLiteral("Не удалось проверить полноту истории Мосбиржи.");return false;}
    const auto cr=data.first().toArray();const auto c=[&](const char* name){int i=cursorCols.value(name,-1);return i>=0 && i<cr.size()?cr[i].toInt(-1):-1;};
    const int start=c("INDEX"),total=c("TOTAL"),size=c("PAGESIZE");
    if(start<0 || total<0 || size<=0 || total>100000 || start>total || table.value("data").toArray().size()!=qMin(size,total-start)){error=QStringLiteral("Некорректная нумерация страниц истории.");return false;}
    if(start+size<total)next=start+size;return true;
}
bool mergeBenchmarkRows(const QVariantList& rows,QVariantList& prices,QString& error) {
    QMap<QString,QVariantMap> byDate;
    for(const auto& value:rows){const auto r=value.toMap();const auto date=r.value("price_date").toString();
        if(!byDate.contains(date) || r.value("priority").toInt()>byDate.value(date).value("priority").toInt())byDate[date]=r;
        else if(r.value("priority")==byDate[date].value("priority") && (r.value("price_micros")!=byDate[date].value("price_micros") || r.value("currency")!=byDate[date].value("currency"))){error=QStringLiteral("Для одной даты найдены неоднозначные цены ориентира.");return false;}
    }
    for(const auto& r:byDate)prices.append(r);return true;
}
QVariantMap compareInvestmentBenchmark(const QVariantList& portfolio,const QVariantList& prices,const QVariantList& rates,const QString& currency) {
    QVariantMap out{{"available",false},{"points",QVariantList()}};
    const auto fail=[&](const QString& reason){out["reason"]=reason;return out;};
    if(portfolio.size()<2)return fail(QStringLiteral("Нужны две завершённые дневные оценки портфеля и непрерывная история между ними."));
    QMap<QDate,QVariantMap> quotes;for(const auto& p:prices){const auto r=p.toMap();const auto d=QDate::fromString(r.value("price_date").toString(),Qt::ISODate);if(d.isValid())quotes[d]=r;}
    QMap<QString,QMap<QDate,QPair<qint64,qint64>>> rateTable;
    for(const auto& v:rates){const auto r=v.toMap();const auto date=QDate::fromString(r.value("rate_date").toString(),Qt::ISODate);
        if(!date.isValid() || r.value("source")=="estimated")continue;
        auto& entries=rateTable[r.value("currency").toString()];const auto observed=r.value("observed_at").toLongLong();
        if(!entries.contains(date) || observed>entries.value(date).second)entries[date]={r.value("rub_per_unit_micros").toLongLong(),observed};
    }
    const auto fx=[&](const QString& code,const QDate& date)->std::optional<long double>{
        if(code=="RUB")return 1'000'000.0L;
        const auto entries=rateTable.value(code);auto it=entries.upperBound(date);
        if(it==entries.begin())return {};--it;
        return it->first>0?std::optional<long double>(it->first):std::nullopt;
    };
    QVariantList points;std::optional<long double> base;QDate previous;int maximumQuoteAge=0;
    for(const auto& v:portfolio){const auto p=v.toMap();const auto date=QDate::fromString(p.value("date").toString(),Qt::ISODate);
        bool valid=false;const auto own=p.value("percent").toDouble(&valid);
        if(!date.isValid() || !valid || !std::isfinite(own) || (previous.isValid() && previous.daysTo(date)!=1))return fail(QStringLiteral("Дневная история портфеля неполна."));
        auto q=quotes.upperBound(date);if(q==quotes.begin())return fail(QStringLiteral("Нет цены ориентира на начало периода."));--q;
        maximumQuoteAge=qMax(maximumQuoteAge,int(q.key().daysTo(date)));
        if(q->value("price_micros").isNull() || !q->value("issues").toString().isEmpty())return fail(QStringLiteral("Для части периода цена или валюта ориентира неизвестна."));
        const auto code=q->value("currency").toString();const auto price=q->value("price_micros").toLongLong();if(price<=0)return fail(QStringLiteral("Некорректная цена ориентира."));
        long double converted=price;
        if(code!=currency){const auto from=fx(code,date),to=fx(currency,date);if(!from || !to)return fail(QStringLiteral("Для сравнения не найден исторический валютный курс."));converted*=*from / *to;}
        if(!std::isfinite(converted) || converted<=0)return fail(QStringLiteral("Цена после пересчёта выходит за допустимый диапазон."));
        if(!base)base=converted;const long double benchmark=(converted/ *base-1)*100;
        if(!std::isfinite(benchmark) || !std::isfinite(double(benchmark)))return fail(QStringLiteral("Доходность ориентира выходит за допустимый диапазон."));
        points.append(QVariantMap{{"date",date.toString(Qt::ISODate)},{"portfolio",own},{"benchmark",double(benchmark)},{"quoteDate",q.key().toString(Qt::ISODate)}});previous=date;
    }
    const auto first=points.first().toMap(),last=points.last().toMap();out["available"]=true;out["points"]=points;out["maximumQuoteAgeDays"]=maximumQuoteAge;
    out["from"]=first.value("date");out["to"]=last.value("date");out["portfolioPercent"]=last.value("portfolio");out["benchmarkPercent"]=last.value("benchmark");out["differencePoints"]=last.value("portfolio").toDouble()-last.value("benchmark").toDouble();return out;
}
