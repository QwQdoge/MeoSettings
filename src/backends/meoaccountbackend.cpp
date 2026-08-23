#include "meoaccountbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace
{
const QString &serviceName()
{
    static const QString value = QStringLiteral("org.meo.Accounts1");
    return value;
}

const QString &objectPath()
{
    static const QString value = QStringLiteral("/org/meo/Accounts1");
    return value;
}

const QString &interfaceName()
{
    static const QString value = QStringLiteral("org.meo.Accounts1");
    return value;
}

const QString &settingsClientId()
{
    static const QString value = QStringLiteral("org.meo.Settings");
    return value;
}

bool isSafeText(const QString &value, const int maximum)
{
    return !value.isEmpty() && value.size() <= maximum
        && std::none_of(value.cbegin(), value.cend(), [](const QChar character) {
               return character.isNull() || character.category() == QChar::Other_Control;
           });
}

QString safeText(const QVariant &value, const int maximum)
{
    const QString text = value.toString().trimmed();
    return isSafeText(text, maximum) ? text : QString();
}

QString safeAvatarUrl(const QVariant &value)
{
    const QString text = safeText(value, 2048);
    const QUrl url(text);
    // The broker's profile metadata is user-controlled remote data.  Do not
    // turn it into a local-file or arbitrary-scheme image request in QML.
    if (!url.isValid() || url.scheme() != QLatin1String("https") || url.host().isEmpty()) {
        return {};
    }
    return url.toString(QUrl::FullyEncoded);
}

bool brokerIsRunning()
{
    auto *interface = QDBusConnection::sessionBus().interface();
    if (!interface) {
        return false;
    }
    const QDBusReply<bool> reply = interface->isServiceRegistered(serviceName());
    return reply.isValid() && reply.value();
}
} // namespace

QString MeoAccountContract::safeProfileText(const QVariant &value)
{
    return safeText(value, 256);
}

QString MeoAccountContract::safeRemoteAvatarSource(const QVariant &value)
{
    return safeAvatarUrl(value);
}

MeoAccountBackend::MeoAccountBackend(QObject *parent)
    : BackendBase(parent)
{
    // Account changes are emitted by the system-owned broker.  Connecting to
    // this signal does not activate the service and refresh() below explicitly
    // refuses to start one merely to draw the Settings home page.
    QDBusConnection::sessionBus().connect(serviceName(), objectPath(),
                                          interfaceName(), QStringLiteral("accountChanged"),
                                          this, SLOT(refresh()));
    updateLauncherAvailability();
    refresh();
}

bool MeoAccountBackend::serviceRunning() const
{
    return m_serviceRunning;
}

bool MeoAccountBackend::settingsLauncherAvailable() const
{
    return !m_settingsLauncher.isEmpty();
}

bool MeoAccountBackend::signedIn() const
{
    return m_signedIn;
}

bool MeoAccountBackend::identityGranted() const
{
    return m_identityGranted;
}

bool MeoAccountBackend::oauthConfigured() const
{
    return m_oauthConfigured;
}

QString MeoAccountBackend::cloudName() const
{
    return m_cloudName;
}

QString MeoAccountBackend::cloudId() const
{
    return m_cloudId;
}

QString MeoAccountBackend::cloudAvatarSource() const
{
    return m_cloudAvatarSource;
}

QString MeoAccountBackend::summary() const
{
    if (!m_serviceRunning) {
        return settingsLauncherAvailable()
            ? tr("Meo Account is installed but not running")
            : tr("Meo Account is not installed");
    }
    if (!m_signedIn) {
        return m_oauthConfigured
            ? tr("No Meo Account is connected on this device")
            : tr("Meo Account sign-in is not configured on this device");
    }
    if (!m_cloudId.isEmpty()) {
        return tr("Connected as %1").arg(m_cloudId);
    }
    return m_cloudName.isEmpty() ? tr("Meo Account is connected")
                                 : tr("Connected as %1").arg(m_cloudName);
}

