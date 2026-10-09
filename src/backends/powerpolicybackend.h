#pragma once
#include "../core/backendbase.h"
#include <QVariantList>

class PowerPolicyBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList policies READ policies NOTIFY changed)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY changed)
public:
    explicit PowerPolicyBackend(QObject *parent = nullptr);
    QVariantList policies() const { return m_policies; }
    bool canSuspend() const { return m_canSuspend; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setTimeout(const QString &profile, const QString &kind, int minutes);
Q_SIGNALS:
    void changed();
private:
    QVariantList m_policies;
    bool m_canSuspend = false;
};
