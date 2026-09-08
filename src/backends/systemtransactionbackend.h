#pragma once

#include "../core/backendbase.h"

#include <QVariantMap>

// Client for the privileged SystemTransaction1 service.  UI code receives a
// truthful unavailable state until the separately packaged service is present.
class SystemTransactionBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QVariantMap lastPlan READ lastPlan NOTIFY changed)

public:
    explicit SystemTransactionBackend(QObject *parent = nullptr);

    bool serviceAvailable() const;
    QString phase() const;
    QVariantMap lastPlan() const;

    Q_INVOKABLE void inspect(const QString &kind, const QVariantMap &request);
    Q_INVOKABLE void refresh();

public Q_SLOTS:
    void submitConfigurationRequest(const QString &configurationId, const QString &operation,
                                    const QVariantMap &payload);

Q_SIGNALS:
    void changed();

private:
    bool m_serviceAvailable = false;
    QString m_phase = QStringLiteral("idle");
    QVariantMap m_lastPlan;
};
