#include "updateschedulebackend.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>

namespace
{
constexpr auto kTimerUnit = "omnistore-update.timer";
constexpr int kPackagedIntervalHours = 6;
const QList<int> kChoices{1, 3, 6, 12, 24, 72, 168};

QString timerUnit()
{
    return QString::fromLatin1(kTimerUnit);
}

bool timerInstalled()
{
    const QStringList roots{QStringLiteral("/usr/lib/systemd/user"), QStringLiteral("/etc/systemd/user"),
                            QDir::homePath() + QStringLiteral("/.config/systemd/user")};
    for (const QString &root : roots) {
        if (QFileInfo::exists(root + QLatin1Char('/') + timerUnit()))
            return true;
    }
    return false;
}

QDBusMessage managerCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.systemd1"),
                                          QStringLiteral("/org/freedesktop/systemd1"),
                                          QStringLiteral("org.freedesktop.systemd1.Manager"), method);
}
} // namespace

UpdateScheduleBackend::UpdateScheduleBackend(QObject *parent)
    : BackendBase(parent)
{
    // Startup stays file-only; the Updates page calls refresh(), which also
    // asks the systemd user manager whether the timer is enabled.
    setAvailable(timerInstalled());
    QFile file(dropInPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        m_intervalHours = parseIntervalHours(QString::fromUtf8(file.readAll()), kPackagedIntervalHours);
}

QString UpdateScheduleBackend::dropInPath()
{
    // The systemd user manager reads ~/.config/systemd/user, independent of
    // the Meo session's private XDG_CONFIG_HOME.
    return QDir::homePath() + QStringLiteral("/.config/systemd/user/") + timerUnit()
        + QStringLiteral(".d/interval.conf");
}

int UpdateScheduleBackend::parseIntervalHours(const QString &dropIn, int fallback)
{
    static const QRegularExpression line(QStringLiteral("^\\s*OnUnitInactiveSec\\s*=\\s*(\\d+)\\s*(h|hour|hours|d|day|days|w|week|weeks)?\\s*$"),
                                         QRegularExpression::MultilineOption);
    int hours = fallback;
    auto it = line.globalMatch(dropIn);
    while (it.hasNext()) {
        const auto match = it.next();
        const int value = match.captured(1).toInt();
        const QString unit = match.captured(2);
        if (unit.startsWith(QLatin1Char('d')))
            hours = value * 24;
        else if (unit.startsWith(QLatin1Char('w')))
            hours = value * 168;
        else
            hours = value; // systemd's default unit is seconds, but Meo always writes hours
    }
    return hours > 0 ? hours : fallback;
}

QVariantList UpdateScheduleBackend::choices() const
{
    QVariantList out;
    out.append(QVariantMap{{QStringLiteral("value"), 0}, {QStringLiteral("label"), tr("Off – check only when I ask")}});
    for (int hours : kChoices) {
        QString label;
        if (hours == 1)
            label = tr("Every hour");
        else if (hours < 24)
            label = tr("Every %1 hours").arg(hours);
        else if (hours == 24)
            label = tr("Daily");
        else if (hours == 168)
            label = tr("Weekly");
        else
            label = tr("Every %1 days").arg(hours / 24);
        out.append(QVariantMap{{QStringLiteral("value"), hours}, {QStringLiteral("label"), label}});
    }
    return out;
}

bool UpdateScheduleBackend::callManager(const QString &method, const QVariantList &arguments, QString *error) const
{
    QDBusMessage message = managerCall(method);
    message.setArguments(arguments);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error)
            *error = reply.errorMessage();
        return false;
    }
    return true;
}

QString UpdateScheduleBackend::unitFileState() const
{
    QDBusMessage message = managerCall(QStringLiteral("GetUnitFileState"));
    message.setArguments({timerUnit()});
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
    return reply.isValid() ? reply.value() : QString();
}

void UpdateScheduleBackend::refresh()
{
    const bool installed = timerInstalled();
    setAvailable(installed);
    int hours = kPackagedIntervalHours;
    QFile file(dropInPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        hours = parseIntervalHours(QString::fromUtf8(file.readAll()), kPackagedIntervalHours);
    if (installed) {
        const QString state = unitFileState();
        if (state == QLatin1String("disabled") || state == QLatin1String("masked"))
            hours = 0;
    }
    m_intervalHours = hours;
    Q_EMIT changed();
}

void UpdateScheduleBackend::setIntervalHours(int hours)
{
    if (!available()) {
        setError(tr("OmniStore's background update check is not installed."));
        return;
    }
    if (hours != 0 && !kChoices.contains(hours)) {
        setError(tr("That update check interval is not supported."));
        return;
    }
    clearError();
    QString error;
    if (hours == 0) {
        callManager(QStringLiteral("StopUnit"), {timerUnit(), QStringLiteral("replace")});
        if (!callManager(QStringLiteral("DisableUnitFiles"), {QStringList{timerUnit()}, false}, &error)) {
            setError(tr("Automatic update checks could not be turned off: %1").arg(error));
            refresh();
            return;
        }
        refresh();
        return;
    }

    const QString path = dropInPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(tr("The update check schedule could not be saved."));
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(tr("The update check schedule could not be saved."));
        return;
    }
    const QByteArray content = QByteArrayLiteral("# Written by Meo Settings: how often OmniStore checks for updates.\n"
                                                 "[Timer]\n"
                                                 "OnUnitInactiveSec=\n")
        + QByteArrayLiteral("OnUnitInactiveSec=") + QByteArray::number(hours) + QByteArrayLiteral("h\n");
    file.write(content);
    if (!file.commit()) {
        setError(tr("The update check schedule could not be saved."));
        return;
    }

    if (!callManager(QStringLiteral("Reload"), {}, &error)) {
        setError(tr("The new schedule is saved and applies after you sign in again (%1).").arg(error));
        refresh();
        return;
    }
    const QString state = unitFileState();
    if (state == QLatin1String("disabled")) {
        callManager(QStringLiteral("EnableUnitFiles"), {QStringList{timerUnit()}, false, true}, &error);
    }
    // Restart so the timer recalculates its next elapse from the new interval.
    if (!callManager(QStringLiteral("RestartUnit"), {timerUnit(), QStringLiteral("replace")}, &error))
        setError(tr("The new schedule is saved and applies after you sign in again (%1).").arg(error));
    refresh();
}
