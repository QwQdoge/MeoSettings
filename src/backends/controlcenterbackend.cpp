#include "controlcenterbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtGlobal>

namespace
{
constexpr auto plasmaShellService = "org.kde.plasmashell";
constexpr auto plasmaShellPath = "/PlasmaShell";
constexpr auto plasmaShellInterface = "org.kde.PlasmaShell";
constexpr auto topbarPlugin = "org.meo.topbar";
constexpr auto appearanceGroup = "Appearance";
constexpr auto defaultDensity = "comfortable";

const QStringList &canonicalTileIds()
{
    static const QStringList ids{
        QStringLiteral("wifi"),
        QStringLiteral("bluetooth"),
        QStringLiteral("focus"),
        QStringLiteral("nightLight"),
        QStringLiteral("keepAwake"),
        QStringLiteral("powerMode"),
        QStringLiteral("microphone"),
        QStringLiteral("audioDevices"),
        QStringLiteral("display"),
        QStringLiteral("screenshot"),
    };
    return ids;
}

bool isKnownTile(const QString &id)
{
    return canonicalTileIds().contains(id);
}

bool isKnownDensity(const QString &density)
{
    return density == QLatin1String("compact")
        || density == QLatin1String("comfortable")
        || density == QLatin1String("spacious");
}

QString normalizedDensity(const QString &density)
{
    return isKnownDensity(density) ? density : QString::fromLatin1(defaultDensity);
}

QStringList orderedKnownIds(const QString &serialized, const bool appendMissing)
{
    QStringList result;
    QSet<QString> seen;
    for (const auto &id : serialized.split(',', Qt::SkipEmptyParts)) {
        if (isKnownTile(id) && !seen.contains(id)) {
            result.push_back(id);
            seen.insert(id);
        }
    }
    if (appendMissing) {
        for (const auto &id : canonicalTileIds()) {
            if (!seen.contains(id)) {
                result.push_back(id);
            }
        }
    }
    return result;
}

QHash<QString, int> parsedSpans(const QString &serialized)
{
    QHash<QString, int> spans;
    for (const auto &entry : serialized.split(',', Qt::SkipEmptyParts)) {
        const auto pair = entry.split(':');
        if (pair.size() != 2 || !isKnownTile(pair.at(0))) {
            continue;
        }
        const int span = pair.at(1).toInt();
        spans.insert(pair.at(0), span == 1 ? 1 : 2);
    }
    return spans;
}

QString quoteScriptString(const QString &value)
{
    QJsonArray values;
    values.append(value);
    const auto json = QJsonDocument(values).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

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
    return error.message().isEmpty() ? QStringLiteral("Plasma Shell did not return a control-center response.")
                                     : error.message();
}
}

