#pragma once

#include "../core/backendbase.h"

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
 * Preview is read-only. Applying a preview reopens and revalidates the file,
 * then serializes the already-registered Meo settings capabilities. The backup
 * coordinator intentionally talks to those capabilities through QObject's
 * public property/signal/invokable contract rather than linking their concrete
 * implementations; this keeps the backup manifest contract independently
 * testable and avoids creating a second settings authority.
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
    bool previewValid() const;

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
        resetRestoreStatus();
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

        const bool needsControl = controlCenter.contains(QStringLiteral("layout"))
            || controlCenter.contains(QStringLiteral("topBar"));
        const bool needsShell = !shell.isEmpty();
        if (needsControl && !backendReady(m_controlCenterBackend.data())) {
            setError(tr("The active Meo Control Center is unavailable, so this restore cannot start."));
            return false;
        }
        if (needsShell && !backendReady(m_shellSettingsBackend.data())) {
            setError(tr("The active Meo shell surfaces are unavailable, so this restore cannot start."));
            return false;
        }
        if ((m_controlCenterBackend && m_controlCenterBackend->property("busy").toBool())
            || (m_shellSettingsBackend && m_shellSettingsBackend->property("busy").toBool())) {
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

private Q_SLOTS:
    void handleRestoreStepSaved()
    {
        finishRestoreStep();
    }

    void handleRestoreBackendError()
    {
        if (m_restoreState != QLatin1String("applying") || !m_restoreActiveBackend)
            return;
        const QString detail = m_restoreActiveBackend->property("error").toString();
        if (!detail.isEmpty())
            failRestoreStep(detail);
    }

private:
    QVariantMap portableSettingsSnapshot(QString *error = nullptr) const;
    bool createLocalManifestInternal(const QVariantMap &settings);

    bool backendReady(QObject *backend) const
    {
        return backend && backend->property("available").toBool();
    }

    void resetRestoreStatus()
    {
        if (m_restoreState == QLatin1String("applying"))
            return;
        m_restoreSettings.clear();
        m_restoreQueue.clear();
        m_restoreResults.clear();
        m_restoreCurrentStep.clear();
        m_restoreState = QStringLiteral("idle");
        m_restoreSummary.clear();
        m_restoreCompletedCount = 0;
        ++m_restoreGeneration;
        Q_EMIT changed();
    }

    void disconnectRestoreStep()
    {
        QObject::disconnect(m_restoreSuccessConnection);
        QObject::disconnect(m_restoreErrorConnection);
        m_restoreSuccessConnection = {};
        m_restoreErrorConnection = {};
        m_restoreActiveBackend.clear();
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
        if (m_restoreState != Qatin1String("applying"))
            return;
        disconnectRestoreStep();
        ++m_restoreGeneration;
        appendRestoreResult(m_restoreCurrentStep, QStringLiteral("applied"));
        ++m_restoreCompletedCount;
        startNextRestoreStep();
    }

    bool connectRestoreSignals(QObject *source, const char *successSignal)
    {
        if (!source)
            return false;
        m_restoreActiveBackend = source;
        m_restoreSuccessConnection = QObject::connect(
            source, successSignal, this, SLOT(handleRestoreStepSaved()));
        m_restoreErrorConnection = QObject::connect(
            source, SIGNAL(errorChanged()), this, SLOT(handleRestoreBackendError()));
        return static_cast<bool>(m_restoreSuccessConnection)
            && static_cast<bool>(m_restoreErrorConnection);
    }

    bool invokeMapSave(QObject *source, const char *method, const QVariantMap &settings)
    {
        return QMetaObject::invokeMethod(source, method, Qt::DirectConnection,
                                         Q_ARG(QVariantMap, settings));
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
        QObject *source = nullptr;
        const char *successSignal = nullptr;
        const char *method = nullptr;
        QVariantMap mapArgument;
        bool layoutCall = false;

        const QVariantMap controlSettings = m_restoreSettings.value(QStringLiteral("controlCenter")).toMap();
        const QVariantMap shellSettings = m_restoreSettings.value(QStringLiteral("shell")).toMap();

        if (m_restoreCurrentStep == QLatin1String("control-center")) {
            source = m_controlCenterBackend.data();
            successSignal = SIGNAL(layoutSaved());
            method = "saveLayout";
            layoutCall = true;
            mapArgument = controlSettings.value(QStringLiteral("layout")).toMap();
        } else if (m_restoreCurrentStep == QLatin1String("top-bar")) {
            source = m_controlCenterBackend.data();
            successSignal = SIGNAL(topBarSaved());
            method = "saveTopBar";
            mapArgument = controlSettings.value(QStringLiteral("topBar")).toMap();
        } else if (m_restoreCurrentStep == QLatin1String("shelf")) {
            source = m_shellSettingsBackend.data();
            successSignal = SIGNAL(shelfSaved());
            method = "saveShelf";
            mapArgument = shellSettings.value(QStringLiteral("shelf")).toMap();
        } else if (m_restoreCurrentStep == QLatin1String("notifications")) {
            source = m_shellSettingsBackend.data();
            successSignal = SIGNAL(notificationsSaved());
            method = "saveNotifications";
            mapArgument = shellSettings.value(QStringLiteral("notifications")).toMap();
        } else if (m_restoreCurrentStep == QLatin1String("time-center")) {
            source = m_shellSettingsBackend.data();
            successSignal = SIGNAL(timeCenterSaved());
            method = "saveTimeCenter";
            mapArgument = shellSettings.value(QStringLiteral("timeCenter")).toMap();
        } else if (m_restoreCurrentStep == QLatin1String("top-tasks")) {
            source = m_shellSettingsBackend.data();
            successSignal = SIGNAL(topTasksSaved());
            method = "saveTopTasks";
            mapArgument = shellSettings.value(QStringLiteral("topTasks")).toMap();
        } else {
            failRestoreStep(tr("The restore plan contains an unknown portable setting group."));
            return;
        }

        if (!backendReady(source) || source->property("busy").toBool()) {
            failRestoreStep(tr("The target shell settings surface became unavailable or busy."));
            return;
        }
        if (!connectRestoreSignals(source, successSignal)) {
            failRestoreStep(tr("The target shell settings surface does not expose the required restore confirmation signal."));
            return;
        }

        bool invoked = false;
        if (layoutCall) {
            const QVariantList tiles = mapArgument.value(QStringLiteral("tiles")).toList();
            const QString density = mapArgument.value(QStringLiteral("density")).toString();
            invoked = QMetaObject::invokeMethod(source, method, Qt::DirectConnection,
                                                Q_ARG(QVariantList, tiles),
                                                Q_ARG(QString, density));
        } else {
            invoked = invokeMapSave(source, method, mapArgument);
        }
        if (!invoked) {
            failRestoreStep(tr("The target shell settings surface does not expose the required restore operation."));
            return;
        }

        QTimer::singleShot(30000, this, [this, generation] {
            if (m_restoreState == QLatin1String("applying")
                && generation == m_restoreGeneration) {
                failRestoreStep(tr("The shell did not confirm this setting group within 30 seconds."));
            }
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
    QPointer<QObject> m_restoreActiveBackend;
    QMetaObject::Connection m_restoreSuccessConnection;
    QMetaObject::Connection m_restoreErrorConnection;
};
