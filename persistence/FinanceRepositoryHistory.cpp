#include "FinanceRepository.h"
#include "HistorySavepoint.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlRecord>
#include <QTimeZone>
#include <limits>

namespace {
QString snapshotTable(AssetDailyValuation::Kind kind) {
    switch(kind) {
    case AssetDailyValuation::Fiat: return QStringLiteral("fiat_daily_snapshots");
    case AssetDailyValuation::Crypto: return QStringLiteral("crypto_daily_snapshots");
    case AssetDailyValuation::Investment: return QStringLiteral("investment_daily_snapshots");
    }
    return {};
}
QVariant optionalValue(const std::optional<qint64>& value) { return value ? QVariant(*value) : QVariant(); }
QVariantMap rowMap(const QSqlQuery& query) {
    QVariantMap row; const auto record=query.record();
    for(int i=0;i<record.count();++i) row.insert(record.fieldName(i),query.value(i));
    return row;
}
}

bool FinanceRepository::migrateAnalyticsHistorySchema() {
    HistorySavepoint atomic(database_);
    if(!atomic.active()){setLastError(database_.lastError().text());return false;}
    QSqlQuery q(database_);
    const auto exec=[&](const QString& sql){if(q.exec(sql))return true;setLastError(q.lastError().text());return false;};
    const auto ensure=[&](const QString& column,const QString& definition){
        if(!exec("PRAGMA table_info(investment_operations)"))return false;
        while(q.next())if(q.value(1).toString()==column){q.finish();return true;}
        q.finish();return exec("ALTER TABLE investment_operations ADD COLUMN "+column+" "+definition);
    };
    if(!ensure("fee_minor","INTEGER CHECK(fee_minor>=0)")
       || !ensure("tax_minor","INTEGER CHECK(tax_minor>=0)")
       || !ensure("trade_price_micros","INTEGER")
       || !ensure("trade_currency","TEXT"))return false;
    const QString definition=
        "(snapshot_date TEXT NOT NULL CHECK(length(snapshot_date)=10),source_id TEXT NOT NULL,"
        "account_id TEXT NOT NULL,instrument_id TEXT NOT NULL DEFAULT '',currency TEXT NOT NULL,"
        "value_minor INTEGER,quantity_atomic INTEGER,quantity_decimals INTEGER NOT NULL CHECK(quantity_decimals BETWEEN 0 AND 18),"
        "price_micros INTEGER,cost_basis_minor INTEGER CHECK(cost_basis_minor>=0),valued_at INTEGER,"
        "observed_at INTEGER NOT NULL,source TEXT NOT NULL,issues TEXT NOT NULL DEFAULT '',details_json TEXT NOT NULL DEFAULT '{}',"
        "PRIMARY KEY(snapshot_date,source_id))";
    for(auto kind:{AssetDailyValuation::Fiat,AssetDailyValuation::Crypto,AssetDailyValuation::Investment}) {
        const auto table=snapshotTable(kind);
        if(!exec("CREATE TABLE IF NOT EXISTS "+table+definition)
           || !exec("CREATE INDEX IF NOT EXISTS idx_"+table+"_account_date ON "+table+"(account_id,snapshot_date)"))return false;
    }
    const QStringList statements{
        "CREATE TABLE IF NOT EXISTS daily_valuation_runs(snapshot_date TEXT PRIMARY KEY,observed_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS crypto_price_history(asset_id TEXT NOT NULL,quoted_at INTEGER NOT NULL,"
        "source TEXT NOT NULL,price_micros INTEGER NOT NULL,currency TEXT NOT NULL,observed_at INTEGER NOT NULL,terms_json TEXT NOT NULL DEFAULT '{}',"
        "PRIMARY KEY(asset_id,quoted_at,source))",
        "CREATE TABLE IF NOT EXISTS investment_price_history(asset_id TEXT NOT NULL,quoted_at INTEGER NOT NULL,"
        "source TEXT NOT NULL,price_micros INTEGER NOT NULL,currency TEXT NOT NULL,observed_at INTEGER NOT NULL,terms_json TEXT NOT NULL DEFAULT '{}',"
        "PRIMARY KEY(asset_id,quoted_at,source))",
        "CREATE TABLE IF NOT EXISTS fx_rate_history(currency TEXT NOT NULL,rate_date TEXT NOT NULL,source TEXT NOT NULL,"
        "rub_per_unit_micros INTEGER NOT NULL CHECK(rub_per_unit_micros>0),observed_at INTEGER NOT NULL,"
        "PRIMARY KEY(currency,rate_date,source))",
        "CREATE TABLE IF NOT EXISTS investment_position_history(sequence INTEGER PRIMARY KEY,position_id TEXT NOT NULL,"
        "account_id TEXT NOT NULL,instrument_id TEXT NOT NULL,currency TEXT NOT NULL,event TEXT NOT NULL,"
        "operation_id TEXT,occurred_at INTEGER NOT NULL,recorded_at INTEGER NOT NULL,quantity_micros INTEGER NOT NULL,"
        "before_quantity_micros INTEGER,cost_basis_minor INTEGER CHECK(cost_basis_minor>=0),before_cost_minor INTEGER,"
        "released_cost_minor INTEGER,realized_net_minor INTEGER,state_json TEXT NOT NULL,issues TEXT NOT NULL DEFAULT '')",
        "CREATE INDEX IF NOT EXISTS idx_position_history_position ON investment_position_history(position_id,sequence)",
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_position_history_operation ON investment_position_history(operation_id) WHERE operation_id IS NOT NULL"
    };
    for(const auto& sql:statements)if(!exec(sql))return false;
    for(const auto& action:{QStringLiteral("INSERT"),QStringLiteral("UPDATE"),QStringLiteral("DELETE")}) {
        QString predicate;
        if(action!="DELETE")predicate="(account_id=NEW.account_id AND snapshot_date>=date(NEW.occurred_at/1000,'unixepoch','localtime'))";
        if(action!="INSERT")predicate+=(predicate.isEmpty()?QString():QStringLiteral(" OR "))+"(account_id=OLD.account_id AND snapshot_date>=date(OLD.occurred_at/1000,'unixepoch','localtime'))";
        QString body;
        for(auto kind:{AssetDailyValuation::Fiat,AssetDailyValuation::Crypto,AssetDailyValuation::Investment}) {
            const auto table=snapshotTable(kind);
            body+="UPDATE "+table+" SET issues=CASE WHEN instr(issues,'history_changed')>0 THEN issues ELSE issues||';history_changed' END WHERE "+predicate+";";
        }
        if(!exec("CREATE TRIGGER IF NOT EXISTS history_transactions_"+action.toLower()+" AFTER "+action+" ON transactions BEGIN "+body+" END"))return false;
    }
    QString body;
    for(auto kind:{AssetDailyValuation::Fiat,AssetDailyValuation::Crypto,AssetDailyValuation::Investment})
        body+="UPDATE "+snapshotTable(kind)+" SET issues=CASE WHEN instr(issues,'history_changed')>0 THEN issues ELSE issues||';history_changed' END WHERE account_id=NEW.id;";
    if(!exec("CREATE TRIGGER IF NOT EXISTS history_account_basis AFTER UPDATE OF initial_balance_minor,currency ON accounts "
             "WHEN OLD.initial_balance_minor<>NEW.initial_balance_minor OR OLD.currency<>NEW.currency BEGIN "+body+" END"))return false;
    // Existing records are evidence of an observed state, not evidence of past acquisitions.
    QStringList positions;
    if(!exec("SELECT id FROM investment_positions WHERE NOT EXISTS (SELECT 1 FROM investment_position_history h WHERE h.position_id=investment_positions.id)"))return false;
    while(q.next())positions.append(q.value(0).toString());q.finish();
    for(const auto& id:positions)if(!appendPositionState(id,"baseline"))return false;
    const qint64 now=QDateTime::currentMSecsSinceEpoch();
    q.prepare("INSERT OR IGNORE INTO crypto_price_history(asset_id,quoted_at,source,price_micros,currency,observed_at) "
              "SELECT symbol,fetched_at,'legacy',price_usd_micros,'USD',? FROM crypto_prices c WHERE NOT EXISTS (SELECT 1 FROM crypto_price_history h WHERE h.asset_id=c.symbol)");q.addBindValue(now);
    if(!q.exec()){setLastError(q.lastError().text());return false;}
    q.prepare("INSERT OR IGNORE INTO investment_price_history(asset_id,quoted_at,source,price_micros,currency,observed_at,terms_json) "
              "SELECT q.instrument_id,q.quoted_at,'legacy',q.price_micros,i.currency,?,q.terms_json FROM investment_quotes q JOIN investment_instruments i ON i.id=q.instrument_id WHERE NOT EXISTS (SELECT 1 FROM investment_price_history h WHERE h.asset_id=q.instrument_id)");q.addBindValue(now);
    if(!q.exec()){setLastError(q.lastError().text());return false;}
    return atomic.commit();
}

