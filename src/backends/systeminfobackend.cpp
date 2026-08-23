#include "systeminfobackend.h"

#include <KMemoryInfo>
#include <KOSRelease>
#include <KUser>

#include <QFileInfo>
#include <QGuiApplication>
#include <QSysInfo>
#include <QUrl>

namespace
{
QString formatBytes(const quint64 bytes)
{
    constexpr qreal gib = 1024.0 * 1024.0 * 1024.0;
    constexpr qreal mib = 1024.0 * 1024.0;
    if (bytes >= static_cast<quint64>(gib)) {
        return QString::number(bytes / gib, 'f', 1) + QStringLiteral(" GiB");
    }
    return QString::number(bytes / mib, 'f', 0) + QStringLiteral(" MiB");
}
}

SystemInfoBackend::SystemInfoBackend(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QVariantList SystemInfoBackend::facts() const
{
    return m_facts;
}

QString SystemInfoBackend::userName() const
{
    return m_userName;
}

QString SystemInfoBackend::userAvatarSource() const
{
    return m_userAvatarSource;
}

QString SystemInfoBackend::deviceName() const
{
    return m_deviceName;
}

QString SystemInfoBackend::operatingSystemName() const
{
    return m_operatingSystemName;
}

void SystemInfoBackend::refresh()
{
    const KOSRelease release;
    const KMemoryInfo memory;
    const KUser user(KUser::UseRealUserID);
    const QString userLogin = user.isValid() ? user.loginName() : tr("Unavailable");
    const QString userFullName = user.isValid()
        ? user.property(KUser::FullName).toString().trimmed()
        : QString();
    const QString nextUserName = userFullName.isEmpty() ? userLogin : userFullName;
    const QString facePath = user.isValid() ? user.faceIconPath() : QString();
    const QString nextUserAvatarSource = QFileInfo(facePath).isFile()
        ? QUrl::fromLocalFile(facePath).toString()
        : QString();
    const QString nextDeviceName = QSysInfo::machineHostName().trimmed();
    const QString nextOperatingSystemName = release.prettyName().isEmpty()
        ? release.name()
        : release.prettyName();

    QVariantList nextFacts{
        QVariantMap{{QStringLiteral("label"), tr("Operating system")},
                    {QStringLiteral("value"), nextOperatingSystemName}},
        QVariantMap{{QStringLiteral("label"), tr("Kernel")},
                    {QStringLiteral("value"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion()}},
        QVariantMap{{QStringLiteral("label"), tr("Architecture")},
                    {QStringLiteral("value"), QSysInfo::currentCpuArchitecture()}},
        QVariantMap{{QStringLiteral("label"), tr("Memory")},
                    {QStringLiteral("value"), memory.isNull() ? tr("Unavailable") : formatBytes(memory.totalPhysical())}},
        QVariantMap{{QStringLiteral("label"), tr("Host name")},
                    {QStringLiteral("value"), nextDeviceName}},
        QVariantMap{{QStringLiteral("label"), tr("User")},
                    {QStringLiteral("value"), userLogin}},
        QVariantMap{{QStringLiteral("label"), tr("KDE platform")},
                    {QStringLiteral("value"), QGuiApplication::platformName()}},
        QVariantMap{{QStringLiteral("label"), tr("Qt")},
                    {QStringLiteral("value"), QString::fromLatin1(qVersion())}},
        QVariantMap{{QStringLiteral("label"), tr("Meo Settings")},
                    {QStringLiteral("value"), QStringLiteral(MEO_SETTINGS_VERSION)}},
    };
    if (m_facts == nextFacts
        && m_userName == nextUserName
        && m_userAvatarSource == nextUserAvatarSource
        && m_deviceName == nextDeviceName
        && m_operatingSystemName == nextOperatingSystemName) {
        return;
    }
    m_facts = std::move(nextFacts);
    m_userName = nextUserName;
    m_userAvatarSource = nextUserAvatarSource;
    m_deviceName = nextDeviceName;
    m_operatingSystemName = nextOperatingSystemName;
    Q_EMIT changed();
}
