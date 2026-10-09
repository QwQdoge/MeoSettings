#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QStringList>
#include <KConfigWatcher>

class FontSettingsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList fonts READ fonts NOTIFY changed)
    Q_PROPERTY(QStringList families READ families CONSTANT)
public:
    explicit FontSettingsBackend(QObject *parent = nullptr);
    QVariantList fonts() const { return m_fonts; }
    QStringList families() const { return m_families; }
    Q_INVOKABLE QStringList styles(const QString &family) const;
    Q_INVOKABLE void setFont(const QString &role, const QString &family, const QString &style, double points);
    Q_INVOKABLE void refresh();
Q_SIGNALS:
    void changed();
private:
    KConfigWatcher::Ptr m_watcher;
    QVariantList m_fonts;
    QStringList m_families;
};