void MeoAccountBackend::refresh()
{
    updateLauncherAvailability();
    clearError();

    if (!brokerIsRunning()) {
        applyUnavailableState();
        return;
    }

    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    if (!broker.isValid()) {
        applyUnavailableState();
        setError(tr("The Meo Account service could not be reached."));
        Q_EMIT changed();
        return;
    }

    const QDBusReply<QVariantMap> statusReply = broker.call(QStringLiteral("GetStatus"));
    if (!statusReply.isValid()) {
        applyUnavailableState();
        setError(tr("The Meo Account service did not return a readable status."));
        Q_EMIT changed();
        return;
    }

    const QVariantMap status = statusReply.value();
    QVariantMap identity;
    if (status.value(QStringLiteral("signedIn")).toBool()) {
        // Profile data is deliberately requested through the broker's
        // manifest/executable gate.  A package that has not installed the
        // Settings profile manifest still shows the public status name/avatar,
        // but never invents an account ID.
        const QDBusReply<QVariantMap> identityReply = broker.call(
            QStringLiteral("GetIdentity"), settingsClientId());
        if (identityReply.isValid()) {
            identity = identityReply.value();
        }
    }
    applyStatus(status, identity);
}

bool MeoAccountBackend::openAccountSettings()
{
    updateLauncherAvailability();
    clearError();

    if (brokerIsRunning()) {
        QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
        const QDBusReply<bool> reply = broker.call(QStringLiteral("OpenSettings"));
        if (reply.isValid() && reply.value()) {
            return true;
        }
    }

    if (m_settingsLauncher.isEmpty()) {
        setError(tr("The Meo Account settings application is not installed."));
        Q_EMIT changed();
        return false;
    }
    if (!QProcess::startDetached(m_settingsLauncher, {})) {
        setError(tr("Meo Account settings could not be started."));
        Q_EMIT changed();
        return false;
    }
    return true;
}

void MeoAccountBackend::updateLauncherAvailability()
{
    const QString nextLauncher = QStandardPaths::findExecutable(QStringLiteral("meo-account-settings"));
    if (m_settingsLauncher == nextLauncher) {
        return;
    }
    m_settingsLauncher = nextLauncher;
    Q_EMIT changed();
}

void MeoAccountBackend::applyUnavailableState()
{
    const bool stateChanged = m_serviceRunning || m_signedIn || m_identityGranted || m_oauthConfigured
        || !m_cloudName.isEmpty() || !m_cloudId.isEmpty() || !m_cloudAvatarSource.isEmpty()
        || available();
    m_serviceRunning = false;
    m_signedIn = false;
    m_identityGranted = false;
    m_oauthConfigured = false;
    m_cloudName.clear();
    m_cloudId.clear();
    m_cloudAvatarSource.clear();
    setAvailable(false);
    if (stateChanged) {
        Q_EMIT changed();
    }
}

void MeoAccountBackend::applyStatus(const QVariantMap &status, const QVariantMap &identity)
{
    const bool nextSignedIn = status.value(QStringLiteral("signedIn")).toBool();
    const bool nextOauthConfigured = status.value(QStringLiteral("oauthConfigured")).toBool();
    const QString statusName = MeoAccountContract::safeProfileText(status.value(QStringLiteral("name")));
    const QString statusAvatar = MeoAccountContract::safeRemoteAvatarSource(status.value(QStringLiteral("avatarUrl")));
    const QString scopedId = MeoAccountContract::safeProfileText(identity.value(QStringLiteral("id")));
    const QString scopedName = MeoAccountContract::safeProfileText(identity.value(QStringLiteral("name")));
    const QString scopedAvatar = MeoAccountContract::safeRemoteAvatarSource(identity.value(QStringLiteral("avatarUrl")));
    const bool nextIdentityGranted = nextSignedIn && !scopedId.isEmpty();
    const QString nextName = nextSignedIn ? (!scopedName.isEmpty() ? scopedName : statusName) : QString();
    const QString nextAvatar = nextSignedIn ? (!scopedAvatar.isEmpty() ? scopedAvatar : statusAvatar) : QString();

    const bool stateChanged = !m_serviceRunning || m_signedIn != nextSignedIn
        || m_identityGranted != nextIdentityGranted || m_oauthConfigured != nextOauthConfigured
        || m_cloudName != nextName || m_cloudId != (nextIdentityGranted ? scopedId : QString())
        || m_cloudAvatarSource != nextAvatar || !available();
    m_serviceRunning = true;
    m_signedIn = nextSignedIn;
    m_identityGranted = nextIdentityGranted;
    m_oauthConfigured = nextOauthConfigured;
    m_cloudName = nextName;
    m_cloudId = nextIdentityGranted ? scopedId : QString();
    m_cloudAvatarSource = nextAvatar;
    setAvailable(true);
    if (stateChanged) {
        Q_EMIT changed();
    }
}
