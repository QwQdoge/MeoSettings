#include "fontsettingsbackend.h"
#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QtMath>

namespace {
struct Role { const char *id; const char *group; const char *key; };
constexpr Role roles[] = {{"general", "General", "font"}, {"fixed", "General", "fixed"},
    {"menu", "General", "menuFont"}, {"toolbar", "General", "toolBarFont"},
    {"small", "General", "smallestReadableFont"}, {"title", "WM", "activeFont"}};
}

FontSettingsBackend::FontSettingsBackend(QObject *parent) : BackendBase(parent), m_families(QFontDatabase::families())
{
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this] { refresh(); });
    refresh();
}

QStringList FontSettingsBackend::styles(const QString &family) const
{
    return m_families.contains(family) ? QFontDatabase::styles(family) : QStringList();
}

void FontSettingsBackend::refresh()
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals")); config->reparseConfiguration();
    const QStringList names{tr("Interface"), tr("Fixed width"), tr("Menus"), tr("Toolbars"), tr("Small text"), tr("Window titles")};
    m_fonts.clear(); int index = 0;
    for (const auto &role : roles) {
        QFont fallback = QFontDatabase::systemFont(QByteArray(role.id) == "fixed" ? QFontDatabase::FixedFont : QFontDatabase::GeneralFont);
        const QFont font = KConfigGroup(config, QString::fromLatin1(role.group)).readEntry(role.key, fallback);
        m_fonts.append(QVariantMap{{"id", QString::fromLatin1(role.id)}, {"label", names[index++]},
            {"family", font.family()}, {"style", font.styleName()}, {"points", font.pointSizeF() > 0 ? font.pointSizeF() : 10.0}});
    }
    setAvailable(!m_families.isEmpty()); Q_EMIT changed();
}

void FontSettingsBackend::setFont(const QString &id, const QString &family, const QString &style, double points)
{
    const Role *selected = nullptr;
    for (const auto &role : roles) if (id == QLatin1String(role.id)) selected = &role;
    if (!selected || !m_families.contains(family) || !styles(family).contains(style) || !qIsFinite(points) || points < 6 || points > 72) {
        setError(tr("Choose an installed font style and a size between 6 and 72 points.")); return;
    }
    QFont font = QFontDatabase::font(family, style, qRound(points)); font.setPointSizeF(points);
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup group(config, QString::fromLatin1(selected->group));
    group.writeEntry(selected->key, font, KConfig::Notify);
    if (!group.sync()) { setError(tr("The font setting could not be saved.")); return; }
    clearError();
    // KDE platform themes listen for the established FontChanged (1) notification.
    auto signal = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"), QStringLiteral("notifyChange"));
    signal << 1 << 0; QDBusConnection::sessionBus().send(signal);
    if (id == QStringLiteral("general")) QGuiApplication::setFont(font);
    refresh();
}