bool FinanceRepository::savePriceHistory(const QString& kind,const QString& id,qint64 price,
    const QDateTime& at,const QString& currency,const QString& source,const QString& terms) {
    if((kind!="crypto" && kind!="investment") || id.isEmpty() || !at.isValid() || currency.isEmpty() || source.isEmpty()){
        setLastError(QStringLiteral("Некорректная историческая котировка"));return false;
    }
    QSqlQuery q(database_);
    q.prepare("INSERT INTO "+kind+"_price_history(asset_id,quoted_at,source,price_micros,currency,observed_at,terms_json) VALUES(?,?,?,?,?,?,?) "
              "ON CONFLICT(asset_id,quoted_at,source) DO UPDATE SET price_micros=excluded.price_micros,currency=excluded.currency,observed_at=excluded.observed_at,terms_json=excluded.terms_json");
    q.addBindValue(id);q.addBindValue(at.toMSecsSinceEpoch());q.addBindValue(source);q.addBindValue(price);
    q.addBindValue(currency);q.addBindValue(QDateTime::currentMSecsSinceEpoch());q.addBindValue(terms);
    if(!q.exec()){setLastError(q.lastError().text());return false;}return true;
}

bool FinanceRepository::saveDailyValuations(const DailyValuationBatch& batch) {
    if(!batch.date.isValid() || !batch.observedAtUtc.isValid()
       || batch.observedAtUtc.toLocalTime().date()!=batch.date){setLastError(QStringLiteral("Некорректная дата дневной оценки"));return false;}
    HistorySavepoint atomic(database_);if(!atomic.active()){setLastError(database_.lastError().text());return false;}
    QSqlQuery q(database_);q.prepare("SELECT observed_at FROM daily_valuation_runs WHERE snapshot_date=?");
    q.addBindValue(batch.date.toString(Qt::ISODate));
    if(!q.exec()){setLastError(q.lastError().text());return false;}
    if(q.next() && q.value(0).toLongLong()>batch.observedAtUtc.toMSecsSinceEpoch())return atomic.commit();
    q.finish();
    for(auto kind:{AssetDailyValuation::Fiat,AssetDailyValuation::Crypto,AssetDailyValuation::Investment}) {
        q.prepare("DELETE FROM "+snapshotTable(kind)+" WHERE snapshot_date=?");q.addBindValue(batch.date.toString(Qt::ISODate));
        if(!q.exec()){setLastError(q.lastError().text());return false;}
    }
    QSet<QString> ids;
    for(const auto& a:batch.assets){
        const auto table=snapshotTable(a.kind), key=QString::number(int(a.kind))+":"+a.sourceId;
        if(table.isEmpty() || a.sourceId.isEmpty() || a.accountId.isEmpty() || a.currency.isEmpty() || a.source.isEmpty()
           || ids.contains(key) || (a.costBasisMinor && *a.costBasisMinor<0)
           || (!a.valueMinor && a.issues.isEmpty())){setLastError(QStringLiteral("Некорректная или повторная оценка актива"));return false;}
        ids.insert(key);
        q.prepare("INSERT INTO "+table+"(snapshot_date,source_id,account_id,instrument_id,currency,value_minor,quantity_atomic,quantity_decimals,"
                  "price_micros,cost_basis_minor,valued_at,observed_at,source,issues,details_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(batch.date.toString(Qt::ISODate));q.addBindValue(a.sourceId);q.addBindValue(a.accountId);
        q.addBindValue(a.instrumentId.isNull()?QStringLiteral(""):a.instrumentId);q.addBindValue(a.currency);
        q.addBindValue(optionalValue(a.valueMinor));q.addBindValue(optionalValue(a.quantityAtomic));q.addBindValue(a.quantityDecimals);
        q.addBindValue(optionalValue(a.priceMicros));q.addBindValue(optionalValue(a.costBasisMinor));
        q.addBindValue(a.valuedAtUtc.isValid()?QVariant(a.valuedAtUtc.toMSecsSinceEpoch()):QVariant());
        q.addBindValue(batch.observedAtUtc.toMSecsSinceEpoch());q.addBindValue(a.source);
        q.addBindValue(a.issues.isNull()?QStringLiteral(""):a.issues);q.addBindValue(a.detailsJson);
        if(!q.exec()){setLastError(q.lastError().text());return false;}
    }
    for(const auto& rate:batch.rates){
        if(rate.currency.isEmpty() || !rate.rateDate.isValid() || !rate.observedAtUtc.isValid() || rate.source.isEmpty() || rate.rubPerUnitMicros<=0){
            setLastError(QStringLiteral("Некорректный исторический курс"));return false;
        }
        q.prepare("INSERT INTO fx_rate_history(currency,rate_date,source,rub_per_unit_micros,observed_at) VALUES(?,?,?,?,?) "
                  "ON CONFLICT(currency,rate_date,source) DO UPDATE SET rub_per_unit_micros=excluded.rub_per_unit_micros,observed_at=excluded.observed_at "
                  "WHERE excluded.observed_at>=fx_rate_history.observed_at");
        q.addBindValue(rate.currency);q.addBindValue(rate.rateDate.toString(Qt::ISODate));q.addBindValue(rate.source);
        q.addBindValue(rate.rubPerUnitMicros);q.addBindValue(rate.observedAtUtc.toMSecsSinceEpoch());
        if(!q.exec()){setLastError(q.lastError().text());return false;}
    }
    q.prepare("INSERT INTO daily_valuation_runs(snapshot_date,observed_at) VALUES(?,?) ON CONFLICT(snapshot_date) DO UPDATE SET observed_at=excluded.observed_at");
    q.addBindValue(batch.date.toString(Qt::ISODate));q.addBindValue(batch.observedAtUtc.toMSecsSinceEpoch());
    if(!q.exec()){setLastError(q.lastError().text());return false;}
    if(batch.totalMinor){
        if(batch.totalCurrency.isEmpty()){setLastError(QStringLiteral("Не указана валюта общего капитала"));return false;}
        q.prepare("INSERT INTO capital_snapshots(snapshot_date,currency,total_minor,updated_at) VALUES(?,?,?,?) ON CONFLICT(snapshot_date) DO UPDATE SET currency=excluded.currency,total_minor=excluded.total_minor,updated_at=excluded.updated_at");
        q.addBindValue(batch.date.toString(Qt::ISODate));q.addBindValue(batch.totalCurrency);q.addBindValue(*batch.totalMinor);q.addBindValue(batch.observedAtUtc.toMSecsSinceEpoch());
        if(!q.exec()){setLastError(q.lastError().text());return false;}
    } else {
        q.prepare("DELETE FROM capital_snapshots WHERE snapshot_date=?");q.addBindValue(batch.date.toString(Qt::ISODate));
        if(!q.exec()){setLastError(q.lastError().text());return false;}
    }
    return atomic.commit();
}

QVariantList FinanceRepository::loadDailyValuations(AssetDailyValuation::Kind kind,const QDate& from,const QDate& to) {
    QVariantList rows;const auto table=snapshotTable(kind);if(table.isEmpty())return rows;
    QSqlQuery q(database_);q.prepare("SELECT * FROM "+table+" WHERE (?='' OR snapshot_date>=?) AND (?='' OR snapshot_date<=?) ORDER BY snapshot_date,source_id");
    for(auto date:{from,from,to,to})q.addBindValue(date.isValid()?date.toString(Qt::ISODate):QStringLiteral(""));
    if(!q.exec()){setLastError(q.lastError().text());return rows;}while(q.next())rows.append(rowMap(q));return rows;
}
QVariantList FinanceRepository::loadPositionHistory(const QString& id) {
    QVariantList rows;QSqlQuery q(database_);q.prepare("SELECT * FROM investment_position_history WHERE position_id=? ORDER BY sequence");q.addBindValue(id);
    if(!q.exec()){setLastError(q.lastError().text());return rows;}while(q.next())rows.append(rowMap(q));return rows;
}
QVariant FinanceRepository::investmentCostBasis(const QString& id) const {
    QSqlQuery q(database_);q.prepare("SELECT cost_basis_minor FROM investment_position_history WHERE position_id=? ORDER BY sequence DESC LIMIT 1");q.addBindValue(id);
    return q.exec() && q.next()?q.value(0):QVariant();
}

bool FinanceRepository::appendPositionState(const QString& id,const QString& event) {
    QSqlQuery q(database_);q.prepare("SELECT p.account_id,p.instrument_id,p.quantity_micros,p.average_price_micros,p.settings_json,p.is_archived,"
        "a.currency,i.symbol,i.type,i.terms_json,q.terms_json AS quote_terms FROM investment_positions p JOIN accounts a ON a.id=p.account_id JOIN investment_instruments i ON i.id=p.instrument_id LEFT JOIN investment_quotes q ON q.instrument_id=p.instrument_id WHERE p.id=?");
    q.addBindValue(id);if(!q.exec() || !q.next()){setLastError(QStringLiteral("Не удалось сохранить историю позиции"));return false;}
    const auto row=rowMap(q);const qint64 qty=row.value("quantity_micros").toLongLong();
    const bool closed=row.value("is_archived").toBool() || event=="delete";
    const auto settings=InvestmentPositionSettings::deserialize(row.value("settings_json").toString());
    const auto quotedTerms=InvestmentTerms::deserialize(row.value("quote_terms").toString());
    const auto terms=quotedTerms.currencyCode.isEmpty()?InvestmentTerms::deserialize(row.value("terms_json").toString()):quotedTerms;
    QJsonObject state{{"symbol",row.value("symbol").toString()}, {"type",row.value("type").toInt()},
        {"averagePriceMicros",QString::number(row.value("average_price_micros").toLongLong())},
        {"quantityMicros",QString::number(qty)}, {"settings",QJsonDocument::fromJson(settings.serialize().toUtf8()).object()},
        {"terms",QJsonDocument::fromJson(terms.serialize().toUtf8()).object()}};
    QVariant basis;QString issues=QStringLiteral("cost_unknown");
    if(event=="correction" || event=="archive" || event=="delete") {
        QSqlQuery prior(database_);prior.prepare("SELECT account_id,instrument_id,quantity_micros,cost_basis_minor,state_json FROM investment_position_history WHERE position_id=? ORDER BY sequence DESC LIMIT 1");prior.addBindValue(id);
        if(!prior.exec()){setLastError(prior.lastError().text());return false;}
        if(prior.next()){
            const auto old=QJsonDocument::fromJson(prior.value(4).toString().toUtf8()).object();
            if(prior.value(0)==row.value("account_id") && prior.value(1)==row.value("instrument_id")
               && prior.value(2).toLongLong()==qty && old.value("averagePriceMicros")==state.value("averagePriceMicros")
               && old.value("settings").toObject().value("direction").toInt()==settings.direction)basis=prior.value(3);
        }
    }
    if(!basis.isNull())issues.clear();
    if(closed && !basis.isNull())basis=qint64(0);
    const auto now=QDateTime::currentMSecsSinceEpoch();q.finish();
    q.prepare("INSERT INTO investment_position_history(position_id,account_id,instrument_id,currency,event,occurred_at,recorded_at,quantity_micros,cost_basis_minor,state_json,issues) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(id);q.addBindValue(row.value("account_id"));q.addBindValue(row.value("instrument_id"));q.addBindValue(row.value("currency"));q.addBindValue(event);
    q.addBindValue(now);q.addBindValue(now);q.addBindValue(closed?0:qty);q.addBindValue(basis);
    q.addBindValue(QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));q.addBindValue(issues);
    if(!q.exec()){setLastError(q.lastError().text());return false;}return true;
}

bool FinanceRepository::completePositionEvent(const InvestmentOperation& op,const QVariant& previousBasis,qint64 before,qint64 after) {
    QSqlQuery q(database_);q.prepare("SELECT sequence,state_json FROM investment_position_history WHERE position_id=? ORDER BY sequence DESC LIMIT 1");q.addBindValue(op.positionId);
    if(!q.exec() || !q.next()){setLastError(QStringLiteral("Не найдена история позиции"));return false;}
    const auto sequence=q.value(0);auto state=QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
    q.finish();
    q.prepare("SELECT terms_json FROM investment_quotes WHERE instrument_id=?");q.addBindValue(op.instrumentId);
    if(!q.exec()){setLastError(q.lastError().text());return false;}
    if(q.next()){
        const auto terms=InvestmentTerms::deserialize(q.value(0).toString());
        if(!terms.currencyCode.isEmpty())state.insert("terms",QJsonDocument::fromJson(terms.serialize().toUtf8()).object());
    }
    const auto settings=state.value("settings").toObject();const int type=state.value("type").toInt();
    const bool ordinary=type!=9 && type!=10 && settings.value("direction").toInt(1)==1;
    QVariant basis=previousBasis,released,realized;QString issues;
    const auto checked=[&](const __int128_t n,QVariant& value){
        if(n>std::numeric_limits<qint64>::max() || n<std::numeric_limits<qint64>::min())return false;
        value=static_cast<qint64>(n);return true;
    };
    if(!ordinary){basis={};issues="derivative_cost_not_applicable";}
    else if(op.kind=="buy"){
        if(op.quantityDeltaMicros<=0 || after-before!=op.quantityDeltaMicros || op.cashDeltaMinor>=0){setLastError(QStringLiteral("Некорректное приобретение позиции"));return false;}
        if(!basis.isNull() && !checked(__int128_t(basis.toLongLong())-op.cashDeltaMinor,basis)){setLastError(QStringLiteral("Стоимость приобретения вне диапазона"));return false;}
    } else if(op.kind=="sell" || op.kind=="expiry"){
        if(before<=0 || after<0 || after>=before || op.quantityDeltaMicros!=after-before){setLastError(QStringLiteral("Некорректное уменьшение позиции"));return false;}
        if(!basis.isNull()){
            const qint64 old=basis.toLongLong();const qint64 used=after==0?old:static_cast<qint64>(__int128_t(old)*(before-after)/before);
            released=used;basis=old-used;
            if(!checked(__int128_t(op.cashDeltaMinor)-used,realized)){setLastError(QStringLiteral("Результат продажи вне диапазона"));return false;}
        } else if(after==0) basis=qint64(0);
    } else if(op.kind=="amortization"){
        q.finish();q.prepare("SELECT state_json FROM investment_position_history WHERE position_id=? AND sequence<? ORDER BY sequence DESC LIMIT 1");
        q.addBindValue(op.positionId);q.addBindValue(sequence);
        if(!q.exec()){setLastError(q.lastError().text());return false;}
        const auto oldState=q.next()?QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object():QJsonObject();
        const auto oldFace=oldState.value("terms").toObject().value("face").toString().toLongLong();
        const auto newFace=state.value("terms").toObject().value("face").toString().toLongLong();
        if(!basis.isNull() && oldFace>0 && newFace>=0 && newFace<oldFace){
            const qint64 old=basis.toLongLong(),remaining=static_cast<qint64>(__int128_t(old)*newFace/oldFace);
            released=old-remaining;basis=remaining;
            if(!checked(__int128_t(op.cashDeltaMinor)-(old-remaining),realized)){setLastError(QStringLiteral("Результат погашения вне диапазона"));return false;}
        }else{basis={};issues="amortization_cost_unknown";}
    }
    if(basis.isNull() && issues.isEmpty())issues="cost_unknown";
    if(!op.feeMinor || !op.taxMinor)issues+=(issues.isEmpty()?QString():QStringLiteral(";"))+"charges_unknown";
    q.finish();q.prepare("UPDATE investment_position_history SET event=?,operation_id=?,occurred_at=?,before_quantity_micros=?,quantity_micros=?,"
        "before_cost_minor=?,cost_basis_minor=?,released_cost_minor=?,realized_net_minor=?,issues=?,state_json=? WHERE sequence=?");
    q.addBindValue(op.kind);q.addBindValue(op.id);q.addBindValue(op.occurredAtUtc.toMSecsSinceEpoch());q.addBindValue(before);q.addBindValue(after);
    q.addBindValue(previousBasis);q.addBindValue(basis);q.addBindValue(released);q.addBindValue(realized);q.addBindValue(issues);q.addBindValue(QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));q.addBindValue(sequence);
    if(!q.exec() || q.numRowsAffected()!=1){setLastError(q.lastError().text());return false;}
    // Previously captured dates affected by a newly booked historical event must be recalculated.
    const auto day=op.occurredAtUtc.toLocalTime().date().toString(Qt::ISODate);
    if(op.occurredAtUtc.toLocalTime().date()<QDate::currentDate()) {
        q.prepare("UPDATE investment_daily_snapshots SET issues=CASE WHEN instr(issues,'history_changed')>0 THEN issues ELSE issues||';history_changed' END WHERE account_id=? AND snapshot_date>=?");
        q.addBindValue(op.accountId);q.addBindValue(day);if(!q.exec()){setLastError(q.lastError().text());return false;}
    }
    return true;
}

