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
    m_configWatcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("powerdevilrc")));
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this,
        [this](const KConfigGroup &, const QByteArrayList &) { refresh(); });
}

void PowerPolicyBackend::refresh()
{
    auto *bus = QDBusConnection::sessionBus().interface();
    setAvailable(bus && bus->isServiceRegistered(service));
    const int generation = ++m_generation;
    if (!available()) { m_lidPresent = false; m_lidActionSupported = false; }
    else {
        for (const QString &method : {QStringLiteral("isLidPresent"), QStringLiteral("isActionSupported")}) {
            auto request = QDBusMessage::createMethodCall(service, QStringLiteral("/org/kde/Solid/PowerManagement"), service, method);
            if (method == QStringLiteral("isActionSupported")) request << QStringLiteral("HandleButtonEvents");
            auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
            connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation, method](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<bool> result = *finished; finished->deleteLater();
                if (generation != m_generation) return;
                const bool supported = !result.isError() && result.value();
                if (method == QStringLiteral("isLidPresent")) m_lidPresent = supported; else m_lidActionSupported = supported;
                Q_EMIT changed();
            });
        }
    }
    for (const QString &method : {QStringLiteral("CanSuspend"), QStringLiteral("CanHibernate")}) {
        auto request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), method);
        auto *call = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request, 3000), this);
        connect(call, &QDBusPendingCallWatcher::finished, this, [this, generation, method](QDBusPendingCallWatcher *finished) {
            const QDBusPendingReply<QString> result = *finished; finished->deleteLater();
            if (generation != m_generation) return;
            const bool supported = !result.isError() && (result.value() == "yes" || result.value() == "challenge");
            if (method == QStringLiteral("CanSuspend")) m_canSuspend = supported; else m_canHibernate = supported;
            Q_EMIT changed();
        });
    }
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
        m_policies.append(QVariantMap{{"id", ids[index]}, {"label", labels[index]}, {"screenMinutes", screen}, {"sleepMinutes", sleep},
            {"lidAction", suspend.hasKey("LidAction") ? suspend.readEntry("LidAction", -1) : -1},
            {"inhibitLidWithMonitor", suspend.readEntry("InhibitLidActionWhenExternalMonitorPresent", true)},
            {"lidWritable", !suspend.isEntryImmutable("LidAction") && !suspend.isEntryImmutable("InhibitLidActionWhenExternalMonitorPresent")},
            {"screenWritable", !display.isEntryImmutable("TurnOffDisplayWhenIdle") && !display.isEntryImmutable("TurnOffDisplayIdleTimeoutSec")},
            {"sleepWritable", !suspend.isEntryImmutable("AutoSuspendAction") && !suspend.isEntryImmutable("AutoSuspendIdleTimeoutSec")}});
    }
    const auto battery = config->group(QStringLiteral("BatteryManagement"));
    m_batteryPolicy = {{"lowPercent", battery.readEntry("BatteryLowLevel", 10)},
        {"criticalPercent", battery.readEntry("BatteryCriticalLevel", 5)},
        {"action", battery.hasKey("BatteryCriticalAction") ? battery.readEntry("BatteryCriticalAction", -1) : -1},
        {"writable", !battery.isEntryImmutable("BatteryLowLevel") && !battery.isEntryImmutable("BatteryCriticalLevel") && !battery.isEntryImmutable("BatteryCriticalAction")}};
    Q_EMIT changed();
}

void PowerPolicyBackend::setTimeout(const QString &profile, const QString &kind, int minutes)
{
    if (busy()) return;
    if (!available() || !QStringList{"AC", "Battery", "LowBattery"}.contains(profile)
        || (kind != "screen" && kind != "sleep") || minutes < 0 || minutes > 180 || (kind == "sleep" && minutes && !m_canSuspend)) {
        setError(tr("This power policy is unavailable or outside the supported range.")); return;
    }
    QVariantMap values;
    if (kind == "screen") values.insert("TurnOffDisplayWhenIdle", minutes > 0);
    else values.insert("AutoSuspendAction", minutes > 0 ? 1u : 0u);
    if (minutes) values.insert(kind == "screen" ? "TurnOffDisplayIdleTimeoutSec" : "AutoSuspendIdleTimeoutSec", minutes * 60);
    applySettings(profile, kind == "screen" ? QStringLiteral("Display") : QStringLiteral("SuspendAndShutdown"), values);
}

bool PowerPolicyBackend::supportsAction(int action, bool lid) const
{
    // PowerDevil::PowerButtonAction values from the maintained public schema.
    return action == 0 || action == 8 || (action == 1 && m_canSuspend)
        || (action == 2 && m_canHibernate) || (lid && (action == 32 || action == 64));
}

