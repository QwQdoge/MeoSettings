#include "meoaccountbackend.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
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

QString safeText(const QVariant &value, int maximum);

bool validAiIconPackDesktopId(const QString &value)
{
    static const QRegularExpression identifier(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,511}$"));
    return identifier.match(value).hasMatch();
}

bool validSha256(const QString &value)
{
    static const QRegularExpression hash(QStringLiteral("^[a-f0-9]{64}$"));
    return hash.match(value).hasMatch();
}

bool validAiIconPackStyleId(const QString &value)
{
    static const QRegularExpression identifier(
        QStringLiteral("^[a-z0-9][a-z0-9-]{0,79}$"));
    return identifier.match(value).hasMatch();
}

bool validAiIconPackJobId(const QString &value)
{
    const QUuid id(value);
    return !id.isNull()
        && id.toString(QUuid::WithoutBraces) == value.trimmed().toLower();
}

bool isAiIconPackOperation(const QString &operation)
{
    return operation == QLatin1String("ai_icon_styles")
        || operation.startsWith(QStringLiteral("ai_icon_pack_"));
}

bool isPrivateAiIconPackManifestPath(const QString &value, const QString &jobId)
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty() || !validAiIconPackJobId(jobId) || !QDir::isAbsolutePath(value)
        || value.size() > 4096) {
        return false;
    }
    const QString root = QDir::cleanPath(QDir(runtime).absoluteFilePath(
        QStringLiteral("meo-account/icon-packs")));
    const QString path = QDir::cleanPath(value);
    if (!path.startsWith(root + QLatin1Char('/'))
        || QFileInfo(path).fileName() != QLatin1String("pack.json")) {
        return false;
    }
    return QDir(root).relativeFilePath(path)
        == jobId + QStringLiteral("/pack.json");
}

QVariantMap safeAiIconPackConsent(const QVariantMap &value)
{
    const QString requestId = value.value(QStringLiteral("requestId")).toString().trimmed();
    const QString payloadSha256 = value.value(QStringLiteral("payloadSha256")).toString();
    const QString styleId = value.value(QStringLiteral("styleId")).toString().trimmed();
    const QString recipe = safeText(value.value(QStringLiteral("promptRecipeVersion")), 128);
    const QString shape = value.value(QStringLiteral("shape")).toString().trimmed().toLower();
    const int itemCount = value.value(QStringLiteral("itemCount")).toInt();
    if (!validAiIconPackJobId(requestId) || !validSha256(payloadSha256)
        || value.value(QStringLiteral("confirmationVersion")).toInt() != 1
        || !validAiIconPackStyleId(styleId)
        || !QSet<QString>{QStringLiteral("circle"), QStringLiteral("pixel"),
                          QStringLiteral("squircle"), QStringLiteral("rounded")}.contains(shape)
        || recipe.isEmpty() || itemCount < 1 || itemCount > 128) {
        return {};
    }

    QVariantList categories;
    const QVariantList submittedCategories = value.value(QStringLiteral("dataCategories")).toList();
    for (const QVariant &category : submittedCategories) {
        const QString text = safeText(category, 128);
        if (text.isEmpty() || categories.contains(text)) return {};
        categories.append(text);
    }
    if (categories.isEmpty()) return {};

    return {
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("payloadSha256"), payloadSha256},
        {QStringLiteral("confirmationVersion"), 1},
        {QStringLiteral("expiresAt"), safeText(value.value(QStringLiteral("expiresAt")), 64)},
        {QStringLiteral("provider"), safeText(value.value(QStringLiteral("provider")), 128)},
        {QStringLiteral("providerName"), safeText(value.value(QStringLiteral("providerName")), 128)},
        {QStringLiteral("destination"), safeText(value.value(QStringLiteral("destination")), 256)},
        {QStringLiteral("model"), safeText(value.value(QStringLiteral("model")), 160)},
        {QStringLiteral("clientId"), safeText(value.value(QStringLiteral("clientId")), 128)},
        {QStringLiteral("purpose"), safeText(value.value(QStringLiteral("purpose")), 240)},
        {QStringLiteral("dataCategories"), categories},
        {QStringLiteral("operation"), safeText(value.value(QStringLiteral("operation")), 128)},
        {QStringLiteral("styleId"), styleId},
        {QStringLiteral("promptRecipeVersion"), recipe},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("itemCount"), itemCount},
    };
}

