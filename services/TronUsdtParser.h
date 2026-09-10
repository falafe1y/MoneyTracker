#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

bool decodeTronAddress(
    const QString& address,
    QByteArray& payload,
    QString* error = nullptr
    );

bool isValidTronAddress(const QString& address);

QString tronUsdtBalanceParameter(const QString& address);

bool parseTronUsdtBalanceResponse(
    const QByteArray& response,
    qint64& balanceAtomic,
    QString* error = nullptr
    );

bool parseCoinGeckoUsdtPriceResponse(
    const QByteArray& response,
    qint64& priceUsdMicros,
    QString* error = nullptr
    );
