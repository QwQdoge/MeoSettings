#include "packageinventorybackend.h"

#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace
{
constexpr qsizetype kMaximumOutputBytes = 12 * 1024 * 1024;
constexpr int kStageTimeoutMs = 20'000;

const QRegularExpression kPackageNameExpression(
    QStringLiteral("^[A-Za-z0-9@._+:-]{1,128}$"));
const QRegularExpression kSizeExpression(
    QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*(B|KiB|MiB|GiB|TiB)$"));

QString safePackageName(const QString &candidate)
{
    const QString name = candidate.trimmed();
    return kPackageNameExpression.match(name).hasMatch() ? name : QString{};
}

bool parseInstalledSize(const QString &candidate, qulonglong *result)
{
    if (!result) {
        return false;
    }
    const auto match = kSizeExpression.match(candidate.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    bool numberOk = false;
    const long double value = match.captured(1).toDouble(&numberOk);
    if (!numberOk || value < 0) {
        return false;
    }
    const QString unit = match.captured(2);
    long double multiplier = 1;
    if (unit == QLatin1String("KiB")) {
        multiplier = 1024;
    } else if (unit == QLatin1String("MiB")) {
        multiplier = 1024 * 1024;
    } else if (unit == QLatin1String("GiB")) {
        multiplier = 1024 * 1024 * 1024;
    } else if (unit == QLatin1String("TiB")) {
        multiplier = static_cast<long double>(1024) * 1024 * 1024 * 1024;
    }
    const long double bytes = value * multiplier;
    if (bytes > static_cast<long double>(std::numeric_limits<qulonglong>::max())) {
        return false;
    }
    *result = static_cast<qulonglong>(bytes + 0.5L);
    return true;
}

QString fieldValue(const QString &line, const QString &field)
{
    const int separator = line.indexOf(QLatin1Char(':'));
    if (separator < 0 || line.left(separator).trimmed() != field) {
        return {};
    }
    return line.mid(separator + 1).trimmed();
}

QProcessEnvironment cLocaleEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    return environment;
}
} // namespace

QVariantList PackageInventoryContract::parsePacmanLocalInfo(const QByteArray &output)
{
    QVariantList packages;
    QString name;
    qulonglong sizeBytes = 0;
    bool sizeKnown = false;

    const auto flush = [&] {
        if (name.isEmpty()) {
            return;
        }
        packages.push_back(QVariantMap{{QStringLiteral("name"), name},
                                       {QStringLiteral("sizeBytes"), QVariant::fromValue(sizeBytes)},
                                       {QStringLiteral("sizeKnown"), sizeKnown}});
        name.clear();
        sizeBytes = 0;
        sizeKnown = false;
    };

    for (const QString &rawLine : QString::fromUtf8(output).split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            flush();
            continue;
        }
        const QString rawName = fieldValue(line, QStringLiteral("Name"));
        if (!rawName.isEmpty()) {
            flush();
            name = safePackageName(rawName);
            continue;
        }
        if (name.isEmpty()) {
            continue;
        }
        qulonglong parsedSize = 0;
        if (parseInstalledSize(fieldValue(line, QStringLiteral("Installed Size")), &parsedSize)) {
            sizeBytes = parsedSize;
            sizeKnown = true;
        }
    }
    flush();
    return packages;
}

QVariantList PackageInventoryContract::summarize(const QVariantList &packages,
                                                  const QStringList &foreignPackageNames)
{
    QSet<QString> foreign;
    for (const QString &candidate : foreignPackageNames) {
        const QString name = safePackageName(candidate);
        if (!name.isEmpty()) {
            foreign.insert(name.toCaseFolded());
        }
    }

    struct Totals {
        int packageCount = 0;
        int sizeKnownCount = 0;
        qulonglong knownSizeBytes = 0;
    } official, foreignTotals;

    for (const QVariant &value : packages) {
        const QVariantMap package = value.toMap();
        const QString name = safePackageName(package.value(QStringLiteral("name")).toString());
        if (name.isEmpty()) {
            continue;
        }
        Totals &totals = foreign.contains(name.toCaseFolded()) ? foreignTotals : official;
        ++totals.packageCount;
        if (!package.value(QStringLiteral("sizeKnown")).toBool()) {
            continue;
        }
        const qulonglong size = package.value(QStringLiteral("sizeBytes")).toULongLong();
        if (size > std::numeric_limits<qulonglong>::max() - totals.knownSizeBytes) {
            continue;
        }
        totals.knownSizeBytes += size;
        ++totals.sizeKnownCount;
    }

    const auto group = [](const QString &id, const Totals &totals) {
        return QVariantMap{{QStringLiteral("id"), id},
                           {QStringLiteral("packageCount"), totals.packageCount},
                           {QStringLiteral("sizeKnownCount"), totals.sizeKnownCount},
                           {QStringLiteral("knownSizeBytes"), QVariant::fromValue(totals.knownSizeBytes)}};
    };
    return {group(QStringLiteral("pacman"), official),
            group(QStringLiteral("foreign"), foreignTotals)};
}

