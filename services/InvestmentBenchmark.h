#pragma once
#include <QVariant>
#include <QDate>
#include <QByteArray>

QVariantList investmentBenchmarkIndices();
bool parseBenchmarkPage(const QByteArray& payload, const QVariantMap& descriptor,
                        QVariantList& rows, int& next, QString& error);
bool mergeBenchmarkRows(const QVariantList& rows, QVariantList& prices, QString& error);
QVariantMap compareInvestmentBenchmark(const QVariantList& portfolio, const QVariantList& prices,
                                      const QVariantList& rates, const QString& currency);