QVariantMap safeAiIconPackSummary(const QVariantMap &value)
{
    QVariantMap summary;
    const QString jobId = value.value(QStringLiteral("jobId")).toString().trimmed();
    if (validAiIconPackJobId(jobId)) summary.insert(QStringLiteral("jobId"), jobId);
    const QString styleId = value.value(QStringLiteral("styleId")).toString().trimmed();
    if (validAiIconPackStyleId(styleId)) summary.insert(QStringLiteral("styleId"), styleId);
    const QString recipe = safeText(value.value(QStringLiteral("promptRecipeVersion")), 128);
    if (!recipe.isEmpty()) summary.insert(QStringLiteral("promptRecipeVersion"), recipe);
    const QString shape = value.value(QStringLiteral("shape")).toString().trimmed().toLower();
    if (QSet<QString>{QStringLiteral("circle"), QStringLiteral("pixel"),
                      QStringLiteral("squircle"), QStringLiteral("rounded")}.contains(shape)) {
        summary.insert(QStringLiteral("shape"), shape);
    }
    const QString provider = safeText(value.value(QStringLiteral("provider")), 128);
    if (!provider.isEmpty()) summary.insert(QStringLiteral("provider"), provider);
    const QString model = safeText(value.value(QStringLiteral("model")), 160);
    if (!model.isEmpty()) summary.insert(QStringLiteral("model"), model);
    const int itemCount = value.value(QStringLiteral("itemCount")).toInt();
    if (itemCount >= 1 && itemCount <= 128) summary.insert(QStringLiteral("itemCount"), itemCount);
    const QString expiresAt = safeText(value.value(QStringLiteral("expiresAt")), 64);
    if (!expiresAt.isEmpty()) summary.insert(QStringLiteral("expiresAt"), expiresAt);
    const QString manifestSha256 = value.value(QStringLiteral("manifestSha256")).toString();
    if (validSha256(manifestSha256))
        summary.insert(QStringLiteral("manifestSha256"), manifestSha256);
    return summary;
}

QVariantList safeAiIconStyleCatalog(const QVariantList &value)
{
    if (value.isEmpty() || value.size() > 32) return {};
    QSet<QString> seen;
    QList<QVariantMap> styles;
    styles.reserve(value.size());
    for (const QVariant &entry : value) {
        const QVariantMap style = entry.toMap();
        const QString id = style.value(QStringLiteral("id")).toString().trimmed();
        const QString recipeVersion = safeText(style.value(QStringLiteral("recipeVersion")), 128);
        const QString displayName = safeText(style.value(QStringLiteral("displayName")), 128);
        const QString description = safeText(style.value(QStringLiteral("description")), 512);
        if (!validAiIconPackStyleId(id) || recipeVersion.isEmpty() || displayName.isEmpty()
            || description.isEmpty() || seen.contains(id)) {
            return {};
        }
        seen.insert(id);
        const bool available = style.value(QStringLiteral("available"), true).toBool();
        QVariantMap safe{{QStringLiteral("id"), id},
                         {QStringLiteral("recipeVersion"), recipeVersion},
                         {QStringLiteral("displayName"), displayName},
                         {QStringLiteral("description"), description},
                         {QStringLiteral("available"), available}};
        const QString unavailableReason = safeText(style.value(QStringLiteral("unavailableReason")), 256);
        if (!unavailableReason.isEmpty())
            safe.insert(QStringLiteral("unavailableReason"), unavailableReason);
        styles.append(safe);
    }
    std::sort(styles.begin(), styles.end(), [](const QVariantMap &left, const QVariantMap &right) {
        return left.value(QStringLiteral("id")).toString()
            < right.value(QStringLiteral("id")).toString();
    });
    QVariantList result;
    result.reserve(styles.size());
    for (const QVariantMap &style : std::as_const(styles)) result.append(style);
    return result;
}

