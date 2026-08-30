#pragma once

#include "../core/backendbase.h"

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantList>

class QTimer;

/**
 * Small, testable parsers for the read-only pacman inspection contract.
 *
 * They intentionally consume only output produced with LC_ALL=C.  The
 * application never uses these helpers to form a package-manager mutation.
 */
class SystemUpdatesContract final
{
public:
    static QVariantMap parsePacmanUpdateLine(const QString &line);
    static QVariantMap syncInfoForPackage(const QByteArray &output, const QString &packageName);
    static QStringList configuredRepositoryNames(const QByteArray &pacmanConfig);
    static QString channelForRepositories(const QStringList &repositories);
    static QString updateFamily(const QString &packageName, const QString &repository,
                                const QStringList &foreignPackages,
                                const QStringList &configuredRepositories);
    static bool isCustomRepository(const QString &repository);
    static QVariantMap parseSharedUpdateState(const QByteArray &payload);
};

/**
 * Shows package-update facts from *already downloaded* pacman metadata.
 *
 * `refresh()` runs only `pacman -Qu`, `pacman -Qqm`, and `pacman -Si` with a
 * C locale.  It deliberately never refreshes package databases, invokes
 * `pacman -S*`, uses sudo, or changes packages.  The optional AUR check is a
 * separate explicit network-aware request through the user's configured
 * paru/yay helper and has no install/update action.
 */
class UpdatesBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList updates READ updates NOTIFY changed)
    Q_PROPERTY(QVariantList configuredRepositories READ configuredRepositories NOTIFY changed)
    Q_PROPERTY(QString updateChannel READ updateChannel NOTIFY changed)
    Q_PROPERTY(QVariantList aurUpdates READ aurUpdates NOTIFY changed)
    Q_PROPERTY(QString cachedMetadataTimestamp READ cachedMetadataTimestamp NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString aurSummary READ aurSummary NOTIFY changed)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY changed)
    Q_PROPERTY(int meoUpdateCount READ meoUpdateCount NOTIFY changed)
    Q_PROPERTY(int kdeUpdateCount READ kdeUpdateCount NOTIFY changed)
    Q_PROPERTY(int systemUpdateCount READ systemUpdateCount NOTIFY changed)
    Q_PROPERTY(int customRepositoryUpdateCount READ customRepositoryUpdateCount NOTIFY changed)
    Q_PROPERTY(int aurUpdateCount READ aurUpdateCount NOTIFY changed)
    Q_PROPERTY(bool pacmanAvailable READ pacmanAvailable NOTIFY changed)
    Q_PROPERTY(bool aurHelperAvailable READ aurHelperAvailable NOTIFY changed)
    Q_PROPERTY(bool checkingAur READ checkingAur NOTIFY changed)
    Q_PROPERTY(bool orchestratorAvailable READ orchestratorAvailable NOTIFY changed)
    Q_PROPERTY(int orchestratedUpdateCount READ orchestratedUpdateCount NOTIFY changed)
    Q_PROPERTY(QString orchestratorCheckedAt READ orchestratorCheckedAt NOTIFY changed)
    Q_PROPERTY(QVariantList orchestratorSources READ orchestratorSources NOTIFY changed)

public:
    explicit UpdatesBackend(QObject *parent = nullptr);

    QVariantList updates() const;
    QVariantList configuredRepositories() const;
    QString updateChannel() const;
    QVariantList aurUpdates() const;
    QString cachedMetadataTimestamp() const;
    QString summary() const;
    QString aurSummary() const;
    int updateCount() const;
    int meoUpdateCount() const;
    int kdeUpdateCount() const;
    int systemUpdateCount() const;
    int customRepositoryUpdateCount() const;
    int aurUpdateCount() const;
    bool pacmanAvailable() const;
    bool aurHelperAvailable() const;
    bool checkingAur() const;
    bool orchestratorAvailable() const;
    int orchestratedUpdateCount() const;
    QString orchestratorCheckedAt() const;
    QVariantList orchestratorSources() const;

    /// Reads the local package and sync databases only.
    Q_INVOKABLE void refresh();
    /// Explicitly asks the configured AUR helper to query upgrade candidates.
    /// This has no package mutation but can contact the helper's configured
    /// remote service, so it never runs automatically.
    Q_INVOKABLE void refreshAurUpdates();

Q_SIGNALS:
    void changed();

private:
    enum class Stage {
        Idle,
        SharedState,
        ConfiguredRepositories,
        NativeUpdates,
        ForeignPackages,
        SyncInformation,
        AurUpdates,
    };

    void updateRuntimeAvailability();
    void updateConfiguredRepositories();
    void setConfiguredRepositoryNames(const QStringList &names);
    void updateCachedMetadataTimestamp();
    void startStage(Stage stage, const QString &program, const QStringList &arguments);
    void finishStage(int exitCode, QProcess::ExitStatus exitStatus);
    void finishWithError(const QString &message);
    void captureStandardOutput();
    void discardStandardError();
    void stageTimedOut();
    void applyNativeUpdates();
    void applyAurUpdates();
    void completeOperation();
    QStringList parsedForeignPackages() const;

    QProcess *m_process = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QByteArray m_standardOutput;
    QString m_pacmanPath;
    QString m_pacmanConfPath;
    QString m_aurHelperPath;
    QString m_orchestratorPath;
    Stage m_stage = Stage::Idle;
    bool m_operationActive = false;
    bool m_checkingAur = false;
    bool m_hasNativeSnapshot = false;
    bool m_hasAurSnapshot = false;
    QVariantList m_updates;
    QVariantList m_configuredRepositories;
    QString m_updateChannel = QStringLiteral("unconfigured");
    QVariantList m_aurUpdates;
    QStringList m_candidatePackageNames;
    QStringList m_foreignPackageNames;
    QString m_cachedMetadataTimestamp;
    int m_meoUpdateCount = 0;
    int m_kdeUpdateCount = 0;
    int m_systemUpdateCount = 0;
    int m_customRepositoryUpdateCount = 0;
    int m_orchestratedUpdateCount = 0;
    QString m_orchestratorCheckedAt;
    QVariantList m_orchestratorSources;
};
