#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>

// Monetary and quantity values use six decimal places. Store integers as JSON
// strings, so backup/restore does not lose precision above 2^53.
struct InvestmentTerms
{
    QString engine;
    QString market;
    QString pricing = QStringLiteral("unit");
    QString currencyCode;
    QString quoteSource;
    QString quantityUnit = QStringLiteral("шт.");
    QString underlying;
    QString optionRight;
    qint64 faceValueMicros = 0;
    qint64 accruedInterestMicros = -1; // -1 means unavailable, zero is valid.
    qint64 priceStepMicros = 0;
    qint64 stepValueMicros = 0;
    qint64 multiplierMicros = 1'000'000;
    qint64 lotSizeMicros = 1'000'000;
    qint64 settlementPriceMicros = 0;
    qint64 strikeMicros = 0;
    QDate maturity;
    bool perpetual = false;

    QJsonObject toJson() const
    {
        return {{"engine", engine}, {"market", market}, {"pricing", pricing},
                {"currencyCode", currencyCode}, {"quoteSource", quoteSource}, {"quantityUnit", quantityUnit},
                {"underlying", underlying}, {"optionRight", optionRight},
                {"face", QString::number(faceValueMicros)},
                {"accrued", QString::number(accruedInterestMicros)},
                {"step", QString::number(priceStepMicros)},
                {"stepValue", QString::number(stepValueMicros)},
                {"multiplier", QString::number(multiplierMicros)},
                {"lot", QString::number(lotSizeMicros)},
                {"settlement", QString::number(settlementPriceMicros)},
                {"strike", QString::number(strikeMicros)},
                {"maturity", maturity.toString(Qt::ISODate)}, {"perpetual", perpetual}};
    }
    QString serialize() const { return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact)); }
    static InvestmentTerms fromJson(const QJsonObject& j)
    {
        InvestmentTerms t;
        t.engine = j.value("engine").toString(); t.market = j.value("market").toString();
        t.pricing = j.value("pricing").toString(QStringLiteral("unit"));
        t.currencyCode = j.value("currencyCode").toString();
        t.quoteSource = j.value("quoteSource").toString();
        t.quantityUnit = j.value("quantityUnit").toString(QStringLiteral("шт."));
        t.underlying = j.value("underlying").toString(); t.optionRight = j.value("optionRight").toString();
        const auto number = [&j](const char* key, qint64 fallback = 0) {
            bool ok = false; const qint64 n = j.value(QLatin1String(key)).toVariant().toLongLong(&ok);
            return ok ? n : fallback;
        };
        t.faceValueMicros = number("face"); t.accruedInterestMicros = number("accrued", -1);
        t.priceStepMicros = number("step"); t.stepValueMicros = number("stepValue");
        t.multiplierMicros = number("multiplier", 1'000'000); t.lotSizeMicros = number("lot", 1'000'000);
        t.settlementPriceMicros = number("settlement"); t.strikeMicros = number("strike");
        t.maturity = QDate::fromString(j.value("maturity").toString(), Qt::ISODate);
        t.perpetual = j.value("perpetual").toBool(); return t;
    }
    static InvestmentTerms deserialize(const QString& s) { return fromJson(QJsonDocument::fromJson(s.toUtf8()).object()); }
};

struct InvestmentPositionSettings
{
    int direction = 1; // -1: sold option or short derivative position.
    bool manualValuation = false;
    qint64 manualValueMinor = 0; // Total position value in the account currency.
    qint64 referencePriceMicros = 0; // Opening price or last booked clearing price.
    QDateTime referenceAtUtc;
    qint64 blockedMarginMinor = 0; // Part of cash, never an additional asset/expense.
    qint64 unsettledAdjustmentMinor = 0; // Funding/other unbooked adjustments.
    qint64 manualFxRateMicros = 0; // Quote currency -> account currency; zero: automatic.
    QDateTime valuedAtUtc;

    QString serialize() const
    {
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"direction", direction}, {"manual", manualValuation},
            {"value", QString::number(manualValueMinor)},
            {"reference", QString::number(referencePriceMicros)},
            {"referenceAt", referenceAtUtc.toString(Qt::ISODateWithMs)},
            {"margin", QString::number(blockedMarginMinor)},
            {"adjustment", QString::number(unsettledAdjustmentMinor)},
            {"fx", QString::number(manualFxRateMicros)},
            {"valuedAt", valuedAtUtc.toString(Qt::ISODateWithMs)}}).toJson(QJsonDocument::Compact));
    }
    static InvestmentPositionSettings deserialize(const QString& s)
    {
        const auto j = QJsonDocument::fromJson(s.toUtf8()).object(); InvestmentPositionSettings p;
        p.direction = j.value("direction").toInt(1); p.manualValuation = j.value("manual").toBool();
        p.manualValueMinor = j.value("value").toVariant().toLongLong();
        p.referencePriceMicros = j.value("reference").toVariant().toLongLong();
        p.referenceAtUtc = QDateTime::fromString(j.value("referenceAt").toString(), Qt::ISODateWithMs).toUTC();
        p.blockedMarginMinor = j.value("margin").toVariant().toLongLong();
        p.unsettledAdjustmentMinor = j.value("adjustment").toVariant().toLongLong();
        p.manualFxRateMicros = j.value("fx").toVariant().toLongLong();
        p.valuedAtUtc = QDateTime::fromString(j.value("valuedAt").toString(), Qt::ISODateWithMs).toUTC(); return p;
    }
};
