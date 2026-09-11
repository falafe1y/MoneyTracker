#pragma once

#include "../core/CryptoTransaction.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

bool isValidBitcoinAddress(const QString& address);
QString normalizeBitcoinAddress(const QString& address);
bool isValidEthereumAddress(const QString& address);
QString normalizeEthereumAddress(const QString& address);

bool parseBitcoinBalanceResponse(
    const QByteArray& response,
    qint64& balanceSatoshis,
    QString* error = nullptr
    );

bool parseBitcoinTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    const QString& address,
    QVector<CryptoTransaction>& transactions,
    QString* error = nullptr
    );

bool parseEthereumBalanceResponse(
    const QByteArray& response,
    qint64& balanceAtomic,
    QString* error = nullptr
    );

bool parseEthereumTransactionsResponse(
    const QByteArray& response,
    const QString& walletId,
    const QString& address,
    QVector<CryptoTransaction>& transactions,
    QString* error = nullptr
    );

bool parseCoinGeckoPricesResponse(
    const QByteArray& response,
    QHash<QString, qint64>& pricesUsdMicros,
    QString* error = nullptr
    );
