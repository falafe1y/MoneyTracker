#include "MoexInvestmentProvider.h"

#include "MoexInvestmentParser.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>

namespace
{
QNetworkRequest requestFor(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("Ledgera/0.1 (personal finance tracker)"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15'000);
    return request;
}

QString replyError(QNetworkReply* reply, const QString& operation)
{
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        return QStringLiteral("%1: %2").arg(operation, reply->errorString());
    }
    if (status < 200 || status >= 300) {
        return QStringLiteral("%1: сервер вернул HTTP %2").arg(operation).arg(status);
    }
    return {};
}
}

MoexInvestmentProvider::MoexInvestmentProvider(QObject* parent)
    : QObject(parent)
{
}


void MoexInvestmentProvider::cancelAll()
{
    ++searchGeneration_;
    const auto replies = network_.findChildren<QNetworkReply*>();
    for (auto* reply : replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

void MoexInvestmentProvider::search(const QString& query)
{
    searchPage(query.trimmed(), 0, ++searchGeneration_, {});
}

void MoexInvestmentProvider::searchPage(const QString& query, int start, quint64 generation,
    QVector<InvestmentMarketInstrument> collected)
{
    QUrl url(QStringLiteral("https://iss.moex.com/iss/securities.json")); QUrlQuery p;
    p.addQueryItem("q",query); p.addQueryItem("lang","ru"); p.addQueryItem("iss.meta","off");
    p.addQueryItem("iss.only","securities"); p.addQueryItem("limit","100"); p.addQueryItem("start",QString::number(start));
    p.addQueryItem("securities.columns","secid,shortname,name,isin,type,group,primary_boardid,is_traded"); url.setQuery(p);
    auto* reply=network_.get(requestFor(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,query,start,generation,collected]() mutable {
        const auto message=replyError(reply,QStringLiteral("Не удалось выполнить поиск"));
        const auto payload=reply->readAll(); reply->deleteLater(); if(generation!=searchGeneration_)return;
        if(!message.isEmpty()){emit searchFailed(message);return;}
        QVector<InvestmentMarketInstrument> page; QString error;
        if(!parseMoexInvestmentSearch(payload,page,&error)){emit searchFailed(error);return;}
        QSet<QString> seen;for(const auto& i:collected)seen.insert(i.id());
        for(const auto& i:page)if(!seen.contains(i.id())){collected.append(i);seen.insert(i.id());}
        const int rows=QJsonDocument::fromJson(payload).object().value("securities").toObject().value("data").toArray().size();
        // Search all returned pages: reference indices must not crowd out assets.
        if(rows==100 && collected.size()<100 && start<1900){searchPage(query,start+100,generation,collected);return;}
        emit searchSucceeded(collected);
    });
}

void MoexInvestmentProvider::requestQuote(const InvestmentMarketInstrument& instrument)
{
    QUrl url(QStringLiteral("https://iss.moex.com/iss/securities/%1.json").arg(QString::fromLatin1(QUrl::toPercentEncoding(instrument.symbol()))));
    QUrlQuery p;p.addQueryItem("iss.meta","off");p.addQueryItem("iss.only","description,boards");url.setQuery(p);
    auto* reply=network_.get(requestFor(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,instrument]() mutable {
        auto resolved=instrument;const auto message=replyError(reply,QStringLiteral("Не удалось получить параметры инструмента"));
        const auto payload=reply->readAll();reply->deleteLater();QString error;
        if(!message.isEmpty()){emit quoteFailed(instrument.id(),message);return;}
        if(!parseMoexInstrumentDetails(payload,resolved,&error)){emit quoteFailed(instrument.id(),error);return;}
        emit instrumentResolved(resolved);
        requestMarketQuote(resolved);
    });
}

void MoexInvestmentProvider::requestMarketQuote(const InvestmentMarketInstrument& instrument)
{
    const auto& t=instrument.terms();
    const auto escaped=[](const QString& s){return QString::fromLatin1(QUrl::toPercentEncoding(s));};
    QUrl url(QStringLiteral("https://iss.moex.com/iss/engines/%1/markets/%2/boards/%3/securities/%4.json")
        .arg(escaped(t.engine),escaped(t.market),escaped(instrument.primaryBoardId()),escaped(instrument.symbol())));
    QUrlQuery p;p.addQueryItem("iss.meta","off");p.addQueryItem("iss.only","securities,marketdata");url.setQuery(p);
    auto* reply=network_.get(requestFor(url));
    connect(reply,&QNetworkReply::finished,this,[this,reply,instrument]() mutable {
        auto quoted=instrument;const auto message=replyError(reply,QStringLiteral("Не удалось получить котировку"));
        const auto payload=reply->readAll();reply->deleteLater();QString error;
        if(!message.isEmpty()){emit quoteFailed(instrument.id(),message);return;}
        if(!parseMoexInvestmentQuote(payload,quoted,&error)){emit instrumentResolved(quoted);emit quoteFailed(instrument.id(),error);return;}
        emit quoteSucceeded(quoted);
    });
}
