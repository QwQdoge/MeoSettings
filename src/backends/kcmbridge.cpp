#include "kcmbridge.h"

#include <KPluginMetaData>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

KcmBridge::KcmBridge(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

bool KcmBridge::launcherAvailable() const
{
    return !m_launcher.isEmpty();
}

bool KcmBridge::partitionManagerAvailable() const
{
    return !m_partitionManager.isEmpty();
}

QVariantList KcmBridge::modules() const
{
    return m_modules;
}

bool KcmBridge::isAvailable(const QString &pluginId) const
{
    return m_modulesById.contains(pluginId);
}

void KcmBridge::open(const QString &pluginId)
{
    clearError();
    if (!m_modulesById.contains(pluginId)) {
        setError(tr("This KDE settings module is not installed."));
        return;
    }
    if (m_launcher.isEmpty()) {
        setError(tr("The KDE settings-module launcher is unavailable."));
        return;
    }
    // An external KCM process is the compatibility fallback for QWidget-based
    // and otherwise non-embeddable modules.  The id is looked up from installed
    // plugin metadata before launch, so QML never forms an arbitrary command.
    if (!QProcess::startDetached(m_launcher, {pluginId})) {
        setError(tr("Unable to open the KDE settings module."));
    }
}

void KcmBridge::openPartitionManager()
{
    clearError();
    if (m_partitionManager.isEmpty()) {
        setError(tr("KDE Partition Manager is not installed."));
        return;
    }
    // Do not expose a generic process launcher to QML.  Launching this one
    // allowlisted tool performs no storage operation by itself; any privileged
    // change and its recovery information remain in the maintained tool.
    if (!QProcess::startDetached(m_partitionManager, {})) {
        setError(tr("Unable to open KDE Partition Manager."));
    }
}

void KcmBridge::refresh()
{
    m_launcher = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    m_partitionManager = QStandardPaths::findExecutable(QStringLiteral("partitionmanager"));
    m_modulesById.clear();

    const QStringList kcmSubdirectories{
        QStringLiteral("plasma/kcms/systemsettings"),
        QStringLiteral("plasma/kcms/systemsettings_qwidgets"),
        QStringLiteral("plasma/kcms/kinfocenter"),
    };
    QSet<QString> visitedDirectories;
    for (const auto &libraryPath : QCoreApplication::libraryPaths()) {
        for (const auto &subdirectory : kcmSubdirectories) {
            const auto directory = QDir(libraryPath).filePath(subdirectory);
            if (!QFileInfo(directory).isDir() || visitedDirectories.contains(directory)) {
                continue;
            }
            visitedDirectories.insert(directory);
            for (const auto &metadata : KPluginMetaData::findPlugins(directory)) {
                const auto id = metadata.pluginId();
                if (id.isEmpty()) {
                    continue;
                }
                m_modulesById.insert(id, QVariantMap{
                    {QStringLiteral("id"), id},
                    {QStringLiteral("name"), metadata.name()},
                    {QStringLiteral("description"), metadata.description()},
                    {QStringLiteral("icon"), metadata.iconName()},
                });
            }
        }
    }

    QStringList ids = m_modulesById.keys();
    std::sort(ids.begin(), ids.end(), [this](const QString &left, const QString &right) {
        return m_modulesById.value(left).value(QStringLiteral("name")).toString().localeAwareCompare(
                   m_modulesById.value(right).value(QStringLiteral("name")).toString()) < 0;
    });
    QVariantList nextModules;
    nextModules.reserve(ids.size());
    for (const auto &id : ids) {
        nextModules.push_back(m_modulesById.value(id));
    }
    m_modules = nextModules;
    setAvailable(!m_launcher.isEmpty());
    Q_EMIT changed();
}
