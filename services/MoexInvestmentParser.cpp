#include "MoexInvestmentParser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QTimeZone>

#include <cmath>
#include <limits>

namespace
{
void setError(QString* error, const QString& value)
{
    if (error) {
        *error = value;
    }
}

QHash<QString, int> columnIndexes(const QJsonObject& table)
{
    QHash<QString, int> result;
    const QJsonArray columns = table.value(QStringLiteral("columns")).toArray();
    for (int index = 0; index < columns.size(); ++index) {
        result.insert(columns.at(index).toString().toLower(), index);
    }
    return result;
}

QJsonValue cell(
    const QJsonArray& row,
    const QHash<QString, int>& columns,
    const QString& name
    )
{
    const int index = columns.value(name.toLower(), -1);
    return index >= 0 && index < row.size() ? row.at(index) : QJsonValue();
}

bool instrumentType(const QString& rawType, const QString& rawGroup,
    InvestmentInstrumentType& result)
{
    const auto type = rawType.toLower(), group = rawGroup.toLower();
    if (group.contains("index") || type.contains("index")) return false;
    if (type.contains("option") || group == "futures_options") result = InvestmentInstrumentType::Option;
    else if (type == "futures" || group == "futures_forts") result = InvestmentInstrumentType::Future;
    else if (type.contains("bond") || group == "stock_bonds" || group == "stock_eurobond") result = InvestmentInstrumentType::Bond;
    else if (type == "preferred_share") result = InvestmentInstrumentType::PreferredStock;
    else if (type == "depositary_receipt" || group == "stock_dr") result = InvestmentInstrumentType::DepositaryReceipt;
    else if (type == "common_share" || group == "stock_shares") result = InvestmentInstrumentType::Stock;
    else if (type.contains("etf") || group.contains("etf")) result = InvestmentInstrumentType::Etf;
    else if (type.contains("ppif") || type.contains("mutual") || group.contains("ppif")) result = InvestmentInstrumentType::Fund;
    else if (group == "currency_metal" || type.endsWith("_metal")) result = InvestmentInstrumentType::Metal;
    else if (group == "currency_selt" || type == "currency") result = InvestmentInstrumentType::Currency;
    else if (group.startsWith("stock_")) result = InvestmentInstrumentType::Other;
    else return false;
    return true;
}

double positiveNumber(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        return std::isfinite(number) && number > 0.0 ? number : 0.0;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        return ok && std::isfinite(number) && number > 0.0 ? number : 0.0;
    }
    return 0.0;
}

QString normalizedCurrency(QString value)
{
    value = value.trimmed().toUpper();
    if (value == QStringLiteral("SUR")) {
        return QStringLiteral("RUB");
    }
    if (value == QStringLiteral("RUB") ||
        value == QStringLiteral("USD") ||
        value == QStringLiteral("EUR")) {
        return value;
    }
    return value;
}
}

