#include "welcomebackend.h"

#include <QProcess>
#include <QStandardPaths>

WelcomeBackend::WelcomeBackend(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

bool WelcomeBackend::launcherAvailable() const
{
    return !m_launcher.isEmpty();
}

bool WelcomeBackend::open()
{
    clearError();
    if (m_launcher.isEmpty()) {
        setError(tr("Meo Welcome is not installed."));
        return false;
    }
    if (!QProcess::startDetached(m_launcher, {QStringLiteral("--show")})) {
        setError(tr("Unable to open Meo Welcome."));
        return false;
    }
    return true;
}

void WelcomeBackend::refresh()
{
    m_launcher = QStandardPaths::findExecutable(QStringLiteral("meo-welcome"));
    setAvailable(!m_launcher.isEmpty());
    Q_EMIT changed();
}
