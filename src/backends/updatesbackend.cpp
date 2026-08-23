#include "updatesbackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace
{
constexpr qsizetype kMaximumOutputBytes = 2 * 1024 * 1024;
constexpr int kMaximumUpdates = 256;
constexpr int kStageTimeoutMs = 15'000;
constexpr auto kPacmanConfiguration = "/etc/pacman.conf";
constexpr auto kPacmanSyncDirectory = "/var/lib/pacman/sync";

const QRegularExpression kPackageNameExpression(
    QStringLiteral("^[A-Za-z0-9@._+:-]{1,128}$"));
const QRegularExpression kUpdateLineExpression(
    QStringLiteral("^([A-Za-z0-9@._+:-]+)\\s+([^\\s]+)\\s+->\\s+([^\\s]+)\\s*$"));
const QRegularExpression kRepositoryHeaderExpression(
    QStringLiteral("^\\s*\\[([A-Za-z0-9@._+:-]+)\\]\\s*(?:#.*)?$"));

QString safeText(const QString &value, const int maximum)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > maximum
        || std::any_of(trimmed.cbegin(), trimmed.cend(), [](const QChar character) {
               return character.isNull() || character.category() == QChar::Other_Control;
           })) {
        return {};
    }
    return trimmed;
}

bool isKdePackage(const QString &name)
{
    static const QStringList prefixes{
        QStringLiteral("plasma-"), QStringLiteral("kde-"), QStringLiteral("kf6-"),
        QStringLiteral("kcm"), QStringLiteral("kwin"), QStringLiteral("kirigami"),
        QStringLiteral("breeze"), QStringLiteral("discover"), QStringLiteral("dolphin"),
        QStringLiteral("konsole"), QStringLiteral("ark"), QStringLiteral("okular"),
        QStringLiteral("spectacle"), QStringLiteral("sddm-kcm"), QStringLiteral("kglobalaccel"),
        QStringLiteral("kservice"), QStringLiteral("kconfig"), QStringLiteral("kpackage"),
        QStringLiteral("kio"), QStringLiteral("baloo"), QStringLiteral("oxygen"),
    };
    const QString normalized = name.toCaseFolded();
    return std::any_of(prefixes.cbegin(), prefixes.cend(), [&normalized](const QString &prefix) {
        return normalized.startsWith(prefix);
    });
}

bool isMeoPackage(const QString &name)
{
    const QString normalized = name.toCaseFolded();
    return normalized == QLatin1String("meoarch") || normalized.startsWith(QLatin1String("meo-"))
        || normalized.startsWith(QLatin1String("meoarch-"));
}

bool isMeoRepository(const QString &repository)
{
    const QString normalized = repository.toCaseFolded();
    return normalized == QLatin1String("meo") || normalized.startsWith(QLatin1String("meo-"))
        || normalized.startsWith(QLatin1String("meoarch"));
}

bool isDistributionRepository(const QString &repository)
{
    static const QStringList names{
        QStringLiteral("core"), QStringLiteral("extra"), QStringLiteral("multilib"),
        QStringLiteral("core-testing"), QStringLiteral("extra-testing"),
        QStringLiteral("multilib-testing"), QStringLiteral("testing"),
        QStringLiteral("cachyos"), QStringLiteral("cachyos-v3"),
        QStringLiteral("cachyos-v4"), QStringLiteral("cachyos-core-v3"),
        QStringLiteral("cachyos-extra-v3"), QStringLiteral("cachyos-core-v4"),
        QStringLiteral("cachyos-extra-v4"),
    };
    return names.contains(repository.toCaseFolded());
}

QString repositoryKind(const QString &repository)
{
    if (isMeoRepository(repository)) {
        return QStringLiteral("meo");
    }
    return isDistributionRepository(repository) ? QStringLiteral("system") : QStringLiteral("custom");
}

QString repositoryDescription(const QString &kind)
{
    if (kind == QLatin1String("meo")) {
        return QObject::tr("Meo repository");
    }
    if (kind == QLatin1String("custom")) {
        return QObject::tr("Custom configured repository");
    }
    return QObject::tr("Distribution repository");
}

QProcessEnvironment cLocaleEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    return environment;
}

QString valueAfterColon(const QString &line, const QString &label)
{
    if (!line.startsWith(label)) {
        return {};
    }
    const int separator = line.indexOf(QLatin1Char(':'));
    if (separator < 0) {
        return {};
    }
    return safeText(line.mid(separator + 1), 256);
}
} // namespace

