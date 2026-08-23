#pragma once

#include "../core/backendbase.h"

#include <QProcess>
#include <QString>
#include <QVariantList>

/**
 * Read-only projection of Flatpak's effective sandbox permission metadata.
 *
 * Desktop Linux does not provide a trustworthy universal historical “which
 * native application used a permission” API. This backend therefore exposes
 * only a source that can be verified locally: `flatpak info
 * --show-permissions`. It never requests a portal grant, changes a sandbox,
 * starts an application, or talks to a network service.
 */
class AppPermissionsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList apps READ apps NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool inspecting READ inspecting NOTIFY changed)
    Q_PROPERTY(QString selectedAppId READ selectedAppId NOTIFY changed)
    Q_PROPERTY(QVariantList selectedPermissions READ selectedPermissions NOTIFY changed)
    Q_PROPERTY(QString sourceDescription READ sourceDescription NOTIFY changed)

public:
    explicit AppPermissionsBackend(QObject *parent = nullptr);

    QVariantList apps() const;
    QString summary() const;
    bool inspecting() const;
    QString selectedAppId() const;
    QVariantList selectedPermissions() const;
    QString sourceDescription() const;

    /// Re-reads only the locally installed Flatpak application list.
    Q_INVOKABLE void refresh();
    /// Reads one selected app's local effective Flatpak sandbox metadata.
    Q_INVOKABLE void inspect(const QString &appId);
    Q_INVOKABLE void clearInspection();

    /// Public deterministic parser for the Flatpak permission format. It is
    /// intentionally independent of a host Flatpak installation.
    static QVariantList parsePermissionOutput(const QByteArray &payload);

Q_SIGNALS:
    void changed();

private:
    enum class Request { None, List, Inspect };

    void run(const QStringList &arguments, Request request);
    void finishRequest(int exitCode, QProcess::ExitStatus exitStatus);
    void parseApplicationList(const QByteArray &payload);

    QProcess m_process;
    QString m_flatpakPath;
    Request m_request = Request::None;
    QByteArray m_output;
    QVariantList m_apps;
    QVariantList m_selectedPermissions;
    QString m_summary;
    QString m_selectedAppId;
    bool m_inspecting = false;
};