void PowerPolicyBackend::setLidPolicy(const QString &profile, int action, bool inhibitWithExternalMonitor)
{
    if (busy()) return;
    if (!lidPresent() || !QStringList{"AC", "Battery", "LowBattery"}.contains(profile) || !supportsAction(action, true)) {
        setError(tr("The requested lid action is not supported by this device.")); return;
    }
    applySettings(profile, QStringLiteral("SuspendAndShutdown"),
        {{"LidAction", uint(action)}, {"InhibitLidActionWhenExternalMonitorPresent", inhibitWithExternalMonitor}});
}

void PowerPolicyBackend::setBatteryPolicy(int lowPercent, int criticalPercent, int action)
{
    if (busy()) return;
    if (criticalPercent < 1 || lowPercent > 100 || criticalPercent >= lowPercent
        || (action != -1 && !supportsAction(action, false))) {
        setError(tr("Choose a critical level below the low-battery level and a supported action.")); return;
    }
    QVariantMap values{{"BatteryLowLevel", lowPercent}, {"BatteryCriticalLevel", criticalPercent}};
    // -1 preserves the service's dynamic default or the existing action.
    if (action != -1) values.insert("BatteryCriticalAction", uint(action));
    applySettings(QString(), QStringLiteral("BatteryManagement"), values);
}

void PowerPolicyBackend::applySettings(const QString &profile, const QString &subgroup, const QVariantMap &values)
{
    if (!available()) { setError(tr("The desktop power service is unavailable.")); return; }
    setBusy(true); clearError();
    if (profile.isEmpty()) { saveSettings(profile, subgroup, values); return; }
    // refreshStatus reloads the active profile and may retrigger a closed lid.
    // Read immediately before changing profile settings, rather than trusting a UI snapshot.
    auto request = QDBusMessage::createMethodCall(service, QStringLiteral("/org/kde/Solid/PowerManagement"), service, QStringLiteral("isLidClosed"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, profile, subgroup, values](QDBusPendingCallWatcher *finished) {
        const QDBusPendingReply<bool> result = *finished; finished->deleteLater();
        if (result.isError() || result.value()) {
            setError(tr("Open the laptop lid before applying power policies. The desktop must confirm that it is open."));
            setBusy(false); return;
        }
        saveSettings(profile, subgroup, values);
    });
}

void PowerPolicyBackend::saveSettings(const QString &profile, const QString &subgroup, const QVariantMap &values)
{
    if (!available()) { setError(tr("The desktop power service is unavailable.")); setBusy(false); return; }
    auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc")); config->reparseConfiguration();
    auto group = profile.isEmpty() ? config->group(subgroup) : config->group(profile).group(subgroup);
    QVariantMap previous;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (group.isEntryImmutable(it.key())) { setError(tr("This power policy is managed by the system administrator.")); setBusy(false); return; }
        if (group.hasKey(it.key())) previous.insert(it.key(), group.readEntry(it.key(), QString()));
    }
    const auto restore = [group, values, previous]() mutable {
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            if (previous.contains(it.key())) group.writeEntry(it.key(), previous.value(it.key()), KConfig::Notify);
            else group.deleteEntry(it.key(), KConfig::Notify);
        }
        return group.sync();
    };
    for (auto it = values.cbegin(); it != values.cend(); ++it) group.writeEntry(it.key(), it.value(), KConfig::Notify);
    if (!group.sync()) {
        auto recover = restore;
        setError(recover() ? tr("The power policy could not be saved; previous preferences were restored.")
                           : tr("The power policy and its recovery could not be saved. Check configuration permissions."));
        setBusy(false); refresh(); return;
    }
    const QString method = profile.isEmpty() ? QStringLiteral("reparseConfiguration") : QStringLiteral("refreshStatus");
    auto request = QDBusMessage::createMethodCall(service, QStringLiteral("/org/kde/Solid/PowerManagement"), service, method);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, restore](QDBusPendingCallWatcher *call) mutable {
        const QDBusPendingReply<> result = *call; call->deleteLater();
        if (result.isError()) {
            auto recover = restore;
            const bool restored = recover();
            // Reparse only: a second profile reload could itself trigger a lid action.
            if (restored) QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service,
                QStringLiteral("/org/kde/Solid/PowerManagement"), service, QStringLiteral("reparseConfiguration")), 3000);
            setError(restored ? tr("The power service did not confirm the change; previous preferences were restored: %1").arg(result.error().message())
                              : tr("Power policy recovery failed. Review advanced power settings: %1").arg(result.error().message()));
        }
        setBusy(false); refresh();
    });
}
