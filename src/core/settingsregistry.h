#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class SettingsRegistry final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries CONSTANT)
    Q_PROPERTY(QVariantList sidebarEntries READ sidebarEntries CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)

public:
    explicit SettingsRegistry(QObject *parent = nullptr);

    QVariantList entries() const;
    QVariantList sidebarEntries() const;
    QVariantList categories() const;

    Q_INVOKABLE QVariantList search(const QString &query) const;
    Q_INVOKABLE QVariantMap entry(const QString &idOrRoute) const;
    Q_INVOKABLE QVariantMap category(const QString &id) const;
    Q_INVOKABLE QVariantList entriesForCategory(const QString &categoryId) const;

private:
    QVariantList m_entries;
    QVariantList m_sidebarEntries;
    QVariantList m_categories;
};
