#include "kdeconfigbackend.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {
using Map = QVariantMap;
const char *Ctx = "KdeConfigBackend";

QString tr(const char *text) { return QCoreApplication::translate(Ctx, text); }

Map base(const char *id, const char *label, const char *description, const char *type,
         const char *file, const char *group, const char *key, const char *hook)
{
    return {{"id", QString::fromLatin1(id)}, {"label", tr(label)}, {"description", description ? tr(description) : QString()},
            {"type", QString::fromLatin1(type)}, {"file", QString::fromLatin1(file)}, {"group", QString::fromLatin1(group)},
            {"key", QString::fromLatin1(key)}, {"hook", QString::fromLatin1(hook)}};
}

Map toggle(const char *id, const char *label, const char *description, const char *file, const char *group,
           const char *key, bool def, const char *hook = "", bool invert = false)
{
    Map row = base(id, label, description, "toggle", file, group, key, hook);
    row.insert("default", def);
    row.insert("invert", invert);
    return row;
}

Map number(const char *id, const char *label, const char *description, const char *file, const char *group,
           const char *key, int def, int min, int max, int step, const char *suffix, const char *hook = "")
{
    Map row = base(id, label, description, "number", file, group, key, hook);
    row.insert("default", def);
    row.insert("min", min);
    row.insert("max", max);
    row.insert("step", step);
    row.insert("suffix", suffix ? tr(suffix) : QString());
    return row;
}

QVariantList options(std::initializer_list<std::pair<const char *, const char *>> list)
{
    QVariantList out;
    for (const auto &item : list)
        out.append(Map{{"value", QString::fromLatin1(item.first)}, {"label", tr(item.second)}});
    return out;
}

Map choice(const char *id, const char *label, const char *description, const char *file, const char *group,
           const char *key, const char *def, const QVariantList &choices, const char *hook = "", bool numeric = false)
{
    Map row = base(id, label, description, "choice", file, group, key, hook);
    row.insert("default", QString::fromLatin1(def));
    row.insert("choices", choices);
    row.insert("numeric", numeric);
    return row;
}

// Built-in KWin effect. The enabled flag lives in kwinrc [Plugins]; the hook
// loads or unloads it in the running compositor so the change is immediate.
Map effect(const char *id, const char *label, const char *description, const char *name, bool def)
{
    Map row = toggle(id, label, description, "kwinrc", "Plugins", "", def);
    row.insert("key", QString::fromLatin1(name) + QStringLiteral("Enabled"));
    row.insert("hook", QStringLiteral("effect:") + QString::fromLatin1(name));
    return row;
}

Map section(const char *title, const QVariantList &rows)
{
    return {{"title", title ? tr(title) : QString()}, {"rows", rows}};
}

QVariantList animationChoices()
{
    return options({{"0", QT_TRANSLATE_NOOP("KdeConfigBackend", "Instant")},
                    {"0.5", QT_TRANSLATE_NOOP("KdeConfigBackend", "Fast")},
                    {"1", QT_TRANSLATE_NOOP("KdeConfigBackend", "Default")},
                    {"2", QT_TRANSLATE_NOOP("KdeConfigBackend", "Relaxed")}});
}

QVariantList edgeActions()
{
    return options({{"None", QT_TRANSLATE_NOOP("KdeConfigBackend", "No action")},
                    {"ShowDesktop", QT_TRANSLATE_NOOP("KdeConfigBackend", "Show desktop")},
                    {"LockScreen", QT_TRANSLATE_NOOP("KdeConfigBackend", "Lock screen")},
                    {"KRunner", QT_TRANSLATE_NOOP("KdeConfigBackend", "Open search")},
                    {"ApplicationLauncher", QT_TRANSLATE_NOOP("KdeConfigBackend", "Open application launcher")}});
}

Map edge(const char *id, const char *label, const char *key)
{
    return choice(id, label, nullptr, "kwinrc", "ElectricBorders", key, "None", edgeActions(), "kwin");
}