ControlCenterBackend::ControlCenterBackend(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

QVariantList ControlCenterBackend::tiles() const
{
    return m_tiles;
}

QString ControlCenterBackend::density() const
{
    return m_density;
}

QVariantMap ControlCenterBackend::topBar() const
{
    return m_topBar;
}

QString ControlCenterBackend::summary() const
{
    if (!available()) {
        return tr("Meo Control Center is unavailable");
    }
    if (busy()) {
        return tr("Updating Control Center…");
    }

    int visibleCount = 0;
    for (const auto &tile : m_tiles) {
        if (tile.toMap().value(QStringLiteral("visible")).toBool()) {
            ++visibleCount;
        }
    }
    return tr("%n visible quick setting(s)", "", visibleCount);
}

void ControlCenterBackend::refresh()
{
    if (busy()) {
        return;
    }
    clearError();
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so the Meo Control Center cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                          QString::fromLatin1(plasmaShellPath),
                          QString::fromLatin1(plasmaShellInterface),
                          QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the Control Center configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(shell.asyncCall(QStringLiteral("evaluateScript"), readLayoutScript()), this);
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
                    setError(tr("Plasma Shell returned an invalid Control Center configuration response."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }
                const auto matches = document.array();
                if (matches.isEmpty()) {
                    setAvailable(false);
                    setError(tr("No Meo Control Center was found in the active Plasma layout."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }
                if (matches.size() != 1 || !matches.at(0).isObject()) {
                    setAvailable(false);
                    setError(tr("More than one Meo Control Center was found. Reapply the standard Meo layout before changing it here."));
                    Q_EMIT changed();
                    watcher->deleteLater();
                    return;
                }

                const auto configuration = matches.at(0).toObject();
                setLayout(normalizedLayout(configuration.value(QStringLiteral("order")).toString(),
                                           configuration.value(QStringLiteral("sizes")).toString(),
                                           configuration.value(QStringLiteral("visibility")).toString(),
                                           configuration.value(QStringLiteral("density")).toString()));
                setTopBar(normalizedTopBar(configuration.value(QStringLiteral("topBar")).toObject().toVariantMap()));
                setAvailable(true);
                Q_EMIT changed();
                watcher->deleteLater();
            });
}

void ControlCenterBackend::saveLayout(const QVariantList &tiles, const QString &density)
{
    if (busy()) {
        return;
    }
    clearError();
    QString validationError;
    const auto layout = serializeLayout(tiles, density, &validationError);
    if (layout.isEmpty()) {
        setError(validationError);
        return;
    }
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so the Meo Control Center cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                          QString::fromLatin1(plasmaShellPath),
                          QString::fromLatin1(plasmaShellInterface),
                          QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the Control Center configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        shell.asyncCall(QStringLiteral("evaluateScript"),
                        writeLayoutScript(layout.value(QStringLiteral("order")).toString(),
                                          layout.value(QStringLiteral("sizes")).toString(),
                                          layout.value(QStringLiteral("visibility")).toString(),
                                          layout.value(QStringLiteral("density")).toString())),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, layout](QDBusPendingCallWatcher *) {
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
                    setError(tr("Plasma Shell returned an invalid result while updating the Meo Control Center."));
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

                setLayout(layout);
                setAvailable(true);
                Q_EMIT changed();
                Q_EMIT layoutSaved();
                watcher->deleteLater();
            });
}

void ControlCenterBackend::resetLayout()
{
    const auto layout = normalizedLayout({}, {}, {}, QString::fromLatin1(defaultDensity));
    saveLayout(layout.value(QStringLiteral("tiles")).toList(),
               layout.value(QStringLiteral("density")).toString());
}

void ControlCenterBackend::saveTopBar(const QVariantMap &settings)
{
    if (busy()) {
        return;
    }

    clearError();
    QString validationError;
    const auto serialized = serializeTopBar(settings, &validationError);
    if (serialized.isEmpty()) {
        setError(validationError);
        return;
    }
    if (!plasmaShellIsAvailable()) {
        setAvailable(false);
        setError(tr("Plasma Shell is unavailable, so the Meo top bar cannot be configured."));
        Q_EMIT changed();
        return;
    }

    QDBusInterface shell(QString::fromLatin1(plasmaShellService),
                          QString::fromLatin1(plasmaShellPath),
                          QString::fromLatin1(plasmaShellInterface),
                          QDBusConnection::sessionBus());
    if (!shell.isValid()) {
        setAvailable(false);
        setError(tr("Plasma Shell does not expose the top-bar configuration interface."));
        Q_EMIT changed();
        return;
    }

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        shell.asyncCall(QStringLiteral("evaluateScript"), writeTopBarScript(serialized)), this);
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
                    setError(tr("Plasma Shell returned an invalid result while updating the Meo top bar."));
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

                setTopBar(serialized);
                setAvailable(true);
                Q_EMIT changed();
                Q_EMIT topBarSaved();
                watcher->deleteLater();
            });
}

void ControlCenterBackend::resetTopBar()
{
    saveTopBar(normalizedTopBar({}));
}

QStringList ControlCenterBackend::defaultTileIds()
{
    return canonicalTileIds();
}