bool aiIconPackOperationIsExpected(const QString &action, const QString &operation)
{
    if (operation == QLatin1String("ai_icon_pack_request_started")
        || operation == QLatin1String("ai_icon_pack_failed")
        || operation == QLatin1String("ai_icon_pack_expired")) {
        return true;
    }
    if (action == QLatin1String("list_ai_icon_styles"))
        return operation == QLatin1String("ai_icon_styles");
    if (action == QLatin1String("prepare_ai_icon_material_pack"))
        return operation == QLatin1String("ai_icon_pack_consent");
    if (action == QLatin1String("generate_ai_icon_material_pack"))
        return operation == QLatin1String("ai_icon_pack_staging")
            || operation == QLatin1String("ai_icon_pack_ready");
    if (action == QLatin1String("deny_ai_icon_material_pack"))
        return operation == QLatin1String("ai_icon_pack_denied");
    if (action == QLatin1String("get_ai_icon_material_pack_status"))
        return operation == QLatin1String("ai_icon_pack_status");
    if (action == QLatin1String("cancel_ai_icon_material_pack"))
        return operation == QLatin1String("ai_icon_pack_cancelled");
    if (action == QLatin1String("release_ai_icon_material_pack"))
        return operation == QLatin1String("ai_icon_pack_released");
    return false;
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

QVariantMap MeoAccountContract::normalizedAiIconPackRequest(const QVariantMap &value)
{
    bool versionOk = false;
    const int contractVersion = value.value(QStringLiteral("contractVersion")).toInt(&versionOk);
    const QString styleId = value.value(QStringLiteral("styleId")).toString().trimmed();
    const QString shape = value.value(QStringLiteral("shape")).toString().trimmed().toLower();
    static const QSet<QString> supportedShapes{
        QStringLiteral("circle"), QStringLiteral("pixel"),
        QStringLiteral("squircle"), QStringLiteral("rounded"),
    };
    if (value.value(QStringLiteral("schema")).toString()
            != QLatin1String("org.meo.ai-icon-pack-request/v1")
        || !versionOk || contractVersion != 1 || !validAiIconPackStyleId(styleId)
        || !supportedShapes.contains(shape)) {
        return {};
    }

    const QVariantList submittedItems = value.value(QStringLiteral("items")).toList();
    if (submittedItems.isEmpty() || submittedItems.size() > 128) return {};

    QSet<QString> seenDesktopIds;
    QList<QVariantMap> items;
    items.reserve(submittedItems.size());
    for (const QVariant &submittedItem : submittedItems) {
        const QVariantMap item = submittedItem.toMap();
        const QString desktopId = item.value(QStringLiteral("desktopId")).toString().trimmed();
        const QString sourceIconHash = item.value(QStringLiteral("sourceIconHash")).toString();
        if (!validAiIconPackDesktopId(desktopId) || !validSha256(sourceIconHash)
            || seenDesktopIds.contains(desktopId)) {
            return {};
        }
        seenDesktopIds.insert(desktopId);
        items.append({{QStringLiteral("desktopId"), desktopId},
                      {QStringLiteral("sourceIconHash"), sourceIconHash}});
    }
    std::sort(items.begin(), items.end(), [](const QVariantMap &left, const QVariantMap &right) {
        return left.value(QStringLiteral("desktopId")).toString()
            < right.value(QStringLiteral("desktopId")).toString();
    });

    QVariantList canonicalItems;
    canonicalItems.reserve(items.size());
    for (const QVariantMap &item : std::as_const(items)) canonicalItems.append(item);
    return {
        {QStringLiteral("schema"), QStringLiteral("org.meo.ai-icon-pack-request/v1")},
        {QStringLiteral("contractVersion"), 1},
        {QStringLiteral("styleId"), styleId},
        {QStringLiteral("shape"), shape},
        {QStringLiteral("items"), canonicalItems},
    };
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
QVariantList MeoAccountBackend::localAiConnections() const { return m_localAiConnections; }
QVariantList MeoAccountBackend::localAiConsumers() const { return m_localAiConsumers; }
bool MeoAccountBackend::localAiDiscoveryBusy() const { return m_localAiDiscoveryBusy; }
QVariantMap MeoAccountBackend::aiConsent() const { return m_aiConsent; }
QString MeoAccountBackend::aiImageSource() const { return m_aiImageSource; }
QString MeoAccountBackend::aiTargetDesktopId() const { return m_aiTargetDesktopId; }
QString MeoAccountBackend::aiTargetApplicationName() const { return m_aiTargetApplicationName; }
QVariantList MeoAccountBackend::aiIconStyles() const { return m_aiIconStyles; }
bool MeoAccountBackend::aiIconPackBusy() const { return m_aiIconPackBusy; }
QString MeoAccountBackend::aiIconPackState() const { return m_aiIconPackState; }
QVariantMap MeoAccountBackend::aiIconPackConsent() const { return m_aiIconPackConsent; }
QString MeoAccountBackend::aiIconPackJobId() const { return m_aiIconPackJobId; }
QString MeoAccountBackend::aiIconPackManifestPath() const { return m_aiIconPackManifestPath; }
QString MeoAccountBackend::aiIconPackManifestSha256() const { return m_aiIconPackManifestSha256; }
QVariantMap MeoAccountBackend::aiIconPackSummary() const { return m_aiIconPackSummary; }

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
                refreshLocalAiConnections();
                Q_EMIT changed();
            });
}

