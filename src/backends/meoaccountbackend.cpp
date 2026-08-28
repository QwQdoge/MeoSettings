#include "meoaccountbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
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
    QDBusConnection::sessionBus().connect(serviceName(), objectPath(),
                                          interfaceName(), QStringLiteral("requestChanged"),
                                          this, SLOT(handleRequestChanged(QString,QString,QVariantMap)));
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

bool MeoAccountBackend::busy() const { return m_busy; }
QString MeoAccountBackend::accountState() const { return m_accountState; }
qulonglong MeoAccountBackend::logoutEpoch() const { return m_logoutEpoch; }
QVariantList MeoAccountBackend::clients() const { return m_clients; }
QVariantList MeoAccountBackend::sessions() const { return m_sessions; }
bool MeoAccountBackend::mfaEnabled() const { return m_mfaEnabled; }
QString MeoAccountBackend::syncState() const { return m_syncState; }
QString MeoAccountBackend::syncError() const { return m_syncError; }
QString MeoAccountBackend::lastSyncedAt() const { return m_lastSyncedAt; }
QString MeoAccountBackend::requestState() const { return m_requestState; }

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

    setBusy(true);
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("GetAccountOverview")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantMap> reply = *completed;
                completed->deleteLater();
                setBusy(false);
                if (!reply.isValid()) {
                    applyUnavailableState();
                    setError(tr("The Meo Account service did not return a readable account overview."));
                    Q_EMIT changed();
                    return;
                }
                const QVariantMap overview = reply.value();
                m_clients = overview.value(QStringLiteral("clients")).toList();
                m_sessions = overview.value(QStringLiteral("sessions")).toList();
                m_mfaEnabled = overview.value(QStringLiteral("mfaEnabled")).toBool();
                m_syncState = safeText(overview.value(QStringLiteral("syncState")), 64);
                m_syncError = safeText(overview.value(QStringLiteral("syncError")), 512);
                m_lastSyncedAt = safeText(overview.value(QStringLiteral("lastSyncedAt")), 64);
                m_logoutEpoch = overview.value(QStringLiteral("logoutEpoch")).toULongLong();
                applyStatus(overview, overview.value(QStringLiteral("identity")).toMap());
                Q_EMIT changed();
            });
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

void MeoAccountBackend::requestAuthentication(const QString &mode)
{
    clearError();
    setBusy(true);
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("RequestAuthentication"), settingsClientId(), mode,
                         QVariantMap{}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    setBusy(false);
                    m_signOutAfterReauth = false;
                    m_clientToRevokeAfterReauth.clear();
                    setError(tr("The Meo Account authentication dialog could not be opened."));
                    Q_EMIT changed();
                    return;
                }
                m_activeRequestId = reply.value();
                m_requestState = QStringLiteral("waiting_for_user");
                Q_EMIT changed();
            });
}

void MeoAccountBackend::openHostedAction(const QString &action)
{
    clearError();
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    const QDBusReply<bool> reply = broker.call(QStringLiteral("OpenHostedAction"), action);
    if (!reply.isValid() || !reply.value()) {
        setError(tr("The requested Meo Account page could not be opened."));
        Q_EMIT changed();
    }
}

void MeoAccountBackend::signOutAll()
{
    if (!m_signedIn || m_busy) return;
    m_clientToRevokeAfterReauth.clear();
    m_signOutAfterReauth = true;
    requestAuthentication(QStringLiteral("reauthenticate"));
}

void MeoAccountBackend::revokeClient(const QString &clientId)
{
    if (!m_signedIn || m_busy || clientId.isEmpty() || clientId.size() > 128) return;
    m_signOutAfterReauth = false;
    m_clientToRevokeAfterReauth = clientId;
    requestAuthentication(QStringLiteral("reauthenticate"));
}

void MeoAccountBackend::startSignOutOperation()
{
    clearError();
    setBusy(true);
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartAccountOperation"),
                         QStringLiteral("sign_out_all"), QVariantMap{}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    setBusy(false);
                    setError(tr("Meo Account could not sign out all applications on this device."));
                    Q_EMIT changed();
                    return;
                }
                m_activeRequestId = reply.value();
            });
}

void MeoAccountBackend::startClientRevocation(const QString &clientId)
{
    clearError();
    setBusy(true);
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartAccountOperation"),
                         QStringLiteral("revoke_client"),
                         QVariantMap{{QStringLiteral("clientId"), clientId}}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    setBusy(false);
                    setError(tr("The Meo application authorization could not be revoked."));
                    Q_EMIT changed();
                    return;
                }
                m_activeRequestId = reply.value();
            });
}

void MeoAccountBackend::handleRequestChanged(const QString &requestId, const QString &state,
                                             const QVariantMap &result)
{
    if (result.value(QStringLiteral("clientId")).toString() != settingsClientId()) return;
    if (!m_activeRequestId.isEmpty() && requestId != m_activeRequestId) return;
    m_activeRequestId = requestId;
    m_requestState = state;
    const QString requestError = result.value(QStringLiteral("error")).toString();
    if (!requestError.isEmpty()) setError(requestError);
    const bool terminal = state == QStringLiteral("completed")
        || state == QStringLiteral("failed") || state == QStringLiteral("denied")
        || state == QStringLiteral("expired");
    if (terminal) setBusy(false);
    if (state == QStringLiteral("completed")) {
        if (m_signOutAfterReauth) {
            m_signOutAfterReauth = false;
            QMetaObject::invokeMethod(this, &MeoAccountBackend::startSignOutOperation,
                                      Qt::QueuedConnection);
        } else if (!m_clientToRevokeAfterReauth.isEmpty()) {
            const QString clientId = m_clientToRevokeAfterReauth;
            m_clientToRevokeAfterReauth.clear();
            QMetaObject::invokeMethod(this, [this, clientId] {
                startClientRevocation(clientId);
            }, Qt::QueuedConnection);
        } else {
            refresh();
        }
    } else if (terminal) {
        m_signOutAfterReauth = false;
        m_clientToRevokeAfterReauth.clear();
    }
    Q_EMIT changed();
}

void MeoAccountBackend::setBusy(const bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    Q_EMIT changed();
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
        || available() || m_accountState != QStringLiteral("unavailable") || !m_clients.isEmpty()
        || !m_sessions.isEmpty() || m_syncState != QStringLiteral("not_loaded")
        || !m_syncError.isEmpty() || !m_lastSyncedAt.isEmpty() || m_mfaEnabled;
    m_serviceRunning = false;
    m_signedIn = false;
    m_identityGranted = false;
    m_oauthConfigured = false;
    m_accountState = QStringLiteral("unavailable");
    m_logoutEpoch = 0;
    m_clients.clear();
    m_sessions.clear();
    m_mfaEnabled = false;
    m_syncState = QStringLiteral("not_loaded");
    m_syncError.clear();
    m_lastSyncedAt.clear();
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
    const QString nextState = safeText(status.value(QStringLiteral("state")), 64);
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
        || m_cloudAvatarSource != nextAvatar || m_accountState != nextState || !available();
    m_serviceRunning = true;
    m_signedIn = nextSignedIn;
    m_identityGranted = nextIdentityGranted;
    m_oauthConfigured = nextOauthConfigured;
    m_accountState = nextState.isEmpty()
        ? (nextSignedIn ? QStringLiteral("signed_in") : QStringLiteral("signed_out")) : nextState;
    m_cloudName = nextName;
    m_cloudId = nextIdentityGranted ? scopedId : QString();
    m_cloudAvatarSource = nextAvatar;
    setAvailable(true);
    if (stateChanged) {
        Q_EMIT changed();
    }
}
