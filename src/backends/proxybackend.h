#pragma once
#include "../core/backendbase.h"
#include <QVariantMap>

class ProxyBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap configuration READ configuration NOTIFY changed)
public:
    explicit ProxyBackend(QObject *parent = nullptr);
    QVariantMap configuration() const { return m_configuration; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void apply(const QVariantMap &configuration);
Q_SIGNALS:
    void changed();
private:
    QVariantMap m_configuration;
};