void MeoAccountBackend::refreshLocalAiConnections()
{
    if (!brokerIsRunning()) {
        m_localAiConnections.clear();
        m_localAiConsumers.clear();
        Q_EMIT changed();
        return;
    }
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("ListLocalAiConnections")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantList> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid()) {
                    setError(tr("The local AI connection store could not be read."));
                    Q_EMIT changed();
                    return;
                }
                m_localAiConnections = reply.value();
                Q_EMIT changed();
            });
    auto *consumerWatcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("ListLocalAiConsumers")), this);
    connect(consumerWatcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantList> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid()) {
                    m_localAiConsumers.clear();
                    Q_EMIT changed();
                    return;
                }
                m_localAiConsumers = reply.value();
                Q_EMIT changed();
            });
}

void MeoAccountBackend::saveLocalAiConnection(const QString &id,
                                              const QString &provider,
                                              const QString &displayName,
                                              const QString &endpoint,
                                              const QString &defaultModel,
                                              const QString &apiKey)
{
    clearError();
    if (!brokerIsRunning()) {
        setError(tr("Meo Account must be running to access the password wallet."));
        Q_EMIT changed();
        return;
    }
    const QVariantMap connection{{QStringLiteral("id"), id},
                                 {QStringLiteral("provider"), provider},
                                 {QStringLiteral("displayName"), displayName},
                                 {QStringLiteral("endpoint"), endpoint},
                                 {QStringLiteral("defaultModel"), defaultModel},
                                 {QStringLiteral("enabled"), true}};
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("UpsertLocalAiConnection"), connection, apiKey), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantMap> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    const QString message = tr("The local AI connection could not be saved to the password wallet.");
                    setError(message);
                    Q_EMIT localAiConnectionSaveFailed(message);
                    Q_EMIT changed();
                    return;
                }
                Q_EMIT localAiConnectionSaved(reply.value().value(QStringLiteral("id")).toString());
                refreshLocalAiConnections();
            });
}

void MeoAccountBackend::discoverLocalAiModels(const QString &id)
{
    clearError();
    if (!brokerIsRunning() || QUuid(id).isNull() || m_localAiDiscoveryBusy) {
        const QString message = tr("Save the device AI connection before detecting models.");
        setError(message);
        Q_EMIT changed();
        return;
    }
    m_localAiDiscoveryBusy = true;
    m_localAiDiscoveryConnectionId = id;
    m_activeLocalAiRequestId.clear();
    Q_EMIT changed();
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartLocalAiOperation"), settingsClientId(),
                         QStringLiteral("discover_models"),
                         QVariantMap{{QStringLiteral("connectionId"), id}}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || reply.value().isEmpty()) {
                    m_localAiDiscoveryBusy = false;
                    m_localAiDiscoveryConnectionId.clear();
                    setError(tr("The AI model catalog could not be requested."));
                    Q_EMIT changed();
                    return;
                }
                m_activeLocalAiRequestId = reply.value();
            });
}

void MeoAccountBackend::removeLocalAiConnection(const QString &id)
{
    clearError();
    if (!brokerIsRunning()) return;
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("RemoveLocalAiConnection"), id), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<bool> reply = *completed;
                completed->deleteLater();
                if (!reply.isValid() || !reply.value()) {
                    setError(tr("The local AI connection could not be removed."));
                    Q_EMIT changed();
                    return;
                }
                refreshLocalAiConnections();
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

void MeoAccountBackend::refreshAiIconStyles()
{
    if (!m_signedIn || m_aiBusy || m_aiIconPackBusy || !m_aiIconPackJobId.isEmpty()) {
        if (!m_signedIn) setError(tr("Connect Meo Account before loading AI icon styles."));
        else if (!m_aiIconPackJobId.isEmpty())
            setError(tr("Finish, cancel, or release the current AI icon pack first."));
        Q_EMIT changed();
        return;
    }
    startAiIconPackOperation(QStringLiteral("list_ai_icon_styles"), {},
                             QStringLiteral("loading_styles"));
}