bool parseMoexInvestmentSearch(
    const QByteArray& payload,
    QVector<InvestmentMarketInstrument>& instruments,
    QString* error
    )
{
    instruments.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Московская биржа вернула некорректный ответ"));
        return false;
    }

    const QJsonObject table = document.object().value(
        QStringLiteral("securities")).toObject();
    const QHash<QString, int> columns = columnIndexes(table);
    if (!columns.contains(QStringLiteral("secid")) ||
        !columns.contains(QStringLiteral("primary_boardid"))) {
        setError(error, QStringLiteral("В ответе Московской биржи нет обязательных полей"));
        return false;
    }

    QSet<QString> seenIds;
    const QJsonArray rows = table.value(QStringLiteral("data")).toArray();
    for (const QJsonValue& rowValue : rows) {
        const QJsonArray row = rowValue.toArray();
        InvestmentInstrumentType type;
        if (!instrumentType(
                cell(row, columns, QStringLiteral("type")).toString(),
                cell(row, columns, QStringLiteral("group")).toString(),
                type)) {
            continue;
        }

        const QString symbol = cell(
            row, columns, QStringLiteral("secid")).toString().trimmed();
        const QString board = cell(
            row, columns, QStringLiteral("primary_boardid")).toString().trimmed();
        const auto fullName = cell(row, columns, QStringLiteral("name")).toString();
        if (symbol.isEmpty() || seenIds.contains(symbol.toUpper()) ||
            (columns.contains("is_traded") && cell(row, columns, "is_traded").toInt() == 0) ||
            ((type == InvestmentInstrumentType::Metal || type == InvestmentInstrumentType::Currency) &&
             (fullName.contains(QStringLiteral("СВОП"), Qt::CaseInsensitive) ||
              fullName.contains("SWAP", Qt::CaseInsensitive) || symbol.contains("FWD") || symbol.contains("LTV")))) {
            continue;
        }

        QString name = cell(
            row, columns, QStringLiteral("shortname")).toString().trimmed();
        if (name.isEmpty()) {
            name = cell(row, columns, QStringLiteral("name")).toString().trimmed();
        }
        if (name.isEmpty()) {
            name = symbol;
        }

        seenIds.insert(symbol.toUpper());
        InvestmentTerms terms = moexDefaultTerms(type);
        instruments.append(InvestmentMarketInstrument(
            QStringLiteral("moex:") + symbol,
            symbol,
            cell(row, columns, QStringLiteral("isin")).toString().trimmed().toUpper(),
            name,
            type,
            board, terms));
        if (instruments.size() == 100) {
            break;
        }
    }
    return true;
}

bool parseMoexInvestmentQuote(
    const QByteArray& payload,
    const QString& primaryBoardId,
    qint64& priceMicros,
    QString& currencyCode,
    QString* error
    )
{
    priceMicros = 0;
    currencyCode.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Московская биржа вернула некорректную котировку"));
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject securities = root.value(QStringLiteral("securities")).toObject();
    const QJsonObject marketData = root.value(QStringLiteral("marketdata")).toObject();
    const QHash<QString, int> securityColumns = columnIndexes(securities);
    const QHash<QString, int> marketColumns = columnIndexes(marketData);

    QJsonArray selectedSecurity;
    for (const QJsonValue& rowValue : securities.value(QStringLiteral("data")).toArray()) {
        const QJsonArray row = rowValue.toArray();
        const QString board = cell(
            row, securityColumns, QStringLiteral("boardid")).toString();
        if (selectedSecurity.isEmpty() || board == primaryBoardId) {
            selectedSecurity = row;
        }
        if (board == primaryBoardId) {
            break;
        }
    }

    QJsonArray selectedMarketData;
    for (const QJsonValue& rowValue : marketData.value(QStringLiteral("data")).toArray()) {
        const QJsonArray row = rowValue.toArray();
        const QString board = cell(
            row, marketColumns, QStringLiteral("boardid")).toString();
        if (selectedMarketData.isEmpty() || board == primaryBoardId) {
            selectedMarketData = row;
        }
        if (board == primaryBoardId) {
            break;
        }
    }

    double price = 0.0;
    for (const QString& column : {
             QStringLiteral("last"),
             QStringLiteral("marketprice"),
             QStringLiteral("lcurrentprice"),
             QStringLiteral("legalcloseprice")}) {
        price = positiveNumber(cell(selectedMarketData, marketColumns, column));
        if (price > 0.0) {
            break;
        }
    }
    if (price <= 0.0) {
        price = positiveNumber(cell(
            selectedSecurity, securityColumns, QStringLiteral("prevprice")));
    }

    currencyCode = normalizedCurrency(cell(
        selectedSecurity, securityColumns, QStringLiteral("currencyid")).toString());
    if (price <= 0.0 || currencyCode.isEmpty()) {
        setError(error, currencyCode.isEmpty()
            ? QStringLiteral("Валюта инструмента пока не поддерживается")
            : QStringLiteral("Для инструмента пока нет доступной рыночной цены"));
        return false;
    }

    const long double scaled = static_cast<long double>(price) * 1'000'000.0L;
    if (scaled > static_cast<long double>(std::numeric_limits<qint64>::max())) {
        setError(error, QStringLiteral("Котировка выходит за допустимый диапазон"));
        return false;
    }
    priceMicros = static_cast<qint64>(std::llround(scaled));
    return priceMicros > 0;
}

