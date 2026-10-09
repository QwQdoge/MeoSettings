#include "powerpolicybackend.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

namespace { const QString service = QStringLiteral("org.kde.Solid.PowerManagement"); }

PowerPolicyBackend::PowerPolicyBackend(QObject *parent) : BackendBase(parent)
{
    auto *watcher = new QDBusServiceWatcher(service, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &PowerPolicyBackend::refresh);
    refresh();
    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("CanSuspend"));
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request, 3000), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *finished) {
        const QDBusPendingReply<QString> result = *finished;
        m_canSuspend = !result.isError() && (result.value() == "yes" || result.value() == "challenge");
        finished->deleteLater(); Q_EMIT changed();
    });
}

void PowerPolicyBackend::refresh()
{
    setAvailable(QDBusConnection::sessionBus().interface()->isServiceRegistered(service));
    auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc")); config->reparseConfiguration();
    m_policies.clear();
    const QStringList ids{QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")};
    const QStringList labels{tr("Plugged in"), tr("On battery"), tr("Low battery")};
    for (int index = 0; index < ids.size(); ++index) {
        KConfigGroup group(config, ids[index]);
        const auto display = group.group(QStringLiteral("Display"));
        const auto suspend = group.group(QStringLiteral("SuspendAndShutdown"));
        // A missing entry means the service's platform-dependent default, never an invented value.
        const int screen = !display.hasKey("TurnOffDisplayWhenIdle") ? -1
            : !display.readEntry("TurnOffDisplayWhenIdle", true) ? 0
            : display.hasKey("TurnOffDisplayIdleTimeoutSec") ? display.readEntry("TurnOffDisplayIdleTimeoutSec", 0) / 60 : -1;
        const int sleep = !suspend.hasKey("AutoSuspendAction") ? -1
            : suspend.readEntry("AutoSuspendAction", 0u) == 0 ? 0
            : suspend.readEntry("AutoSuspendAction", 0u) == 1 && suspend.hasKey("AutoSuspendIdleTimeoutSec") ? suspend.readEntry("AutoSuspendIdleTimeoutSec", 0) / 60 : -1;
        m_policies.append(QVariantMap{{"id", ids[index]}, {"label", labels[index]}, {"screenMinutes", screen}, {"sleepMinutes", sleep}});
    }
    Q_EMIT changed();
}

void PowerPolicyBackend::setTimeout(const QString &profile, const QString &kind, int minutes)
{
    if (busy()) return;
    if (!available() || !QStringList{"AC", "Battery", "LowBattery"}.contains(profile)
        || (kind != "screen" && kind != "sleep") || minutes < 0 || minutes > 180 || (kind == "sleep" && minutes && !m_canSuspend)) {
        setError(tr("This power policy is unavailable or outside the supported range.")); return;
    }
    auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    KConfigGroup group = config->group(profile).group(kind == "screen" ? QStringLiteral("Display") : QStringLiteral("SuspendAndShutdown"));
    const QByteArray toggle = kind == "screen" ? QByteArray("TurnOffDisplayWhenIdle") : QByteArray("AutoSuspendAction");
    const QByteArray timeout = kind == "screen" ? QByteArray("TurnOffDisplayIdleTimeoutSec") : QByteArray("AutoSuspendIdleTimeoutSec");
    const bool hadToggle = group.hasKey(toggle.constData()), hadTimeout = group.hasKey(timeout.constData());
    const QVariant oldToggle = group.readEntry(toggle.constData(), QVariant()), oldTimeout = group.readEntry(timeout.constData(), QVariant());
    if (kind == "screen") group.writeEntry(toggle.constData(), minutes > 0, KConfig::Notify);
    else group.writeEntry(toggle.constData(), minutes > 0 ? 1u : 0u, KConfig::Notify);
    if (minutes) group.writeEntry(timeout.constData(), minutes * 60, KConfig::Notify);
    if (!group.sync()) { setError(tr("The power policy could not be saved.")); return; }
    setBusy(true); clearError();
    auto request = QDBusMessage::createMethodCall(service, QStringLiteral("/org/kde/Solid/PowerManagement"), service, QStringLiteral("refreshStatus"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, group, toggle, timeout, hadToggle, hadTimeout, oldToggle, oldTimeout](QDBusPendingCallWatcher *call) mutable {
        const QDBusPendingReply<> result = *call;
        if (result.isError()) {
            if (hadToggle) group.writeEntry(toggle.constData(), oldToggle, KConfig::Notify); else group.deleteEntry(toggle.constData(), KConfig::Notify);
            if (hadTimeout) group.writeEntry(timeout.constData(), oldTimeout, KConfig::Notify); else group.deleteEntry(timeout.constData(), KConfig::Notify);
            group.sync(); setError(result.error().message());
        }
        call->deleteLater(); setBusy(false); refresh();
    });
}
