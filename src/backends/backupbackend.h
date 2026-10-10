#pragma once

#include "../core/backendbase.h"
#include "controlcenterbackend.h"
#include "shellsettingsbackend.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
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
 * Preview remains read-only. Applying a preview reopens and revalidates the
 * selected manifest to avoid trusting stale preview state, then serializes the
 * existing Meo-owned backend save operations. A step advances only after the
 * corresponding backend emits its explicit saved signal. Any backend error or
 * timeout stops the sequence and reports a partial result; applications,
 * user-data payloads, secrets, and system recovery are never applied here.
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
    Q_PROPERTY(QString restoreState READ restoreState NOTIFY changed)
    Q_PROPERTY(QString restoreSummary READ restoreSummary NOTIFY changed)
    Q_PROPERTY(QVariantList restoreResults READ restoreResults NOTIFY changed)
    Q_PROPERTY(bool canApplyRestore READ canApplyRestore NOTIFY changed)

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

    QString restoreState() const { return m_restoreState; }
    QString restoreSummary() const { return m_restoreSummary; }
    QVariantList restoreResults() const { return m_restoreResults; }
    bool canApplyRestore() const
    {
        return m_previewValid && !m_previewPath.isEmpty() && !busy()
            && m_restoreState != QLatin1String("applying");
    }

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

    Q_INVOKABLE bool applyPreviewSettings()
    {
        clearError();
        if (!canApplyRestore()) {
            setError(tr("Choose and validate a backup before applying portable settings."));
            return false;
        }

        const QFileInfo info(m_previewPath);
        if (!info.exists() || !info.isFile() || info.isSymLink()
            || info.size() < 2 || info.size() > 2 * 1024 * 1024) {
            setError(tr("The selected backup is no longer a safe regular manifest file."));
            return false;
        }

        QFile file(info.canonicalFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            setError(tr("The selected backup could not be reopened for restore."));
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            setError(tr("The selected backup changed and is no longer valid JSON."));
            return false;
        }

        QString contractError;
        const QJsonObject manifest = document.object();
        if (!BackupManifestContract::validate(manifest, &contractError)) {
            setError(tr("The selected backup changed and no longer passes validation: %1").arg(contractError));
            return false;
        }

        const QJsonObject settings = manifest.value(QStringLiteral("contents"))
                                         .toObject().value(QStringLiteral("settings")).toObject();
        const QJsonObject controlCenter = settings.value(QStringLiteral("controlCenter")).toObject();
        const QJsonObject shell = settings.value(QStringLiteral("shell")).toObject();

        m_restoreQueue.clear();
        if (controlCenter.contains(QStringLiteral("layout")))
            m_restoreQueue.push_back(QStringLiteral("control-center"));
        if (controlCenter.contains(QStringLiteral("topBar")))
            m_restoreQueue.push_back(QStringLiteral("top-bar"));
        if (shell.contains(QStringLiteral("shelf")))
            m_restoreQueue.push_back(QStringLiteral("shelf"));
        if (shell.contains(QStringLiteral("notifications")))
            m_restoreQueue.push_back(QStringLiteral("notifications"));
        if (shell.contains(QStringLiteral("timeCenter")))
            m_restoreQueue.push_back(QStringLiteral("time-center"));
        if (shell.contains(QStringLiteral("topTasks")))
            m_restoreQueue.push_back(QStringLiteral("top-tasks"));

        if (m_restoreQueue.isEmpty()) {
            setError(tr("This backup does not contain portable Meo settings to apply."));
            return false;
        }

        auto *control = qobject_cast<ControlCenterBackend *>(m_controlCenterBackend.data());
        auto *shellBackend = qobject_cast<ShellSettingsBackend *>(m_shellSettingsBackend.data());
        const bool needsControl = controlCenter.contains(QStringLiteral("layout"))
            || controlCenter.contains(QStringLiteral("topBar"));
        const bool needsShell = !shell.isEmpty();
        if (needsControl && (!control || !control->available())) {
            setError(tr("The active Meo Control Center is unavailable, so this restore cannot start."));
            return false;
        }
        if (needsShell && (!shellBackend || !shellBackend->available())) {
            setError(tr("The active Meo shell surfaces are unavailable, so this restore cannot start."));
            return false;
        }
        if ((control && control->busy()) || (shellBackend && shellBackend->busy())) {
            setError(tr("Wait for the current shell settings operation to finish before restoring."));
            return false;
        }

        m_restoreSettings = settings.toVariantMap();
        m_restoreResults.clear();
        m_restoreCompletedCount = 0;
        m_restoreState = QStringLiteral("applying");
        m_restoreSummary = tr("Applying portable Meo settings…");
        setBusy(true);
        Q_EMIT changed();
        startNextRestoreStep();
        return true;
    }