PackageInventoryBackend::PackageInventoryBackend(QObject *parent)
    : BackendBase(parent)
    , m_process(new QProcess(this))
    , m_timeout(new QTimer(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProcessEnvironment(cLocaleEnvironment());
    m_timeout->setSingleShot(true);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &PackageInventoryBackend::captureOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &PackageInventoryBackend::discardErrorOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &PackageInventoryBackend::finishStage);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_stage != Stage::Idle) {
            finishWithError(m_outputLimitExceeded
                                ? tr("The local package inventory exceeded its safety limit.")
                                : tr("The local Pacman inventory could not be started."));
        }
    });
    connect(m_timeout, &QTimer::timeout, this, &PackageInventoryBackend::timeoutCurrentStage);
    updateAvailability();
    m_summary = pacmanAvailable()
        ? tr("Inspect locally installed Pacman packages on demand.")
        : tr("Pacman is not installed; local package storage cannot be inspected.");
}

QVariantList PackageInventoryBackend::groups() const { return m_groups; }
QString PackageInventoryBackend::summary() const { return m_summary; }
bool PackageInventoryBackend::snapshotAvailable() const { return m_snapshotAvailable; }
bool PackageInventoryBackend::pacmanAvailable() const { return !m_pacmanPath.isEmpty(); }

void PackageInventoryBackend::updateAvailability()
{
    m_pacmanPath = QStandardPaths::findExecutable(QStringLiteral("pacman"));
    setAvailable(!m_pacmanPath.isEmpty());
}

void PackageInventoryBackend::refresh()
{
    if (m_stage != Stage::Idle || busy()) {
        return;
    }
    updateAvailability();
    clearError();
    if (!pacmanAvailable()) {
        m_snapshotAvailable = false;
        m_groups.clear();
        m_summary = tr("Pacman is not installed; local package storage cannot be inspected.");
        setError(m_summary);
        Q_EMIT changed();
        return;
    }
    m_packages.clear();
    m_groups.clear();
    m_snapshotAvailable = false;
    m_summary = tr("Reading the local installed-package database…");
    startStage(Stage::LocalInfo, {QStringLiteral("-Qi")});
}

void PackageInventoryBackend::startStage(const Stage stage, const QStringList &arguments)
{
    m_output.clear();
    m_outputLimitExceeded = false;
    m_stage = stage;
    setBusy(true);
    Q_EMIT changed();
    m_process->start(m_pacmanPath, arguments);
    m_timeout->start(kStageTimeoutMs);
}

void PackageInventoryBackend::captureOutput()
{
    m_output += m_process->readAllStandardOutput();
    if (m_output.size() > kMaximumOutputBytes) {
        m_outputLimitExceeded = true;
        m_process->kill();
    }
}

void PackageInventoryBackend::discardErrorOutput()
{
    // Fixed local read-only pacman commands can still emit warnings. Keep the
    // bounded primary output authoritative and do not surface raw noise as an
    // application-level storage claim.
    m_process->readAllStandardError();
}

QStringList PackageInventoryBackend::parsedForeignPackageNames() const
{
    QStringList names;
    for (const QString &rawLine : QString::fromUtf8(m_output).split(QLatin1Char('\n'))) {
        const QString name = safePackageName(rawLine);
        if (!name.isEmpty() && !names.contains(name)) {
            names.push_back(name);
        }
    }
    return names;
}

void PackageInventoryBackend::finishStage(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    if (m_stage == Stage::Idle) {
        return;
    }
    m_timeout->stop();
    m_output += m_process->readAllStandardOutput();
    if (m_outputLimitExceeded) {
        finishWithError(tr("The local package inventory exceeded its safety limit."));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        finishWithError(tr("Pacman could not read the local package database."));
        return;
    }

    if (m_stage == Stage::LocalInfo) {
        m_packages = PackageInventoryContract::parsePacmanLocalInfo(m_output);
        if (m_packages.isEmpty()) {
            finishWithError(tr("Pacman returned no readable installed-package records."));
            return;
        }
        startStage(Stage::ForeignNames, {QStringLiteral("-Qqm")});
        return;
    }

    const QStringList foreignNames = parsedForeignPackageNames();
    m_groups = PackageInventoryContract::summarize(m_packages, foreignNames);
    m_stage = Stage::Idle;
    m_snapshotAvailable = true;
    m_summary = tr("%n installed package(s) from the local Pacman database.", "", m_packages.size());
    clearError();
    setBusy(false);
    Q_EMIT changed();
}

void PackageInventoryBackend::finishWithError(const QString &message)
{
    m_timeout->stop();
    m_stage = Stage::Idle;
    m_snapshotAvailable = false;
    m_groups.clear();
    m_summary = tr("Local package inventory is unavailable.");
    setBusy(false);
    setError(message);
    Q_EMIT changed();
}

void PackageInventoryBackend::timeoutCurrentStage()
{
    if (m_stage == Stage::Idle) {
        return;
    }
    m_process->kill();
    finishWithError(tr("The local package inventory timed out before completing."));
}
