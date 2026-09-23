#include "shellsettingsbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QtGlobal>

namespace
{
constexpr auto plasmaShellService = "org.kde.plasmashell";
constexpr auto plasmaShellPath = "/PlasmaShell";
constexpr auto plasmaShellInterface = "org.kde.PlasmaShell";
constexpr auto appearanceGroup = "Appearance";

constexpr auto shelfPlugin = "org.meo.shelf";
constexpr auto notificationsPlugin = "org.meo.notifications";
constexpr auto timeCenterPlugin = "org.meo.timecenter";
constexpr auto topTasksPlugin = "org.meo.toptasks";

bool plasmaShellIsAvailable()
{
    const auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected() || !connection.interface()) {
        return false;
    }
    const auto registered = connection.interface()->isServiceRegistered(QString::fromLatin1(plasmaShellService));
    return registered.isValid() && registered.value();
}

QString scriptErrorMessage(const QDBusError &error)
{
    return error.message().isEmpty()
        ? QStringLiteral("Plasma Shell did not return a shell-settings response.")
        : error.message();
}

bool isOneOf(const QString &value, const QStringList &allowed)
{
    return allowed.contains(value);
}

QVariantMap withAvailability(QVariantMap settings, int count)
{
    settings.insert(QStringLiteral("available"), count == 1);
    settings.insert(QStringLiteral("count"), count);
    return settings;
}

QString writeSurfaceScript(const QString &plugin, const QVariantMap &settings, const QStringList &keys)
{
    const auto payload = QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(settings)).toJson(QJsonDocument::Compact));

    QJsonArray keyArray;
    for (const auto &key : keys) {
        keyArray.append(key);
    }
    const auto serializedKeys = QString::fromUtf8(QJsonDocument(keyArray).toJson(QJsonDocument::Compact));

    return QStringLiteral(R"JS(
var matches = [];
var panelList = panels();
for (var panelIndex = 0; panelIndex < panelList.length; ++panelIndex) {
    var widgets = panelList[panelIndex].widgets();
    for (var widgetIndex = 0; widgetIndex < widgets.length; ++widgetIndex) {
        if (widgets[widgetIndex].type === "%1")
            matches.push(widgets[widgetIndex]);
    }
}
if (matches.length !== 1) {
    print(JSON.stringify({ok: false, reason: matches.length === 0 ? "missing" : "multiple"}));
} else {
    var target = matches[0];
    var desired = %2;
    var keys = %3;
    target.currentConfigGroup = ["%4"];
    for (var index = 0; index < keys.length; ++index)
        target.writeConfig(keys[index], desired[keys[index]]);
    target.reloadConfig();
    print(JSON.stringify({ok: true}));
}
)JS")
        .arg(plugin, payload, serializedKeys, QString::fromLatin1(appearanceGroup));
}

QString failMessage(const QString &name, const QString &detail)
{
    return QObject::tr("%1: %2").arg(name, detail);
}
}

ShellSettingsBackend::ShellSettingsBackend(QObject *parent)
    : BackendBase(parent)
{
    m_shelf = withAvailability(normalizedShelf({}), 0);
    m_notifications = withAvailability(normalizedNotifications({}), 0);
    m_timeCenter = withAvailability(normalizedTimeCenter({}), 0);
    m_topTasks = withAvailability(normalizedTopTasks({}), 0);
    refresh();
}

QVariantMap ShellSettingsBackend::shelf() const { return m_shelf; }
QVariantMap ShellSettingsBackend::notifications() const { return m_notifications; }
QVariantMap ShellSettingsBackend::timeCenter() const { return m_timeCenter; }
QVariantMap ShellSettingsBackend::topTasks() const { return m_topTasks; }