QVariantList build(const QString &page)
{
    if (page == QLatin1String("window-behavior")) {
        return {
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Focus"), {
                choice("focusPolicy", QT_TRANSLATE_NOOP("KdeConfigBackend", "Window focus"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "How a window becomes the active one"),
                       "kwinrc", "Windows", "FocusPolicy", "ClickToFocus",
                       options({{"ClickToFocus", QT_TRANSLATE_NOOP("KdeConfigBackend", "Click to focus")},
                                {"FocusFollowsMouse", QT_TRANSLATE_NOOP("KdeConfigBackend", "Focus follows mouse")},
                                {"FocusUnderMouse", QT_TRANSLATE_NOOP("KdeConfigBackend", "Focus under mouse")},
                                {"FocusStrictlyUnderMouse", QT_TRANSLATE_NOOP("KdeConfigBackend", "Focus strictly under mouse")}}), "kwin"),
                toggle("clickRaise", QT_TRANSLATE_NOOP("KdeConfigBackend", "Raise window when clicked"), nullptr,
                       "kwinrc", "Windows", "ClickRaise", true, "kwin"),
                toggle("autoRaise", QT_TRANSLATE_NOOP("KdeConfigBackend", "Raise window when hovered"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Only applies when focus follows the mouse"),
                       "kwinrc", "Windows", "AutoRaise", false, "kwin"),
                number("autoRaiseInterval", QT_TRANSLATE_NOOP("KdeConfigBackend", "Delay before raising"), nullptr,
                       "kwinrc", "Windows", "AutoRaiseInterval", 750, 0, 3000, 50, QT_TRANSLATE_NOOP("KdeConfigBackend", "ms"), "kwin"),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Placement and title bar"), {
                choice("placement", QT_TRANSLATE_NOOP("KdeConfigBackend", "New window placement"), nullptr,
                       "kwinrc", "Windows", "Placement", "Centered",
                       options({{"Centered", QT_TRANSLATE_NOOP("KdeConfigBackend", "Centered")},
                                {"Smart", QT_TRANSLATE_NOOP("KdeConfigBackend", "Smart (least overlap)")},
                                {"Maximizing", QT_TRANSLATE_NOOP("KdeConfigBackend", "Maximized")},
                                {"Random", QT_TRANSLATE_NOOP("KdeConfigBackend", "Random")},
                                {"UnderMouse", QT_TRANSLATE_NOOP("KdeConfigBackend", "Under the pointer")}}), "kwin"),
                choice("titlebarDoubleClick", QT_TRANSLATE_NOOP("KdeConfigBackend", "Double-click title bar"), nullptr,
                       "kwinrc", "Windows", "TitlebarDoubleClickCommand", "Maximize",
                       options({{"Maximize", QT_TRANSLATE_NOOP("KdeConfigBackend", "Maximize")},
                                {"Minimize", QT_TRANSLATE_NOOP("KdeConfigBackend", "Minimize")},
                                {"Shade", QT_TRANSLATE_NOOP("KdeConfigBackend", "Roll up")},
                                {"Lower", QT_TRANSLATE_NOOP("KdeConfigBackend", "Send to back")},
                                {"OnAllDesktops", QT_TRANSLATE_NOOP("KdeConfigBackend", "Show on all desktops")},
                                {"Close", QT_TRANSLATE_NOOP("KdeConfigBackend", "Close")}}), "kwin"),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Snapping"), {
                number("borderSnap", QT_TRANSLATE_NOOP("KdeConfigBackend", "Snap to screen edges"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Distance at which windows snap to screen borders; 0 turns it off"),
                       "kwinrc", "Windows", "BorderSnapZone", 10, 0, 100, 1, QT_TRANSLATE_NOOP("KdeConfigBackend", "px"), "kwin"),
                number("windowSnap", QT_TRANSLATE_NOOP("KdeConfigBackend", "Snap to other windows"), nullptr,
                       "kwinrc", "Windows", "WindowSnapZone", 10, 0, 100, 1, QT_TRANSLATE_NOOP("KdeConfigBackend", "px"), "kwin"),
            }),
        };
    }
    if (page == QLatin1String("desktop-effects")) {
        return {
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Animation speed"), {
                choice("animationSpeed", QT_TRANSLATE_NOOP("KdeConfigBackend", "Animation speed"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Instant is the same as reducing motion"),
                       "kdeglobals", "KDE", "AnimationDurationFactor", "1", animationChoices(), "globals", true),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Visual effects"), {
                effect("blur", QT_TRANSLATE_NOOP("KdeConfigBackend", "Blur"), QT_TRANSLATE_NOOP("KdeConfigBackend", "Blur behind translucent surfaces"), "blur", true),
                effect("contrast", QT_TRANSLATE_NOOP("KdeConfigBackend", "Background contrast"), nullptr, "contrast", true),
                effect("fade", QT_TRANSLATE_NOOP("KdeConfigBackend", "Fade windows"), QT_TRANSLATE_NOOP("KdeConfigBackend", "Fade windows as they open and close"), "fade", true),
                effect("slidingpopups", QT_TRANSLATE_NOOP("KdeConfigBackend", "Sliding popups"), nullptr, "slidingpopups", true),
                effect("squash", QT_TRANSLATE_NOOP("KdeConfigBackend", "Minimize animation"), nullptr, "squash", true),
                effect("wobblywindows", QT_TRANSLATE_NOOP("KdeConfigBackend", "Wobbly windows"), QT_TRANSLATE_NOOP("KdeConfigBackend", "Windows wobble while being moved"), "wobblywindows", false),
                effect("magiclamp", QT_TRANSLATE_NOOP("KdeConfigBackend", "Magic lamp"), nullptr, "magiclamp", false),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Overview"), {
                effect("overview", QT_TRANSLATE_NOOP("KdeConfigBackend", "Overview"), nullptr, "overview", true),
                effect("desktopgrid", QT_TRANSLATE_NOOP("KdeConfigBackend", "Desktop grid"), nullptr, "desktopgrid", true),
                effect("dimscreen", QT_TRANSLATE_NOOP("KdeConfigBackend", "Dim screen for permission prompts"), nullptr, "dimscreen", true),
            }),
        };
    }
    if (page == QLatin1String("screen-edges")) {
        return {
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Corners"), {
                edge("topLeft", QT_TRANSLATE_NOOP("KdeConfigBackend", "Top left"), "TopLeft"),
                edge("topRight", QT_TRANSLATE_NOOP("KdeConfigBackend", "Top right"), "TopRight"),
                edge("bottomLeft", QT_TRANSLATE_NOOP("KdeConfigBackend", "Bottom left"), "BottomLeft"),
                edge("bottomRight", QT_TRANSLATE_NOOP("KdeConfigBackend", "Bottom right"), "BottomRight"),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Edges"), {
                edge("top", QT_TRANSLATE_NOOP("KdeConfigBackend", "Top edge"), "Top"),
                edge("right", QT_TRANSLATE_NOOP("KdeConfigBackend", "Right edge"), "Right"),
                edge("bottom", QT_TRANSLATE_NOOP("KdeConfigBackend", "Bottom edge"), "Bottom"),
                edge("left", QT_TRANSLATE_NOOP("KdeConfigBackend", "Left edge"), "Left"),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Timing"), {
                number("delay", QT_TRANSLATE_NOOP("KdeConfigBackend", "Activation delay"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "How long the pointer must rest on an edge"),
                       "kwinrc", "Windows", "ElectricBorderDelay", 75, 0, 1000, 25, QT_TRANSLATE_NOOP("KdeConfigBackend", "ms"), "kwin"),
                number("cooldown", QT_TRANSLATE_NOOP("KdeConfigBackend", "Re-activation delay"), nullptr,
                       "kwinrc", "Windows", "ElectricBorderCooldown", 350, 0, 2000, 25, QT_TRANSLATE_NOOP("KdeConfigBackend", "ms"), "kwin"),
            }),
        };
    }
    if (page == QLatin1String("virtual-desktops")) {
        return {
            section(nullptr, {
                number("count", QT_TRANSLATE_NOOP("KdeConfigBackend", "Number of desktops"), nullptr,
                       "kwinrc", "Desktops", "Number", 1, 1, 20, 1, nullptr, "vdesktop:count"),
                number("rows", QT_TRANSLATE_NOOP("KdeConfigBackend", "Rows"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Desktops are arranged in this many rows"),
                       "kwinrc", "Desktops", "Rows", 1, 1, 20, 1, nullptr, "vdesktop:rows"),
                toggle("wrap", QT_TRANSLATE_NOOP("KdeConfigBackend", "Wrap around when switching"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Switching past the last desktop returns to the first"),
                       "kwinrc", "Windows", "RollOverDesktops", true, "kwin"),
                effect("slide", QT_TRANSLATE_NOOP("KdeConfigBackend", "Slide animation"), QT_TRANSLATE_NOOP("KdeConfigBackend", "Animate switching between desktops"), "slide", true),
            }),
        };
    }
    if (page == QLatin1String("search-indexing")) {
        return {
            section(nullptr, {
                toggle("enabled", QT_TRANSLATE_NOOP("KdeConfigBackend", "Index files"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Lets search find files by name and content"),
                       "baloofilerc", "Basic Settings", "Indexing-Enabled", true),
                toggle("basic", QT_TRANSLATE_NOOP("KdeConfigBackend", "Index file content"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Turn off to index only file names, which uses less disk and CPU"),
                       "baloofilerc", "General", "only basic indexing", false, "", true),
                toggle("hidden", QT_TRANSLATE_NOOP("KdeConfigBackend", "Include hidden folders"), nullptr,
                       "baloofilerc", "General", "index hidden folders", false),
            }),
        };
    }
    if (page == QLatin1String("startup-session")) {
        return {
            section(nullptr, {
                choice("loginMode", QT_TRANSLATE_NOOP("KdeConfigBackend", "When signing in"), nullptr,
                       "ksmserverrc", "General", "loginMode", "restorePreviousLogout",
                       options({{"restorePreviousLogout", QT_TRANSLATE_NOOP("KdeConfigBackend", "Restore the previous session")},
                                {"restoreSavedSession", QT_TRANSLATE_NOOP("KdeConfigBackend", "Restore a manually saved session")},
                                {"emptySession", QT_TRANSLATE_NOOP("KdeConfigBackend", "Start with an empty session")}})),
                toggle("confirm", QT_TRANSLATE_NOOP("KdeConfigBackend", "Confirm before signing out"), nullptr,
                       "ksmserverrc", "General", "confirmLogout", true),
                toggle("offerShutdown", QT_TRANSLATE_NOOP("KdeConfigBackend", "Offer shut down and restart"), nullptr,
                       "ksmserverrc", "General", "offerShutdown", true),
            }),
        };
    }
    if (page == QLatin1String("spell-checking")) {
        return {
            section(nullptr, {
                toggle("enabledByDefault", QT_TRANSLATE_NOOP("KdeConfigBackend", "Check spelling by default"), nullptr,
                       "sonnetrc", "General", "checkerEnabledByDefault", false),
                toggle("background", QT_TRANSLATE_NOOP("KdeConfigBackend", "Check while typing"), nullptr,
                       "sonnetrc", "General", "backgroundCheckerEnabled", true),
                toggle("autodetect", QT_TRANSLATE_NOOP("KdeConfigBackend", "Detect language automatically"), nullptr,
                       "sonnetrc", "General", "autodetectLanguage", true),
                toggle("skipUppercase", QT_TRANSLATE_NOOP("KdeConfigBackend", "Ignore ALL-CAPS words"), nullptr,
                       "sonnetrc", "General", "skipUppercase", true),
                toggle("skipRunTogether", QT_TRANSLATE_NOOP("KdeConfigBackend", "Ignore run-together words"), nullptr,
                       "sonnetrc", "General", "skipRunTogether", true),
            }),
        };
    }
    if (page == QLatin1String("workspace")) {
        return {
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "Behavior"), {
                choice("click", QT_TRANSLATE_NOOP("KdeConfigBackend", "Open files and folders"), nullptr,
                       "kdeglobals", "KDE", "SingleClick", "true",
                       options({{"true", QT_TRANSLATE_NOOP("KdeConfigBackend", "With a single click")},
                                {"false", QT_TRANSLATE_NOOP("KdeConfigBackend", "With a double click")}}), "globals"),
                toggle("pageScroll", QT_TRANSLATE_NOOP("KdeConfigBackend", "Scrollbar click jumps by one page"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Off jumps straight to the clicked position"),
                       "kdeglobals", "KDE", "ScrollbarLeftClickNavigatesByPage", true, "globals"),
            }),
            section(QT_TRANSLATE_NOOP("KdeConfigBackend", "On-screen display"), {
                toggle("osd", QT_TRANSLATE_NOOP("KdeConfigBackend", "Show volume and brightness pop-ups"), nullptr,
                       "plasmarc", "OSD", "Enabled", true),
            }),
        };
    }
    if (page == QLatin1String("automount")) {
        return {
            section(nullptr, {
                toggle("enabled", QT_TRANSLATE_NOOP("KdeConfigBackend", "Mount removable drives automatically"), nullptr,
                       "device_automounter_kcmrc", "General", "AutomountEnabled", true),
                toggle("login", QT_TRANSLATE_NOOP("KdeConfigBackend", "Mount drives that are connected at sign-in"), nullptr,
                       "device_automounter_kcmrc", "General", "AutomountOnLogin", true),
                toggle("plugin", QT_TRANSLATE_NOOP("KdeConfigBackend", "Mount drives when they are plugged in"), nullptr,
                       "device_automounter_kcmrc", "General", "AutomountOnPlugin", true),
                toggle("unknown", QT_TRANSLATE_NOOP("KdeConfigBackend", "Mount drives not seen before"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Off mounts only drives you have approved earlier"),
                       "device_automounter_kcmrc", "General", "AutomountUnknownDevices", false),
            }),
        };
    }
    if (page == QLatin1String("wallet")) {
        return {
            section(nullptr, {
                toggle("enabled", QT_TRANSLATE_NOOP("KdeConfigBackend", "Use the wallet to store passwords"),
                       QT_TRANSLATE_NOOP("KdeConfigBackend", "Applications can keep saved passwords encrypted in the wallet"),
                       "kwalletrc", "Wallet", "Enabled", true),
                toggle("closeIdle", QT_TRANSLATE_NOOP("KdeConfigBackend", "Lock the wallet when idle"), nullptr,
                       "kwalletrc", "Wallet", "Close When Idle", false),
                number("idle", QT_TRANSLATE_NOOP("KdeConfigBackend", "Lock after"), nullptr,
                       "kwalletrc", "Wallet", "Idle Timeout", 10, 1, 240, 1, QT_TRANSLATE_NOOP("KdeConfigBackend", "min")),
            }),
        };
    }
    return {};
}

bool needsKwin(const QString &hook)
{
    return hook == QLatin1String("kwin") || hook.startsWith(QLatin1String("effect:")) || hook.startsWith(QLatin1String("vdesktop:"));
}

QDBusMessage kwinCall(const QString &path, const QString &iface, const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), path, iface, method);
}
}

