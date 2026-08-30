#include "meoaccountbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <utility>

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
bool MeoAccountBackend::aiBusy() const { return m_aiBusy; }
QString MeoAccountBackend::aiState() const { return m_aiState; }
QVariantList MeoAccountBackend::aiCredentials() const { return m_aiCredentials; }
QVariantMap MeoAccountBackend::aiConsent() const { return m_aiConsent; }
QString MeoAccountBackend::aiImageSource() const { return m_aiImageSource; }
QString MeoAccountBackend::aiTargetDesktopId() const { return m_aiTargetDesktopId; }
QString MeoAccountBackend::aiTargetApplicationName() const { return m_aiTargetApplicationName; }

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

void MeoAccountBackend::refreshAiCredentials()
{
    if (!m_signedIn || m_aiBusy) return;
    startAiOperation(QStringLiteral("list_ai_credentials"), {},
                     QStringLiteral("loading_credentials"));
}

void MeoAccountBackend::prepareIconImage(const QString &desktopId,
                                         const QString &applicationName,
                                         const QString &credentialId,
                                         const QString &model,
                                         const QString &prompt)
{
    if (!m_signedIn || m_aiBusy) return;
    const QString cleanDesktopId = desktopId.trimmed();
    const QString cleanName = applicationName.trimmed();
    const QString cleanCredential = credentialId.trimmed();
    const QString cleanModel = model.trimmed();
    const QString cleanPrompt = prompt.trimmed();
    if (!isSafeText(cleanDesktopId, 512) || !isSafeText(cleanName, 256)
        || QUuid(cleanCredential).isNull() || !isSafeText(cleanModel, 160)
        || !isSafeText(cleanPrompt, 4000)) {
        setError(tr("Choose one application, an AI connection, a model, and a valid prompt."));
        Q_EMIT changed();
        return;
    }
    m_aiBatchItems.clear();
    m_aiBatchPrepared.clear();
    m_aiBatchIndex = -1;
    m_aiBatchPreparing = false;
    m_aiBatchGenerating = false;
    m_aiBatchDenying = false;
    m_aiTargetDesktopId = cleanDesktopId;
    m_aiTargetApplicationName = cleanName;
    m_aiImageSource.clear();
    m_aiConsent.clear();
    m_pendingAiArguments = iconImageArguments(cleanDesktopId, cleanName, cleanCredential,
                                              cleanModel, cleanPrompt);
    startAiOperation(QStringLiteral("prepare_ai_image"), m_pendingAiArguments,
                     QStringLiteral("preparing_consent"));
}

QVariantMap MeoAccountBackend::iconImageArguments(const QString &desktopId,
                                                  const QString &applicationName,
                                                  const QString &credentialId,
                                                  const QString &model,
                                                  const QString &prompt) const
{
    const QString requestPrompt = QStringLiteral(
        "%1\n\nCreate exactly one desktop launcher icon for %2 (%3). Preserve its recognizable "
        "brand silhouette, internal cuts, negative spaces, and key visual features. Use one "
        "centered Pixel / Material You container with a unified wallpaper-derived Monet palette. "
        "Use at most three foreground tonal layers and only mild Easel-like paper, crayon, or "
        "watercolor texture. Keep the mark readable at 48 px. Transparent canvas. No words, "
        "watermark, device mockup, screenshot, perspective, extra logo, second badge, or second "
        "background plate.")
                                      .arg(prompt, applicationName, desktopId);
    return {
        {QStringLiteral("credentialId"), credentialId},
        {QStringLiteral("model"), model},
        {QStringLiteral("userPrompt"), requestPrompt},
        {QStringLiteral("imageSize"), QStringLiteral("1024x1024")},
        {QStringLiteral("imageQuality"), QStringLiteral("provider_default")},
        {QStringLiteral("imageBackground"), QStringLiteral("transparent")},
    };
}

