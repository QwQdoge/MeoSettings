#include "omnistoreappsbackend.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr auto kSchema = "org.meo.omnistore.installed-usage";
constexpr int kSchemaVersion = 1;
constexpr qsizetype kMaximumPayloadBytes = 4 * 1024 * 1024;
constexpr int kMaximumApplications = 10'000;
constexpr int kMaximumTopApplications = 6;
constexpr qulonglong kMaximumSizeBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

struct SourceAccumulator
{
    QString id;
    QString name;
    int applicationCount = 0;
    int unknownSizeCount = 0;
    int exactSizeCount = 0;
    int reportedSizeCount = 0;
    qulonglong knownSizeBytes = 0;
};

bool isSafeDisplayText(const QString &value, const int maximum)
{
    if (value.isEmpty() || value.size() > maximum) {
        return false;
    }
    return std::none_of(value.cbegin(), value.cend(), [](const QChar character) {
        return character.isNull() || character.category() == QChar::Other_Control;
    });
}

std::optional<qulonglong> nonNegativeInteger(const QJsonValue &value, const qulonglong maximum)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0 || number > static_cast<double>(maximum)
        || std::floor(number) != number) {
        return std::nullopt;
    }
    return static_cast<qulonglong>(number);
}

void setParseError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString sourceSortName(const QVariant &value)
{
    return value.toMap().value(QStringLiteral("name")).toString().toCaseFolded();
}
} // namespace

