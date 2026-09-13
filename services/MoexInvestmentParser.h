#pragma once

#include "../core/InvestmentMarketInstrument.h"

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

bool parseMoexInvestmentSearch(
    const QByteArray& payload,
    QVector<InvestmentMarketInstrument>& instruments,
    QString* error = nullptr
    );

bool parseMoexInvestmentQuote(
    const QByteArray& payload,
    const QString& primaryBoardId,
    qint64& priceMicros,
    QString& currencyCode,
    QString* error = nullptr
    );
