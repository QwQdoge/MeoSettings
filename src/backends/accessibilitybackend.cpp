#include "accessibilitybackend.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QStandardPaths>
#include <QProcess>

namespace {
const QMap<QString, QString> effects{{"zoom", "zoom"}, {"magnifier", "magnifier"}, {"invert", "invert"}, {"shakePointer", "shakecursor"}};
struct Preference { const char *id; const char *group; const char *key; bool fallback; };
constexpr Preference keyboard[] = {{"stickyKeys", "Keyboard", "StickyKeys", false},
    {"stickyLatch", "Keyboard", "StickyKeysLatch", true}, {"stickyAutoOff", "Keyboard", "StickyKeysAutoOff", false},
    {"mouseKeys", "Mouse", "MouseKeys", false}};
QDBusMessage effectRequest(const QString &method) { return QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"), method); }
}

AccessibilityBackend::AccessibilityBackend(QObject *parent) : BackendBase(parent)
{
    for (const auto &file : {QStringLiteral("kaccessrc"), QStringLiteral("kdeglobals"), QStringLiteral("kwinrc")}) {
        auto watcher = KConfigWatcher::create(KSharedConfig::openConfig(file));
        connect(watcher.data(), &KConfigWatcher::configChanged, this, [this] { refresh(); }); m_watchers.append(watcher);
    }
    refresh();
}

void AccessibilityBackend::refresh()
{
    const bool kwin = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.KWin"));
    const bool keyboardSupported = (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" && kwin)
        || (qEnvironmentVariable("XDG_SESSION_TYPE") == "x11" && !QStandardPaths::findExecutable(QStringLiteral("kaccess"), {QStringLiteral("/usr/bin")}).isEmpty());
    auto access = KSharedConfig::openConfig(QStringLiteral("kaccessrc")); access->reparseConfiguration();
    for (const auto &preference : keyboard) {
        m_values.insert(QString::fromLatin1(preference.id), access->group(QString::fromLatin1(preference.group)).readEntry(preference.key, preference.fallback));
        m_capabilities.insert(QString::fromLatin1(preference.id), keyboardSupported);
    }
    auto global = KSharedConfig::openConfig(QStringLiteral("kdeglobals")); global->reparseConfiguration();
    m_values.insert("reduceMotion", global->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0) <= 0.0);
    m_capabilities.insert("reduceMotion", true);
    setAvailable(true); Q_EMIT changed();
    const int generation = ++m_generation;
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if (!kwin) { m_capabilities.insert(it.key(), false); continue; }
        auto request = effectRequest(QStringLiteral("isEffectSupported")); request << it.value();
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, id = it.key(), effect = it.value()](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<bool> result = *call; call->deleteLater();
            if (generation != m_generation) return;
            m_capabilities.insert(id, !result.isError() && result.value()); Q_EMIT changed();
            auto request = effectRequest(QStringLiteral("isEffectLoaded")); request << effect;
            auto *loaded = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
            connect(loaded, &QDBusPendingCallWatcher::finished, this, [this, generation, id](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<bool> reply = *finished; finished->deleteLater();
                if (generation != m_generation) return;
                m_values.insert(id, !reply.isError() && reply.value()); Q_EMIT changed();
            });
        });
    }
    if (!kwin) Q_EMIT changed();
}

void AccessibilityBackend::setOption(const QString &id, bool enabled)
{
    if (busy()) return;
    if (!m_capabilities.value(id).toBool()) { setError(tr("This accessibility feature is unavailable.")); return; }
    clearError();
    if (id == "reduceMotion") {
        auto group = KSharedConfig::openConfig(QStringLiteral("kdeglobals"))->group(QStringLiteral("KDE"));
        group.writeEntry("AnimationDurationFactor", enabled ? 0.0 : 1.0, KConfig::Notify);
        if (!group.sync()) { setError(tr("The motion preference could not be saved.")); return; }
        auto signal = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"), QStringLiteral("notifyChange"));
        signal << 3 << 4; QDBusConnection::sessionBus().send(signal); refresh(); return;
    }
    for (const auto &preference : keyboard) if (id == QLatin1String(preference.id)) {
        auto group = KSharedConfig::openConfig(QStringLiteral("kaccessrc"))->group(QString::fromLatin1(preference.group));
        group.writeEntry(preference.key, enabled, KConfig::Notify);
        if (!group.sync()) { setError(tr("The accessibility preference could not be saved.")); return; }
        // Wayland KWin filters observe kaccessrc directly; X11 kaccess reloads on activation.
        if (qEnvironmentVariable("XDG_SESSION_TYPE") == "x11") {
            const QString owner = QStandardPaths::findExecutable(QStringLiteral("kaccess"), {QStringLiteral("/usr/bin")});
            if (!QProcess::startDetached(owner, {})) setError(tr("The accessibility service could not be activated."));
        }
        refresh(); return;
    }
    if (!effects.contains(id)) { setError(tr("Unsupported accessibility preference.")); return; }
    const QString effect = effects.value(id), key = effect + QStringLiteral("Enabled");
    auto group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Plugins"));
    const bool hadEntry = group.hasKey(key); const QVariant previous = group.readEntry(key, QVariant());
    group.writeEntry(key, enabled, KConfig::Notify);
    if (!group.sync()) { setError(tr("The visual accessibility preference could not be saved.")); return; }
    setBusy(true); ++m_generation;
    auto request = effectRequest(enabled ? QStringLiteral("loadEffect") : QStringLiteral("unloadEffect")); request << effect;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, enabled, group, key, hadEntry, previous](QDBusPendingCallWatcher *call) mutable {
        const auto result = call->reply();
        const bool failed = result.type() == QDBusMessage::ErrorMessage || (enabled && (result.arguments().isEmpty() || !result.arguments().first().toBool()));
        if (failed) {
            if (hadEntry) group.writeEntry(key, previous, KConfig::Notify); else group.deleteEntry(key, KConfig::Notify);
            group.sync(); setError(result.errorMessage().isEmpty() ? tr("KWin could not enable this accessibility effect.") : result.errorMessage());
        }
        call->deleteLater(); setBusy(false); refresh();
    });
}