void MeoAccountBackend::prepareAiIconMaterialPack(const QVariantMap &iconPack)
{
    if (!m_signedIn || m_aiBusy || m_aiIconPackBusy || !m_aiIconPackJobId.isEmpty()) {
        if (!m_signedIn) setError(tr("Connect Meo Account before creating an AI icon pack."));
        else if (!m_aiIconPackJobId.isEmpty())
            setError(tr("Finish, cancel, or release the current AI icon pack first."));
        Q_EMIT changed();
        return;
    }
    const QVariantMap canonical = MeoAccountContract::normalizedAiIconPackRequest(iconPack);
    if (canonical.isEmpty()) {
        setError(tr("The selected application identities or AI icon style are invalid."));
        Q_EMIT changed();
        return;
    }
    clearError();
    clearAiIconPackPresentation(true);
    startAiIconPackOperation(QStringLiteral("prepare_ai_icon_material_pack"),
                             {{QStringLiteral("iconPack"), canonical}},
                             QStringLiteral("preparing"));
}

void MeoAccountBackend::generatePreparedAiIconMaterialPack()
{
    if (m_aiIconPackBusy || m_aiIconPackState != QLatin1String("consent_ready")
        || !validAiIconPackJobId(m_aiIconPackJobId) || m_aiIconPackConsent.isEmpty()) {
        return;
    }
    const QString consentRequestId = m_aiIconPackConsent.value(QStringLiteral("requestId")).toString();
    const QString consentHash = m_aiIconPackConsent.value(QStringLiteral("payloadSha256")).toString();
    if (!validAiIconPackJobId(consentRequestId) || !validSha256(consentHash)
        || m_aiIconPackConsent.value(QStringLiteral("confirmationVersion")).toInt() != 1) {
        failAiIconPackOperation(tr("The AI icon pack consent is no longer valid."));
        return;
    }
    startAiIconPackOperation(QStringLiteral("generate_ai_icon_material_pack"), {
        {QStringLiteral("jobId"), m_aiIconPackJobId},
        {QStringLiteral("consent"), QVariantMap{
            {QStringLiteral("requestId"), consentRequestId},
            {QStringLiteral("payloadSha256"), consentHash},
            {QStringLiteral("confirmationVersion"), 1},
        }},
    }, QStringLiteral("generating"));
}

void MeoAccountBackend::denyPreparedAiIconMaterialPack()
{
    if (m_aiIconPackBusy || m_aiIconPackState != QLatin1String("consent_ready")
        || !validAiIconPackJobId(m_aiIconPackJobId)) {
        return;
    }
    startAiIconPackOperation(QStringLiteral("deny_ai_icon_material_pack"),
                             {{QStringLiteral("jobId"), m_aiIconPackJobId}},
                             QStringLiteral("denying"));
}

void MeoAccountBackend::refreshAiIconMaterialPackStatus()
{
    if (m_aiIconPackBusy || !validAiIconPackJobId(m_aiIconPackJobId)) return;
    startAiIconPackOperation(QStringLiteral("get_ai_icon_material_pack_status"),
                             {{QStringLiteral("jobId"), m_aiIconPackJobId}},
                             QStringLiteral("checking_status"));
}

void MeoAccountBackend::cancelAiIconMaterialPack()
{
    if (!validAiIconPackJobId(m_aiIconPackJobId)) return;
    // Account can abort local staging while a provider request is in flight.
    // Replacing the active request intentionally makes a late generation
    // result stale; it must not repopulate a cancelled pack in Settings.
    startAiIconPackOperation(QStringLiteral("cancel_ai_icon_material_pack"),
                             {{QStringLiteral("jobId"), m_aiIconPackJobId}},
                             QStringLiteral("cancelling"), true);
}

void MeoAccountBackend::releaseAiIconMaterialPack()
{
    if (m_aiIconPackBusy || m_aiIconPackState != QLatin1String("ready")
        || !validAiIconPackJobId(m_aiIconPackJobId)) {
        return;
    }
    startAiIconPackOperation(QStringLiteral("release_ai_icon_material_pack"),
                             {{QStringLiteral("jobId"), m_aiIconPackJobId}},
                             QStringLiteral("releasing"));
}

void MeoAccountBackend::clearAiIconMaterialPack()
{
    if (m_aiIconPackBusy) return;
    if (m_aiIconPackState == QLatin1String("ready")) {
        setError(tr("Release the private AI icon pack after applying or discarding it."));
        Q_EMIT changed();
        return;
    }
    clearError();
    clearAiIconPackPresentation(true);
    m_aiIconPackState = QStringLiteral("idle");
    Q_EMIT changed();
}

