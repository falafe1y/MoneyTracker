#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

// Can be nested inside a financial transaction: data and history commit together.
class HistorySavepoint {
public:
    explicit HistorySavepoint(QSqlDatabase& db) : db_(db), name_("history_" + QUuid::createUuid().toString(QUuid::Id128)) {
        QSqlQuery q(db_); active_ = q.exec("SAVEPOINT " + name_);
    }
    ~HistorySavepoint() {
        if (active_) { QSqlQuery q(db_); q.exec("ROLLBACK TO " + name_); q.exec("RELEASE " + name_); }
    }
    bool active() const { return active_; }
    bool commit() {
        QSqlQuery q(db_); if (!q.exec("RELEASE " + name_)) return false;
        active_ = false; return true;
    }
    HistorySavepoint(const HistorySavepoint&) = delete;
    HistorySavepoint& operator=(const HistorySavepoint&) = delete;
private:
    QSqlDatabase& db_; QString name_; bool active_ = false;
};