void ShellSettingsBackend::refresh()
{
    if (busy()) {
        return;
    }

    clearError();
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so Meo shell surfaces cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                         QString::fromLatin1(plasmaShellPath),
                         QString::fromLatin1(plasmaShellInterface),
                         QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the shell configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        shell.asyncCall(QStringLiteral("evaluateScript"), readScript()), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QString> reply = *watcher;
                setBusy(false);
                if (reply.isError()) {
                    setAvailable(false);
                    setError(scriptErrorMessage(reply.error()));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto document = QJsonDocument::fromJson(reply.value().trimmed().toUtf8());
                if (!document.isObject()) {
                    setAvailable(false);
                    setError(tr("Plasma Shell returned an invalid shell-settings response."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto root = document.object();
                const auto apply = [this, &root](const QString &surface) {
                    const auto object = root.value(surface).toObject();
                    setSurface(surface, object.toVariantMap(), object.value(QStringLiteral("count")).toInt());
                };

                apply(QStringLiteral("shelf"));
                apply(QStringLiteral("notifications"));
                apply(QStringLiteral("timeCenter"));
                apply(QStringLiteral("topTasks"));
                setAvailable(true);
                Q_EMIT changed();
                watcher->deleteLater();
            });
}

void ShellSettingsBackend::saveShelf(const QVariantMap &settings)
{
    QString error;
    const auto serialized = serializeShelf(settings, &error);
    if (serialized.isEmpty()) {
        setError(error);
        return;
    }
    saveSurface(QStringLiteral("shelf"), serialized);
}

void ShellSettingsBackend::saveNotifications(const QVariantMap &settings)
{
    QString error;
    const auto serialized = serializeNotifications(settings, &error);
    if (serialized.isEmpty()) {
        setError(error);
        return;
    }
    saveSurface(QStringLiteral("notifications"), serialized);
}

void ShellSettingsBackend::saveTimeCenter(const QVariantMap &settings)
{
    QString error;
    const auto serialized = serializeTimeCenter(settings, &error);
    if (serialized.isEmpty()) {
        setError(error);
        return;
    }
    saveSurface(QStringLiteral("timeCenter"), serialized);
}

void ShellSettingsBackend::saveTopTasks(const QVariantMap &settings)
{
    QString error;
    const auto serialized = serializeTopTasks(settings, &error);
    if (serialized.isEmpty()) {
        setError(error);
        return;
    }
    saveSurface(QStringLiteral("topTasks"), serialized);
}

void ShellSettingsBackend::resetShelf() { saveShelf(normalizedShelf({})); }
void ShellSettingsBackend::resetNotifications() { saveNotifications(normalizedNotifications({})); }
void ShellSettingsBackend::resetTimeCenter() { saveTimeCenter(normalizedTimeCenter({})); }
void ShellSettingsBackend::resetTopTasks() { saveTopTasks(normalizedTopTasks({})); }

QVariantMap ShellSettingsBackend::normalizedShelf(const QVariantMap &settings)
{
    const QString defaultPage = settings.value(QStringLiteral("launcherDefaultPage"), QStringLiteral("home")).toString();
    const QString width = settings.value(QStringLiteral("launcherWidth"), QStringLiteral("standard")).toString();

    return {
        {QStringLiteral("showLauncherButton"), settings.value(QStringLiteral("showLauncherButton"), true).toBool()},
        {QStringLiteral("filterTasksByVirtualDesktop"), settings.value(QStringLiteral("filterTasksByVirtualDesktop"), false).toBool()},
        {QStringLiteral("showRunningIndicators"), settings.value(QStringLiteral("showRunningIndicators"), true).toBool()},
        {QStringLiteral("showTooltips"), settings.value(QStringLiteral("showTooltips"), true).toBool()},
        {QStringLiteral("launcherDefaultPage"), isOneOf(defaultPage, {QStringLiteral("home"), QStringLiteral("apps")})
                                                ? defaultPage : QStringLiteral("home")},
        {QStringLiteral("launcherWidth"), isOneOf(width, {QStringLiteral("compact"), QStringLiteral("standard"), QStringLiteral("wide")})
                                          ? width : QStringLiteral("standard")},
        {QStringLiteral("launcherShowFavorites"), settings.value(QStringLiteral("launcherShowFavorites"), true).toBool()},
        {QStringLiteral("launcherShowRecents"), settings.value(QStringLiteral("launcherShowRecents"), true).toBool()},
    };
}

QVariantMap ShellSettingsBackend::normalizedNotifications(const QVariantMap &settings)
{
    const QString density = settings.value(QStringLiteral("density"), QStringLiteral("comfortable")).toString();
    const QString surface = settings.value(QStringLiteral("surfaceStyle"), QStringLiteral("theme")).toString();
    const QString motion = settings.value(QStringLiteral("motionProfile"), QStringLiteral("pixel")).toString();
    const QString view = settings.value(QStringLiteral("notificationView"), QStringLiteral("cards")).toString();
    const QString preview = settings.value(QStringLiteral("notificationPreview"), QStringLiteral("full")).toString();

    return {
        {QStringLiteral("density"), isOneOf(density, {QStringLiteral("compact"), QStringLiteral("comfortable")})
                                      ? density : QStringLiteral("comfortable")},
        {QStringLiteral("textScalePercent"), qBound(85, settings.value(QStringLiteral("textScalePercent"), 100).toInt(), 125)},
        {QStringLiteral("surfaceStyle"), isOneOf(surface, {QStringLiteral("theme"), QStringLiteral("flat"),
                                                            QStringLiteral("tonal"), QStringLiteral("translucent")})
                                           ? surface : QStringLiteral("theme")},
        {QStringLiteral("surfaceOpacityPercent"), qBound(70, settings.value(QStringLiteral("surfaceOpacityPercent"), 100).toInt(), 100)},
        {QStringLiteral("motionProfile"), isOneOf(motion, {QStringLiteral("calm"), QStringLiteral("pixel"), QStringLiteral("playful")})
                                            ? motion : QStringLiteral("pixel")},
        {QStringLiteral("showUnreadBadge"), settings.value(QStringLiteral("showUnreadBadge"), true).toBool()},
        {QStringLiteral("showJobs"), settings.value(QStringLiteral("showJobs"), true).toBool()},
        {QStringLiteral("showNotificationHistory"), settings.value(QStringLiteral("showNotificationHistory"), true).toBool()},
        {QStringLiteral("notificationView"), isOneOf(view, {QStringLiteral("cards"), QStringLiteral("compact")})
                                               ? view : QStringLiteral("cards")},
        {QStringLiteral("notificationPreview"), isOneOf(preview, {QStringLiteral("full"), QStringLiteral("summary"), QStringLiteral("hidden")})
                                                  ? preview : QStringLiteral("full")},
    };
}

QVariantMap ShellSettingsBackend::normalizedTimeCenter(const QVariantMap &settings)
{
    auto result = normalizedNotifications(settings);
    const QString clock = settings.value(QStringLiteral("clockFormat"), QStringLiteral("system")).toString();
    const QString popup = settings.value(QStringLiteral("popupLayout"), QStringLiteral("standard")).toString();
    const QString page = settings.value(QStringLiteral("defaultPage"), QStringLiteral("notifications")).toString();

    result.insert(QStringLiteral("textScalePercent"),
                  qBound(75, settings.value(QStringLiteral("textScalePercent"), 100).toInt(), 150));
    result.insert(QStringLiteral("clockFormat"),
                  isOneOf(clock, {QStringLiteral("system"), QStringLiteral("24h"), QStringLiteral("12h")})
                      ? clock : QStringLiteral("system"));
    result.insert(QStringLiteral("showSeconds"), settings.value(QStringLiteral("showSeconds"), false).toBool());
    result.insert(QStringLiteral("popupLayout"),
                  isOneOf(popup, {QStringLiteral("standard"), QStringLiteral("wide")})
                      ? popup : QStringLiteral("standard"));
    result.insert(QStringLiteral("defaultPage"),
                  isOneOf(page, {QStringLiteral("notifications"), QStringLiteral("calendar")})
                      ? page : QStringLiteral("notifications"));
    result.insert(QStringLiteral("showWeekNumbers"), settings.value(QStringLiteral("showWeekNumbers"), false).toBool());
    result.insert(QStringLiteral("showSecondaryCalendar"), settings.value(QStringLiteral("showSecondaryCalendar"), true).toBool());
    result.insert(QStringLiteral("showDate"), settings.value(QStringLiteral("showDate"), true).toBool());
    result.insert(QStringLiteral("showNotifications"), settings.value(QStringLiteral("showNotifications"), true).toBool());
    result.insert(QStringLiteral("use24HourClock"), settings.value(QStringLiteral("use24HourClock"), true).toBool());
    return result;
}

QVariantMap ShellSettingsBackend::normalizedTopTasks(const QVariantMap &settings)
{
    return {
        {QStringLiteral("taskLimit"), qBound(1, settings.value(QStringLiteral("taskLimit"), 8).toInt(), 12)},
    };
}

QVariantMap ShellSettingsBackend::serializeShelf(const QVariantMap &settings, QString *error)
{
    const auto page = settings.value(QStringLiteral("launcherDefaultPage"), QStringLiteral("home")).toString();
    if (!isOneOf(page, {QStringLiteral("home"), QStringLiteral("apps")})) {
        if (error) *error = tr("Choose Home or All apps as the launcher default page.");
        return {};
    }
    const auto width = settings.value(QStringLiteral("launcherWidth"), QStringLiteral("standard")).toString();
    if (!isOneOf(width, {QStringLiteral("compact"), QStringLiteral("standard"), QStringLiteral("wide")})) {
        if (error) *error = tr("Choose a supported launcher width.");
        return {};
    }
    return normalizedShelf(settings);
}

QVariantMap ShellSettingsBackend::serializeNotifications(const QVariantMap &settings, QString *error)
{
    const int textScale = settings.value(QStringLiteral("textScalePercent"), 100).toInt();
    const int opacity = settings.value(QStringLiteral("surfaceOpacityPercent"), 100).toInt();
    const auto density = settings.value(QStringLiteral("density"), QStringLiteral("comfortable")).toString();
    const auto surface = settings.value(QStringLiteral("surfaceStyle"), QStringLiteral("theme")).toString();
    const auto motion = settings.value(QStringLiteral("motionProfile"), QStringLiteral("pixel")).toString();
    const auto view = settings.value(QStringLiteral("notificationView"), QStringLiteral("cards")).toString();
    const auto preview = settings.value(QStringLiteral("notificationPreview"), QStringLiteral("full")).toString();

    if (textScale < 85 || textScale > 125) {
        if (error) *error = tr("Notification text size must be between 85% and 125%.");
        return {};
    }
    if (opacity < 70 || opacity > 100) {
        if (error) *error = tr("Notification surface opacity must be between 70% and 100%.");
        return {};
    }
    if (!isOneOf(density, {QStringLiteral("compact"), QStringLiteral("comfortable")})
        || !isOneOf(surface, {QStringLiteral("theme"), QStringLiteral("flat"), QStringLiteral("tonal"), QStringLiteral("translucent")})
        || !isOneOf(motion, {QStringLiteral("calm"), QStringLiteral("pixel"), QStringLiteral("playful")})
        || !isOneOf(view, {QStringLiteral("cards"), QStringLiteral("compact")})
        || !isOneOf(preview, {QStringLiteral("full"), QStringLiteral("summary"), QStringLiteral("hidden")})) {
        if (error) *error = tr("One or more notification presentation options are unsupported.");
        return {};
    }
    return normalizedNotifications(settings);
}

QVariantMap ShellSettingsBackend::serializeTimeCenter(const QVariantMap &settings, QString *error)
{
    const int textScale = settings.value(QStringLiteral("textScalePercent"), 100).toInt();
    if (textScale < 75 || textScale > 150) {
        if (error) *error = tr("Time Center text size must be between 75% and 150%.");
        return {};
    }

    auto notificationCandidate = settings;
    notificationCandidate.insert(QStringLiteral("textScalePercent"),
                                 qBound(85, textScale, 125));
    if (serializeNotifications(notificationCandidate, error).isEmpty()) {
        return {};
    }

    const auto clock = settings.value(QStringLiteral("clockFormat"), QStringLiteral("system")).toString();
    const auto popup = settings.value(QStringLiteral("popupLayout"), QStringLiteral("standard")).toString();
    const auto page = settings.value(QStringLiteral("defaultPage"), QStringLiteral("notifications")).toString();
    if (!isOneOf(clock, {QStringLiteral("system"), QStringLiteral("24h"), QStringLiteral("12h")})
        || !isOneOf(popup, {QStringLiteral("standard"), QStringLiteral("wide")})
        || !isOneOf(page, {QStringLiteral("notifications"), QStringLiteral("calendar")})) {
        if (error) *error = tr("One or more Time Center options are unsupported.");
        return {};
    }
    return normalizedTimeCenter(settings);
}

QVariantMap ShellSettingsBackend::serializeTopTasks(const QVariantMap &settings, QString *error)
{
    const int limit = settings.value(QStringLiteral("taskLimit"), 8).toInt();
    if (limit < 1 || limit > 12) {
        if (error) *error = tr("Top tasks must show between 1 and 12 application icons.");
        return {};
    }
    return normalizedTopTasks(settings);
}

QString ShellSettingsBackend::readScript()
{
    return QStringLiteral(R"JS(
function findWidgets(type) {
    var matches = [];
    var panelList = panels();
    for (var panelIndex = 0; panelIndex < panelList.length; ++panelIndex) {
        var widgets = panelList[panelIndex].widgets();
        for (var widgetIndex = 0; widgetIndex < widgets.length; ++widgetIndex) {
            if (widgets[widgetIndex].type === type)
                matches.push(widgets[widgetIndex]);
        }
    }
    return matches;
}

function readSurface(type, defaults) {
    var matches = findWidgets(type);
    var result = {count: matches.length};
    if (matches.length !== 1)
        return result;

    var target = matches[0];
    target.currentConfigGroup = ["Appearance"];
    for (var key in defaults)
        result[key] = target.readConfig(key, defaults[key]);
    return result;
}

print(JSON.stringify({
    shelf: readSurface("org.meo.shelf", {
        showLauncherButton: true,
        filterTasksByVirtualDesktop: false,
        showRunningIndicators: true,
        showTooltips: true,
        launcherDefaultPage: "home",
        launcherWidth: "standard",
        launcherShowFavorites: true,
        launcherShowRecents: true
    }),
    notifications: readSurface("org.meo.notifications", {
        density: "comfortable",
        textScalePercent: 100,
        surfaceStyle: "theme",
        surfaceOpacityPercent: 100,
        motionProfile: "pixel",
        showUnreadBadge: true,
        showJobs: true,
        showNotificationHistory: true,
        notificationView: "cards",
        notificationPreview: "full"
    }),
    timeCenter: readSurface("org.meo.timecenter", {
        textScalePercent: 100,
        density: "comfortable",
        surfaceStyle: "theme",
        surfaceOpacityPercent: 100,
        motionProfile: "pixel",
        showUnreadBadge: true,
        showJobs: true,
        showNotificationHistory: true,
        notificationView: "cards",
        notificationPreview: "full",
        clockFormat: "system",
        showSeconds: false,
        popupLayout: "standard",
        defaultPage: "notifications",
        showWeekNumbers: false,
        showSecondaryCalendar: true,
        showDate: true,
        showNotifications: true,
        use24HourClock: true
    }),
    topTasks: readSurface("org.meo.toptasks", {
        taskLimit: 8
    })
}));
)JS");
}

QString ShellSettingsBackend::writeShelfScript(const QVariantMap &settings)
{
    return writeSurfaceScript(QString::fromLatin1(shelfPlugin), normalizedShelf(settings), {
        QStringLiteral("showLauncherButton"),
        QStringLiteral("filterTasksByVirtualDesktop"),
        QStringLiteral("showRunningIndicators"),
        QStringLiteral("showTooltips"),
        QStringLiteral("launcherDefaultPage"),
        QStringLiteral("launcherWidth"),
        QStringLiteral("launcherShowFavorites"),
        QStringLiteral("launcherShowRecents"),
    });
}

QString ShellSettingsBackend::writeNotificationsScript(const QVariantMap &settings)
{
    return writeSurfaceScript(QString::fromLatin1(notificationsPlugin), normalizedNotifications(settings), {
        QStringLiteral("density"),
        QStringLiteral("textScalePercent"),
        QStringLiteral("surfaceStyle"),
        QStringLiteral("surfaceOpacityPercent"),
        QStringLiteral("motionProfile"),
        QStringLiteral("showUnreadBadge"),
        QStringLiteral("showJobs"),
        QStringLiteral("showNotificationHistory"),
        QStringLiteral("notificationView"),
        QStringLiteral("notificationPreview"),
    });
}

QString ShellSettingsBackend::writeTimeCenterScript(const QVariantMap &settings)
{
    return writeSurfaceScript(QString::fromLatin1(timeCenterPlugin), normalizedTimeCenter(settings), {
        QStringLiteral("textScalePercent"),
        QStringLiteral("density"),
        QStringLiteral("surfaceStyle"),
        QStringLiteral("surfaceOpacityPercent"),
        QStringLiteral("motionProfile"),
        QStringLiteral("showUnreadBadge"),
        QStringLiteral("showJobs"),
        QStringLiteral("showNotificationHistory"),
        QStringLiteral("notificationView"),
        QStringLiteral("notificationPreview"),
        QStringLiteral("clockFormat"),
        QStringLiteral("showSeconds"),
        QStringLiteral("popupLayout"),
        QStringLiteral("defaultPage"),
        QStringLiteral("showWeekNumbers"),
        QStringLiteral("showSecondaryCalendar"),
        QStringLiteral("showDate"),
        QStringLiteral("showNotifications"),
        QStringLiteral("use24HourClock"),
    });
}

QString ShellSettingsBackend::writeTopTasksScript(const QVariantMap &settings)
{
    return writeSurfaceScript(QString::fromLatin1(topTasksPlugin), normalizedTopTasks(settings), {
        QStringLiteral("taskLimit"),
    });
}

void ShellSettingsBackend::saveSurface(const QString &surface, const QVariantMap &settings)
{
    if (busy()) {
        return;
    }

    clearError();
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so Meo shell surfaces cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QString script;
    if (surface == QLatin1String("shelf")) {
        script = writeShelfScript(settings);
    } else if (surface == QLatin1String("notifications")) {
        script = writeNotificationsScript(settings);
    } else if (surface == QLatin1String("timeCenter")) {
        script = writeTimeCenterScript(settings);
    } else if (surface == QLatin1String("topTasks")) {
        script = writeTopTasksScript(settings);
    } else {
        setError(tr("Unknown Meo shell surface."));
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                         QString::fromLatin1(plasmaShellPath),
                         QString::fromLatin1(plasmaShellInterface),
                         QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the shell configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        shell.asyncCall(QStringLiteral("evaluateScript"), script), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, surface, settings](QDBusPendingCallWatcher *) {
                const QDBusPendingReply<QString> reply = *watcher;
                setBusy(false);
                if (reply.isError()) {
                    setError(scriptErrorMessage(reply.error()));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto document = QJsonDocument::fromJson(reply.value().trimmed().toUtf8());
                if (!document.isObject()) {
                    setError(tr("Plasma Shell returned an invalid result while updating a Meo shell surface."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto result = document.object();
                if (!result.value(QStringLiteral("ok")).toBool()) {
                    setError(errorForScriptReason(surface, result.value(QStringLiteral("reason")).toString()));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                setSurface(surface, settings, 1);
                setAvailable(true);
                Q_EMIT changed();
                if (surface == QLatin1String("shelf")) {
                    Q_EMIT shelfSaved();
                } else if (surface == QLatin1String("notifications")) {
                    Q_EMIT notificationsSaved();
                } else if (surface == QLatin1String("timeCenter")) {
                    Q_EMIT timeCenterSaved();
                } else if (surface == QLatin1String("topTasks")) {
                    Q_EMIT topTasksSaved();
                }
                watcher->deleteLater();
            });
}

void ShellSettingsBackend::setSurface(const QString &surface, const QVariantMap &settings, int count)
{
    if (surface == QLatin1String("shelf")) {
        m_shelf = withAvailability(normalizedShelf(settings), count);
    } else if (surface == QLatin1String("notifications")) {
        m_notifications = withAvailability(normalizedNotifications(settings), count);
    } else if (surface == QLatin1String("timeCenter")) {
        m_timeCenter = withAvailability(normalizedTimeCenter(settings), count);
    } else if (surface == QLatin1String("topTasks")) {
        m_topTasks = withAvailability(normalizedTopTasks(settings), count);
    }
}

QString ShellSettingsBackend::errorForScriptReason(const QString &surface, const QString &reason) const
{
    QString name;
    if (surface == QLatin1String("shelf")) name = tr("Shelf");
    else if (surface == QLatin1String("notifications")) name = tr("Notification Center");
    else if (surface == QLatin1String("timeCenter")) name = tr("Time Center");
    else if (surface == QLatin1String("topTasks")) name = tr("Top tasks");
    else name = tr("Meo shell surface");

    if (reason == QLatin1String("missing")) {
        return failMessage(name, tr("the active Plasma layout does not contain this component."));
    }
    if (reason == QLatin1String("multiple")) {
        return failMessage(name, tr("more than one copy is present; restore the standard Meo layout before editing it here."));
    }
    return failMessage(name, tr("Plasma Shell could not update the component."));
}
