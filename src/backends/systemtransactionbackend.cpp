#include "systemtransactionbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace
{
constexpr auto kService = "org.meo.SystemTransaction1";
constexpr auto kPath = "/org/meo/SystemTransaction1";
constexpr auto kInterface = "org.meo.SystemTransaction1";
constexpr auto kInspectMethod = "Inspect";
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
    if (m_phase == QLatin1String("inspecting")) {
        setError(tr("A system configuration request is already being inspected."));
        return;
    }

    QDBusInterface transaction(QString::fromLatin1(kService),
                               QString::fromLatin1(kPath),
                               QString::fromLatin1(kInterface),
                               QDBusConnection::systemBus());
    if (!transaction.isValid()) {
        m_phase = QStringLiteral("error");
        setError(transaction.lastError().message().isEmpty()
                     ? tr("The Meo transaction service could not be opened.")
                     : transaction.lastError().message());
        Q_EMIT changed();
        return;
    }

    // Inspection is deliberately the only operation exposed by this client at
    // this stage. The privileged service validates the closed request and
    // returns a structured, non-executing plan. Authorization and Apply remain
    // separate service operations so showing a preview can never mutate state.
    m_phase = QStringLiteral("inspecting");
    m_lastPlan = {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("request"), request},
        {QStringLiteral("state"), QStringLiteral("inspecting")},
    };
    clearError();
    Q_EMIT changed();

    auto *watcher = new QDBusPendingCallWatcher(
        transaction.asyncCall(QString::fromLatin1(kInspectMethod), kind, request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *finished) {
                QDBusPendingReply<QVariantMap> reply = *finished;
                finished->deleteLater();

                if (reply.isError()) {
                    m_phase = QStringLiteral("error");
                    m_lastPlan.insert(QStringLiteral("state"), QStringLiteral("error"));
                    setError(reply.error().message().isEmpty()
                                 ? tr("The Meo transaction service rejected the inspection request.")
                                 : reply.error().message());
                    Q_EMIT changed();
                    return;
                }

                const QVariantMap plan = reply.value();
                if (plan.isEmpty()) {
                    m_phase = QStringLiteral("error");
                    m_lastPlan.insert(QStringLiteral("state"), QStringLiteral("error"));
                    setError(tr("The Meo transaction service returned an empty plan."));
                    Q_EMIT changed();
                    return;
                }

                m_lastPlan = plan;
                m_phase = QStringLiteral("planned");
                clearError();
                Q_EMIT changed();
            });
}

void SystemTransactionBackend::submitConfigurationRequest(const QString &configurationId,
                                                          const QString &operation,
                                                          const QVariantMap &payload)
{
    inspect(QStringLiteral("configuration"), {
        {QStringLiteral("configurationId"), configurationId},
        {QStringLiteral("operation"), operation},
        {QStringLiteral("payload"), payload},
    });
}