QVariantMap ControlCenterBackend::normalizedLayout(const QString &order,
                                                    const QString &sizes,
                                                    const QString &visibility,
                                                    const QString &density)
{
    const auto orderedIds = orderedKnownIds(order, true);
    const auto spans = parsedSpans(sizes);
    auto visibleIds = orderedKnownIds(visibility, false);
    if (visibleIds.isEmpty()) {
        visibleIds = canonicalTileIds();
    }
    QSet<QString> visibleSet;
    for (const auto &id : visibleIds) {
        visibleSet.insert(id);
    }

    QVariantList tileList;
    QStringList serializedSizes;
    QStringList serializedVisibility;
    for (const auto &id : orderedIds) {
        const int span = spans.value(id, 2);
        const bool visible = visibleSet.contains(id);
        tileList.push_back(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("span"), span},
            {QStringLiteral("visible"), visible},
        });
        serializedSizes.push_back(id + QLatin1Char(':') + QString::number(span));
        if (visible) {
            serializedVisibility.push_back(id);
        }
    }

    const auto resolvedDensity = normalizedDensity(density);
    return {
        {QStringLiteral("tiles"), tileList},
        {QStringLiteral("order"), orderedIds.join(QLatin1Char(','))},
        {QStringLiteral("sizes"), serializedSizes.join(QLatin1Char(','))},
        {QStringLiteral("visibility"), serializedVisibility.join(QLatin1Char(','))},
        {QStringLiteral("density"), resolvedDensity},
    };
}

QVariantMap ControlCenterBackend::serializeLayout(const QVariantList &tiles,
                                                   const QString &density,
                                                   QString *error)
{
    const auto setError = [error](const QString &message) {
        if (error) {
            *error = message;
        }
    };
    if (!isKnownDensity(density)) {
        setError(QObject::tr("Choose a supported tile density."));
        return {};
    }
    if (tiles.size() != canonicalTileIds().size()) {
        setError(QObject::tr("The Control Center layout must include every supported tile."));
        return {};
    }

    QSet<QString> seen;
    QStringList order;
    QStringList sizes;
    QStringList visibility;
    QVariantList normalizedTiles;
    for (const auto &entry : tiles) {
        const auto tile = entry.toMap();
        const auto id = tile.value(QStringLiteral("id")).toString();
        if (!isKnownTile(id) || seen.contains(id)) {
            setError(QObject::tr("The Control Center layout contains an unknown or duplicate tile."));
            return {};
        }
        const int span = tile.value(QStringLiteral("span"), 2).toInt();
        if (span != 1 && span != 2) {
            setError(QObject::tr("Each Control Center tile must use the compact or wide presentation."));
            return {};
        }
        const bool visible = tile.value(QStringLiteral("visible"), true).toBool();
        seen.insert(id);
        order.push_back(id);
        sizes.push_back(id + QLatin1Char(':') + QString::number(span));
        if (visible) {
            visibility.push_back(id);
        }
        normalizedTiles.push_back(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("span"), span},
            {QStringLiteral("visible"), visible},
        });
    }
    if (seen.size() != canonicalTileIds().size()) {
        setError(QObject::tr("The Control Center layout is missing a supported tile."));
        return {};
    }
    if (visibility.isEmpty()) {
        setError(QObject::tr("Keep at least one Control Center tile visible."));
        return {};
    }

    return {
        {QStringLiteral("tiles"), normalizedTiles},
        {QStringLiteral("order"), order.join(QLatin1Char(','))},
        {QStringLiteral("sizes"), sizes.join(QLatin1Char(','))},
        {QStringLiteral("visibility"), visibility.join(QLatin1Char(','))},
        {QStringLiteral("density"), density},
    };
}

QVariantMap ControlCenterBackend::normalizedTopBar(const QVariantMap &settings)
{
    const QString densityValue = settings.value(QStringLiteral("density"), QStringLiteral("comfortable")).toString();
    const QString surfaceStyleValue = settings.value(QStringLiteral("surfaceStyle"), QStringLiteral("theme")).toString();
    const QString motionProfileValue = settings.value(QStringLiteral("motionProfile"), QStringLiteral("pixel")).toString();

    const QStringList densities{QStringLiteral("compact"), QStringLiteral("comfortable")};
    const QStringList surfaceStyles{QStringLiteral("theme"), QStringLiteral("flat"),
                                    QStringLiteral("tonal"), QStringLiteral("translucent")};
    const QStringList motionProfiles{QStringLiteral("calm"), QStringLiteral("pixel"), QStringLiteral("playful")};

    return {
        {QStringLiteral("textScalePercent"),
         qBound(75, settings.value(QStringLiteral("textScalePercent"), 100).toInt(), 150)},
        {QStringLiteral("density"), densities.contains(densityValue) ? densityValue : QStringLiteral("comfortable")},
        {QStringLiteral("surfaceStyle"), surfaceStyles.contains(surfaceStyleValue) ? surfaceStyleValue : QStringLiteral("theme")},
        {QStringLiteral("surfaceOpacityPercent"),
         qBound(70, settings.value(QStringLiteral("surfaceOpacityPercent"), 100).toInt(), 100)},
        {QStringLiteral("motionProfile"), motionProfiles.contains(motionProfileValue) ? motionProfileValue : QStringLiteral("pixel")},
        {QStringLiteral("showUnreadBadge"), settings.value(QStringLiteral("showUnreadBadge"), true).toBool()},
        {QStringLiteral("showJobs"), settings.value(QStringLiteral("showJobs"), true).toBool()},
        {QStringLiteral("showNetwork"), settings.value(QStringLiteral("showNetwork"), true).toBool()},
        {QStringLiteral("showBluetooth"), settings.value(QStringLiteral("showBluetooth"), true).toBool()},
        {QStringLiteral("showVolume"), settings.value(QStringLiteral("showVolume"), true).toBool()},
        {QStringLiteral("batteryDisplay"),
         qBound(0, settings.value(QStringLiteral("batteryDisplay"), 2).toInt(), 3)},
        {QStringLiteral("showDate"), settings.value(QStringLiteral("showDate"), true).toBool()},
        {QStringLiteral("showNotifications"), settings.value(QStringLiteral("showNotifications"), true).toBool()},
        {QStringLiteral("use24HourClock"), settings.value(QStringLiteral("use24HourClock"), true).toBool()},
    };
}

