#pragma once

#include <QString>
#include <utility>

enum class AssetType {
    Fiat,
    Crypto,
    Investment
};

class Asset
{
public:
    Asset(
        QString id,
        QString name,
        AssetType type
        )
        : id_(std::move(id))
        , name_(std::move(name))
        , type_(type)
    {
    }

    const QString& id() const noexcept
    {
        return id_;
    }

    const QString& name() const noexcept
    {
        return name_;
    }

    AssetType type() const noexcept
    {
        return type_;
    }

private:
    QString id_;
    QString name_;
    AssetType type_;
};
