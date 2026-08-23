#pragma once

#include "../core/backendbase.h"

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QVariantList>

class QTimer;

/**
 * Read-only parsers for Pacman's local installed-package database output.
 *
 * They deliberately consume only output produced with `LC_ALL=C`.  A foreign
 * package is an AUR *candidate*, not proof that it came from AUR: local,
 * manually built packages are foreign too.  The UI preserves that distinction.
 */
class PackageInventoryContract final
{
public:
    static QVariantList parsePacmanLocalInfo(const QByteArray &output);
    static QVariantList summarize(const QVariantList &packages,
                                  const QStringList &foreignPackageNames);
};

/**
 * An explicit, local-only Pacman storage inventory.
 *
 * It runs only after the user asks to inspect installed packages.  The fixed
 * commands are `pacman -Qi` and `pacman -Qqm`; no database refresh, package
 * action, elevated privilege, helper, or network access is involved.
 */
class PackageInventoryBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool snapshotAvailable READ snapshotAvailable NOTIFY changed)
    Q_PROPERTY(bool pacmanAvailable READ pacmanAvailable NOTIFY changed)

public:
    explicit PackageInventoryBackend(QObject *parent = nullptr);

    QVariantList groups() const;
    QString summary() const;
    bool snapshotAvailable() const;
    bool pacmanAvailable() const;

    /// Explicitly inspect the local installed-package database. No package
    /// metadata is downloaded and no package state is changed.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    enum class Stage {
        Idle,
        LocalInfo,
        ForeignNames,
    };

    void updateAvailability();
    void startStage(Stage stage, const QStringList &arguments);
    void finishStage(int exitCode, QProcess::ExitStatus exitStatus);
    void finishWithError(const QString &message);
    void captureOutput();
    void discardErrorOutput();
    void timeoutCurrentStage();
    QStringList parsedForeignPackageNames() const;

    QProcess *m_process = nullptr;
    QTimer *m_timeout = nullptr;
    QByteArray m_output;
    QString m_pacmanPath;
    QVariantList m_packages;
    QVariantList m_groups;
    QString m_summary;
    Stage m_stage = Stage::Idle;
    bool m_snapshotAvailable = false;
    bool m_outputLimitExceeded = false;
};
