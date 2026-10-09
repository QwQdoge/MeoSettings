#pragma once
#include "../core/backendbase.h"
#include <QVariantMap>
#include <KConfigWatcher>

class AccessibilityBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY changed)
public:
    explicit AccessibilityBackend(QObject *parent = nullptr);
    QVariantMap values() const { return m_values; }
    QVariantMap capabilities() const { return m_capabilities; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setOption(const QString &id, bool enabled);
Q_SIGNALS:
    void changed();
private:
    QVariantMap m_values, m_capabilities;
    QList<KConfigWatcher::Ptr> m_watchers;
    int m_generation = 0;
};
