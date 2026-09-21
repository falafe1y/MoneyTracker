#pragma once

#include <QString>
#include <utility>

class Project
{
public:
    Project(QString id, QString name)
        : id_(std::move(id))
        , name_(std::move(name))
    {
    }

    const QString& id() const noexcept { return id_; }
    const QString& name() const noexcept { return name_; }

private:
    QString id_;
    QString name_;
};
