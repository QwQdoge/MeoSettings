#pragma once

#include "../core/backendbase.h"

#include <KConfigWatcher>

/**
 * Reads and writes the Meo shell's `[Appearance]` keys in `meo-shellrc`
 * (consumed by Meo.System.ShellAppearance, MeoStyle and the Meo window
 * decoration in meo-kde):
 * - `SurfaceTransparency` and `SurfaceBlur`, both off by default. Blur is only
 *   effective together with transparency, so enabling blur also enables
 *   transparency; disabling blur leaves transparency as it is.
 * - `CornerScale`, the roundness of Meo corners in percent of the design
 *   radii (0 square - 150 extra round, default 100). It drives
 *   MeoTheme.cornerScale in MeoUI surfaces, KDE application controls and
 *   window corners alike.
 */
class SurfaceEffectsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool transparency READ transparency NOTIFY changed)
    Q_PROPERTY(bool blur READ blur NOTIFY changed)
    Q_PROPERTY(int cornerScale READ cornerScale NOTIFY changed)
    Q_PROPERTY(int minimumCornerScale READ minimumCornerScale CONSTANT)
    Q_PROPERTY(int maximumCornerScale READ maximumCornerScale CONSTANT)

public:
    explicit SurfaceEffectsBackend(QObject *parent = nullptr);

    bool transparency() const { return m_transparency; }
    bool blur() const { return m_blur; }
    int cornerScale() const { return m_cornerScale; }
    static constexpr int minimumCornerScale() { return 0; }
    static constexpr int maximumCornerScale() { return 150; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setBlur(bool enabled);
    Q_INVOKABLE void setTransparency(bool enabled);
    // Percent; clamped to minimumCornerScale..maximumCornerScale.
    Q_INVOKABLE void setCornerScale(int percent);

Q_SIGNALS:
    void changed();

private:
    bool write(bool transparency, bool blur);

    bool m_transparency = false;
    bool m_blur = false;
    int m_cornerScale = 100;
    KConfigWatcher::Ptr m_watcher;
};