QVariantMap SystemUpdatesContract::parsePacmanUpdateLine(const QString &line)
{
    const auto match = kUpdateLineExpression.match(line);
    if (!match.hasMatch()) {
        return {};
    }
    const QString packageName = safeText(match.captured(1), 128);
    const QString installedVersion = safeText(match.captured(2), 256);
    const QString availableVersion = safeText(match.captured(3), 256);
    if (packageName.isEmpty() || installedVersion.isEmpty() || availableVersion.isEmpty()
        || !kPackageNameExpression.match(packageName).hasMatch()) {
        return {};
    }
    return {
        {QStringLiteral("id"), packageName},
        {QStringLiteral("name"), packageName},
        {QStringLiteral("installedVersion"), installedVersion},
        {QStringLiteral("availableVersion"), availableVersion},
    };
}

QVariantMap SystemUpdatesContract::syncInfoForPackage(const QByteArray &output, const QString &packageName)
{
    const QString wanted = safeText(packageName, 128);
    if (wanted.isEmpty()) {
        return {};
    }

    QString currentName;
    QString currentRepository;
    const auto flushRecord = [&]() -> QVariantMap {
        if (currentName == wanted && !currentRepository.isEmpty()) {
            return {{QStringLiteral("name"), currentName},
                    {QStringLiteral("repository"), currentRepository}};
        }
        return {};
    };
    const auto lines = QString::fromUtf8(output).split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            const QVariantMap record = flushRecord();
            if (!record.isEmpty()) {
                return record;
            }
            currentName.clear();
            currentRepository.clear();
            continue;
        }
        const QString name = valueAfterColon(line, QStringLiteral("Name"));
        if (!name.isEmpty()) {
            const QVariantMap record = flushRecord();
            if (!record.isEmpty()) {
                return record;
            }
            currentName = name;
            continue;
        }
        const QString repository = valueAfterColon(line, QStringLiteral("Repository"));
        if (!repository.isEmpty()) {
            currentRepository = repository;
        }
    }
    return flushRecord();
}

QStringList SystemUpdatesContract::configuredRepositoryNames(const QByteArray &pacmanConfig)
{
    QStringList repositories;
    const auto lines = QString::fromUtf8(pacmanConfig).split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1Char('#')) || line.isEmpty()) {
            continue;
        }
        const auto match = kRepositoryHeaderExpression.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        const QString name = match.captured(1).toCaseFolded();
        if (name == QLatin1String("options")) {
            continue;
        }
        if (!repositories.contains(name)) {
            repositories.push_back(name);
        }
    }
    return repositories;
}

QString SystemUpdatesContract::updateFamily(const QString &packageName, const QString &repository,
                                             const QStringList &foreignPackages,
                                             const QStringList &configuredRepositories)
{
    const QString name = packageName.toCaseFolded();
    const QString sourceRepository = repository.toCaseFolded();
    if (isMeoPackage(name) || isMeoRepository(sourceRepository)) {
        return QStringLiteral("meo");
    }
    if (foreignPackages.contains(name)) {
        return QStringLiteral("aur");
    }
    if (isKdePackage(name)) {
        return QStringLiteral("kde");
    }
    if (!sourceRepository.isEmpty() && configuredRepositories.contains(sourceRepository)
        && isCustomRepository(sourceRepository)) {
        return QStringLiteral("custom");
    }
    return QStringLiteral("system");
}

bool SystemUpdatesContract::isCustomRepository(const QString &repository)
{
    return !repository.isEmpty() && !isDistributionRepository(repository)
        && !isMeoRepository(repository);
}

UpdatesBackend::UpdatesBackend(QObject *parent)
    : BackendBase(parent)
    , m_process(new QProcess(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &UpdatesBackend::captureStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &UpdatesBackend::discardStandardError);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &UpdatesBackend::finishStage);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_operationActive) {
            finishWithError(tr("The package metadata inspector could not be started."));
        }
    });
    connect(m_timeoutTimer, &QTimer::timeout, this, &UpdatesBackend::stageTimedOut);
    refresh();
}

QVariantList UpdatesBackend::updates() const { return m_updates; }
QVariantList UpdatesBackend::configuredRepositories() const { return m_configuredRepositories; }
QVariantList UpdatesBackend::aurUpdates() const { return m_aurUpdates; }
QString UpdatesBackend::cachedMetadataTimestamp() const { return m_cachedMetadataTimestamp; }