InvestmentTerms moexDefaultTerms(InvestmentInstrumentType type)
{
    InvestmentTerms t;
    t.engine = QStringLiteral("stock"); t.market = QStringLiteral("shares");
    if (type == InvestmentInstrumentType::Bond) { t.market = "bonds"; t.pricing = "bond"; }
    if (type == InvestmentInstrumentType::Metal || type == InvestmentInstrumentType::Currency) {
        t.engine = "currency"; t.market = "selt";
        t.quantityUnit = type == InvestmentInstrumentType::Metal ? QStringLiteral("г") : QStringLiteral("ед. валюты");
    }
    if (type == InvestmentInstrumentType::Future || type == InvestmentInstrumentType::Option) {
        t.engine = "futures"; t.market = type == InvestmentInstrumentType::Future ? "forts" : "options";
        t.pricing = type == InvestmentInstrumentType::Future ? "future" : "manual";
        t.quantityUnit = QStringLiteral("контр.");
    }
    if (type == InvestmentInstrumentType::Other) t.pricing = "manual";
    return t;
}

bool parseMoexInstrumentDetails(const QByteArray& payload, InvestmentMarketInstrument& instrument, QString* error)
{
    QJsonParseError e; const auto d = QJsonDocument::fromJson(payload, &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) { setError(error, QStringLiteral("Некорректное описание инструмента")); return false; }
    const auto table = d.object().value("boards").toObject(); const auto c = columnIndexes(table);
    QJsonArray selected;
    for (const auto& v : table.value("data").toArray()) {
        const auto row = v.toArray();
        const auto engine = cell(row,c,"engine").toString(), market = cell(row,c,"market").toString();
        if (!((engine == "stock" && (market == "shares" || market == "bonds")) ||
            (engine == "currency" && market == "selt") ||
            (engine == "futures" && (market == "forts" || market == "options")))) continue;
        if (selected.isEmpty()) selected = row;
        if (cell(row,c,"boardid").toString() == instrument.primaryBoardId()) { selected = row; break; }
        if (cell(row,c,"is_primary").toInt() == 1) selected = row;
    }
    if (selected.isEmpty()) { setError(error, QStringLiteral("Для инструмента нет поддерживаемого режима торгов; доступна ручная оценка")); return false; }
    auto t = instrument.terms();
    t.engine = cell(selected,c,"engine").toString(); t.market = cell(selected,c,"market").toString();
    const auto desc = d.object().value("description").toObject(); const auto dc = columnIndexes(desc);
    QHash<QString,QString> values;
    for (const auto& v : desc.value("data").toArray()) { const auto row=v.toArray(); values.insert(cell(row,dc,"name").toString().toUpper(),cell(row,dc,"value").toString()); }
    const auto name = values.value("NAME") + QLatin1Char(' ') + values.value("CONTRACTNAME");
    if (instrument.type() == InvestmentInstrumentType::Option) {
        if (name.contains(QStringLiteral("Прем"),Qt::CaseInsensitive)) t.pricing = "premium_option";
        else if (name.contains(QStringLiteral("Марж"),Qt::CaseInsensitive)) t.pricing = "margined_option";
        else t.pricing = "manual";
    }
    t.perpetual = name.contains(QStringLiteral("автопролонг"),Qt::CaseInsensitive) || name.contains(QStringLiteral("одноднев"),Qt::CaseInsensitive);
    t.currencyCode = normalizedCurrency(values.value("FACEUNIT"));
    // Foreign-currency and perpetual futures have additional settlement terms.
    // Do not apply a fixed-ruble tick formula without the contract's full model.
    if(instrument.type()==InvestmentInstrumentType::Future &&
        (t.perpetual || (!t.currencyCode.isEmpty() && t.currencyCode!=QStringLiteral("RUB"))))t.pricing="manual";
    t.maturity = QDate::fromString(values.value("LSTDELDATE"),Qt::ISODate);
    // Preserve the exchange's case-sensitive contract code.
    instrument = InvestmentMarketInstrument(instrument.id(), values.value("SECID", instrument.symbol()),
        instrument.isin(), instrument.name(), instrument.type(), cell(selected,c,"boardid").toString(), t);
    return true;
}

