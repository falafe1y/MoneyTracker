#pragma once
#include <QString>
#include <QPair>

// Credentials never enter SQLite, backups, logs, or command-line arguments.
namespace ExchangeCredentials {
QPair<QString, QString> load(const QString& id);
bool save(const QString& id, const QString& apiKey, const QString& secret);
void remove(const QString& id);
}