QString UpdatesBackend::summary() const
{
    if (!pacmanAvailable()) {
        return tr("Pacman is not installed");
    }
    if (m_operationActive && !m_checkingAur) {
        return tr("Reading locally cached package metadata");
    }
    if (!m_hasNativeSnapshot) {
        return tr("No local update snapshot is available yet");
    }
    if (m_updates.isEmpty()) {
        return tr("No update candidates in the cached pacman metadata");
    }
    return tr("%n update candidate(s) from cached pacman metadata", "", m_updates.size());
}

QString UpdatesBackend::aurSummary() const
{
    if (!aurHelperAvailable()) {
        return tr("No supported AUR helper is installed");
    }
    if (m_checkingAur) {
        return tr("Checking AUR update candidates");
    }
    if (!m_hasAurSnapshot) {
        return tr("AUR updates have not been checked in this Settings session");
    }
    if (m_aurUpdates.isEmpty()) {
        return tr("No AUR update candidates were reported");
    }
    return tr("%n AUR update candidate(s)", "", m_aurUpdates.size());
}

int UpdatesBackend::updateCount() const { return m_updates.size(); }
int UpdatesBackend::meoUpdateCount() const { return m_meoUpdateCount; }
int UpdatesBackend::kdeUpdateCount() const { return m_kdeUpdateCount; }
int UpdatesBackend::systemUpdateCount() const { return m_systemUpdateCount; }
int UpdatesBackend::customRepositoryUpdateCount() const { return m_customRepositoryUpdateCount; }
int UpdatesBackend::aurUpdateCount() const { return m_aurUpdates.size(); }
bool UpdatesBackend::pacmanAvailable() const { return !m_pacmanPath.isEmpty(); }
bool UpdatesBackend::aurHelperAvailable() const { return !m_aurHelperPath.isEmpty(); }
bool UpdatesBackend::checkingAur() const { return m_checkingAur; }

void UpdatesBackend::refresh()
{
    if (m_operationActive) {
        return;
    }
    updateRuntimeAvailability();
    updateConfiguredRepositories();
    updateCachedMetadataTimestamp();
    clearError();
    if (!pacmanAvailable()) {
        setAvailable(false);
        setError(tr("Pacman is not installed, so system package metadata cannot be inspected."));
        Q_EMIT changed();
        return;
    }
    m_candidatePackageNames.clear();
    m_foreignPackageNames.clear();
    m_updates.clear();
    m_meoUpdateCount = 0;
    m_kdeUpdateCount = 0;
    m_systemUpdateCount = 0;
    m_customRepositoryUpdateCount = 0;
    setAvailable(true);
    startStage(Stage::NativeUpdates, m_pacmanPath, {QStringLiteral("-Qu")});
}

void UpdatesBackend::refreshAurUpdates()
{
    if (m_operationActive) {
        return;
    }
    updateRuntimeAvailability();
    clearError();
    if (!aurHelperAvailable()) {
        setError(tr("No supported AUR helper is installed."));
        Q_EMIT changed();
        return;
    }
    m_aurUpdates.clear();
    m_checkingAur = true;
    startStage(Stage::AurUpdates, m_aurHelperPath, {QStringLiteral("-Qua")});
}

void UpdatesBackend::updateRuntimeAvailability()
{
    const QString pacman = QStandardPaths::findExecutable(QStringLiteral("pacman"));
    const QString paru = QStandardPaths::findExecutable(QStringLiteral("paru"));
    const QString yay = QStandardPaths::findExecutable(QStringLiteral("yay"));
    const QString aurHelper = !paru.isEmpty() ? paru : yay;
    if (m_pacmanPath == pacman && m_aurHelperPath == aurHelper) {
        return;
    }
    m_pacmanPath = pacman;
    m_aurHelperPath = aurHelper;
    Q_EMIT changed();
}

void UpdatesBackend::updateConfiguredRepositories()
{
    QFile config(QString::fromLatin1(kPacmanConfiguration));
    QByteArray contents;
    if (config.open(QIODevice::ReadOnly)) {
        contents = config.read(1024 * 1024);
    }
    const QStringList names = SystemUpdatesContract::configuredRepositoryNames(contents);
    QVariantList nextRepositories;
    nextRepositories.reserve(names.size());
    for (const QString &name : names) {
        const QString kind = repositoryKind(name);
        nextRepositories.push_back(QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("custom"), kind == QLatin1String("custom")},
            {QStringLiteral("description"), repositoryDescription(kind)},
        });
    }
    if (m_configuredRepositories == nextRepositories) {
        return;
    }
    m_configuredRepositories = std::move(nextRepositories);
    Q_EMIT changed();
}

