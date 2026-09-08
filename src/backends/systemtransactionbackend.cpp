#include "systemtransactionbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

namespace
{
constexpr auto kService = "org.meo.SystemTransaction1";
}

SystemTransactionBackend::SystemTransactionBackend(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

bool SystemTransactionBackend::serviceAvailable() const { return m_serviceAvailable; }
QString SystemTransactionBackend::phase() const { return m_phase; }
QVariantMap SystemTransactionBackend::lastPlan() const { return m_lastPlan; }

void SystemTransactionBackend::refresh()
{
    const bool next = QDBusConnection::systemBus().interface()
        && QDBusConnection::systemBus().interface()->isServiceRegistered(QString::fromLatin1(kService));
    if (next == m_serviceAvailable) {
        return;
    }
    m_serviceAvailable = next;
    setAvailable(next);
    if (!next) {
        m_phase = QStringLiteral("unavailable");
        setError(tr("The Meo transaction service is not installed or running."));
    } else {
        m_phase = QStringLiteral("idle");
        clearError();
    }
    Q_EMIT changed();
}

void SystemTransactionBackend::inspect(const QString &kind, const QVariantMap &request)
{
    refresh();
    if (!m_serviceAvailable) {
        return;
    }
    // The service owns all privileged inspection.  The client stores only the
    // structured request, never a command string or a password.
    m_phase = QStringLiteral("inspect");
    m_lastPlan = {{QStringLiteral("kind"), kind}, {QStringLiteral("request"), request},
                  {QStringLiteral("state"), QStringLiteral("requested")}};
    Q_EMIT changed();
}

void SystemTransactionBackend::submitConfigurationRequest(const QString &configurationId,
                                                          const QString &operation,
                                                          const QVariantMap &payload)
{
    inspect(QStringLiteral("configuration"), {{QStringLiteral("configurationId"), configurationId},
                                                {QStringLiteral("operation"), operation},
                                                {QStringLiteral("payload"), payload}});
}
