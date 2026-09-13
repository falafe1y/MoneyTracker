#include "MoexInvestmentProvider.h"

#include "MoexInvestmentParser.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

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

void MoexInvestmentProvider::search(const QString& query)
{
    QUrl url(QStringLiteral("https://iss.moex.com/iss/securities.json"));
    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("q"), query.trimmed());
    parameters.addQueryItem(QStringLiteral("lang"), QStringLiteral("ru"));
    parameters.addQueryItem(QStringLiteral("iss.meta"), QStringLiteral("off"));
    parameters.addQueryItem(QStringLiteral("iss.only"), QStringLiteral("securities"));
    parameters.addQueryItem(
        QStringLiteral("securities.columns"),
        QStringLiteral("secid,shortname,name,isin,type,group,primary_boardid"));
    parameters.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    url.setQuery(parameters);

    QNetworkReply* reply = network_.get(requestFor(url));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString networkError = replyError(
            reply, QStringLiteral("Не удалось выполнить поиск"));
        const QByteArray payload = reply->readAll();
        reply->deleteLater();
        if (!networkError.isEmpty()) {
            emit searchFailed(networkError);
            return;
        }

        QVector<InvestmentMarketInstrument> instruments;
        QString parseError;
        if (!parseMoexInvestmentSearch(payload, instruments, &parseError)) {
            emit searchFailed(parseError);
            return;
        }
        emit searchSucceeded(instruments);
    });
}

void MoexInvestmentProvider::requestQuote(
    const InvestmentMarketInstrument& instrument
    )
{
    const QString encodedBoard = QString::fromLatin1(
        QUrl::toPercentEncoding(instrument.primaryBoardId()));
    const QString encodedSymbol = QString::fromLatin1(
        QUrl::toPercentEncoding(instrument.symbol()));
    QUrl url(QStringLiteral(
        "https://iss.moex.com/iss/engines/stock/markets/shares/boards/%1/"
        "securities/%2.json").arg(encodedBoard, encodedSymbol));
    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("iss.meta"), QStringLiteral("off"));
    parameters.addQueryItem(
        QStringLiteral("iss.only"), QStringLiteral("securities,marketdata"));
    parameters.addQueryItem(
        QStringLiteral("securities.columns"),
        QStringLiteral("SECID,BOARDID,PREVPRICE,CURRENCYID"));
    parameters.addQueryItem(
        QStringLiteral("marketdata.columns"),
        QStringLiteral(
            "SECID,BOARDID,LAST,MARKETPRICE,LCURRENTPRICE,LEGALCLOSEPRICE"));
    url.setQuery(parameters);

    QNetworkReply* reply = network_.get(requestFor(url));
    QObject::connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, instrument]() {
            const QString networkError = replyError(
                reply, QStringLiteral("Не удалось получить котировку"));
            const QByteArray payload = reply->readAll();
            reply->deleteLater();
            if (!networkError.isEmpty()) {
                emit quoteFailed(instrument.id(), networkError);
                return;
            }

            qint64 priceMicros = 0;
            QString currencyCode;
            QString parseError;
            if (!parseMoexInvestmentQuote(
                    payload,
                    instrument.primaryBoardId(),
                    priceMicros,
                    currencyCode,
                    &parseError)) {
                emit quoteFailed(instrument.id(), parseError);
                return;
            }
            emit quoteSucceeded(
                instrument.id(),
                priceMicros,
                currencyCode,
                QDateTime::currentDateTimeUtc());
        });
}
