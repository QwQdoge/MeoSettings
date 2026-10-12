#include "defaultappsbackend.h"
#include <KApplicationTrader>
#include <KService>
#include <KSycoca>
#include <QMimeDatabase>
#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

DefaultAppsBackend::DefaultAppsBackend(QObject *parent) : BackendBase(parent)
{
    // Enumerating every MIME type and resolving preferred applications through
    // KSycoca is useful only on the Default apps / File associations routes.
    // Keep process startup cheap and let those pages request the inventory
    // after their visual shell has painted.
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, [this] {
        if (!m_mimeTypes.isEmpty())
            refresh();
    });
}

void DefaultAppsBackend::ensureMimeTypes()
{
    if (!m_mimeTypes.isEmpty())
        return;
    for (const auto &type : QMimeDatabase().allMimeTypes())
        m_mimeTypes.append(type.name());
    for (const auto &scheme : {"x-scheme-handler/http", "x-scheme-handler/https", "x-scheme-handler/mailto"})
        if (!m_mimeTypes.contains(QString::fromLatin1(scheme)))
            m_mimeTypes.append(QString::fromLatin1(scheme));
    m_mimeTypes.sort();
}

bool DefaultAppsBackend::supportedMime(const QString &mime) const { return m_mimeTypes.contains(mime); }

QVariantList DefaultAppsBackend::applicationsFor(const QString &mime) const
{
    QVariantList result;
    if (!supportedMime(mime)) return result;
    for (const auto &app : KApplicationTrader::queryByMimeType(mime)) {
        if (!app->isApplication() || app->storageId().isEmpty()) continue;
        result.append(QVariantMap{{"id", app->storageId()}, {"label", app->name()}, {"icon", app->icon()}});
    }
    return result;
}

QString DefaultAppsBackend::preferredApplication(const QString &mime) const
{
    if (!supportedMime(mime)) return {};
    const auto app = KApplicationTrader::preferredService(mime);
    return app ? app->storageId() : QString();
}

void DefaultAppsBackend::refresh()
{
    setBusy(true);
    clearError();
    ensureMimeTypes();
    m_roles.clear();
    const QList<QPair<QString, QString>> roles{
        {tr("Web browser"), QStringLiteral("x-scheme-handler/http")},
        {tr("Secure web links"), QStringLiteral("x-scheme-handler/https")},
        {tr("Email"), QStringLiteral("x-scheme-handler/mailto")},
        {tr("File manager"), QStringLiteral("inode/directory")},
        {tr("PDF documents"), QStringLiteral("application/pdf")},
        {tr("Text documents"), QStringLiteral("text/plain")},
        {tr("Images"), QStringLiteral("image/png")},
        {tr("Audio"), QStringLiteral("audio/mpeg")},
        {tr("Video"), QStringLiteral("video/mp4")}
    };
    for (const auto &[label, mime] : roles)
        m_roles.append(QVariantMap{{"label", label}, {"mime", mime}, {"applications", applicationsFor(mime)}, {"preferred", preferredApplication(mime)}});
    setAvailable(true);
    setBusy(false);
    Q_EMIT changed();
}

void DefaultAppsBackend::setPreferredApplication(const QString &mime, const QString &storageId)
{
    if (busy()) return;
    KService::Ptr choice;
    if (supportedMime(mime)) for (const auto &app : KApplicationTrader::queryByMimeType(mime))
        if (app->isApplication() && app->storageId() == storageId) choice = app;
    if (!choice) { setError(tr("Choose an installed application that supports this file type.")); return; }
    const QString builder = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca6"), {QStringLiteral("/usr/bin")});
    if (builder.isEmpty()) { setError(tr("The KDE application cache service is unavailable.")); return; }
    // KService writes the per-user MIME association; no executable or command line is supplied by the UI.
    setBusy(true); clearError();
    KApplicationTrader::setPreferredService(mime, choice);
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process); timeout->setSingleShot(true); timeout->setInterval(15000);
    connect(timeout, &QTimer::timeout, process, [process] { process->kill(); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        setBusy(false); setError(tr("The application cache could not be updated: %1").arg(process->errorString())); process->deleteLater(); refresh();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, timeout, mime, storageId](int code, QProcess::ExitStatus status) {
            timeout->stop(); setBusy(false); refresh();
            if (status != QProcess::NormalExit || code != 0) setError(tr("The association was saved, but the application cache update failed."));
            else if (preferredApplication(mime) != storageId) setError(tr("KDE did not confirm the requested default application."));
            process->deleteLater();
        });
    process->start(builder, {QStringLiteral("--noincremental")}); timeout->start();
}
