#pragma once

#include "../core/backendbase.h"

#include <QString>

/**
 * Narrow launcher for the independent first-login application.
 *
 * Settings never edits Welcome's per-user completion state.  The executable
 * owns that state and receives --show only after an explicit user action.
 */
class WelcomeBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool launcherAvailable READ launcherAvailable NOTIFY changed)

public:
    explicit WelcomeBackend(QObject *parent = nullptr);

    bool launcherAvailable() const;

    Q_INVOKABLE bool open();
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QString m_launcher;
};