std::optional<OmniStoreAppsSnapshot> OmniStoreAppsContract::parse(const QByteArray &payload,
                                                                    QString *error)
{
    if (payload.isEmpty() || payload.size() > kMaximumPayloadBytes) {
        setParseError(error, QStringLiteral("The app overview is empty or too large."));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setParseError(error, QStringLiteral("The app overview is not valid JSON."));
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("schema")).toString() != QLatin1String(kSchema)
        || object.value(QStringLiteral("version")).toInt(-1) != kSchemaVersion
        || object.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        setParseError(error, QStringLiteral("The app overview uses an unsupported OmniStore contract."));
        return std::nullopt;
    }

    const QString generatedAt = object.value(QStringLiteral("generatedAt")).toString();
    if (generatedAt.size() > 64
        || (!QDateTime::fromString(generatedAt, Qt::ISODateWithMs).isValid()
            && !QDateTime::fromString(generatedAt, Qt::ISODate).isValid())) {
        setParseError(error, QStringLiteral("The app overview has an invalid timestamp."));
        return std::nullopt;
    }

    const QJsonValue applicationsValue = object.value(QStringLiteral("applications"));
    const QJsonValue sourcesValue = object.value(QStringLiteral("sources"));
    if (!applicationsValue.isArray() || !sourcesValue.isArray()) {
        setParseError(error, QStringLiteral("The app overview is missing its application data."));
        return std::nullopt;
    }
    const QJsonArray applicationsArray = applicationsValue.toArray();
    if (applicationsArray.size() > kMaximumApplications) {
        setParseError(error, QStringLiteral("The app overview contains too many applications."));
        return std::nullopt;
    }

    const auto declaredApplicationCount = nonNegativeInteger(
        object.value(QStringLiteral("applicationCount")), static_cast<qulonglong>(kMaximumApplications));
    if (!declaredApplicationCount || *declaredApplicationCount != static_cast<qulonglong>(applicationsArray.size())) {
        setParseError(error, QStringLiteral("The app overview has an inconsistent application count."));
        return std::nullopt;
    }

    QVariantList applicationRows;
    applicationRows.reserve(applicationsArray.size());
    QHash<QString, SourceAccumulator> sourceAccumulators;
    qulonglong knownSizeBytes = 0;
    int unknownSizeCount = 0;
    int exactSizeCount = 0;
    int reportedSizeCount = 0;

    const QRegularExpression sourceIdExpression(QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
    for (const QJsonValue &applicationValue : applicationsArray) {
        if (!applicationValue.isObject()) {
            setParseError(error, QStringLiteral("The app overview has an invalid application record."));
            return std::nullopt;
        }
        const QJsonObject application = applicationValue.toObject();
        const QString name = application.value(QStringLiteral("name")).toString();
        const QString sourceId = application.value(QStringLiteral("sourceId")).toString();
        const QString sourceName = application.value(QStringLiteral("sourceName")).toString();
        const QString sizeKind = application.value(QStringLiteral("sizeKind")).toString();
        if (!isSafeDisplayText(name, 256) || !isSafeDisplayText(sourceName, 80)
            || !sourceIdExpression.match(sourceId).hasMatch()
            || (sizeKind != QLatin1String("exact") && sizeKind != QLatin1String("reported")
                && sizeKind != QLatin1String("unknown"))) {
            setParseError(error, QStringLiteral("The app overview has an unsafe application record."));
            return std::nullopt;
        }

        std::optional<qulonglong> sizeBytes;
        if (application.contains(QStringLiteral("sizeBytes"))) {
            sizeBytes = nonNegativeInteger(application.value(QStringLiteral("sizeBytes")), kMaximumSizeBytes);
            if (!sizeBytes) {
                setParseError(error, QStringLiteral("The app overview has an invalid size value."));
                return std::nullopt;
            }
        }
        if ((sizeKind == QLatin1String("unknown")) != !sizeBytes) {
            setParseError(error, QStringLiteral("The app overview has inconsistent size evidence."));
            return std::nullopt;
        }

        auto accumulator = sourceAccumulators.value(sourceId);
        if (accumulator.applicationCount == 0) {
            accumulator.id = sourceId;
            accumulator.name = sourceName;
        } else if (accumulator.name != sourceName) {
            setParseError(error, QStringLiteral("The app overview has inconsistent source metadata."));
            return std::nullopt;
        }
        ++accumulator.applicationCount;

        QVariantMap row{
            {QStringLiteral("name"), name},
            {QStringLiteral("sourceId"), sourceId},
            {QStringLiteral("sourceName"), sourceName},
            {QStringLiteral("sizeKind"), sizeKind},
        };
        if (sizeBytes) {
            if (*sizeBytes > kMaximumSizeBytes - knownSizeBytes) {
                setParseError(error, QStringLiteral("The app overview size total is too large."));
                return std::nullopt;
            }
            knownSizeBytes += *sizeBytes;
            accumulator.knownSizeBytes += *sizeBytes;
            row.insert(QStringLiteral("sizeBytes"), QVariant::fromValue(*sizeBytes));
            if (sizeKind == QLatin1String("exact")) {
                ++exactSizeCount;
                ++accumulator.exactSizeCount;
            } else {
                ++reportedSizeCount;
                ++accumulator.reportedSizeCount;
            }
        } else {
            ++unknownSizeCount;
            ++accumulator.unknownSizeCount;
        }
        sourceAccumulators.insert(sourceId, accumulator);
        applicationRows.push_back(row);
    }

    const auto declaredKnownSize = nonNegativeInteger(object.value(QStringLiteral("knownSizeBytes")), kMaximumSizeBytes);
    const auto declaredUnknownCount = nonNegativeInteger(
        object.value(QStringLiteral("unknownSizeCount")), static_cast<qulonglong>(kMaximumApplications));
    if (!declaredKnownSize || !declaredUnknownCount || *declaredKnownSize != knownSizeBytes
        || *declaredUnknownCount != static_cast<qulonglong>(unknownSizeCount)) {
        setParseError(error, QStringLiteral("The app overview has inconsistent size totals."));
        return std::nullopt;
    }

    QVariantList sourceRows;
    sourceRows.reserve(sourceAccumulators.size());
    for (auto iterator = sourceAccumulators.cbegin(); iterator != sourceAccumulators.cend(); ++iterator) {
        const SourceAccumulator &source = iterator.value();
        sourceRows.push_back(QVariantMap{
            {QStringLiteral("id"), source.id},
            {QStringLiteral("name"), source.name},
            {QStringLiteral("applicationCount"), source.applicationCount},
            {QStringLiteral("knownSizeBytes"), QVariant::fromValue(source.knownSizeBytes)},
            {QStringLiteral("unknownSizeCount"), source.unknownSizeCount},
            {QStringLiteral("exactSizeCount"), source.exactSizeCount},
            {QStringLiteral("reportedSizeCount"), source.reportedSizeCount},
            {QStringLiteral("sharePercent"),
             knownSizeBytes > 0
                 ? (static_cast<double>(source.knownSizeBytes) * 100.0
                    / static_cast<double>(knownSizeBytes))
                 : 0.0},
        });
    }
    std::sort(sourceRows.begin(), sourceRows.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap first = left.toMap();
        const QVariantMap second = right.toMap();
        const qulonglong firstSize = first.value(QStringLiteral("knownSizeBytes")).toULongLong();
        const qulonglong secondSize = second.value(QStringLiteral("knownSizeBytes")).toULongLong();
        if (firstSize != secondSize) {
            return firstSize > secondSize;
        }
        return sourceSortName(left) < sourceSortName(right);
    });
    std::sort(applicationRows.begin(), applicationRows.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap first = left.toMap();
        const QVariantMap second = right.toMap();
        const qulonglong firstSize = first.value(QStringLiteral("sizeBytes")).toULongLong();
        const qulonglong secondSize = second.value(QStringLiteral("sizeBytes")).toULongLong();
        if (firstSize != secondSize) {
            return firstSize > secondSize;
        }
        const int nameOrder = first.value(QStringLiteral("name")).toString()
                                  .localeAwareCompare(second.value(QStringLiteral("name")).toString());
        if (nameOrder != 0) {
            return nameOrder < 0;
        }
        return first.value(QStringLiteral("sourceId")).toString()
            < second.value(QStringLiteral("sourceId")).toString();
    });
    while (applicationRows.size() > kMaximumTopApplications) {
        applicationRows.removeLast();
    }

    OmniStoreAppsSnapshot snapshot;
    snapshot.sources = std::move(sourceRows);
    snapshot.topApplications = std::move(applicationRows);
    snapshot.applicationCount = static_cast<int>(*declaredApplicationCount);
    snapshot.knownSizeBytes = knownSizeBytes;
    snapshot.unknownSizeCount = unknownSizeCount;
    snapshot.exactSizeCount = exactSizeCount;
    snapshot.reportedSizeCount = reportedSizeCount;
    snapshot.generatedAt = generatedAt;
    return snapshot;
}

