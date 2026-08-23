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
            density: widget.readConfig("quickTileDensity", %6)
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

void ControlCenterBackend::setLayout(const QVariantMap &layout)
{
    const auto tiles = layout.value(QStringLiteral("tiles")).toList();
    const auto density = layout.value(QStringLiteral("density"), QString::fromLatin1(defaultDensity)).toString();
    if (m_tiles != tiles || m_density != density) {
        m_tiles = tiles;
        m_density = density;
    }
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
