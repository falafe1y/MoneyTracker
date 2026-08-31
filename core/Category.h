#pragma once

#include <QString>
#include <utility>

enum class CategoryType {
    Income,
    Expense
};

class Category
{
public:
    Category(
        QString id,
        QString name,
        CategoryType type
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

    CategoryType type() const noexcept
    {
        return type_;
    }

private:
    QString id_;
    QString name_;
    CategoryType type_;
};