#include "apppermissionsbackend.h"

#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QMap>

#include <algorithm>

namespace {
QStringList values(const QString &serialized)
{
    QStringList result;
    for (const QString &value : serialized.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            result.push_back(trimmed);
        }
    }
    return result;
}

QVariantMap permission(const QString &id, const QString &title, const QString &details,
                       const QString &icon, const QString &tone)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
            {QStringLiteral("details"), details}, {QStringLiteral("icon"), icon},
            {QStringLiteral("tone"), tone}};
}

QString joinedDetails(const QStringList &entries)
{
    return entries.join(QStringLiteral(", "));
}
}

AppPermissionsBackend::AppPermissionsBackend(QObject *parent)
    : BackendBase(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_process.setProcessEnvironment(environment);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_request == Request::None) {
            return;
        }
        setError(m_process.errorString().trimmed());
        m_request = Request::None;
        m_inspecting = false;
        setBusy(false);
        Q_EMIT changed();
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                finishRequest(exitCode, exitStatus);
            });

    m_flatpakPath = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    setAvailable(!m_flatpakPath.isEmpty());
    m_summary = available() ? tr("Permission inventory has not been refreshed.")
                            : tr("Flatpak is not installed; no sandbox permission inventory is available.");
}

QVariantList AppPermissionsBackend::apps() const { return m_apps; }
QString AppPermissionsBackend::summary() const { return m_summary; }
bool AppPermissionsBackend::inspecting() const { return m_inspecting; }
QString AppPermissionsBackend::selectedAppId() const { return m_selectedAppId; }
QVariantList AppPermissionsBackend::selectedPermissions() const { return m_selectedPermissions; }
QString AppPermissionsBackend::sourceDescription() const
{
    return tr("Effective Flatpak sandbox permissions from local metadata");
}

void AppPermissionsBackend::run(const QStringList &arguments, const Request request)
{
    if (m_request != Request::None || busy()) {
        return;
    }
    if (m_flatpakPath.isEmpty()) {
        setError(tr("Flatpak is not installed."));
        return;
    }
    clearError();
    m_output.clear();
    m_request = request;
    m_inspecting = request == Request::Inspect;
    setBusy(true);
    Q_EMIT changed();
    m_process.start(m_flatpakPath, arguments);
}

void AppPermissionsBackend::refresh()
{
    m_flatpakPath = QStandardPaths::findExecutable(QStringLiteral("flatpak"));
    setAvailable(!m_flatpakPath.isEmpty());
    if (m_flatpakPath.isEmpty()) {
        m_apps.clear();
        m_summary = tr("Flatpak is not installed; no sandbox permission inventory is available.");
        clearError();
        Q_EMIT changed();
        return;
    }
    run({QStringLiteral("list"), QStringLiteral("--app"),
         QStringLiteral("--columns=application,name")}, Request::List);
}

void AppPermissionsBackend::inspect(const QString &appId)
{
    const QString id = appId.trimmed();
    if (id.isEmpty()) {
        return;
    }
    m_selectedAppId = id;
    m_selectedPermissions.clear();
    run({QStringLiteral("info"), QStringLiteral("--show-permissions"), id}, Request::Inspect);
}

void AppPermissionsBackend::clearInspection()
{
    if (m_request == Request::Inspect) {
        return;
    }
    m_selectedAppId.clear();
    m_selectedPermissions.clear();
    Q_EMIT changed();
}

void AppPermissionsBackend::finishRequest(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    if (m_request == Request::None) {
        return;
    }
    m_output += m_process.readAllStandardOutput();
    const QString errorText = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    const Request completedRequest = m_request;
    m_request = Request::None;
    m_inspecting = false;
    setBusy(false);

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        setError(errorText.isEmpty() ? tr("Flatpak permission information could not be read.") : errorText);
        Q_EMIT changed();
        return;
    }

    clearError();
    if (completedRequest == Request::List) {
        parseApplicationList(m_output);
    } else if (completedRequest == Request::Inspect) {
        m_selectedPermissions = parsePermissionOutput(m_output);
    }
    Q_EMIT changed();
}

void AppPermissionsBackend::parseApplicationList(const QByteArray &payload)
{
    QVariantList nextApps;
    const auto lines = QString::fromUtf8(payload).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const auto columns = line.split(QLatin1Char('\t'));
        const QString id = columns.value(0).trimmed();
        if (id.isEmpty()) {
            continue;
        }
        const QString name = columns.value(1).trimmed();
        nextApps.push_back(QVariantMap{{QStringLiteral("id"), id},
                                       {QStringLiteral("name"), name.isEmpty() ? id : name},
                                       {QStringLiteral("icon"), QStringLiteral("admin_panel_settings")}});
    }
    std::sort(nextApps.begin(), nextApps.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
                   right.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    m_apps = nextApps;
    m_summary = nextApps.isEmpty() ? tr("No locally installed Flatpak applications were found.")
                                   : tr("%1 sandboxed application(s) found.").arg(nextApps.size());
}

QVariantList AppPermissionsBackend::parsePermissionOutput(const QByteArray &payload)
{
    QVariantList result;
    QString section;
    QMap<QString, QStringList> context;
    QVariantList busRules;

    for (const QString &rawLine : QString::fromUtf8(payload).split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int separator = line.indexOf(QLatin1Char('='));
        if (separator < 1) {
            continue;
        }
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (section == QLatin1String("Context")) {
            context.insert(key, values(value));
        } else if (!section.isEmpty()) {
            busRules.push_back(permission(section + QLatin1Char('.') + key, section,
                                          key + QStringLiteral(": ") + value,
                                          QStringLiteral("dns"), QStringLiteral("neutral")));
        }
    }

    const auto shared = context.value(QStringLiteral("shared"));
    if (!shared.isEmpty()) {
        result.push_back(permission(QStringLiteral("shared"), QObject::tr("Shared services"),
                                    joinedDetails(shared), QStringLiteral("hub"), QStringLiteral("primary")));
    }
    const auto sockets = context.value(QStringLiteral("sockets"));
    if (!sockets.isEmpty()) {
        result.push_back(permission(QStringLiteral("sockets"), QObject::tr("Display, audio & IPC"),
                                    joinedDetails(sockets), QStringLiteral("desktop_windows"),
                                    QStringLiteral("secondary")));
    }
    const auto devices = context.value(QStringLiteral("devices"));
    if (!devices.isEmpty()) {
        result.push_back(permission(QStringLiteral("devices"), QObject::tr("Devices"),
                                    joinedDetails(devices), QStringLiteral("devices"),
                                    QStringLiteral("tertiary")));
    }
    const auto filesystems = context.value(QStringLiteral("filesystems"));
    if (!filesystems.isEmpty()) {
        result.push_back(permission(QStringLiteral("filesystems"), QObject::tr("Files"),
                                    joinedDetails(filesystems), QStringLiteral("folder"),
                                    QStringLiteral("primary")));
    }
    const auto persistent = context.value(QStringLiteral("persistent"));
    if (!persistent.isEmpty()) {
        result.push_back(permission(QStringLiteral("persistent"), QObject::tr("Persistent folders"),
                                    joinedDetails(persistent), QStringLiteral("folder_shared"),
                                    QStringLiteral("secondary")));
    }
    const auto features = context.value(QStringLiteral("features"));
    if (!features.isEmpty()) {
        result.push_back(permission(QStringLiteral("features"), QObject::tr("Sandbox features"),
                                    joinedDetails(features), QStringLiteral("extension"),
                                    QStringLiteral("neutral")));
    }
    for (const auto &rule : busRules) {
        result.push_back(rule);
    }
    return result;
}
