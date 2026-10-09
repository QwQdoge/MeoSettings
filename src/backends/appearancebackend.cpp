#include "appearancebackend.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <csignal>
#include <unistd.h>

AppearanceBackend::AppearanceBackend(QObject *parent) : BackendBase(parent)
{
    m_process.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
    m_timeout.setSingleShot(true); m_timeout.setInterval(60000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_process.processId() > 0) ::kill(-pid_t(m_process.processId()), SIGKILL);
        m_process.kill(); setError(tr("The appearance tool timed out. Some layers may have changed; use Restore previous appearance if available."));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) { m_timeout.stop(); setBusy(false); setError(m_process.errorString()); }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus result) {
        m_timeout.stop(); setBusy(false);
        if (code || result != QProcess::NormalExit) {
            if (error().isEmpty()) setError(tr("Appearance was only partially applied: %1. Restore the previous appearance before retrying.")
                .arg(QString::fromUtf8(m_process.readAllStandardError()).trimmed().left(2048)));
        } else { m_status = tr("Appearance saved. Sign out and back in to reload every desktop component."); }
        refresh();
    });
    refresh();
}

bool AppearanceBackend::hasComponent(const QString &path) const
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation, path).isEmpty();
}
void AppearanceBackend::refresh()
{
    setAvailable(QFileInfo(QStringLiteral("/usr/bin/meo-desktop-apply")).isExecutable()
        && QFileInfo(QStringLiteral("/usr/bin/meo-theme-mode")).isExecutable()
        && hasComponent(QStringLiteral("plasma/look-and-feel/org.meo.desktop/contents/defaults")));
    Q_EMIT changed();
}
bool AppearanceBackend::recoveryAvailable() const
{
    const QString state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/meo-desktop");
    QFile marker(state + QStringLiteral("/last-backup"));
    if (!marker.open(QIODevice::ReadOnly)) return false;
    const QString path = QString::fromUtf8(marker.read(4096)).trimmed();
    return path.startsWith(state + QStringLiteral("/backups/")) && path.endsWith(QStringLiteral("-theme-apply"))
        && !QFileInfo(path).isSymLink() && QFileInfo(path + QStringLiteral("/appearance-manifest")).isFile();
}
void AppearanceBackend::run(const QString &tool, const QStringList &arguments)
{
    if (busy()) return;
    clearError(); m_status.clear(); Q_EMIT changed(); setBusy(true);
    m_process.start(tool, arguments); m_timeout.start();
}
void AppearanceBackend::applyPreset()
{
    if (!available()) { setError(tr("Install the Meo desktop preset and its application tools first.")); return; }
    // The installed Meo owner makes the recovery snapshot, applies Look-and-Feel
    // and KWin defaults, without resetting panels or enabling input frameworks.
    run(QStringLiteral("/usr/bin/meo-desktop-apply"), {QStringLiteral("--appearance-only"), QStringLiteral("--quiet")});
}
void AppearanceBackend::applyMode(const QString &mode)
{
    if (!QStringList{QStringLiteral("light"), QStringLiteral("dark")}.contains(mode)) { setError(tr("Choose light or dark appearance.")); return; }
    const QString suffix = mode == QStringLiteral("dark") ? QStringLiteral("Dark") : QStringLiteral("Light");
    if (!available() || !hasComponent(QStringLiteral("color-schemes/Meo") + suffix + QStringLiteral(".colors"))
        || !hasComponent(QStringLiteral("plasma/desktoptheme/Meo") + suffix + QStringLiteral("/metadata.json"))
        || !hasComponent(mode == QStringLiteral("dark") ? QStringLiteral("icons/MeoSymbolsDark/index.theme") : QStringLiteral("icons/MeoSymbols/index.theme"))) {
        setError(tr("The matching Meo color, desktop or icon variant is not installed.")); return;
    }
    run(QStringLiteral("/usr/bin/meo-theme-mode"), {mode});
}
void AppearanceBackend::restore()
{
    if (!recoveryAvailable()) { setError(tr("No Meo appearance snapshot is available.")); return; }
    run(QStringLiteral("/usr/bin/meo-desktop-apply"), {QStringLiteral("--restore-latest"), QStringLiteral("--quiet")});
}