void MeoAccountBackend::startAiIconPackOperation(const QString &action,
                                                 const QVariantMap &arguments,
                                                 const QString &state,
                                                 const bool replacesActiveRequest)
{
    if (!m_signedIn || m_aiBusy || (m_aiIconPackBusy && !replacesActiveRequest)) {
        if (!m_signedIn) setError(tr("Connect Meo Account before using AI icon packs."));
        Q_EMIT changed();
        return;
    }

    clearError();
    const quint64 generation = ++m_aiIconPackGeneration;
    m_aiIconPackBusy = true;
    m_aiIconPackState = state;
    m_activeAiIconPackRequestId.clear();
    m_activeAiIconPackAction = action;
    Q_EMIT changed();

    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartAccountOperation"), action, arguments), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QString> reply = *completed;
                completed->deleteLater();
                if (generation != m_aiIconPackGeneration) return;
                if (!reply.isValid() || !validAiIconPackJobId(reply.value())) {
                    // Account operation request IDs are also UUIDs.  Reject an
                    // empty or malformed reply before it can become a broad
                    // signal match.
                    failAiIconPackOperation(tr("The Meo Account AI icon pack request could not be started."));
                    return;
                }
                m_activeAiIconPackRequestId = reply.value();
                fetchAiIconPackRequest(reply.value(), generation);
            });
}

void MeoAccountBackend::fetchAiIconPackRequest(const QString &requestId,
                                               const quint64 operationGeneration)
{
    if (operationGeneration != m_aiIconPackGeneration
        || requestId != m_activeAiIconPackRequestId) {
        return;
    }
    QDBusInterface broker(serviceName(), objectPath(), interfaceName(), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("GetRequest"), requestId), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestId, operationGeneration](QDBusPendingCallWatcher *completed) {
                const QDBusPendingReply<QVariantMap> reply = *completed;
                completed->deleteLater();
                if (operationGeneration != m_aiIconPackGeneration
                    || requestId != m_activeAiIconPackRequestId) {
                    return;
                }
                if (!reply.isValid() || reply.value().isEmpty()) {
                    failAiIconPackOperation(tr("The private AI icon pack result could not be read."));
                    return;
                }
                const QVariantMap request = reply.value();
                handleAiIconPackUpdate(requestId,
                                       request.value(QStringLiteral("state")).toString(),
                                       request, true);
            });
}

void MeoAccountBackend::failAiIconPackOperation(const QString &message)
{
    m_aiIconPackBusy = false;
    m_aiIconPackState = QStringLiteral("failed");
    m_activeAiIconPackRequestId.clear();
    m_activeAiIconPackAction.clear();
    setError(message);
    Q_EMIT changed();
}

void MeoAccountBackend::clearAiIconPackPresentation(const bool clearJob)
{
    m_aiIconPackConsent.clear();
    m_aiIconPackManifestPath.clear();
    m_aiIconPackManifestSha256.clear();
    m_aiIconPackSummary.clear();
    if (clearJob) m_aiIconPackJobId.clear();
    m_activeAiIconPackRequestId.clear();
    m_aiIconPackLifecycleRequestId.clear();
    m_activeAiIconPackAction.clear();
}

