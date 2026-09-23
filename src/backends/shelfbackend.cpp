#include "shelfbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
constexpr auto plasmaShellService = "org.kde.plasmashell";
constexpr auto plasmaShellPath = "/PlasmaShell";
constexpr auto plasmaShellInterface = "org.kde.PlasmaShell";
constexpr auto shelfPlugin = "org.meo.shelf";
constexpr auto appearanceGroup = "Appearance";

bool plasmaShellIsAvailable()
{
    const auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected() || !connection.interface())
        return false;
    const auto registered = connection.interface()->isServiceRegistered(QString::fromLatin1(plasmaShellService));
    return registered.isValid() && registered.value();
}

QString scriptErrorMessage(const QDBusError &error)
{
    return error.message().isEmpty()
        ? QStringLiteral("Plasma Shell did not return a Shelf configuration response.")
        : error.message();
}
}

ShelfBackend::ShelfBackend(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

QVariantMap ShelfBackend::settings() const
{
    return m_settings;
}

QString ShelfBackend::summary() const
{
    if (!available())
        return tr("Meo Shelf is unavailable");
    if (busy())
        return tr("Updating Shelf…");

    const auto page = m_settings.value(QStringLiteral("launcherDefaultPage")).toString();
    const auto width = m_settings.value(QStringLiteral("launcherWidth")).toString();
    return tr("Launcher opens to %1 · %2 width")
        .arg(page == QLatin1String("apps") ? tr("All apps") : tr("Home"),
             width == QLatin1String("compact") ? tr("compact")
             : (width == QLatin1String("wide") ? tr("wide") : tr("standard")));
}

QVariantMap ShelfBackend::normalizedSettings(const QVariantMap &settings)
{
    const auto defaultPage = settings.value(QStringLiteral("launcherDefaultPage"),
                                            QStringLiteral("home")).toString();
    const auto width = settings.value(QStringLiteral("launcherWidth"),
                                      QStringLiteral("standard")).toString();

    return {
        {QStringLiteral("showLauncherButton"),
         settings.value(QStringLiteral("showLauncherButton"), true).toBool()},
        {QStringLiteral("filterTasksByVirtualDesktop"),
         settings.value(QStringLiteral("filterTasksByVirtualDesktop"), false).toBool()},
        {QStringLiteral("showRunningIndicators"),
         settings.value(QStringLiteral("showRunningIndicators"), true).toBool()},
        {QStringLiteral("showTooltips"),
         settings.value(QStringLiteral("showTooltips"), true).toBool()},
        {QStringLiteral("launcherDefaultPage"),
         QStringList{QStringLiteral("home"), QStringLiteral("apps")}.contains(defaultPage)
             ? defaultPage : QStringLiteral("home")},
        {QStringLiteral("launcherWidth"),
         QStringList{QStringLiteral("compact"), QStringLiteral("standard"), QStringLiteral("wide")}.contains(width)
             ? width : QStringLiteral("standard")},
        {QStringLiteral("launcherShowFavorites"),
         settings.value(QStringLiteral("launcherShowFavorites"), true).toBool()},
        {QStringLiteral("launcherShowRecents"),
         settings.value(QStringLiteral("launcherShowRecents"), true).toBool()},
    };
}

QVariantMap ShelfBackend::serializeSettings(const QVariantMap &settings, QString *error)
{
    const auto fail = [error](const QString &message) -> QVariantMap {
        if (error)
            *error = message;
        return {};
    };

    const auto page = settings.value(QStringLiteral("launcherDefaultPage"),
                                     QStringLiteral("home")).toString();
    if (!QStringList{QStringLiteral("home"), QStringLiteral("apps")}.contains(page))
        return fail(QObject::tr("Choose a supported launcher start page."));

    const auto width = settings.value(QStringLiteral("launcherWidth"),
                                      QStringLiteral("standard")).toString();
    if (!QStringList{QStringLiteral("compact"), QStringLiteral("standard"), QStringLiteral("wide")}.contains(width))
        return fail(QObject::tr("Choose a supported launcher width."));

    return normalizedSettings(settings);
}

QString ShelfBackend::readScript()
{
    return QStringLiteral(R"JS(
var matches = [];
var panelList = panels();
for (var panelIndex = 0; panelIndex < panelList.length; ++panelIndex) {
    var widgets = panelList[panelIndex].widgets();
    for (var widgetIndex = 0; widgetIndex < widgets.length; ++widgetIndex) {
        var widget = widgets[widgetIndex];
        if (widget.type !== "%1")
            continue;
        widget.currentConfigGroup = ["%2"];
        matches.push({
            id: widget.id,
            showLauncherButton: widget.readConfig("showLauncherButton", true),
            filterTasksByVirtualDesktop: widget.readConfig("filterTasksByVirtualDesktop", false),
            showRunningIndicators: widget.readConfig("showRunningIndicators", true),
            showTooltips: widget.readConfig("showTooltips", true),
            launcherDefaultPage: widget.readConfig("launcherDefaultPage", "home"),
            launcherWidth: widget.readConfig("launcherWidth", "standard"),
            launcherShowFavorites: widget.readConfig("launcherShowFavorites", true),
            launcherShowRecents: widget.readConfig("launcherShowRecents", true)
        });
    }
}
print(JSON.stringify(matches));
)JS").arg(QString::fromLatin1(shelfPlugin), QString::fromLatin1(appearanceGroup));
}

QString ShelfBackend::writeScript(const QVariantMap &settings)
{
    const auto safe = normalizedSettings(settings);
    const auto payload = QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(safe)).toJson(QJsonDocument::Compact));

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
    var shelf = matches[0];
    var desired = %3;
    shelf.currentConfigGroup = ["%2"];
    shelf.writeConfig("showLauncherButton", desired.showLauncherButton);
    shelf.writeConfig("filterTasksByVirtualDesktop", desired.filterTasksByVirtualDesktop);
    shelf.writeConfig("showRunningIndicators", desired.showRunningIndicators);
    shelf.writeConfig("showTooltips", desired.showTooltips);
    shelf.writeConfig("launcherDefaultPage", desired.launcherDefaultPage);
    shelf.writeConfig("launcherWidth", desired.launcherWidth);
    shelf.writeConfig("launcherShowFavorites", desired.launcherShowFavorites);
    shelf.writeConfig("launcherShowRecents", desired.launcherShowRecents);
    shelf.reloadConfig();
    print(JSON.stringify({ok: true}));
}
)JS").arg(QString::fromLatin1(shelfPlugin),
           QString::fromLatin1(appearanceGroup),
           payload);
}

