#pragma once

#include "../core/backendbase.h"

#include <QJsonObject>
#include <QString>
#include <QVariantList>

class OmniStoreAppsBackend;

class BackupManifestContract final
{
public:
    static QJsonObject build(const QVariantList &applications,
                             const QString &createdAt,
                             QString *error = nullptr);
    static bool validate(const QJsonObject &manifest, QString *error = nullptr);
};

/**
 * Creates and inspects a deliberately non-secret Meo backup manifest.
 *
 * The first implementation is intentionally conservative: it records only a
 * schema version, creation timestamp, and a reinstall list projected from
 * OmniStore's already validated application snapshot. It never copies KWallet,
 * Account sessions, provider keys, device credentials, filesystem paths, or
 * arbitrary application data.
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
    Q_INVOKABLE bool previewLocalManifest(const QString &path);
    Q_INVOKABLE void clearPreview();

Q_SIGNALS:
    void changed();

private:
    OmniStoreAppsBackend *m_appsBackend = nullptr;
    QString m_lastBackupPath;
    QString m_lastBackupAt;
    QString m_previewSummary;
    QString m_previewPath;
    bool m_previewValid = false;
};