void MeoAccountBackend::prepareIconImageBatch(const QVariantList &applications,
                                              const QString &credentialId,
                                              const QString &model,
                                              const QString &prompt)
{
    if (!m_signedIn || m_aiBusy) return;
    const QString cleanCredential = credentialId.trimmed();
    const QString cleanModel = model.trimmed();
    const QString cleanPrompt = prompt.trimmed();
    if (applications.isEmpty() || applications.size() > 128
        || QUuid(cleanCredential).isNull() || !isSafeText(cleanModel, 160)
        || !isSafeText(cleanPrompt, 4000)) {
        setError(tr("Choose 1 to 128 applications, an AI connection, a model, and a valid prompt."));
        Q_EMIT changed();
        return;
    }
    QVariantList cleanApplications;
    QSet<QString> ids;
    for (const QVariant &value : applications) {
        const QVariantMap item = value.toMap();
        const QString id = item.value(QStringLiteral("desktopId")).toString().trimmed();
        const QString name = item.value(QStringLiteral("name")).toString().trimmed();
        if (!isSafeText(id, 512) || !isSafeText(name, 256) || ids.contains(id)) {
            setError(tr("The AI icon batch contains an invalid or duplicate application."));
            Q_EMIT changed();
            return;
        }
        ids.insert(id);
        cleanApplications.append(QVariantMap{
            {QStringLiteral("desktopId"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("credentialId"), cleanCredential},
            {QStringLiteral("model"), cleanModel},
            {QStringLiteral("prompt"), cleanPrompt},
        });
    }
    clearError();
    m_aiBatchItems = cleanApplications;
    m_aiBatchPrepared.clear();
    m_aiBatchIndex = 0;
    m_aiBatchPreparing = true;
    m_aiBatchGenerating = false;
    m_aiBatchDenying = false;
    m_aiConsent.clear();
    m_aiImageSource.clear();
    prepareNextIconImageBatchItem();
}

void MeoAccountBackend::prepareNextIconImageBatchItem()
{
    if (!m_aiBatchPreparing || m_aiBusy || m_aiBatchIndex < 0
        || m_aiBatchIndex >= m_aiBatchItems.size()) return;
    const QVariantMap item = m_aiBatchItems.at(m_aiBatchIndex).toMap();
    m_aiTargetDesktopId = item.value(QStringLiteral("desktopId")).toString();
    m_aiTargetApplicationName = item.value(QStringLiteral("name")).toString();
    m_pendingAiArguments = iconImageArguments(
        m_aiTargetDesktopId, m_aiTargetApplicationName,
        item.value(QStringLiteral("credentialId")).toString(),
        item.value(QStringLiteral("model")).toString(),
        item.value(QStringLiteral("prompt")).toString());
    startAiOperation(QStringLiteral("prepare_ai_image"), m_pendingAiArguments,
                     QStringLiteral("preparing_batch_consent"));
}

void MeoAccountBackend::startPreparedIconImageBatchItem(const QString &action,
                                                        const QString &state)
{
    if (m_aiBusy || m_aiBatchIndex < 0 || m_aiBatchIndex >= m_aiBatchPrepared.size()) return;
    const QVariantMap item = m_aiBatchPrepared.at(m_aiBatchIndex).toMap();
    m_aiTargetDesktopId = item.value(QStringLiteral("desktopId")).toString();
    m_aiTargetApplicationName = item.value(QStringLiteral("name")).toString();
    m_pendingAiArguments = item.value(QStringLiteral("arguments")).toMap();
    m_aiConsent = item.value(QStringLiteral("consent")).toMap();
    QVariantMap arguments = m_pendingAiArguments;
    arguments.insert(QStringLiteral("consent"), m_aiConsent);
    startAiOperation(action, arguments, state);
}

void MeoAccountBackend::generatePreparedIconImageBatch()
{
    if (m_aiBusy || m_aiBatchPreparing || m_aiBatchPrepared.isEmpty()
        || m_aiState != QLatin1String("batch_consent_ready")) return;
    m_aiBatchGenerating = true;
    m_aiBatchDenying = false;
    m_aiBatchIndex = 0;
    startPreparedIconImageBatchItem(QStringLiteral("generate_ai_image"),
                                    QStringLiteral("generating_batch"));
}

void MeoAccountBackend::continuePreparedIconImageBatch()
{
    if (m_aiBusy || !m_aiBatchGenerating
        || m_aiState != QLatin1String("batch_image_ready")) return;
    m_aiImageSource.clear();
    ++m_aiBatchIndex;
    if (m_aiBatchIndex >= m_aiBatchPrepared.size()) {
        m_aiBatchGenerating = false;
        m_aiState = QStringLiteral("batch_ready");
        m_aiConsent.clear();
        m_pendingAiArguments.clear();
        Q_EMIT changed();
        return;
    }
    startPreparedIconImageBatchItem(QStringLiteral("generate_ai_image"),
                                    QStringLiteral("generating_batch"));
}

void MeoAccountBackend::denyPreparedIconImageBatch()
{
    if (m_aiBusy || m_aiBatchPrepared.isEmpty()) return;
    m_aiBatchPreparing = false;
    m_aiBatchGenerating = false;
    m_aiBatchDenying = true;
    m_aiBatchIndex = 0;
    startPreparedIconImageBatchItem(QStringLiteral("deny_ai_image"),
                                    QStringLiteral("denying_batch"));
}

void MeoAccountBackend::generatePreparedIconImage()
{
    if (m_aiBusy || m_pendingAiArguments.isEmpty() || m_aiConsent.isEmpty()) return;
    QVariantMap arguments = m_pendingAiArguments;
    arguments.insert(QStringLiteral("consent"), m_aiConsent);
    startAiOperation(QStringLiteral("generate_ai_image"), arguments,
                     QStringLiteral("generating"));
}

void MeoAccountBackend::denyPreparedIconImage()
{
    if (m_aiBusy || m_pendingAiArguments.isEmpty() || m_aiConsent.isEmpty()) return;
    QVariantMap arguments = m_pendingAiArguments;
    arguments.insert(QStringLiteral("consent"), m_aiConsent);
    startAiOperation(QStringLiteral("deny_ai_image"), arguments,
                     QStringLiteral("denying"));
}

void MeoAccountBackend::clearGeneratedIconImage()
{
    if (m_aiBusy) return;
    m_aiState = QStringLiteral("idle");
    m_aiImageSource.clear();
    m_aiTargetDesktopId.clear();
    m_aiTargetApplicationName.clear();
    m_aiConsent.clear();
    m_pendingAiArguments.clear();
    m_aiBatchItems.clear();
    m_aiBatchPrepared.clear();
    m_aiBatchIndex = -1;
    m_aiBatchPreparing = false;
    m_aiBatchGenerating = false;
    m_aiBatchDenying = false;
    Q_EMIT changed();
}

void MeoAccountBackend::startAiOperation(const QString &action,
                                         const QVariantMap &arguments,
                                         const QString &state)
{
    clearError();
    m_aiBusy = true;
    m_aiState = state;
    Q_EMIT changed();
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartAccountOperation"), action, arguments), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    m_aiBusy = false;
                    m_aiState = QStringLiteral("failed");
                    setError(tr("The Meo Account AI broker could not start this request."));
                    Q_EMIT changed();
                    return;
                }
                m_activeAiRequestId = reply.value();
            });
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
    const QString operation = result.value(QStringLiteral("operation")).toString();
    if ((!m_activeAiRequestId.isEmpty() && requestId == m_activeAiRequestId)
        || (m_aiBusy && operation.startsWith(QStringLiteral("ai_")))) {
        if (m_activeAiRequestId.isEmpty()) m_activeAiRequestId = requestId;
        m_aiState = state;
        const QString requestError = result.value(QStringLiteral("error")).toString();
        if (!requestError.isEmpty()) setError(requestError);
        if (operation == QStringLiteral("ai_credentials")) {
            m_aiCredentials = result.value(QStringLiteral("credentials")).toList();
            m_aiState = QStringLiteral("credentials_ready");
        } else if (operation == QStringLiteral("ai_image_consent")) {
            const QVariantMap consent = result.value(QStringLiteral("consent")).toMap();
            if (m_aiBatchPreparing) {
                m_aiBatchPrepared.append(QVariantMap{
                    {QStringLiteral("desktopId"), m_aiTargetDesktopId},
                    {QStringLiteral("name"), m_aiTargetApplicationName},
                    {QStringLiteral("arguments"), m_pendingAiArguments},
                    {QStringLiteral("consent"), consent},
                });
                m_aiConsent.clear();
                m_aiState = QStringLiteral("preparing_batch_consent");
            } else {
                m_aiConsent = consent;
                m_aiState = QStringLiteral("consent_ready");
            }
        } else if (operation == QStringLiteral("ai_image_generated")) {
            m_aiImageSource = result.value(QStringLiteral("imageSource")).toString();
            if (m_aiImageSource.startsWith(QStringLiteral("data:image/"))) {
                m_aiState = m_aiBatchGenerating
                    ? QStringLiteral("batch_image_ready") : QStringLiteral("image_ready");
            } else {
                m_aiState = QStringLiteral("failed");
            }
        } else if (operation == QStringLiteral("ai_image_denied")) {
            if (!m_aiBatchDenying) {
                m_aiConsent.clear();
                m_aiState = QStringLiteral("denied");
            }
        }
        const bool terminal = state == QStringLiteral("completed")
            || state == QStringLiteral("failed") || state == QStringLiteral("denied")
            || state == QStringLiteral("expired");
        if (terminal) {
            m_aiBusy = false;
            m_activeAiRequestId.clear();
        }
        if (terminal && m_aiBatchPreparing && operation == QLatin1String("ai_image_consent")) {
            if (state != QLatin1String("completed")) {
                m_aiBatchPreparing = false;
                m_aiState = QStringLiteral("failed");
            } else {
                ++m_aiBatchIndex;
                if (m_aiBatchIndex < m_aiBatchItems.size()) {
                    QMetaObject::invokeMethod(this, &MeoAccountBackend::prepareNextIconImageBatchItem,
                                              Qt::QueuedConnection);
                } else {
                    m_aiBatchPreparing = false;
                    const QVariantMap first = m_aiBatchPrepared.first().toMap()
                                                  .value(QStringLiteral("consent")).toMap();
                    QVariantList names;
                    qsizetype promptCharacters = 0;
                    for (const QVariant &value : std::as_const(m_aiBatchPrepared)) {
                        const QVariantMap item = value.toMap();
                        names.append(item.value(QStringLiteral("name")));
                        promptCharacters += item.value(QStringLiteral("consent")).toMap()
                                                .value(QStringLiteral("promptCharacters")).toLongLong();
                    }
                    m_aiConsent = first;
                    m_aiConsent.insert(QStringLiteral("requestId"),
                                       QStringLiteral("batch:%1").arg(
                                           QUuid::createUuid().toString(QUuid::WithoutBraces)));
                    m_aiConsent.insert(QStringLiteral("itemCount"), m_aiBatchPrepared.size());
                    m_aiConsent.insert(QStringLiteral("applications"), names);
                    m_aiConsent.insert(QStringLiteral("promptCharacters"), promptCharacters);
                    m_aiConsent.insert(QStringLiteral("stylePack"), QStringLiteral("easel-monet"));
                    m_aiState = QStringLiteral("batch_consent_ready");
                }
            }
        } else if (terminal && m_aiBatchDenying
                   && operation == QLatin1String("ai_image_denied")) {
            ++m_aiBatchIndex;
            if (m_aiBatchIndex < m_aiBatchPrepared.size()) {
                QMetaObject::invokeMethod(this, [this] {
                    startPreparedIconImageBatchItem(QStringLiteral("deny_ai_image"),
                                                    QStringLiteral("denying_batch"));
                }, Qt::QueuedConnection);
            } else {
                m_aiBatchDenying = false;
                m_aiBatchPrepared.clear();
                m_aiBatchItems.clear();
                m_aiBatchIndex = -1;
                m_aiConsent.clear();
                m_pendingAiArguments.clear();
                m_aiState = QStringLiteral("denied");
            }
        }
        Q_EMIT changed();
        return;
    }
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
