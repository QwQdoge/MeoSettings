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
    Q_PROPERTY(QString cloudName READ cloudName NOTIFY changed)
    Q_PROPERTY(QString cloudId READ cloudId NOTIFY changed)
    Q_PROPERTY(QString cloudAvatarSource READ cloudAvatarSource NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit MeoAccountBackend(QObject *parent = nullptr);

    bool serviceRunning() const;
    bool settingsLauncherAvailable() const;
    bool signedIn() const;
    bool identityGranted() const;
    bool oauthConfigured() const;
    QString cloudName() const;
    QString cloudId() const;
    QString cloudAvatarSource() const;
    QString summary() const;

public Q_SLOTS:
    /// Refreshes only the current session-bus broker status. It never starts
    /// an OAuth flow or attempts to activate a broker that is not running.
    void refresh();

public:
    /// Opens the maintained Meo Account settings application after an
    /// explicit user action. No credential is passed through Meo Settings.
    Q_INVOKABLE bool openAccountSettings();

Q_SIGNALS:
    void changed();

private:
    void updateLauncherAvailability();
    void applyUnavailableState();
    void applyStatus(const QVariantMap &status, const QVariantMap &identity);

    QString m_settingsLauncher;
    bool m_serviceRunning = false;
    bool m_signedIn = false;
    bool m_identityGranted = false;
    bool m_oauthConfigured = false;
    QString m_cloudName;
    QString m_cloudId;
    QString m_cloudAvatarSource;
};
