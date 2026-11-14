#include "omnistorepreferencesbackend.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocale>
#include <QStandardPaths>
#include <QTimer>

namespace
{
constexpr auto kSchema = "org.meo.omnistore.app-preferences";
constexpr qsizetype kMaximumOutput = 64 * 1024;
const QList<int> kKnownChoices{0, 256, 512, 1024, 2048, 5120, 10240};

QJsonObject lastJsonObject(const QByteArray &payload)
{
    // The ABI prints one JSON object per line; take the last complete one.
    const QList<QByteArray> lines = payload.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QByteArray line = it->trimmed();
        if (!line.startsWith('{'))
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (document.isObject())
            return document.object();
    }
    return {};
}
} // namespace

OmniStorePreferencesBackend::OmniStorePreferencesBackend(QObject *parent)
    : BackendBase(parent)
    , m_process(new QProcess(this))
    , m_timeout(new QTimer(this))
{
    for (int value : kKnownChoices)
        m_cacheLimitChoices.append(value);
    m_timeout->setSingleShot(true);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process->readAllStandardOutput();
        if (m_output.size() > kMaximumOutput) {
            m_process->kill();
            m_output.clear();
        }
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] { m_process->readAllStandardError(); });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &OmniStorePreferencesBackend::finished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(tr("OmniStore could not be started."));
    });
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_process->kill();
        fail(tr("OmniStore did not answer in time."));
    });
    m_managerPath = QStandardPaths::findExecutable(QStringLiteral("omnistore-apps"));
    setAvailable(!m_managerPath.isEmpty());
}

bool OmniStorePreferencesBackend::parsePreferences(const QByteArray &payload, bool *autoUpdate, bool *notifyUpdates,
                                                   int *cacheLimitMb, QVariantList *choices)
{
    const QJsonObject object = lastJsonObject(payload);
    if (object.value(QStringLiteral("schema")).toString() != QLatin1String(kSchema)
        || object.value(QStringLiteral("status")).toString() != QLatin1String("success"))
        return false;
    const QJsonValue update = object.value(QStringLiteral("autoUpdate"));
    const QJsonValue notify = object.value(QStringLiteral("notifyUpdates"));
    const QJsonValue limit = object.value(QStringLiteral("cacheLimitMb"));
    if (!update.isBool() || !notify.isBool() || !limit.isDouble())
        return false;
    const int limitValue = limit.toInt(-1);
    if (limitValue < 0 || limitValue > 1024 * 1024)
        return false;
    QVariantList parsedChoices;
    for (const QJsonValue &choice : object.value(QStringLiteral("cacheLimitChoicesMb")).toArray()) {
        const int value = choice.toInt(-1);
        if (value >= 0 && value <= 1024 * 1024 && !parsedChoices.contains(value))
            parsedChoices.append(value);
    }
    if (parsedChoices.isEmpty())
        for (int value : kKnownChoices)
            parsedChoices.append(value);
    if (!parsedChoices.contains(limitValue))
        parsedChoices.append(limitValue);
    *autoUpdate = update.toBool();
    *notifyUpdates = notify.toBool();
    *cacheLimitMb = limitValue;
    *choices = parsedChoices;
    return true;
}

void OmniStorePreferencesBackend::refresh()
{
    start(QStringLiteral("read"), {QStringLiteral("preferences")});
}

void OmniStorePreferencesBackend::setAutoUpdate(bool enabled)
{
    const QJsonObject change{{QStringLiteral("autoUpdate"), enabled}};
    start(QStringLiteral("write"), {QStringLiteral("set-preferences"),
                                    QString::fromUtf8(QJsonDocument(change).toJson(QJsonDocument::Compact))});
}

void OmniStorePreferencesBackend::setNotifyUpdates(bool enabled)
{
    const QJsonObject change{{QStringLiteral("notifyUpdates"), enabled}};
    start(QStringLiteral("write"), {QStringLiteral("set-preferences"),
                                    QString::fromUtf8(QJsonDocument(change).toJson(QJsonDocument::Compact))});
}

void OmniStorePreferencesBackend::setCacheLimitMb(int megabytes)
{
    if (!m_cacheLimitChoices.contains(megabytes)) {
        setError(tr("That cache limit is not supported."));
        return;
    }
    const QJsonObject change{{QStringLiteral("cacheLimitMb"), megabytes}};
    start(QStringLiteral("write"), {QStringLiteral("set-preferences"),
                                    QString::fromUtf8(QJsonDocument(change).toJson(QJsonDocument::Compact))});
}

void OmniStorePreferencesBackend::trimCachesNow()
{
    start(QStringLiteral("trim"), {QStringLiteral("trim-cache")});
}

void OmniStorePreferencesBackend::start(const QString &operation, const QStringList &arguments)
{
    if (m_managerPath.isEmpty())
        m_managerPath = QStandardPaths::findExecutable(QStringLiteral("omnistore-apps"));
    setAvailable(!m_managerPath.isEmpty());
    if (m_managerPath.isEmpty()) {
        m_supported = false;
        Q_EMIT changed();
        return;
    }
    if (m_process->state() != QProcess::NotRunning) {
        // One request at a time; a newer write replaces an older queued one.
        m_pending = QStringList{operation} + arguments;
        return;
    }
    clearError();
    m_output.clear();
    m_operation = operation;
    setBusy(true);
    m_process->start(m_managerPath, arguments);
    m_timeout->start(operation == QLatin1String("trim") ? 120'000 : 20'000);
}

void OmniStorePreferencesBackend::fail(const QString &message)
{
    m_timeout->stop();
    setBusy(false);
    setError(message);
    Q_EMIT changed();
}

void OmniStorePreferencesBackend::finished(int exitCode, QProcess::ExitStatus status)
{
    m_timeout->stop();
    setBusy(false);
    const QString operation = m_operation;
    m_operation.clear();

    if (status == QProcess::NormalExit && exitCode == 64) {
        // An older OmniStore without the preferences ABI.
        m_supported = false;
        Q_EMIT changed();
    } else if (operation == QLatin1String("trim")) {
        const QJsonObject object = lastJsonObject(m_output);
        if (object.value(QStringLiteral("schema")).toString() == QLatin1String(kSchema)
            && object.value(QStringLiteral("status")).toString() == QLatin1String("success")) {
            const auto freed = static_cast<qulonglong>(qMax(0.0, object.value(QStringLiteral("bytesFreed")).toDouble()));
            m_lastResult = freed > 0 ? tr("Freed %1 of app caches.").arg(QLocale().formattedDataSize(freed))
                                     : tr("App caches are already within the limit.");
            Q_EMIT cachesTrimmed(freed);
            Q_EMIT changed();
        } else {
            fail(tr("App caches could not be cleaned."));
        }
    } else {
        bool autoUpdate = false;
        bool notify = true;
        int limit = 0;
        QVariantList choices;
        if (parsePreferences(m_output, &autoUpdate, &notify, &limit, &choices)) {
            m_supported = true;
            m_autoUpdate = autoUpdate;
            m_notifyUpdates = notify;
            m_cacheLimitMb = limit;
            m_cacheLimitChoices = choices;
            Q_EMIT changed();
        } else if (operation == QLatin1String("write")) {
            fail(tr("The app setting could not be saved."));
        } else {
            m_supported = false;
            Q_EMIT changed();
        }
    }

    if (!m_pending.isEmpty()) {
        const QStringList next = m_pending;
        m_pending.clear();
        start(next.first(), next.mid(1));
    }
}