void ShelfBackend::refresh()
{
    if (busy())
        return;

    clearError();
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so the Meo Shelf cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                         QString::fromLatin1(plasmaShellPath),
                         QString::fromLatin1(plasmaShellInterface),
                         QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the Shelf configuration interface."));
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
                if (!document.isArray()) {
                    setAvailable(false);
                    setError(tr("Plasma Shell returned an invalid Shelf configuration response."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto matches = document.array();
                if (matches.isEmpty()) {
                    setAvailable(false);
                    setError(tr("No Meo Shelf was found in the active Plasma layout."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }
                if (matches.size() != 1 || !matches.at(0).isObject()) {
                    setAvailable(false);
                    setError(tr("More than one Meo Shelf was found. Reapply the standard Meo layout before changing it here."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                setSettings(normalizedSettings(matches.at(0).toObject().toVariantMap()));
                setAvailable(true);
                Q_EMIT changed();
                watcher->deleteLater();
            });
}

void ShelfBackend::save(const QVariantMap &settings)
{
    if (busy())
        return;

    clearError();
    QString validationError;
    const auto serialized = serializeSettings(settings, &validationError);
    if (serialized.isEmpty()) {
        setError(validationError);
        return;
    }

    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so the Meo Shelf cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                         QString::fromLatin1(plasmaShellPath),
                         QString::fromLatin1(plasmaShellInterface),
                         QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the Shelf configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        shell.asyncCall(QStringLiteral("evaluateScript"), writeScript(serialized)), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, serialized](QDBusPendingCallWatcher *) {
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
                    setError(tr("Plasma Shell returned an invalid result while updating the Meo Shelf."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto result = document.object();
                if (!result.value(QStringLiteral("ok")).toBool()) {
                    setError(errorForScriptReason(result.value(QStringLiteral("reason")).toString()));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                setSettings(serialized);
                setAvailable(true);
                Q_EMIT changed();
                Q_EMIT saved();
                watcher->deleteLater();
            });
}

void ShelfBackend::reset()
{
    save(normalizedSettings({}));
}

void ShelfBackend::setSettings(const QVariantMap &settings)
{
    if (m_settings == settings)
        return;
    m_settings = settings;
}

QString ShelfBackend::errorForScriptReason(const QString &reason) const
{
    if (reason == QLatin1String("missing"))
        return tr("No Meo Shelf was found in the active Plasma layout.");
    if (reason == QLatin1String("multiple"))
        return tr("More than one Meo Shelf was found. Reapply the standard Meo layout before changing it here.");
    return tr("Plasma Shell rejected the Shelf configuration change.");
}
