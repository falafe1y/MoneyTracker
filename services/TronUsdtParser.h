#pragma once

#include "../core/CryptoTransaction.h"

#include <QByteArray>
#include <QString>
#include <QVector>
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

bool parseTronUsdtTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    QVector<CryptoTransaction>& transactions,
    QString* error = nullptr
    );
