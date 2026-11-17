#pragma once

#include "../core/backendbase.h"

#include <KConfigWatcher>

/**
 * Reads and writes the optional surface effects of the Meo shell: the
 * `[Appearance]` keys `SurfaceTransparency` and `SurfaceBlur` in `meo-shellrc`
 * (consumed by Meo.System.ShellAppearance in meo-kde). Both default to off.
 * Blur is only effective together with transparency, so enabling blur also
 * enables transparency; disabling blur leaves transparency as it is.
 */
// Raises KWin's blur strength to the frosted Meo default (10, no noise) when
// the user has not chosen one; reloads KWin's blur effect. Used wherever
// Settings turns on SurfaceBlur.
void applyFrostedKWinBlur();

class SurfaceEffectsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool transparency READ transparency NOTIFY changed)
    Q_PROPERTY(bool blur READ blur NOTIFY changed)

public:
    explicit SurfaceEffectsBackend(QObject *parent = nullptr);

    bool transparency() const { return m_transparency; }
    bool blur() const { return m_blur; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setBlur(bool enabled);
    Q_INVOKABLE void setTransparency(bool enabled);

Q_SIGNALS:
    void changed();

private:
    bool write(bool transparency, bool blur);

    bool m_transparency = false;
    bool m_blur = false;
    KConfigWatcher::Ptr m_watcher;
};