KdeConfigBackend::KdeConfigBackend(QObject *parent) : BackendBase(parent)
{
    const QStringList files{QStringLiteral("kwinrc"), QStringLiteral("kdeglobals"), QStringLiteral("baloofilerc"),
                            QStringLiteral("ksmserverrc"), QStringLiteral("sonnetrc"), QStringLiteral("plasmarc"),
                            QStringLiteral("device_automounter_kcmrc"), QStringLiteral("kwalletrc")};
    for (const QString &file : files) {
        auto watcher = KConfigWatcher::create(KSharedConfig::openConfig(file));
        connect(watcher.data(), &KConfigWatcher::configChanged, this, [this] { refresh(); });
        m_watchers.append(watcher);
    }
    setAvailable(true);
}

bool KdeConfigBackend::kwinRunning() const
{
    auto *interface = QDBusConnection::sessionBus().interface();
    return interface && interface->isServiceRegistered(QStringLiteral("org.kde.KWin"));
}

void KdeConfigBackend::refresh()
{
    ++m_revision;
    Q_EMIT changed();
}

bool KdeConfigBackend::hasSchema(const QString &pageId) const
{
    return !build(pageId).isEmpty();
}

QVariantList KdeConfigBackend::sectionsFor(const QString &pageId) const
{
    return build(pageId);
}

