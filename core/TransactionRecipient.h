#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

struct TransactionRecipient {
    QString name;
    QString key;
    QString source;
    QString serialize() const {
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"name",name},{"key",key},{"source",source}}).toJson(QJsonDocument::Compact));
    }
    static TransactionRecipient deserialize(const QString& text) {
        const auto value=QJsonDocument::fromJson(text.toUtf8()).object();
        return {value.value("name").toString(),value.value("key").toString(),value.value("source").toString()};
    }
};
