#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

class InvestmentPosition
{
public:
    static constexpr qint64 Scale = 1'000'000;

    InvestmentPosition(
        QString id,
        QString accountId,
        QString instrumentId,
        qint64 quantityMicros,
        qint64 averagePriceMicros,
        QDateTime createdAtUtc = {},
        QDateTime updatedAtUtc = {}
        )
        : id_(std::move(id))
        , accountId_(std::move(accountId))
        , instrumentId_(std::move(instrumentId))
        , quantityMicros_(quantityMicros)
        , averagePriceMicros_(averagePriceMicros)
        , createdAtUtc_(std::move(createdAtUtc))
        , updatedAtUtc_(std::move(updatedAtUtc))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& accountId() const noexcept { return accountId_; }
    const QString& instrumentId() const noexcept { return instrumentId_; }
    qint64 quantityMicros() const noexcept { return quantityMicros_; }
    qint64 averagePriceMicros() const noexcept { return averagePriceMicros_; }
    const QDateTime& createdAtUtc() const noexcept { return createdAtUtc_; }
    const QDateTime& updatedAtUtc() const noexcept { return updatedAtUtc_; }

private:
    QString id_;
    QString accountId_;
    QString instrumentId_;
    qint64 quantityMicros_ = 0;
    qint64 averagePriceMicros_ = 0;
    QDateTime createdAtUtc_;
    QDateTime updatedAtUtc_;
};