QVariantList KdeConfigBackend::schema(const QString &pageId) const
{
    QVariantList sections = sectionsFor(pageId);
    for (QVariant &sectionValue : sections) {
        Map sectionMap = sectionValue.toMap();
        QVariantList rows = sectionMap.value("rows").toList();
        for (QVariant &rowValue : rows) {
            Map row = rowValue.toMap();
            auto config = KSharedConfig::openConfig(row.value("file").toString());
            config->reparseConfiguration();
            const KConfigGroup group = config->group(row.value("group").toString());
            const QString key = row.value("key").toString();
            const QString type = row.value("type").toString();
            if (type == QLatin1String("toggle")) {
                bool value = group.readEntry(key, row.value("default").toBool());
                if (row.value("invert").toBool())
                    value = !value;
                row.insert("value", value);
            } else if (type == QLatin1String("number")) {
                row.insert("value", group.readEntry(key, row.value("default").toInt()));
            } else {
                const QString fallback = row.value("default").toString();
                QString value = group.readEntry(key, fallback);
                if (row.value("numeric").toBool()) {
                    // Pick the closest option so a value written by another
                    // tool (for example 0.25) still shows a sensible choice.
                    const double current = group.readEntry(key, fallback.toDouble());
                    double best = 1e9;
                    for (const QVariant &option : row.value("choices").toList()) {
                        const QString candidate = option.toMap().value("value").toString();
                        const double distance = std::abs(candidate.toDouble() - current);
                        if (distance < best) { best = distance; value = candidate; }
                    }
                }
                row.insert("value", value);
            }
            rowValue = row;
        }
        sectionMap.insert("rows", rows);
        sectionValue = sectionMap;
    }
    return sections;
}

