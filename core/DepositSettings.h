#pragma once

#include <QDate>
#include <QString>
#include <utility>

enum class DepositPayoutFrequency
{
    Monthly,
    Daily
};

class DepositSettings
{
public:
    DepositSettings(
        QString accountId,
        int annualRateBasisPoints,
        DepositPayoutFrequency payoutFrequency,
        int payoutDay,
        QDate startsOn,
        QDate generatedThrough = {}
        )
        : accountId_(std::move(accountId))
        , annualRateBasisPoints_(annualRateBasisPoints)
        , payoutFrequency_(payoutFrequency)
        , payoutDay_(payoutDay)
        , startsOn_(std::move(startsOn))
        , generatedThrough_(std::move(generatedThrough))
    {
    }

    const QString& accountId() const noexcept { return accountId_; }
    int annualRateBasisPoints() const noexcept { return annualRateBasisPoints_; }
    DepositPayoutFrequency payoutFrequency() const noexcept
    {
        return payoutFrequency_;
    }
    int payoutDay() const noexcept { return payoutDay_; }
    const QDate& startsOn() const noexcept { return startsOn_; }
    const QDate& generatedThrough() const noexcept { return generatedThrough_; }

private:
    QString accountId_;
    int annualRateBasisPoints_ = 0;
    DepositPayoutFrequency payoutFrequency_ = DepositPayoutFrequency::Monthly;
    int payoutDay_ = 1;
    QDate startsOn_;
    QDate generatedThrough_;
};