Q_SIGNALS:
    void changed();
    void restoreFinished(bool success);

private:
    QVariantMap portableSettingsSnapshot(QString *error = nullptr) const;
    bool createLocalManifestInternal(const QVariantMap &settings);

    void disconnectRestoreStep()
    {
        QObject::disconnect(m_restoreSuccessConnection);
        QObject::disconnect(m_restoreErrorConnection);
        m_restoreSuccessConnection = {};
        m_restoreErrorConnection = {};
    }

    void appendRestoreResult(const QString &id, const QString &state, const QString &detail = {})
    {
        QVariantMap result{
            {QStringLiteral("id"), id},
            {QStringLiteral("state"), state},
        };
        if (!detail.isEmpty())
            result.insert(QStringLiteral("detail"), detail);
        m_restoreResults.push_back(result);
    }

    void failRestoreStep(const QString &detail)
    {
        if (m_restoreState != QLatin1String("applying"))
            return;
        disconnectRestoreStep();
        ++m_restoreGeneration;
        appendRestoreResult(m_restoreCurrentStep, QStringLiteral("failed"), detail);
        m_restoreState = m_restoreCompletedCount > 0
            ? QStringLiteral("partial-failure") : QStringLiteral("failed");
        m_restoreSummary = m_restoreCompletedCount > 0
            ? tr("Restore stopped after %n completed setting group(s): %1", "", m_restoreCompletedCount).arg(detail)
            : tr("Restore could not apply the first setting group: %1").arg(detail);
        setBusy(false);
        Q_EMIT changed();
        Q_EMIT restoreFinished(false);
    }

    void finishRestoreStep()
    {
        if (m_restoreState != QLatin1String("applying"))
            return;
        disconnectRestoreStep();
        ++m_restoreGeneration;
        appendRestoreResult(m_restoreCurrentStep, QStringLiteral("applied"));
        ++m_restoreCompletedCount;
        startNextRestoreStep();
    }

    void startNextRestoreStep()
    {
        if (m_restoreState != QLatin1String("applying"))
            return;
        if (m_restoreQueue.isEmpty()) {
            disconnectRestoreStep();
            ++m_restoreGeneration;
            m_restoreCurrentStep.clear();
            m_restoreState = QStringLiteral("succeeded");
            m_restoreSummary = tr("Restored %n portable Meo setting group(s).", "", m_restoreCompletedCount);
            setBusy(false);
            Q_EMIT changed();
            Q_EMIT restoreFinished(true);
            return;
        }

        m_restoreCurrentStep = m_restoreQueue.takeFirst();
        const int generation = ++m_restoreGeneration;
        auto *control = qobject_cast<ControlCenterBackend *>(m_controlCenterBackend.data());
        auto *shellBackend = qobject_cast<ShellSettingsBackend *>(m_shellSettingsBackend.data());
        QObject *errorSource = nullptr;

        const QVariantMap controlSettings = m_restoreSettings.value(QStringLiteral("controlCenter")).toMap();
        const QVariantMap shellSettings = m_restoreSettings.value(QStringLiteral("shell")).toMap();

        if (m_restoreCurrentStep == QLatin1String("control-center")) {
            if (!control || control->busy()) {
                failRestoreStep(tr("Control Center became unavailable or busy."));
                return;
            }
            errorSource = control;
            m_restoreSuccessConnection = connect(control, &ControlCenterBackend::layoutSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            const QVariantMap layout = controlSettings.value(QStringLiteral("layout")).toMap();
            connectRestoreError(errorSource, generation);
            control->saveLayout(layout.value(QStringLiteral("tiles")).toList(),
                                layout.value(QStringLiteral("density")).toString());
        } else if (m_restoreCurrentStep == QLatin1String("top-bar")) {
            if (!control || control->busy()) {
                failRestoreStep(tr("Top Bar became unavailable or busy."));
                return;
            }
            errorSource = control;
            m_restoreSuccessConnection = connect(control, &ControlCenterBackend::topBarSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            connectRestoreError(errorSource, generation);
            control->saveTopBar(controlSettings.value(QStringLiteral("topBar")).toMap());
        } else if (m_restoreCurrentStep == QLatin1String("shelf")) {
            if (!shellBackend || shellBackend->busy()) {
                failRestoreStep(tr("Shelf became unavailable or busy."));
                return;
            }
            errorSource = shellBackend;
            m_restoreSuccessConnection = connect(shellBackend, &ShellSettingsBackend::shelfSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            connectRestoreError(errorSource, generation);
            shellBackend->saveShelf(shellSettings.value(QStringLiteral("shelf")).toMap());
        } else if (m_restoreCurrentStep == QLatin1String("notifications")) {
            if (!shellBackend || shellBackend->busy()) {
                failRestoreStep(tr("Notifications became unavailable or busy."));
                return;
            }
            errorSource = shellBackend;
            m_restoreSuccessConnection = connect(shellBackend, &ShellSettingsBackend::notificationsSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            connectRestoreError(errorSource, generation);
            shellBackend->saveNotifications(shellSettings.value(QStringLiteral("notifications")).toMap());
        } else if (m_restoreCurrentStep == QLatin1String("time-center")) {
            if (!shellBackend || shellBackend->busy()) {
                failRestoreStep(tr("Time Center became unavailable or busy."));
                return;
            }
            errorSource = shellBackend;
            m_restoreSuccessConnection = connect(shellBackend, &ShellSettingsBackend::timeCenterSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            connectRestoreError(errorSource, generation);
            shellBackend->saveTimeCenter(shellSettings.value(QStringLiteral("timeCenter")).toMap());
        } else if (m_restoreCurrentStep == QLatin1String("top-tasks")) {
            if (!shellBackend || shellBackend->busy()) {
                failRestoreStep(tr("Top Tasks became unavailable or busy."));
                return;
            }
            errorSource = shellBackend;
            m_restoreSuccessConnection = connect(shellBackend, &ShellSettingsBackend::topTasksSaved,
                                                 this, &BackupBackend::finishRestoreStep);
            connectRestoreError(errorSource, generation);
            shellBackend->saveTopTasks(shellSettings.value(QStringLiteral("topTasks")).toMap());
        } else {
            failRestoreStep(tr("The restore plan contains an unknown portable setting group."));
            return;
        }

        QTimer::singleShot(30000, this, [this, generation] {
            if (m_restoreState == QLatin1String("applying")
                && generation == m_restoreGeneration) {
                failRestoreStep(tr("The shell did not confirm this setting group within 30 seconds."));
            }
        });
    }

    void connectRestoreError(QObject *source, int generation)
    {
        auto *backend = qobject_cast<BackendBase *>(source);
        if (!backend) {
            failRestoreStep(tr("A restore backend is unavailable."));
            return;
        }
        m_restoreErrorConnection = connect(backend, &BackendBase::errorChanged, this,
                                           [this, backend, generation] {
            if (m_restoreState != QLatin1String("applying")
                || generation != m_restoreGeneration || backend->error().isEmpty()) {
                return;
            }
            failRestoreStep(backend->error());
        });
    }

    OmniStoreAppsBackend *m_appsBackend = nullptr;
    QPointer<QObject> m_controlCenterBackend;
    QPointer<QObject> m_shellSettingsBackend;
    QString m_lastBackupPath;
    QString m_lastBackupAt;
    QString m_previewSummary;
    QString m_previewPath;
    bool m_lastApplicationInventoryIncluded = false;
    bool m_previewValid = false;

    QVariantMap m_restoreSettings;
    QStringList m_restoreQueue;
    QVariantList m_restoreResults;
    QString m_restoreCurrentStep;
    QString m_restoreState = QStringLiteral("idle");
    QString m_restoreSummary;
    int m_restoreCompletedCount = 0;
    int m_restoreGeneration = 0;
    QMetaObject::Connection m_restoreSuccessConnection;
    QMetaObject::Connection m_restoreErrorConnection;
};