QVariantList FinanceRepository::loadPriceHistory(AssetDailyValuation::Kind kind,const QString& id) {
    QVariantList rows;
    const QString table=kind==AssetDailyValuation::Crypto?QStringLiteral("crypto_price_history"):
        kind==AssetDailyValuation::Investment?QStringLiteral("investment_price_history"):QString();
    if(table.isEmpty())return rows;
    QSqlQuery q(database_);q.prepare("SELECT * FROM "+table+" WHERE asset_id=? ORDER BY quoted_at,source");q.addBindValue(id);
    if(!q.exec()){setLastError(q.lastError().text());return rows;}while(q.next())rows.append(rowMap(q));return rows;
}
QVariantList FinanceRepository::loadFxHistory(const QString& currency) {
    QVariantList rows;QSqlQuery q(database_);q.prepare("SELECT * FROM fx_rate_history WHERE currency=? ORDER BY rate_date,source");q.addBindValue(currency);
    if(!q.exec()){setLastError(q.lastError().text());return rows;}while(q.next())rows.append(rowMap(q));return rows;
}

QVariantList FinanceRepository::loadPerformanceAccounts() {
    QVariantList result; QSqlQuery q(database_);
    if(!q.exec("SELECT id,name,currency,initial_balance_minor,created_at FROM accounts WHERE is_archived=0 AND account_type IN (6,7) ORDER BY created_at,id")){setLastError(q.lastError().text());return result;}
    while(q.next())result.append(rowMap(q));return result;
}
QVariantList FinanceRepository::loadAllPositionHistory() {
    QVariantList result; QSqlQuery q(database_);
    if(!q.exec("SELECT * FROM investment_position_history ORDER BY sequence")){setLastError(q.lastError().text());return result;}
    while(q.next())result.append(rowMap(q));return result;
}
QString FinanceRepository::loadPerformanceInflation() const {
    QSqlQuery q(database_);q.prepare("SELECT value FROM settings WHERE key='performance_annual_inflation'");
    return q.exec() && q.next()?q.value(0).toString():QString();
}
bool FinanceRepository::savePerformanceInflation(const QString& value) {
    QSqlQuery q(database_);q.prepare("INSERT INTO settings(key,value) VALUES('performance_annual_inflation',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");q.addBindValue(value.isNull()?QStringLiteral(""):value);
    if(q.exec())return true;setLastError(q.lastError().text());return false;
}