bool parseMoexInvestmentQuote(const QByteArray& payload, InvestmentMarketInstrument& instrument, QString* error)
{
    QJsonParseError e; const auto doc=QJsonDocument::fromJson(payload,&e);
    if (e.error!=QJsonParseError::NoError || !doc.isObject()) { setError(error,QStringLiteral("Некорректная котировка"));return false; }
    const auto root=doc.object(), securities=root.value("securities").toObject(), market=root.value("marketdata").toObject();
    const auto sc=columnIndexes(securities), mc=columnIndexes(market);
    const auto select=[&instrument](const QJsonObject& table,const QHash<QString,int>& c) {
        for (const auto& v:table.value("data").toArray()) {const auto row=v.toArray();
            if (cell(row,c,"secid").toString().compare(instrument.symbol(),Qt::CaseInsensitive)==0 &&
                (instrument.primaryBoardId().isEmpty() || cell(row,c,"boardid").toString()==instrument.primaryBoardId())) return row;
        } return QJsonArray();
    };
    const auto security=select(securities,sc), data=select(market,mc);
    if (security.isEmpty()) {setError(error,QStringLiteral("Не найден выбранный инструмент или режим торгов"));return false;}
    auto t=instrument.terms();
    const auto number=[](QJsonValue v,long double& n) { bool ok=v.isDouble(); n=v.isDouble()?v.toDouble():v.toString().toDouble(&ok); return ok && std::isfinite(n); };
    const auto micros=[&number](QJsonValue v,qint64 fallback=0) {
        long double n=0; if (!number(v,n) || n*1'000'000.0L >= std::numeric_limits<qint64>::max() || n*1'000'000.0L <= std::numeric_limits<qint64>::min()) return fallback;
        return static_cast<qint64>(std::round(n*1'000'000.0L));
    };
    t.lotSizeMicros=micros(cell(security,sc,"lotsize"),1'000'000);
    t.priceStepMicros=micros(cell(security,sc,"minstep"));
    t.stepValueMicros=micros(cell(security,sc,"stepprice"));
    t.settlementPriceMicros=micros(cell(security,sc,"settleprice_clr"),micros(cell(security,sc,"prevsettleprice")));
    t.underlying=cell(security,sc,"underlyingasset").toString(cell(security,sc,"assetcode").toString());
    t.optionRight=cell(security,sc,"optiontype").toString();
    t.strikeMicros=micros(cell(security,sc,"strike"));
    auto maturity=QDate::fromString(cell(security,sc,"lastdeldate").toString(),Qt::ISODate);
    if (!maturity.isValid()) maturity=QDate::fromString(cell(security,sc,"matdate").toString(),Qt::ISODate);
    if (maturity.isValid()) t.maturity=maturity;
    QString monetaryCurrency=normalizedCurrency(cell(security,sc,"currencyid").toString());
    if (t.engine=="futures") {
        // STEPPRICE on FORTS is the monetary cost of one price tick in rubles.
        monetaryCurrency="RUB";
        if (t.pricing=="premium_option") {
            if (t.priceStepMicros>0 && t.stepValueMicros>0)
                t.multiplierMicros=micros(QJsonValue(static_cast<double>(t.stepValueMicros)/t.priceStepMicros));
            else t.multiplierMicros=0;
        }
    }
    if (t.pricing=="bond") {
        t.faceValueMicros=micros(cell(security,sc,"facevalueonsettledate"),micros(cell(security,sc,"facevalue")));
        t.accruedInterestMicros=micros(cell(security,sc,"accruedint"),-1);
        const auto faceCurrency=normalizedCurrency(cell(security,sc,"faceunit").toString());
        if (faceCurrency!=monetaryCurrency) t.pricing="manual"; // Mixed settlement/indexed nominals need explicit valuation.
    } else if (t.engine=="currency") {
        // FX quotes may be for 1, 10, 100... base units. Quantity is always
        // the actual currency/metal amount, not the number of trading lots.
        const auto face=micros(cell(security,sc,"facevalue"));
        t.multiplierMicros=face>0?static_cast<qint64>(std::round(1'000'000'000'000.0L/face)):0;
        if (instrument.type()==InvestmentInstrumentType::Currency) t.quantityUnit=cell(security,sc,"faceunit").toString();
    }
    t.currencyCode=monetaryCurrency;
    instrument.setTerms(t); // Even an absent price must not hide instrument specifications.
    long double price=0; bool found=false;QString source;
    // Zero LAST with no trades is a placeholder on the derivatives market.
    for (const auto& key: {"last","marketprice","lcurrentprice","legalcloseprice"}) {
        long double n=0; if (number(cell(data,mc,QLatin1String(key)),n) && (n>0 || (n<0 && t.pricing=="future"))) {price=n;found=true;source=QLatin1String(key);break;}
    }
    if (!found) for (const auto& key:{"settleprice_clr","prevsettleprice","prevprice"}) {
        long double n=0; if (number(cell(security,sc,QLatin1String(key)),n) && n>0) {price=n;found=true;source=QLatin1String(key);break;}
    }
    if (!found || monetaryCurrency.isEmpty()) {setError(error,QStringLiteral("Нет доступной цены или валюты расчёта; укажите ручную оценку"));return false;}
    const long double scaled=price*1'000'000.0L;
    if (scaled>=std::numeric_limits<qint64>::max() || scaled<=std::numeric_limits<qint64>::min()) {setError(error,QStringLiteral("Котировка вне допустимого диапазона"));return false;}
    // Date the selected price, not the HTTP request. LAST is delayed and
    // previous-close/settlement prices must not be shown as a fresh trade.
    QDateTime at=QDateTime::fromString(cell(data,mc,"systime").toString(),QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if(at.isValid())at=QDateTime(at.date(),at.time(),QTimeZone(3*3600)).toUTC();
    if(source=="last"){
        const auto tradeTime=QTime::fromString(cell(data,mc,"time").toString(),QStringLiteral("HH:mm:ss"));
        auto tradeDate=QDate::fromString(cell(data,mc,"tradedate").toString(),Qt::ISODate);
        if(!tradeDate.isValid() && at.isValid())tradeDate=at.toTimeZone(QTimeZone(3*3600)).date();
        if(tradeTime.isValid() && tradeDate.isValid())at=QDateTime(tradeDate,tradeTime,QTimeZone(3*3600)).toUTC();
    }else if(source=="prevprice"){
        const auto previousDate=QDate::fromString(cell(security,sc,"prevdate").toString(),Qt::ISODate);
        if(previousDate.isValid())at=QDateTime(previousDate,QTime(23,59,59),QTimeZone(3*3600)).toUTC();
    }else if(source=="settleprice_clr" || source=="prevsettleprice"){
        const auto im=QDateTime::fromString(cell(security,sc,"imtime").toString(),QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if(im.isValid())at=QDateTime(im.date(),im.time(),QTimeZone(3*3600)).toUTC();
    }
    if(!at.isValid())at=QDateTime::currentDateTimeUtc();
    t.quoteSource=source;instrument.setTerms(t);
    instrument.setQuote(static_cast<qint64>(std::round(scaled)),monetaryCurrency,at);
    return true;
}
