#include "surfaceeffectsbackend.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>

namespace
{
constexpr auto configFile = "meo-shellrc";
constexpr auto groupName = "Appearance";
constexpr auto transparencyKey = "SurfaceTransparency";
constexpr auto blurKey = "SurfaceBlur";
// KWin's default blur strength (5) is faint. A Pixel-like frosted look needs a
// stronger, soft blur with no grain. Only applied when the user has not chosen
// a strength themselves.
constexpr int frostedBlurStrength = 10;

}

void applyFrostedKWinBlur()
{
    auto group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-blur"));
    if (group.hasKey("BlurStrength")) {
        return;
    }
    group.writeEntry("BlurStrength", frostedBlurStrength, KConfig::Notify);
    if (!group.hasKey("NoiseStrength")) {
        group.writeEntry("NoiseStrength", 0, KConfig::Notify);
    }
    if (group.sync()) {
        auto reload = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"),
                                                     QStringLiteral("org.kde.kwin.Effects"),
                                                     QStringLiteral("reconfigureEffect"));
        reload.setArguments({QStringLiteral("blur")});
        QDBusConnection::sessionBus().send(reload);
    }
}

SurfaceEffectsBackend::SurfaceEffectsBackend(QObject *parent)
    : BackendBase(parent)
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QString::fromLatin1(configFile))))
{
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == QLatin1String(groupName)) {
            refresh();
        }
    });
    setAvailable(true);
    refresh();
}

void SurfaceEffectsBackend::refresh()
{
    const auto config = KSharedConfig::openConfig(QString::fromLatin1(configFile));
    config->reparseConfiguration();
    const KConfigGroup group(config, QString::fromLatin1(groupName));
    const bool transparency = group.readEntry(transparencyKey, false);
    const bool blur = group.readEntry(blurKey, false);
    if (transparency != m_transparency || blur != m_blur) {
        m_transparency = transparency;
        m_blur = blur;
        Q_EMIT changed();
    }
}

void SurfaceEffectsBackend::setBlur(bool enabled)
{
    // Blur needs a translucent surface to show through.
    if (write(enabled ? true : m_transparency, enabled) && enabled) {
        applyFrostedKWinBlur();
    }
}

void SurfaceEffectsBackend::setTransparency(bool enabled)
{
    // Without transparency blur has nothing to act on; keep the file honest.
    write(enabled, enabled ? m_blur : false);
}

bool SurfaceEffectsBackend::write(bool transparency, bool blur)
{
    clearError();
    auto group = KSharedConfig::openConfig(QString::fromLatin1(configFile))->group(QString::fromLatin1(groupName));
    group.writeEntry(transparencyKey, transparency, KConfig::Notify);
    group.writeEntry(blurKey, blur, KConfig::Notify);
    if (!group.sync()) {
        setError(tr("The surface effect preference could not be saved."));
        refresh();
        return false;
    }
    refresh();
    return true;
}