void UpdatesBackend::updateCachedMetadataTimestamp()
{
    QDir directory(QString::fromLatin1(kPacmanSyncDirectory));
    QDateTime newest;
    const auto entries = directory.entryInfoList({QStringLiteral("*.db")}, QDir::Files, QDir::Time);
    for (const QFileInfo &entry : entries) {
        const QDateTime timestamp = entry.lastModified();
        if (!newest.isValid() || timestamp > newest) {
            newest = timestamp;
        }
    }
    const QString nextTimestamp = newest.isValid() ? newest.toString(Qt::ISODate) : QString();
    if (m_cachedMetadataTimestamp == nextTimestamp) {
        return;
    }
    m_cachedMetadataTimestamp = nextTimestamp;
    Q_EMIT changed();
}

void UpdatesBackend::startStage(const Stage stage, const QString &program, const QStringList &arguments)
{
    m_stage = stage;
    m_standardOutput.clear();
    m_operationActive = true;
    setBusy(true);
    m_process->setProcessEnvironment(cLocaleEnvironment());
    m_process->setProgram(program);
    m_process->setArguments(arguments);
    m_process->start();
    m_timeoutTimer->start(kStageTimeoutMs);
    Q_EMIT changed();
}

void UpdatesBackend::finishStage(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    if (!m_operationActive) {
        return;
    }
    captureStandardOutput();
    if (!m_operationActive) {
        return;
    }
    if (exitStatus != QProcess::NormalExit) {
        finishWithError(tr("The package metadata inspector ended unexpectedly."));
        return;
    }

    const QByteArray output = m_standardOutput;
    if (m_stage == Stage::NativeUpdates) {
        // pacman returns 1 with no stdout when its cached sync databases
        // contain no upgrade candidates.  Any non-empty failed result is an
        // error rather than a false “up to date” claim.
        if (exitCode != 0 && !(exitCode == 1 && output.trimmed().isEmpty())) {
            finishWithError(tr("Pacman could not read the local update metadata."));
            return;
        }
        const auto lines = QString::fromUtf8(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QVariantMap update = SystemUpdatesContract::parsePacmanUpdateLine(line);
            if (update.isEmpty()) {
                finishWithError(tr("Pacman returned an unsupported update record."));
                return;
            }
            if (m_candidatePackageNames.size() >= kMaximumUpdates) {
                finishWithError(tr("Too many update candidates were returned by pacman."));
                return;
            }
            m_candidatePackageNames.push_back(update.value(QStringLiteral("name")).toString());
            m_updates.push_back(update);
        }
        startStage(Stage::ForeignPackages, m_pacmanPath, {QStringLiteral("-Qqm")});
        return;
    }

    if (m_stage == Stage::ForeignPackages) {
        if (exitCode != 0 && !(exitCode == 1 && output.trimmed().isEmpty())) {
            finishWithError(tr("Pacman could not read the local foreign-package list."));
            return;
        }
        m_foreignPackageNames = parsedForeignPackages();
        if (m_candidatePackageNames.isEmpty()) {
            applyNativeUpdates();
            completeOperation();
            return;
        }
        startStage(Stage::SyncInformation, m_pacmanPath,
                   QStringList{QStringLiteral("-Si")} + m_candidatePackageNames);
        return;
    }

    if (m_stage == Stage::SyncInformation) {
        // A foreign package can make one multi-target -Si request return a
        // non-zero code. Keep its update record, label it from the local
        // foreign list, and never fabricate a repository name.
        applyNativeUpdates();
        completeOperation();
        return;
    }

    if (m_stage == Stage::AurUpdates) {
        if (exitCode != 0 && !(exitCode == 1 && output.trimmed().isEmpty())) {
            finishWithError(tr("The AUR helper could not provide update candidates."));
            return;
        }
        applyAurUpdates();
        completeOperation();
    }
}

void UpdatesBackend::finishWithError(const QString &message)
{
    if (!m_operationActive) {
        return;
    }
    m_timeoutTimer->stop();
    m_operationActive = false;
    m_checkingAur = false;
    m_stage = Stage::Idle;
    setBusy(false);
    setError(message);
    Q_EMIT changed();
}

void UpdatesBackend::captureStandardOutput()
{
    if (!m_operationActive) {
        m_process->readAllStandardOutput();
        return;
    }
    m_standardOutput.append(m_process->readAllStandardOutput());
    if (m_standardOutput.size() > kMaximumOutputBytes) {
        m_process->kill();
        finishWithError(tr("The package metadata result is too large."));
    }
}

