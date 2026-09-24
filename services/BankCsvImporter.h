#pragma once

#include "CsvCodec.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct BankCsvProfile
{
    QString id;
    QString name;
    QString accountId;
    QString incomeCategoryId;
    QString expenseCategoryId;
    QString encoding = QStringLiteral("auto");
    QString delimiter = QStringLiteral("auto");
    QString dateFormat = QStringLiteral("auto");
    QString amountMode = QStringLiteral("signed");
    int headerRow = 0;
    int dateColumn = -1;
    int amountColumn = -1;
    int incomeColumn = -1;
    int expenseColumn = -1;
    int descriptionColumn = -1;
    int idColumn = -1;
    int categoryColumn = -1;
    int directionColumn = -1;
    int currencyColumn = -1;
    bool positiveMeansIncome = true;

    QJsonObject toJson() const;
    static BankCsvProfile fromJson(const QJsonObject& object);
};

struct BankCsvOperation
{
    QDateTime occurredAt;
    qint64 signedMinor = 0;
    QString description;
    QString externalId;
    QString categoryName;
    QString currencyCode;
    QString fingerprint;
    QString legacyFingerprint;
    int sourceRow = 0;
};

struct BankCsvParseResult
{
    QVector<BankCsvOperation> operations;
    QStringList errors;
    int rejected = 0;
    QChar delimiter;
    QString encoding;
};

class BankCsvImporter final
{
public:
    static CsvCodec::ReadOptions readOptions(const BankCsvProfile& profile);
    static CsvCodec::ReadResult readTable(
        const QString& filePath,
        const BankCsvProfile& profile
        );
    static BankCsvParseResult parse(
        const QString& filePath,
        const BankCsvProfile& profile
        );
    static bool parseAmountMinor(const QString& text, qint64& minorUnits);
    static QDateTime parseDateTime(
        const QString& text,
        const QString& format
        );
};
