#pragma once

#include "../core/backendbase.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <initializer_list>

class OmniStoreAppsBackend;

class BackupManifestContract final
{
public:
    static QJsonObject build(const QVariantList &applications,
                             bool applicationInventoryIncluded,
                             const QVariantMap &settings,
                             const QString &createdAt,
                             QString *error = nullptr);
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
 * projects OmniStore's validated application inventory when one is available
 * and a tightly whitelisted set of Meo-owned presentation settings. An absent
 * application snapshot is recorded as unavailable rather than as zero apps.
 * It never copies KWallet, Account sessions, provider keys, device credentials,
 * filesystem paths, arbitrary application data, or privileged system config.
 *
 * The live settings sources are generic QObjects on purpose: the backup format
 * remains independently testable and does not gain a runtime dependency on
 * Plasma/DBus backend implementation classes. BackupBackend projects only the
 * public properties that belong to the portable schema.
 */
class BackupBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString lastBackupPath READ lastBackupPath NOTIFY changed)
    Q_PROPERTY(QString lastBackupAt READ lastBackupAt NOTIFY changed)
    Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY changed)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY changed)
    Q_PROPERTY(QVariantList previewPlan READ previewPlan NOTIFY changed)
    Q_PROPERTY(bool previewValid READ previewValid NOTIFY changed)

public:
    explicit BackupBackend(OmniStoreAppsBackend *appsBackend,
                           QObject *parent = nullptr);

    QString summary() const;
    QString lastBackupPath() const;
    QString lastBackupAt() const;
    QString previewSummary() const;
    QString previewPath() const;
    QVariantList previewPlan() const
    {
        if (!m_previewValid || m_previewPath.isEmpty())
            return {};

        const QFileInfo info(m_previewPath);
        if (!info.exists() || !info.isFile() || info.isSymLink())
            return {};

        QFile file(info.canonicalFilePath());
        if (!file.open(QIODevice::ReadOnly))
            return {};

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return {};

        QString contractError;
        const QJsonObject manifest = document.object();
        if (!BackupManifestContract::validate(manifest, &contractError))
            return {};

        const QJsonObject contents = manifest.value(QStringLiteral("contents")).toObject();
        const QJsonObject settings = contents.value(QStringLiteral("settings")).toObject();
        const QJsonObject controlCenter = settings.value(QStringLiteral("controlCenter")).toObject();
        const QJsonObject shell = settings.value(QStringLiteral("shell")).toObject();
        const QString applicationsState = contents.value(QStringLiteral("applicationsState")).toString();
        const int applicationCount = contents.value(QStringLiteral("applications")).toArray().size();

        const auto row = [](const QString &id, const QString &state, int count = -1) {
            QVariantMap value{
                {QStringLiteral("id"), id},
                {QStringLiteral("state"), state},
            };
            if (count >= 0)
                value.insert(QStringLiteral("count"), count);
            return QVariant(value);
        };

        return QVariantList{
            row(QStringLiteral("applications"), applicationsState, applicationCount),
            row(QStringLiteral("control-center"),
                controlCenter.contains(QStringLiteral("layout")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("top-bar"),
                controlCenter.contains(QStringLiteral("topBar")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("shelf"),
                shell.contains(QStringLiteral("shelf")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("notifications"),
                shell.contains(QStringLiteral("notifications")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("time-center"),
                shell.contains(QStringLiteral("timeCenter")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("top-tasks"),
                shell.contains(QStringLiteral("topTasks")) ? QStringLiteral("included") : QStringLiteral("not-included")),
            row(QStringLiteral("user-data"), QStringLiteral("not-included"), 0),
            row(QStringLiteral("accounts-secrets"), QStringLiteral("not-included")),
        };
    }
    bool previewValid() const;

    void setPortableSettingsSources(QObject *controlCenterBackend,
                                    QObject *shellSettingsBackend);

    Q_INVOKABLE bool createLocalManifest();
    Q_INVOKABLE bool createLocalManifestWithSettings(const QVariantMap &settings);
    Q_INVOKABLE bool previewLocalManifest(const QString &path);
    Q_INVOKABLE bool previewManifestUrl(const QUrl &url)
    {
        clearError();
        if (!url.isValid() || !url.isLocalFile()) {
            setError(tr("Choose a local Meo backup manifest."));
            return false;
        }
        return previewLocalManifest(url.toLocalFile());
    }
    Q_INVOKABLE void clearPreview();

Q_SIGNALS:
    void changed();

private:
    QVariantMap portableSettingsSnapshot(QString *error = nullptr) const;
    bool createLocalManifestInternal(const QVariantMap &settings);

    OmniStoreAppsBackend *m_appsBackend = nullptr;
    QPointer<QObject> m_controlCenterBackend;
    QPointer<QObject> m_shellSettingsBackend;
    QString m_lastBackupPath;
    QString m_lastBackupAt;
    QString m_previewSummary;
    QString m_previewPath;
    bool m_lastApplicationInventoryIncluded = false;
    bool m_previewValid = false;
};