OmniStoreAppsBackend::OmniStoreAppsBackend(QObject *parent)
    : BackendBase(parent)
    , m_process(new QProcess(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &OmniStoreAppsBackend::captureStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &OmniStoreAppsBackend::discardStandardError);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &OmniStoreAppsBackend::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        processError();
    });
    connect(m_timeoutTimer, &QTimer::timeout, this, &OmniStoreAppsBackend::processTimedOut);
    refresh();
}

QVariantList OmniStoreAppsBackend::sources() const
{
    return m_sources;
}

QVariantList OmniStoreAppsBackend::topApplications() const
{
    return m_topApplications;
}

int OmniStoreAppsBackend::applicationCount() const
{
    return m_applicationCount;
}

qulonglong OmniStoreAppsBackend::knownSizeBytes() const
{
    return m_knownSizeBytes;
}

int OmniStoreAppsBackend::unknownSizeCount() const
{
    return m_unknownSizeCount;
}

int OmniStoreAppsBackend::exactSizeCount() const
{
    return m_exactSizeCount;
}

int OmniStoreAppsBackend::reportedSizeCount() const
{
    return m_reportedSizeCount;
}

QString OmniStoreAppsBackend::generatedAt() const
{
    return m_generatedAt;
}

QString OmniStoreAppsBackend::summary() const
{
    if (!m_hasSnapshot) {
        return exporterAvailable()
            ? tr("Waiting for OmniStore's read-only app overview")
            : tr("Install OmniStore to view its managed applications");
    }
    return tr("%n installed application(s) reported by OmniStore", "", m_applicationCount);
}

bool OmniStoreAppsBackend::exporterAvailable() const
{
    return !m_exporterPath.isEmpty();
}

bool OmniStoreAppsBackend::launcherAvailable() const
{
    return !m_launcherPath.isEmpty();
}