QVariantMap KdeConfigBackend::findRow(const QString &pageId, const QString &rowId) const
{
    for (const QVariant &sectionValue : sectionsFor(pageId))
        for (const QVariant &rowValue : sectionValue.toMap().value("rows").toList())
            if (rowValue.toMap().value("id").toString() == rowId)
                return rowValue.toMap();
    return {};
}

void KdeConfigBackend::setValue(const QString &pageId, const QString &rowId, const QVariant &value)
{
    const Map row = findRow(pageId, rowId);
    if (row.isEmpty()) {
        setError(tr("This setting is not available."));
        return;
    }
    clearError();
    const QString type = row.value("type").toString();
    auto config = KSharedConfig::openConfig(row.value("file").toString());
    KConfigGroup group = config->group(row.value("group").toString());
    const QString key = row.value("key").toString();

    if (type == QLatin1String("toggle")) {
        bool enabled = value.toBool();
        if (row.value("invert").toBool())
            enabled = !enabled;
        group.writeEntry(key, enabled, KConfig::Notify);
    } else if (type == QLatin1String("number")) {
        const int number = std::clamp(value.toInt(), row.value("min").toInt(), row.value("max").toInt());
        group.writeEntry(key, number, KConfig::Notify);
    } else {
        const QString text = value.toString();
        bool valid = false;
        for (const QVariant &option : row.value("choices").toList())
            valid = valid || option.toMap().value("value").toString() == text;
        if (!valid) {
            setError(tr("That option is not supported."));
            return;
        }
        if (row.value("numeric").toBool())
            group.writeEntry(key, text.toDouble(), KConfig::Notify);
        else if (text == QLatin1String("true") || text == QLatin1String("false"))
            group.writeEntry(key, text == QLatin1String("true"), KConfig::Notify);
        else
            group.writeEntry(key, text, KConfig::Notify);
    }
    if (!group.sync()) {
        setError(tr("The setting could not be saved."));
        return;
    }
    runHook(row.value("hook").toString(), type == QLatin1String("number") ? value : QVariant(value));
    refresh();
}

