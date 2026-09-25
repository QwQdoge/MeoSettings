#pragma once

#include "../core/backendbase.h"

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QVariantList>

#include <optional>

class QTimer;

/**
 * A bounded, schema-validated projection of OmniStore's installed-app export.
 *
 * This deliberately contains no install locations, URLs, descriptions, or
 * package-management actions.  The size share is only a share of known
 * OmniStore application metadata, never an assertion about total disk use.
 */
struct OmniStoreAppsSnapshot
{
    QVariantList sources;
    QVariantList applications;
    QVariantList topApplications;
    int applicationCount = 0;
    qulonglong knownSizeBytes = 0;
    int unknownSizeCount = 0;
    int exactSizeCount = 0;
    int reportedSizeCount = 0;
    QString generatedAt;
};

class OmniStoreAppsContract final
{
public:
    static std::optional<OmniStoreAppsSnapshot> parse(const QByteArray &payload,
                                                       QString *error = nullptr);
};

/**
 * Reads OmniStore's documented `omnistore-apps-export` command asynchronously.
 *
 * The backend never starts the Flutter GUI, never talks to OmniStore's private
 * localhost daemon, and does not install, remove, update, or clean packages.
 */
class OmniStoreAppsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
    Q_PROPERTY(QVariantList topApplications READ topApplications NOTIFY changed)
    Q_PROPERTY(int applicationCount READ applicationCount NOTIFY changed)
    Q_PROPERTY(qulonglong knownSizeBytes READ knownSizeBytes NOTIFY changed)
    Q_PROPERTY(int unknownSizeCount READ unknownSizeCount NOTIFY changed)
    Q_PROPERTY(int exactSizeCount READ exactSizeCount NOTIFY changed)
    Q_PROPERTY(int reportedSizeCount READ reportedSizeCount NOTIFY changed)
    Q_PROPERTY(QString generatedAt READ generatedAt NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool exporterAvailable READ exporterAvailable NOTIFY changed)
    Q_PROPERTY(bool managerAvailable READ managerAvailable NOTIFY changed)
    Q_PROPERTY(bool launcherAvailable READ launcherAvailable NOTIFY changed)

public:
    explicit OmniStoreAppsBackend(QObject *parent = nullptr);

    QVariantList sources() const;
    QVariantList applications() const;
    QVariantList topApplications() const;
    int applicationCount() const;
    qulonglong knownSizeBytes() const;
    int unknownSizeCount() const;
    int exactSizeCount() const;
    int reportedSizeCount() const;
    QString generatedAt() const;
    QString summary() const;
    bool exporterAvailable() const;
    bool managerAvailable() const;
    bool launcherAvailable() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool openOmniStore();
    Q_INVOKABLE bool clearCache(const QString &appId, const QString &sourceId);
    Q_INVOKABLE bool resetSettings(const QString &appId, const QString &sourceId);
    Q_INVOKABLE bool clearData(const QString &appId, const QString &sourceId);
    Q_INVOKABLE bool uninstall(const QString &appId, const QString &sourceId);

Q_SIGNALS:
    void changed();
    void actionFinished(const QString &action, const QString &appId, bool success);

private:
    void updateExecutableAvailability();
    void captureStandardOutput();
    void discardStandardError();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError();
    void processTimedOut();
    void applySnapshot(const OmniStoreAppsSnapshot &snapshot);
    void finishWithError(const QString &error);
    bool startAction(const QString &action, const QString &appId, const QString &sourceId);

    QProcess *m_process = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QByteArray m_standardOutput;
    QVariantList m_sources;
    QVariantList m_applications;
    QVariantList m_topApplications;
    int m_applicationCount = 0;
    qulonglong m_knownSizeBytes = 0;
    int m_unknownSizeCount = 0;
    int m_exactSizeCount = 0;
    int m_reportedSizeCount = 0;
    QString m_generatedAt;
    QString m_exporterPath;
    QString m_managerPath;
    QString m_launcherPath;
    QString m_activeAction;
    QString m_activeAppId;
    bool m_hasSnapshot = false;
    bool m_requestActive = false;
};
