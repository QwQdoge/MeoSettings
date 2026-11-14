#pragma once

#include "../core/backendbase.h"

#include <QByteArray>
#include <QProcess>
#include <QVariantList>

class QTimer;

/**
 * OmniStore's app-maintenance preferences through the stable
 * `omnistore-apps preferences | set-preferences JSON | trim-cache` ABI:
 * automatic app updates, update notifications and the automatic cache limit.
 * Meo Settings never edits OmniStore's configuration file itself.
 */
class OmniStorePreferencesBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY changed)
    Q_PROPERTY(bool autoUpdate READ autoUpdate NOTIFY changed)
    Q_PROPERTY(bool notifyUpdates READ notifyUpdates NOTIFY changed)
    Q_PROPERTY(int cacheLimitMb READ cacheLimitMb NOTIFY changed)
    Q_PROPERTY(QVariantList cacheLimitChoicesMb READ cacheLimitChoicesMb NOTIFY changed)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY changed)

public:
    explicit OmniStorePreferencesBackend(QObject *parent = nullptr);

    bool supported() const { return m_supported; }
    bool autoUpdate() const { return m_autoUpdate; }
    bool notifyUpdates() const { return m_notifyUpdates; }
    int cacheLimitMb() const { return m_cacheLimitMb; }
    QVariantList cacheLimitChoicesMb() const { return m_cacheLimitChoices; }
    QString lastResult() const { return m_lastResult; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setAutoUpdate(bool enabled);
    Q_INVOKABLE void setNotifyUpdates(bool enabled);
    Q_INVOKABLE void setCacheLimitMb(int megabytes);
    // Clean application caches down to the configured limit right now.
    Q_INVOKABLE void trimCachesNow();

    // Exposed for tests: validates one `omnistore-apps` preferences reply.
    static bool parsePreferences(const QByteArray &payload, bool *autoUpdate, bool *notifyUpdates,
                                 int *cacheLimitMb, QVariantList *choices);

Q_SIGNALS:
    void changed();
    void cachesTrimmed(qulonglong bytesFreed);

private:
    void start(const QString &operation, const QStringList &arguments);
    void finished(int exitCode, QProcess::ExitStatus status);
    void fail(const QString &message);

    QProcess *m_process = nullptr;
    QTimer *m_timeout = nullptr;
    QByteArray m_output;
    QString m_operation;
    QString m_managerPath;
    QStringList m_pending;
    bool m_supported = false;
    bool m_autoUpdate = false;
    bool m_notifyUpdates = true;
    int m_cacheLimitMb = 0;
    QVariantList m_cacheLimitChoices;
    QString m_lastResult;
};