void MeoAccountBackend::handleAiIconPackUpdate(const QString &requestId, const QString &state,
                                               const QVariantMap &result,
                                               const bool fromPrivateRequest)
{
    // Never use the legacy `operation.startsWith("ai_")` fallback for a pack
    // event.  A request ID is acquired first and every later event must carry
    // it; private material metadata is then read only with GetRequest().
    if (requestId != m_activeAiIconPackRequestId) return;
    const QString operation = result.value(QStringLiteral("operation")).toString();
    if (!isAiIconPackOperation(operation)
        || !aiIconPackOperationIsExpected(m_activeAiIconPackAction, operation)) {
        return;
    }

    const bool listingStyles = m_activeAiIconPackAction == QLatin1String("list_ai_icon_styles");
    const QString returnedJobId = result.value(QStringLiteral("jobId")).toString().trimmed();
    if (!listingStyles) {
        if (!validAiIconPackJobId(returnedJobId)) {
            failAiIconPackOperation(tr("The AI icon pack response did not identify its job."));
            return;
        }
        if (m_aiIconPackJobId.isEmpty()) {
            if (m_activeAiIconPackAction != QLatin1String("prepare_ai_icon_material_pack")) {
                failAiIconPackOperation(tr("The AI icon pack response did not match the active job."));
                return;
            }
            m_aiIconPackJobId = returnedJobId;
        } else if (returnedJobId != m_aiIconPackJobId) {
            // A late response from a replaced/cancelled request is stale. It
            // must not alter the job, consent or private manifest now shown.
            return;
        }
    }

    const QString requestError = safeText(result.value(QStringLiteral("error")), 512);
    if (!requestError.isEmpty()) setError(requestError);
    const QVariantMap summary = safeAiIconPackSummary(result);
    if (!summary.isEmpty()) m_aiIconPackSummary = summary;

    if (operation == QLatin1String("ai_icon_pack_request_started")) {
        if (m_activeAiIconPackAction == QLatin1String("prepare_ai_icon_material_pack")
            || m_activeAiIconPackAction == QLatin1String("generate_ai_icon_material_pack")) {
            m_aiIconPackLifecycleRequestId = requestId;
        }
        m_aiIconPackState = QStringLiteral("contacting");
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_styles")) {
        const QVariantList styles = safeAiIconStyleCatalog(result.value(QStringLiteral("styles")).toList());
        if (styles.isEmpty()) {
            failAiIconPackOperation(tr("The Meo Account AI icon style catalog was invalid."));
            return;
        }
        m_aiIconStyles = styles;
        m_aiIconPackBusy = false;
        m_aiIconPackState = QStringLiteral("styles_ready");
        m_activeAiIconPackRequestId.clear();
        m_activeAiIconPackAction.clear();
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_consent")) {
        const QVariantMap consent = safeAiIconPackConsent(result.value(QStringLiteral("consent")).toMap());
        if (consent.isEmpty()
            || consent.value(QStringLiteral("styleId")).toString()
                != m_aiIconPackSummary.value(QStringLiteral("styleId")).toString()
            || consent.value(QStringLiteral("shape")).toString()
                != m_aiIconPackSummary.value(QStringLiteral("shape")).toString()
            || consent.value(QStringLiteral("itemCount")).toInt()
                != m_aiIconPackSummary.value(QStringLiteral("itemCount")).toInt()) {
            failAiIconPackOperation(tr("The AI icon pack consent did not match the selected identities."));
            return;
        }
        m_aiIconPackConsent = consent;
        m_aiIconPackBusy = false;
        m_aiIconPackState = QStringLiteral("consent_ready");
        m_activeAiIconPackRequestId.clear();
        m_activeAiIconPackAction.clear();
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_staging")) {
        m_aiIconPackState = QStringLiteral("staging");
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_ready")) {
        if (!fromPrivateRequest) {
            m_aiIconPackState = QStringLiteral("staging");
            fetchAiIconPackRequest(requestId, m_aiIconPackGeneration);
            Q_EMIT changed();
            return;
        }
        const QString manifestPath = result.value(QStringLiteral("manifestPath")).toString();
        const QString manifestSha256 = result.value(QStringLiteral("manifestSha256")).toString();
        if (!isPrivateAiIconPackManifestPath(manifestPath, m_aiIconPackJobId)
            || !validSha256(manifestSha256)) {
            failAiIconPackOperation(tr("The private AI icon pack manifest was invalid."));
            return;
        }
        m_aiIconPackManifestPath = manifestPath;
        m_aiIconPackManifestSha256 = manifestSha256;
        m_aiIconPackSummary.insert(QStringLiteral("manifestSha256"), manifestSha256);
        m_aiIconPackConsent.clear();
        m_aiIconPackBusy = false;
        m_aiIconPackState = QStringLiteral("ready");
        m_activeAiIconPackRequestId.clear();
        m_activeAiIconPackAction.clear();
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_status")) {
        const QString packState = result.value(QStringLiteral("state")).toString();
        if (packState == QLatin1String("staged")) {
            if (!fromPrivateRequest) {
                fetchAiIconPackRequest(requestId, m_aiIconPackGeneration);
                return;
            }
            const QString manifestPath = result.value(QStringLiteral("manifestPath")).toString();
            const QString manifestSha256 = result.value(QStringLiteral("manifestSha256")).toString();
            if (!isPrivateAiIconPackManifestPath(manifestPath, m_aiIconPackJobId)
                || !validSha256(manifestSha256)) {
                failAiIconPackOperation(tr("The private AI icon pack manifest was invalid."));
                return;
            }
            m_aiIconPackManifestPath = manifestPath;
            m_aiIconPackManifestSha256 = manifestSha256;
            m_aiIconPackSummary.insert(QStringLiteral("manifestSha256"), manifestSha256);
            m_aiIconPackState = QStringLiteral("ready");
        } else {
            m_aiIconPackState = safeText(packState, 64);
            if (m_aiIconPackState.isEmpty()) m_aiIconPackState = QStringLiteral("unknown");
        }
        m_aiIconPackBusy = false;
        m_activeAiIconPackRequestId.clear();
        m_activeAiIconPackAction.clear();
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_denied")
        || operation == QLatin1String("ai_icon_pack_cancelled")
        || operation == QLatin1String("ai_icon_pack_released")) {
        const QString terminalState = operation == QLatin1String("ai_icon_pack_denied")
            ? QStringLiteral("denied")
            : operation == QLatin1String("ai_icon_pack_cancelled")
                ? QStringLiteral("cancelled") : QStringLiteral("released");
        clearAiIconPackPresentation(true);
        m_aiIconPackBusy = false;
        m_aiIconPackState = terminalState;
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_expired")) {
        clearAiIconPackPresentation(true);
        m_aiIconPackBusy = false;
        m_aiIconPackState = QStringLiteral("expired");
        Q_EMIT changed();
        return;
    }
    if (operation == QLatin1String("ai_icon_pack_failed")) {
        m_aiIconPackBusy = false;
        m_aiIconPackState = state == QLatin1String("expired")
            ? QStringLiteral("expired") : QStringLiteral("failed");
        m_aiIconPackConsent.clear();
        m_activeAiIconPackRequestId.clear();
        m_aiIconPackLifecycleRequestId.clear();
        m_activeAiIconPackAction.clear();
        Q_EMIT changed();
    }
}

