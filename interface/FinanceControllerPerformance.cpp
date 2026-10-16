#include "FinanceController.h"
#include "../services/InvestmentPerformanceCalculator.h"
#include "../services/CryptoCostBasis.h"
#include "../services/InvestmentReturn.h"
#include <QSet>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

void FinanceController::rebuildInvestmentPerformance()
{
    if(!valuationHistoryReady_ || !repository_.isOpen())return;
    InvestmentPerformanceInput input;
    performanceAccounts_.clear();
    // A group is a portfolio of one kind of accounts: all brokerage accounts or all deposits. Money moved between accounts
    // of one group is not a flow; money moved to another group, or out of the accounts, is one.
    const bool brokerageGroup=performanceAccountId_==QStringLiteral("group:brokerage");
    const bool depositGroup=performanceAccountId_==QStringLiteral("group:deposit");
    for(const auto& value:repository_.loadPerformanceAccounts()){
        const auto row=value.toMap();
        performanceAccounts_.append(QVariantMap{{"id",row.value("id")},{"name",row.value("name")}});
        const int type=row.value("account_type").toInt();
        if((brokerageGroup && type!=6) || (depositGroup && type!=7))continue;
        input.accounts.append({row.value("id").toString(),row.value("name").toString(),row.value("currency").toString(),row.value("created_at").toLongLong(),row.value("initial_balance_minor").toLongLong()});
    }
    const bool group=brokerageGroup || depositGroup;
    if(!performanceAccountId_.isEmpty() && !group && std::none_of(performanceAccounts_.cbegin(),performanceAccounts_.cend(),[this](const auto& a){return a.toMap().value("id").toString()==performanceAccountId_;}))performanceAccountId_.clear();
    input.accountId=group?QString():performanceAccountId_;input.currency=currencyCode(appCurrency_);
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
    rebuildAverageReturn();
    emit performanceChanged();
}
// Average annual return: every instrument with the money it holds now and its own rate, from its own history
void FinanceController::rebuildAverageReturn()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QVariantMap creationDates = repository_.loadAccountCreationDates();
    const auto toApp = [this](qint64 minor, Currency currency) {
        return currencyConverter_.convert(Money(minor, currency), appCurrency_).minorUnits();
    };
    const QString investments = QStringLiteral("investments");
    QVariantList items;
    const auto add = [&items](const QString& id, const QString& group, const QString& title, qint64 valueMinor,
                              const AnnualReturn& result) {
        items.append(QVariantMap{{"id", id}, {"group", group}, {"title", title}, {"valueMinor", valueMinor},
                                 {"annualPercent", result.percent ? QVariant(*result.percent) : QVariant()},
                                 {"reason", result.reason}, {"days", result.days}});
    };
    // Investment positions: their own operations, and the value they have now
    for (const InvestmentPosition& position : investmentPositions_) {
        const auto account = std::find_if(accounts_.cbegin(), accounts_.cend(),
                                          [&](const auto& a) { return a.id() == position.accountId(); });
        const auto instrument = std::find_if(investmentInstruments_.cbegin(), investmentInstruments_.cend(),
                                             [&](const auto& i) { return i.id() == position.instrumentId(); });
        if (account == accounts_.cend() || instrument == investmentInstruments_.cend())
            continue;
        const QString id = QStringLiteral("position:") + position.id();
        const QString title = instrument->symbol() + QStringLiteral(" · ") + instrument->name();
        if (instrument->type() == InvestmentInstrumentType::Future || instrument->type() == InvestmentInstrumentType::Option) {
            add(id, investments, title, 0, AnnualReturn{std::nullopt, tr("производные не учитываются"), 0});
            continue;
        }
        QVector<ReturnFlow> flows;
        bool bought = false;
        for (const InvestmentOperation& op : investmentOperations_) {
            if (op.positionId != position.id())
                continue;
            flows.append({op.occurredAtUtc.toMSecsSinceEpoch(), op.cashDeltaMinor});
            bought = bought || op.kind == QStringLiteral("buy");
        }
        if (!bought) {
            // A holding that existed before Vexa: its cost is what was put in when it was added
            const QVariant basis = repository_.investmentCostBasis(position.id());
            if (basis.isNull()) {
                add(id, investments, title, 0, AnnualReturn{std::nullopt, tr("стоимость приобретения неизвестна"), 0});
                continue;
            }
            flows.append({position.createdAtUtc().toMSecsSinceEpoch(), -basis.toLongLong()});
        }
        const InvestmentValuation value = investmentPositionValue(position);
        if (!value.available) {
            add(id, investments, title, 0, AnnualReturn{std::nullopt, tr("нет актуальной оценки"), 0});
            continue;
        }
        add(id, investments, title, toApp(value.valueMinor, account->currency()),
            annualReturnOf(flows, nowMs, value.valueMinor));
    }
    // Deposit-like accounts: the money put in and taken out; interest and the bank's commission stay inside
    for (const Account& account : accounts_) {
        if (account.assetType() != AssetType::Investment || account.type() == AccountType::Brokerage)
            continue;
        const QString id = QStringLiteral("deposit:") + account.id();
        const QString title = accountDisplayName(account);
        const qint64 balance = accountBalanceMinor(account, false);
        const QVariant created = creationDates.value(account.id());
        if (created.isNull()) {
            add(id, investments, title, toApp(balance, account.currency()),
                AnnualReturn{std::nullopt, tr("нет даты открытия счёта"), 0});
            continue;
        }
        QVector<ReturnFlow> flows;
        if (account.initialBalanceMinor() != 0)
            flows.append({created.toLongLong(), -account.initialBalanceMinor()});
        for (const Transaction& transaction : transactions_) {
            if (transaction.accountId() != account.id())
                continue;
            const QString category = transaction.categoryId();
            if (category == QStringLiteral("deposit_interest") || category == QStringLiteral("bank_fee"))
                continue;
            const qint64 signedMinor = transaction.type() == TransactionType::Income ? transaction.money().minorUnits()
                                                                                    : -transaction.money().minorUnits();
            flows.append({transaction.date().toMSecsSinceEpoch(), -signedMinor});
        }
        add(id, investments, title, toApp(balance, account.currency()), annualReturnOf(flows, nowMs, balance));
    }
    // Crypto: the coins held on exchanges, each measured from its own trades. Crypto wallets are not counted.
    const auto usdRubByDay = rubPerUsdByDate(repository_.loadFxHistory(QStringLiteral("USD")));
    const qint64 usdRubNow = rateProvider_.rateToRubMicros(QStringLiteral("USD"));
    QDate firstMissingRate;
    for (const QVariant& value : cryptoExchanges_) {
        const QVariantMap exchange = value.toMap();
        const QString exchangeId = exchange.value(QStringLiteral("id")).toString();
        const QString exchangeName = exchange.value(QStringLiteral("name")).toString();
        // A coin can sit in several sections of the exchange: its quantity is summed over them
        QMap<QString, double> quantity;
        QMap<QString, double> priceUsd;
        for (const QVariant& entry : exchange.value(QStringLiteral("holdings")).toList()) {
            const QVariantMap row = entry.toMap();
            const QString coin = row.value(QStringLiteral("coin")).toString();
            if (row.value(QStringLiteral("summary")).toBool() || isStableCoin(coin))
                continue;
            quantity[coin] += row.value(QStringLiteral("amountText")).toString().toDouble();
            const QString price = row.value(QStringLiteral("priceUsd")).toString();
            if (!price.isEmpty() && !priceUsd.contains(coin))
                priceUsd.insert(coin, price.toDouble());
        }
        for (auto coin = quantity.cbegin(); coin != quantity.cend(); ++coin) {
            if (coin.value() <= 0)
                continue;
            const CoinReturn result = coinReturnOf(coin.key(), coin.value(), priceUsd.value(coin.key(), 0.0), usdRubNow,
                                                   exchange.value(QStringLiteral("history")).toList(), usdRubByDay, nowMs);
            if (result.firstMissingRateDate.isValid() && (!firstMissingRate.isValid() || result.firstMissingRateDate < firstMissingRate))
                firstMissingRate = result.firstMissingRateDate;
            add(QStringLiteral("crypto:") + exchangeId + QStringLiteral(":") + coin.key(), QStringLiteral("crypto"),
                exchangeName + QStringLiteral(" · ") + coin.key(), toApp(result.valueMinor, Currency::RUB), result.annual);
        }
    }
    // The CBR rates of the trade days are asked for when they are missing, at most once an hour
    if (firstMissingRate.isValid()) {
        const QDateTime requestNow = QDateTime::currentDateTimeUtc();
        if (!rateHistoryRequestedUtc_.isValid() || rateHistoryRequestedUtc_.secsTo(requestNow) >= 3600) {
            rateHistoryRequestedUtc_ = requestNow;
            rateProvider_.requestUsdHistory(firstMissingRate, QDate::currentDate());
        }
    }
    averageReturnItems_ = items;
    averageReturnSummary_ = averageReturnSummaryOf(items, averageReturnExcluded_, averageReturnMerged_);
}

void FinanceController::setAverageReturnSelection(const QStringList& excludedIds, bool merged)
{
    averageReturnExcluded_ = excludedIds;
    averageReturnMerged_ = merged;
    averageReturnSummary_ = averageReturnSummaryOf(averageReturnItems_, averageReturnExcluded_, averageReturnMerged_);
    emit performanceChanged();
}

void FinanceController::setPerformanceAccount(const QString& id)
{
    // A group is chosen by its key: all brokerage accounts or all deposits
    const bool group=id==QStringLiteral("group:brokerage") || id==QStringLiteral("group:deposit");
    if(!id.isEmpty() && !group && std::none_of(performanceAccounts_.cbegin(),performanceAccounts_.cend(),[&](const auto& a){return a.toMap().value("id").toString()==id;}))return;
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
