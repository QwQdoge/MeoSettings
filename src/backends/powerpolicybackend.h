#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QVariantMap>
#include <KConfigWatcher>

class PowerPolicyBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList policies READ policies NOTIFY changed)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY changed)
    Q_PROPERTY(bool canHibernate READ canHibernate NOTIFY changed)
    Q_PROPERTY(bool lidPresent READ lidPresent NOTIFY changed)
    Q_PROPERTY(QVariantMap batteryPolicy READ batteryPolicy NOTIFY changed)
public:
    explicit PowerPolicyBackend(QObject *parent = nullptr);
    QVariantList policies() const { return m_policies; }
    bool canSuspend() const { return m_canSuspend; }
    bool canHibernate() const { return m_canHibernate; }
    bool lidPresent() const { return m_lidPresent && m_lidActionSupported; }
    QVariantMap batteryPolicy() const { return m_batteryPolicy; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setLidPolicy(const QString &profile, int action, bool inhibitWithExternalMonitor);
    Q_INVOKABLE void setBatteryPolicy(int lowPercent, int criticalPercent, int action);
    Q_INVOKABLE void setTimeout(const QString &profile, const QString &kind, int minutes);
Q_SIGNALS:
    void changed();
private:
    bool supportsAction(int action, bool lid) const;
    void applySettings(const QString &profile, const QString &subgroup, const QVariantMap &values);
    void saveSettings(const QString &profile, const QString &subgroup, const QVariantMap &values);
    QVariantList m_policies;
    QVariantMap m_batteryPolicy;
    KConfigWatcher::Ptr m_configWatcher;
    int m_generation = 0;
    bool m_canHibernate = false;
    bool m_lidPresent = false;
    bool m_lidActionSupported = false;
    bool m_canSuspend = false;
};