QVariantMap ControlCenterBackend::serializeTopBar(const QVariantMap &settings, QString *error)
{
    const auto fail = [error](const QString &message) -> QVariantMap {
        if (error) {
            *error = message;
        }
        return {};
    };

    const int textScale = settings.value(QStringLiteral("textScalePercent"), 100).toInt();
    if (textScale < 75 || textScale > 150) {
        return fail(QObject::tr("Top-bar text size must be between 75% and 150%."));
    }

    const int opacity = settings.value(QStringLiteral("surfaceOpacityPercent"), 100).toInt();
    if (opacity < 70 || opacity > 100) {
        return fail(QObject::tr("Top-bar surface opacity must be between 70% and 100%."));
    }

    const int batteryDisplay = settings.value(QStringLiteral("batteryDisplay"), 2).toInt();
    if (batteryDisplay < 0 || batteryDisplay > 3) {
        return fail(QObject::tr("Choose a supported battery display mode."));
    }

    const QString densityValue = settings.value(QStringLiteral("density"), QStringLiteral("comfortable")).toString();
    if (!QStringList{QStringLiteral("compact"), QStringLiteral("comfortable")}.contains(densityValue)) {
        return fail(QObject::tr("Choose a supported top-bar density."));
    }

    const QString surfaceStyleValue = settings.value(QStringLiteral("surfaceStyle"), QStringLiteral("theme")).toString();
    if (!QStringList{QStringLiteral("theme"), QStringLiteral("flat"), QStringLiteral("tonal"),
                     QStringLiteral("translucent")}.contains(surfaceStyleValue)) {
        return fail(QObject::tr("Choose a supported top-bar surface style."));
    }

    const QString motionProfileValue = settings.value(QStringLiteral("motionProfile"), QStringLiteral("pixel")).toString();
    if (!QStringList{QStringLiteral("calm"), QStringLiteral("pixel"), QStringLiteral("playful")}.contains(motionProfileValue)) {
        return fail(QObject::tr("Choose a supported top-bar motion profile."));
    }

    return normalizedTopBar(settings);
}