void MeoAccountBackend::startAiOperation(const QString &action,
                                         const QVariantMap &arguments,
                                         const QString &state)
{
    if (m_aiIconPackBusy) {
        setError(tr("Finish the active AI icon pack before starting a legacy image request."));
        Q_EMIT changed();
        return;
    }
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
    if (operation == QLatin1String("ai_icon_pack_expired")
        && requestId == m_aiIconPackLifecycleRequestId
        && result.value(QStringLiteral("jobId")).toString().trimmed() == m_aiIconPackJobId
        && validAiIconPackJobId(m_aiIconPackJobId)) {
        const QString requestError = safeText(result.value(QStringLiteral("error")), 512);
        if (!requestError.isEmpty()) setError(requestError);
        clearAiIconPackPresentation(true);
        m_aiIconPackBusy = false;
        m_aiIconPackState = QStringLiteral("expired");
        Q_EMIT changed();
        return;
    }
    if (isAiIconPackOperation(operation)) {
        // A pack signal received before StartAccountOperation returns is
        // deliberately ignored here; fetchAiIconPackRequest() reads the same
        // private request once its exact request ID is known.  This prevents
        // an unrelated `ai_` signal from claiming the legacy image state.
        if (!m_activeAiIconPackRequestId.isEmpty()
            && requestId == m_activeAiIconPackRequestId) {
            handleAiIconPackUpdate(requestId, state, result, false);
        }
        return;
    }
    if (m_localAiDiscoveryBusy && operation == QLatin1String("local_ai_models")
        && (m_activeLocalAiRequestId.isEmpty() || requestId == m_activeLocalAiRequestId)) {
        if (m_activeLocalAiRequestId.isEmpty()) m_activeLocalAiRequestId = requestId;
        const bool terminal = state == QLatin1String("completed")
            || state == QLatin1String("failed") || state == QLatin1String("denied")
            || state == QLatin1String("expired");
        if (state == QLatin1String("completed")) {
            const QVariantList models = result.value(QStringLiteral("models")).toList();
            if (models.isEmpty()) {
                setError(tr("The provider did not report any available models."));
            } else {
                Q_EMIT localAiModelsDiscovered(m_localAiDiscoveryConnectionId, models);
            }
        }
        const QString requestError = result.value(QStringLiteral("error")).toString();
        if (!requestError.isEmpty()) setError(requestError);
        if (terminal) {
            m_localAiDiscoveryBusy = false;
            m_localAiDiscoveryConnectionId.clear();
            m_activeLocalAiRequestId.clear();
        }
        Q_EMIT changed();
        return;
    }
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
