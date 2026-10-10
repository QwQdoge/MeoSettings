#pragma once

#include "../core/backendbase.h"

#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class OmniStoreAppsBackend;

class BackupManifestContract final
{
public:
    static QJsonObject build(const QVariantList &applications,
                             const QVariantMap &settings,
                             const QString &createdAt,
                             QString *error = nullptr);
    static QJsonObject build(const QVariantList &applications,
                             const QString &createdAt,
                             QString *error = nullptr);
    static bool validate(const QJsonObject &manifest, QString *error = nullptr);
};

/**
 * Creates and inspects a deliberately non-secret Meo backup manifest.
 *
 * The manifest is a portable product backup, not a filesystem snapshot. It
 * projects OmniStore's validated application inventory and a tightly
 * whitelisted set of Meo-owned presentation settings. It never copies KWallet,
 * Account sessions, provider keys, device credentials, filesystem paths,
 * arbitrary application data, or privileged system configuration.
 */
class BackupBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString lastBackupPath READ lastBackupPath NOTIFY changed)
    Q_PROPERTY(QString lastBackupAt READ lastBackupAt NOTIFY changed)
    Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY changed)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY changed)
    Q_PROPERTY(bool previewValid READ previewValid NOTIFY changed)

public:
    explicit BackupBackend(OmniStoreAppsBackend *appsBackend,
                           QObject *parent = nullptr);

    QString summary() const;
    QString lastBackupPath() const;
    QString lastBackupAt() const;
    QString previewSummary() const;
    QString previewPath() const;
    bool previewValid() const;

    Q_INVOKABLE bool createLocalManifest();
    Q_INVOKABLE bool createLocalManifestWithSettings(const QVariantMap &settings);
    Q_INVOKABLE bool previewLocalManifest(const QString &path);
    Q_INVOKABLE void clearPreview();

Q_SIGNALS:
    void changed();

private:
    bool createLocalManifestInternal(const QVariantMap &settings);

    OmniStoreAppsBackend *m_appsBackend = nullptr;
    QString m_lastBackupPath;
    QString m_lastBackupAt;
    QString m_previewSummary;
    QString m_previewPath;
    bool m_previewValid = false;
};