void OmniStoreAppsBackend::refresh()
{
    if (m_requestActive) {
        return;
    }
    updateExecutableAvailability();
    clearError();
    if (!exporterAvailable()) {
        if (!m_hasSnapshot) {
            setAvailable(false);
        }
        setError(tr("The OmniStore app overview exporter is not installed."));
        Q_EMIT changed();
        return;
    }

    m_standardOutput.clear();
    m_requestActive = true;
    setBusy(true);
    m_process->setProgram(m_exporterPath);
    m_process->setArguments(QStringList{});
    m_process->start();
    m_timeoutTimer->start(20'000);
    Q_EMIT changed();
}

bool OmniStoreAppsBackend::openOmniStore()
{
    updateExecutableAvailability();
    if (!launcherAvailable()) {
        setError(tr("OmniStore is not installed."));
        Q_EMIT changed();
        return false;
    }
    if (!QProcess::startDetached(m_launcherPath, QStringList{})) {
        setError(tr("OmniStore could not be started."));
        Q_EMIT changed();
        return false;
    }
    return true;
}

void OmniStoreAppsBackend::updateExecutableAvailability()
{
    const QString exporter = QStandardPaths::findExecutable(QStringLiteral("omnistore-apps-export"));
    const QString launcher = QStandardPaths::findExecutable(QStringLiteral("omnistore"));
    if (m_exporterPath == exporter && m_launcherPath == launcher) {
        return;
    }
    m_exporterPath = exporter;
    m_launcherPath = launcher;
    Q_EMIT changed();
}

void OmniStoreAppsBackend::captureStandardOutput()
{
    if (!m_requestActive) {
        m_process->readAllStandardOutput();
        return;
    }
    m_standardOutput.append(m_process->readAllStandardOutput());
    if (m_standardOutput.size() > kMaximumPayloadBytes) {
        m_process->kill();
        finishWithError(tr("OmniStore returned an app overview that is too large."));
    }
}

void OmniStoreAppsBackend::discardStandardError()
{
    // The exporter may log implementation diagnostics to stderr.  They are not
    // part of the public contract and can contain local paths, so do not expose
    // them to QML.  Reading them prevents a verbose child process from blocking.
    m_process->readAllStandardError();
}

void OmniStoreAppsBackend::processFinished(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    if (!m_requestActive) {
        return;
    }
    captureStandardOutput();
    if (!m_requestActive) {
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        finishWithError(tr("OmniStore could not provide a current app overview."));
        return;
    }

    QString parseError;
    const auto snapshot = OmniStoreAppsContract::parse(m_standardOutput, &parseError);
    if (!snapshot) {
        finishWithError(tr("OmniStore returned an unsupported app overview."));
        return;
    }
    m_requestActive = false;
    m_timeoutTimer->stop();
    setBusy(false);
    applySnapshot(*snapshot);
}

void OmniStoreAppsBackend::processError()
{
    if (!m_requestActive) {
        return;
    }
    finishWithError(tr("OmniStore's app overview exporter could not be started."));
}

void OmniStoreAppsBackend::processTimedOut()
{
    if (!m_requestActive) {
        return;
    }
    m_process->kill();
    finishWithError(tr("OmniStore took too long to provide an app overview."));
}

void OmniStoreAppsBackend::applySnapshot(const OmniStoreAppsSnapshot &snapshot)
{
    const bool changed = m_sources != snapshot.sources
        || m_topApplications != snapshot.topApplications
        || m_applicationCount != snapshot.applicationCount
        || m_knownSizeBytes != snapshot.knownSizeBytes
        || m_unknownSizeCount != snapshot.unknownSizeCount
        || m_exactSizeCount != snapshot.exactSizeCount
        || m_reportedSizeCount != snapshot.reportedSizeCount
        || m_generatedAt != snapshot.generatedAt
        || !m_hasSnapshot;
    m_sources = snapshot.sources;
    m_topApplications = snapshot.topApplications;
    m_applicationCount = snapshot.applicationCount;
    m_knownSizeBytes = snapshot.knownSizeBytes;
    m_unknownSizeCount = snapshot.unknownSizeCount;
    m_exactSizeCount = snapshot.exactSizeCount;
    m_reportedSizeCount = snapshot.reportedSizeCount;
    m_generatedAt = snapshot.generatedAt;
    m_hasSnapshot = true;
    setAvailable(true);
    clearError();
    if (changed) {
        Q_EMIT this->changed();
    }
}

void OmniStoreAppsBackend::finishWithError(const QString &error)
{
    m_requestActive = false;
    m_timeoutTimer->stop();
    setBusy(false);
    if (!m_hasSnapshot) {
        setAvailable(false);
    }
    setError(error);
    Q_EMIT changed();
}
