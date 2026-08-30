#pragma once

#include "../core/backendbase.h"

#include <QString>
#include <QVariant>

class MeoAccountContract final
{
public:
    static QString safeProfileText(const QVariant &value);
    static QString safeRemoteAvatarSource(const QVariant &value);
};

/**
 * Read-only Meo Account session projection.
 *
 * The account broker owns the KWallet session and OAuth flow.  Settings never
 * receives an access token, refresh token, password, or OAuth callback.  It
 * can only show the broker's status and the manifest-scoped profile fields
 * needed to render the desktop identity consistently.
 */
class MeoAccountBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool serviceRunning READ serviceRunning NOTIFY changed)
    Q_PROPERTY(bool settingsLauncherAvailable READ settingsLauncherAvailable NOTIFY changed)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(bool identityGranted READ identityGranted NOTIFY changed)
    Q_PROPERTY(bool oauthConfigured READ oauthConfigured NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString accountState READ accountState NOTIFY changed)
    Q_PROPERTY(qulonglong logoutEpoch READ logoutEpoch NOTIFY changed)
    Q_PROPERTY(QVariantList clients READ clients NOTIFY changed)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(bool mfaEnabled READ mfaEnabled NOTIFY changed)
    Q_PROPERTY(QString syncState READ syncState NOTIFY changed)
    Q_PROPERTY(QString syncError READ syncError NOTIFY changed)
    Q_PROPERTY(QString lastSyncedAt READ lastSyncedAt NOTIFY changed)
    Q_PROPERTY(QString requestState READ requestState NOTIFY changed)
    Q_PROPERTY(QString cloudName READ cloudName NOTIFY changed)
    Q_PROPERTY(QString cloudId READ cloudId NOTIFY changed)
    Q_PROPERTY(QString cloudAvatarSource READ cloudAvatarSource NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY changed)
    Q_PROPERTY(QString aiState READ aiState NOTIFY changed)
    Q_PROPERTY(QVariantList aiCredentials READ aiCredentials NOTIFY changed)
    Q_PROPERTY(QVariantMap aiConsent READ aiConsent NOTIFY changed)
    Q_PROPERTY(QString aiImageSource READ aiImageSource NOTIFY changed)
    Q_PROPERTY(QString aiTargetDesktopId READ aiTargetDesktopId NOTIFY changed)
    Q_PROPERTY(QString aiTargetApplicationName READ aiTargetApplicationName NOTIFY changed)

public:
    explicit MeoAccountBackend(QObject *parent = nullptr);

    bool serviceRunning() const;
    bool settingsLauncherAvailable() const;
    bool signedIn() const;
    bool identityGranted() const;
    bool oauthConfigured() const;
    bool busy() const;
    QString accountState() const;
    qulonglong logoutEpoch() const;
    QVariantList clients() const;
    QVariantList sessions() const;
    bool mfaEnabled() const;
    QString syncState() const;
    QString syncError() const;
    QString lastSyncedAt() const;
    QString requestState() const;
    QString cloudName() const;
    QString cloudId() const;
    QString cloudAvatarSource() const;
    QString summary() const;
    bool aiBusy() const;
    QString aiState() const;
    QVariantList aiCredentials() const;
    QVariantMap aiConsent() const;
    QString aiImageSource() const;
    QString aiTargetDesktopId() const;
    QString aiTargetApplicationName() const;

public Q_SLOTS:
    /// Refreshes only the current session-bus broker status. It never starts
    /// an OAuth flow or attempts to activate a broker that is not running.
    void refresh();

public:
    /// Opens the maintained Meo Account settings application after an
    /// explicit user action. No credential is passed through Meo Settings.
    Q_INVOKABLE bool openAccountSettings();
    Q_INVOKABLE void requestAuthentication(const QString &mode = QStringLiteral("connect_system"));
    Q_INVOKABLE void openHostedAction(const QString &action);
    Q_INVOKABLE void signOutAll();
    Q_INVOKABLE void revokeClient(const QString &clientId);
    Q_INVOKABLE void refreshAiCredentials();
    Q_INVOKABLE void prepareIconImage(const QString &desktopId, const QString &applicationName,
                                      const QString &credentialId, const QString &model,
                                      const QString &prompt);
    Q_INVOKABLE void generatePreparedIconImage();
    Q_INVOKABLE void denyPreparedIconImage();
    Q_INVOKABLE void clearGeneratedIconImage();
    Q_INVOKABLE void prepareIconImageBatch(const QVariantList &applications,
                                            const QString &credentialId,
                                            const QString &model,
                                            const QString &prompt);
    Q_INVOKABLE void generatePreparedIconImageBatch();
    Q_INVOKABLE void continuePreparedIconImageBatch();
    Q_INVOKABLE void denyPreparedIconImageBatch();

Q_SIGNALS:
    void changed();

private:
    void updateLauncherAvailability();
    void applyUnavailableState();
    void applyStatus(const QVariantMap &status, const QVariantMap &identity);
    void setBusy(bool busy);
    void startSignOutOperation();
    void startClientRevocation(const QString &clientId);
    void startAiOperation(const QString &action, const QVariantMap &arguments,
                          const QString &state);
    QVariantMap iconImageArguments(const QString &desktopId, const QString &applicationName,
                                   const QString &credentialId, const QString &model,
                                   const QString &prompt) const;
    void prepareNextIconImageBatchItem();
    void startPreparedIconImageBatchItem(const QString &action, const QString &state);

private Q_SLOTS:
    void handleRequestChanged(const QString &requestId, const QString &state,
                              const QVariantMap &result);

private:
    QString m_settingsLauncher;
    bool m_serviceRunning = false;
    bool m_signedIn = false;
    bool m_identityGranted = false;
    bool m_oauthConfigured = false;
    bool m_busy = false;
    QString m_accountState = QStringLiteral("unavailable");
    qulonglong m_logoutEpoch = 0;
    QVariantList m_clients;
    QVariantList m_sessions;
    bool m_mfaEnabled = false;
    QString m_syncState = QStringLiteral("not_loaded");
    QString m_syncError;
    QString m_lastSyncedAt;
    QString m_requestState;
    QString m_activeRequestId;
    QString m_cloudName;
    QString m_cloudId;
    QString m_cloudAvatarSource;
    bool m_aiBusy = false;
    QString m_aiState = QStringLiteral("idle");
    QVariantList m_aiCredentials;
    QVariantMap m_aiConsent;
    QVariantMap m_pendingAiArguments;
    QString m_aiImageSource;
    QString m_aiTargetDesktopId;
    QString m_aiTargetApplicationName;
    QString m_activeAiRequestId;
    QVariantList m_aiBatchItems;
    QVariantList m_aiBatchPrepared;
    int m_aiBatchIndex = -1;
    bool m_aiBatchPreparing = false;
    bool m_aiBatchGenerating = false;
    bool m_aiBatchDenying = false;
    bool m_signOutAfterReauth = false;
    QString m_clientToRevokeAfterReauth;
};