QString ControlCenterBackend::readLayoutScript()
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
            order: widget.readConfig("quickTileOrder", %3),
            sizes: widget.readConfig("quickTileSizes", %4),
            visibility: widget.readConfig("quickTileVisibility", %5),
            density: widget.readConfig("quickTileDensity", %6),
            topBar: {
                textScalePercent: widget.readConfig("textScalePercent", 100),
                density: widget.readConfig("density", "comfortable"),
                surfaceStyle: widget.readConfig("surfaceStyle", "theme"),
                surfaceOpacityPercent: widget.readConfig("surfaceOpacityPercent", 100),
                motionProfile: widget.readConfig("motionProfile", "pixel"),
                showUnreadBadge: widget.readConfig("showUnreadBadge", true),
                showJobs: widget.readConfig("showJobs", true),
                showNetwork: widget.readConfig("showNetwork", true),
                showBluetooth: widget.readConfig("showBluetooth", true),
                showVolume: widget.readConfig("showVolume", true),
                batteryDisplay: widget.readConfig("batteryDisplay", 2),
                showDate: widget.readConfig("showDate", true),
                showNotifications: widget.readConfig("showNotifications", true),
                use24HourClock: widget.readConfig("use24HourClock", true)
            }
        });
    }
}
print(JSON.stringify(matches));
)JS")
        .arg(QString::fromLatin1(topbarPlugin),
             QString::fromLatin1(appearanceGroup),
             quoteScriptString(canonicalTileIds().join(QLatin1Char(','))),
             quoteScriptString(QStringLiteral("wifi:2,bluetooth:2,focus:2,nightLight:2,keepAwake:2,powerMode:2,microphone:2,audioDevices:2,display:2,screenshot:2")),
             quoteScriptString(canonicalTileIds().join(QLatin1Char(','))),
             quoteScriptString(QString::fromLatin1(defaultDensity)));
}

QString ControlCenterBackend::writeLayoutScript(const QString &order,
                                                 const QString &sizes,
                                                 const QString &visibility,
                                                 const QString &density)
{
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
    var topbar = matches[0];
    topbar.currentConfigGroup = ["%2"];
    topbar.writeConfig("quickTileOrder", %3);
    topbar.writeConfig("quickTileSizes", %4);
    topbar.writeConfig("quickTileVisibility", %5);
    topbar.writeConfig("quickTileDensity", %6);
    topbar.reloadConfig();
    print(JSON.stringify({ok: true}));
}
)JS")
        .arg(QString::fromLatin1(topbarPlugin),
             QString::fromLatin1(appearanceGroup),
             quoteScriptString(order),
             quoteScriptString(sizes),
             quoteScriptString(visibility),
             quoteScriptString(density));
}

QString ControlCenterBackend::writeTopBarScript(const QVariantMap &settings)
{
    const auto safeSettings = normalizedTopBar(settings);
    const auto payload = QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(safeSettings)).toJson(QJsonDocument::Compact));

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
    var topbar = matches[0];
    var desired = %3;
    topbar.currentConfigGroup = ["%2"];
    topbar.writeConfig("textScalePercent", desired.textScalePercent);
    topbar.writeConfig("density", desired.density);
    topbar.writeConfig("surfaceStyle", desired.surfaceStyle);
    topbar.writeConfig("surfaceOpacityPercent", desired.surfaceOpacityPercent);
    topbar.writeConfig("motionProfile", desired.motionProfile);
    topbar.writeConfig("showUnreadBadge", desired.showUnreadBadge);
    topbar.writeConfig("showJobs", desired.showJobs);
    topbar.writeConfig("showNetwork", desired.showNetwork);
    topbar.writeConfig("showBluetooth", desired.showBluetooth);
    topbar.writeConfig("showVolume", desired.showVolume);
    topbar.writeConfig("batteryDisplay", desired.batteryDisplay);
    topbar.writeConfig("showDate", desired.showDate);
    topbar.writeConfig("showNotifications", desired.showNotifications);
    topbar.writeConfig("use24HourClock", desired.use24HourClock);
    topbar.reloadConfig();
    print(JSON.stringify({ok: true}));
}
)JS")
        .arg(QString::fromLatin1(topbarPlugin),
             QString::fromLatin1(appearanceGroup),
             payload);
}

void ControlCenterBackend::setLayout(const QVariantMap &layout)
{
    const auto tiles = layout.value(QStringLiteral("tiles")).toList();
    const auto density = layout.value(QStringLiteral("density"), QString::fromLatin1(defaultDensity)).toString();
    if (m_tiles != tiles || m_density != density) {
        m_tiles = tiles;
        m_density = density;
    }
}

void ControlCenterBackend::setTopBar(const QVariantMap &settings)
{
    m_topBar = normalizedTopBar(settings);
}

QString ControlCenterBackend::errorForScriptReason(const QString &reason) const
{
    if (reason == QLatin1String("missing")) {
        return tr("No Meo Control Center was found in the active Plasma layout.");
    }
    if (reason == QLatin1String("multiple")) {
        return tr("More than one Meo Control Center was found. Reapply the standard Meo layout before changing it here.");
    }
    return tr("Plasma Shell could not update the Meo Control Center.");
}
