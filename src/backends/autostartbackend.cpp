#include "autostartbackend.h"
#include <KApplicationTrader>
#include <KService>
#include <KSycoca>
#include <KDesktopFile>
#include <KConfigGroup>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QSet>

namespace {
bool validId(const QString &id) {
    return !id.isEmpty() && id.endsWith(QStringLiteral(".desktop")) && QFileInfo(id).fileName() == id
        && !id.contains('\\') && !id.contains(QChar::Null);
}
QString userDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
}
QStringList directories() {
    QStringList result;
    for (const auto &path : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        result.append(path + QStringLiteral("/autostart"));
    result.removeDuplicates(); return result;
}
}

AutostartBackend::AutostartBackend(QObject *parent) : BackendBase(parent)
{
    m_refreshTimer.setSingleShot(true); m_refreshTimer.setInterval(100);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AutostartBackend::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_refreshTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_refreshTimer.start(); });
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, &AutostartBackend::refresh);
    refresh();
}

QString AutostartBackend::sourceFor(const QString &id) const
{
    if (!validId(id)) return {};
    for (const auto &directory : directories()) {
        const QString path = QDir(directory).filePath(id);
        if (QFileInfo::exists(path)) return path;
    }
    return {};
}

void AutostartBackend::refresh()
{
    if (!m_watcher.files().isEmpty()) m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty()) m_watcher.removePaths(m_watcher.directories());
    m_entries.clear(); m_applications.clear();
    QSet<QString> seen;
    for (const auto &directory : directories()) {
        const QDir dir(directory);
        if (dir.exists()) m_watcher.addPath(directory);
        else if (QDir(QFileInfo(directory).absolutePath()).exists()) m_watcher.addPath(QFileInfo(directory).absolutePath());
        for (const auto &id : dir.entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name)) {
            if (!validId(id) || seen.contains(id)) continue;
            seen.insert(id);
            const QString path = dir.filePath(id);
            const KDesktopFile file(path);
            const auto group = file.desktopGroup();
            const bool local = directory == userDirectory();
            bool hasSystemEntry = false;
            for (const auto &system : directories())
                if (system != userDirectory() && QFileInfo::exists(QDir(system).filePath(id))) hasSystemEntry = true;
            const QStringList only = group.readXdgListEntry("OnlyShowIn"), excluded = group.readXdgListEntry("NotShowIn");
            const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts);
            bool matches = only.isEmpty();
            for (const auto &desktop : desktops) if (only.contains(desktop)) matches = true;
            for (const auto &desktop : desktops) if (excluded.contains(desktop)) matches = false;
            QString detail = local ? tr("Personal sign-in entry") : tr("System sign-in entry; changes create a personal override");
            if (!matches) detail += desktops.isEmpty() ? tr(" · Desktop restriction applies") : tr(" · Excluded from this desktop");
            if (!file.tryExec()) detail += tr(" · Required executable unavailable");
            if (group.hasKey("X-KDE-autostart-condition") || group.hasKey("AutostartCondition") || group.hasKey("X-GNOME-Autostart-enabled") || group.readEntry("X-systemd-skip", false))
                detail += tr(" · Additional provider conditions apply");
            m_entries.append(QVariantMap{{"id", id}, {"label", file.readName().isEmpty() ? id : file.readName()},
                {"detail", detail}, {"enabled", !group.readEntry("Hidden", false)}, {"local", local}, {"systemEntry", hasSystemEntry},
                {"editable", !group.isEntryImmutable("Hidden") && !(local && QFileInfo(path).isSymLink())}});
            m_watcher.addPath(path);
        }
    }
    for (const auto &app : KApplicationTrader::query([](const KService::Ptr &service) { return service->isApplication() && !service->noDisplay(); })) {
        if (validId(app->storageId()) && QFileInfo::exists(app->entryPath()))
            m_applications.append(QVariantMap{{"id", app->storageId()}, {"label", app->name()}});
    }
    setAvailable(!QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).isEmpty());
    Q_EMIT changed();
}

bool AutostartBackend::writeEntry(const QString &source, const QString &id, bool enabled)
{
    if (!validId(id) || source.isEmpty()) { setError(tr("Choose an existing sign-in entry or installed application.")); return false; }
    const QString destination = QDir(userDirectory()).filePath(id);
    if (QFileInfo(destination).isSymLink()) { setError(tr("This personal entry is a symbolic link; manage its source in advanced settings.")); return false; }
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly) || input.size() > 1024 * 1024) { setError(tr("The application entry cannot be read.")); return false; }
    const QByteArray original = input.readAll();
    QTemporaryFile temporary;
    if (!temporary.open() || temporary.write(original) != original.size() || !temporary.flush()) { setError(tr("The application entry could not be prepared.")); return false; }
    temporary.close();
    {
        KDesktopFile file(temporary.fileName());
        auto group = file.desktopGroup();
        if (group.isEntryImmutable("Hidden")) { setError(tr("This sign-in preference is managed by the system administrator.")); return false; }
        group.writeEntry("Hidden", !enabled);
        if (!file.sync()) { setError(tr("The sign-in preference could not be prepared.")); return false; }
    }
    QFile rendered(temporary.fileName());
    if (!rendered.open(QIODevice::ReadOnly) || !QDir().mkpath(userDirectory())) { setError(tr("The personal autostart directory is unavailable.")); return false; }
    QSaveFile output(destination); output.setDirectWriteFallback(false);
    const QByteArray bytes = rendered.readAll();
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) { setError(tr("The sign-in preference could not be saved.")); return false; }
    clearError(); refresh(); return true;
}

void AutostartBackend::setEnabled(const QString &id, bool enabled)
{
    writeEntry(sourceFor(id), id, enabled);
}

void AutostartBackend::addApplication(const QString &storageId)
{
    bool listed = false;
    for (const auto &item : m_applications) if (item.toMap().value("id").toString() == storageId) listed = true;
    if (!listed) { setError(tr("Choose an installed application from the list.")); return; }
    const auto app = KService::serviceByStorageId(storageId);
    if (!app || !app->isApplication() || app->noDisplay()) { setError(tr("Choose an installed application.")); return; }
    const QString id = app->storageId();
    if (!sourceFor(id).isEmpty()) { setError(tr("This application already has a sign-in entry. Change its existing startup permission instead.")); return; }
    writeEntry(app->entryPath(), id, true);
}

void AutostartBackend::removeUserEntry(const QString &id)
{
    if (!validId(id)) { setError(tr("Invalid sign-in entry.")); return; }
    const QString path = QDir(userDirectory()).filePath(id);
    if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) { setError(tr("No personal entry exists to remove.")); return; }
    if (!QFile::remove(path)) { setError(tr("The personal sign-in entry could not be removed.")); return; }
    clearError(); refresh();
}