void KdeConfigBackend::resetPage(const QString &pageId)
{
    clearError();
    for (const QVariant &sectionValue : sectionsFor(pageId)) {
        for (const QVariant &rowValue : sectionValue.toMap().value("rows").toList()) {
            const Map row = rowValue.toMap();
            auto config = KSharedConfig::openConfig(row.value("file").toString());
            KConfigGroup group = config->group(row.value("group").toString());
            group.deleteEntry(row.value("key").toString(), KConfig::Notify);
            if (!group.sync()) {
                setError(tr("The settings could not be reset."));
                return;
            }
            runHook(row.value("hook").toString(), row.value("default"));
        }
    }
    refresh();
}

void KdeConfigBackend::runHook(const QString &hook, const QVariant &value)
{
    if (hook.isEmpty())
        return;
    if (needsKwin(hook) && !kwinRunning())
        return; // Saved; KWin reads it at its next start.
    auto bus = QDBusConnection::sessionBus();
    if (hook == QLatin1String("kwin")) {
        bus.asyncCall(kwinCall(QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reconfigure")));
    } else if (hook == QLatin1String("globals")) {
        auto signal = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"),
                                                  QStringLiteral("notifyChange"));
        signal << 3 << 4;
        bus.send(signal);
    } else if (hook.startsWith(QLatin1String("effect:"))) {
        const bool enable = value.isValid() ? value.toBool() : false;
        auto call = kwinCall(QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"),
                             enable ? QStringLiteral("loadEffect") : QStringLiteral("unloadEffect"));
        call << hook.mid(7);
        bus.asyncCall(call);
    } else if (hook.startsWith(QLatin1String("vdesktop:"))) {
        // Apply to the running compositor too; config alone only applies at
        // the next KWin start.
        auto call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/VirtualDesktopManager"),
                                                   QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
        call << QStringLiteral("org.kde.KWin.VirtualDesktopManager") << hook.mid(9)
             << QVariant::fromValue(QDBusVariant(static_cast<uint>(std::max(1, value.toInt()))));
        bus.asyncCall(call);
    }
}