void UpdatesBackend::discardStandardError()
{
    // Package tools can include host paths or mirror diagnostics in stderr.
    // It is not part of Settings' presentation contract and must not block a
    // verbose child process, so consume it without exposing it to QML.
    m_process->readAllStandardError();
}

void UpdatesBackend::stageTimedOut()
{
    if (!m_operationActive) {
        return;
    }
    m_process->kill();
    finishWithError(m_checkingAur
                        ? tr("The AUR helper took too long to report update candidates.")
                        : tr("Reading locally cached package metadata took too long."));
}

void UpdatesBackend::applyNativeUpdates()
{
    QVariantList nextUpdates;
    nextUpdates.reserve(m_updates.size());
    int nextMeo = 0;
    int nextKde = 0;
    int nextSystem = 0;
    int nextCustom = 0;
    const QStringList repositories = [&]() {
        QStringList names;
        for (const QVariant &entry : m_configuredRepositories) {
            names.push_back(entry.toMap().value(QStringLiteral("name")).toString());
        }
        return names;
    }();

    for (const QVariant &entry : std::as_const(m_updates)) {
        QVariantMap update = entry.toMap();
        const QString name = update.value(QStringLiteral("name")).toString();
        const QVariantMap info = SystemUpdatesContract::syncInfoForPackage(m_standardOutput, name);
        const QString repository = info.value(QStringLiteral("repository")).toString();
        const QString family = SystemUpdatesContract::updateFamily(name, repository,
                                                                     m_foreignPackageNames, repositories);
        update.insert(QStringLiteral("repository"), repository);
        update.insert(QStringLiteral("family"), family);
        update.insert(QStringLiteral("source"), family == QLatin1String("aur")
                                                ? tr("AUR or local package")
                                                : (repository.isEmpty()
                                                   ? tr("Repository unavailable in local metadata")
                                                   : repository));
        if (family == QLatin1String("meo")) {
            ++nextMeo;
        } else if (family == QLatin1String("kde")) {
            ++nextKde;
        } else if (family == QLatin1String("custom")) {
            ++nextCustom;
        } else if (family == QLatin1String("system")) {
            ++nextSystem;
        }
        nextUpdates.push_back(std::move(update));
    }

    std::sort(nextUpdates.begin(), nextUpdates.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        const QStringList order{QStringLiteral("meo"), QStringLiteral("kde"),
                                QStringLiteral("custom"), QStringLiteral("system"),
                                QStringLiteral("aur")};
        const int aIndex = order.indexOf(a.value(QStringLiteral("family")).toString());
        const int bIndex = order.indexOf(b.value(QStringLiteral("family")).toString());
        if (aIndex != bIndex) {
            return aIndex < bIndex;
        }
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });
    m_updates = std::move(nextUpdates);
    m_meoUpdateCount = nextMeo;
    m_kdeUpdateCount = nextKde;
    m_systemUpdateCount = nextSystem;
    m_customRepositoryUpdateCount = nextCustom;
    m_hasNativeSnapshot = true;
    Q_EMIT changed();
}

void UpdatesBackend::applyAurUpdates()
{
    QVariantList nextUpdates;
    const auto lines = QString::fromUtf8(m_standardOutput).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QVariantMap update = SystemUpdatesContract::parsePacmanUpdateLine(line);
        if (update.isEmpty()) {
            // AUR helpers may add non-package informational lines. Treat
            // those as diagnostics, not a user-visible package update.
            continue;
        }
        update.insert(QStringLiteral("repository"), QStringLiteral("AUR"));
        update.insert(QStringLiteral("family"), QStringLiteral("aur"));
        update.insert(QStringLiteral("source"), tr("AUR helper"));
        nextUpdates.push_back(std::move(update));
        if (nextUpdates.size() >= kMaximumUpdates) {
            break;
        }
    }
    std::sort(nextUpdates.begin(), nextUpdates.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
                   right.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    m_aurUpdates = std::move(nextUpdates);
    m_hasAurSnapshot = true;
    Q_EMIT changed();
}

void UpdatesBackend::completeOperation()
{
    m_timeoutTimer->stop();
    m_operationActive = false;
    m_checkingAur = false;
    m_stage = Stage::Idle;
    setBusy(false);
    clearError();
    Q_EMIT changed();
}

QStringList UpdatesBackend::parsedForeignPackages() const
{
    QStringList packages;
    const auto lines = QString::fromUtf8(m_standardOutput).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawName : lines) {
        const QString name = safeText(rawName, 128).toCaseFolded();
        if (kPackageNameExpression.match(name).hasMatch() && !packages.contains(name)) {
            packages.push_back(name);
        }
    }
    return packages;
}
