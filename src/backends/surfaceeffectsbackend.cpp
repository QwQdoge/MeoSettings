#include "surfaceeffectsbackend.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace
{
constexpr auto configFile = "meo-shellrc";
constexpr auto groupName = "Appearance";
constexpr auto transparencyKey = "SurfaceTransparency";
constexpr auto blurKey = "SurfaceBlur";
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
    write(enabled ? true : m_transparency, enabled);
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
