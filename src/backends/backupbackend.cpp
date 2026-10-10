#include "backupbackend.h"

#include "omnistoreappsbackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace
{
constexpr qsizetype kMaximumManifestBytes = 2 * 1024 * 1024;
constexpr int kMaximumApplications = 10000;
const QString kSchema = QStringLiteral("org.meo.backup/v1");

void setContractError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

bool safeAppId(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+@-]{0,255}$"));
    return expression.match(value).hasMatch();
}

bool safeSourceId(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
    return expression.match(value).hasMatch();
}

bool safeVersion(const QString &value)
{
    if (value.size() > 120)
        return false;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (code < 0x20 || code == 0x7f
            || (code >= 0x202a && code <= 0x202e)
            || (code >= 0x2066 && code <= 0x2069)) {
            return false;
        }
    }
    return true;
}
}

QJsonObject BackupManifestContract::build(const QVariantList &applications,
                                          const QString &createdAt,
                                          QString *error)
{
    if (applications.size() > kMaximumApplications) {
        setContractError(error, QStringLiteral("Too many applications for one backup manifest."));
        return {};
    }
    const QDateTime created = QDateTime::fromString(createdAt, Qt::ISODate);
    if (!created.isValid()) {
        setContractError(error, QStringLiteral("The backup timestamp is invalid."));
        return {};
    }

    QJsonArray reinstallList;
    for (const QVariant &item : applications) {
        const QVariantMap application = item.toMap();
        const QString id = application.value(QStringLiteral("id")).toString();
        const QString sourceId = application.value(QStringLiteral("sourceId")).toString();
        const QString version = application.value(QStringLiteral("version")).toString();
        if (!safeAppId(id) || !safeSourceId(sourceId) || !safeVersion(version)) {
            setContractError(error, QStringLiteral("Application metadata is unsafe for backup."));
            return {};
        }

        QJsonObject row{
            {QStringLiteral("id"), id},
            {QStringLiteral("sourceId"), sourceId},
        };
        if (!version.isEmpty())
            row.insert(QStringLiteral("version"), version);
        reinstallList.push_back(row);
    }

    return {
        {QStringLiteral("schema"), kSchema},
        {QStringLiteral("createdAt"), created.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("contents"), QJsonObject{
             {QStringLiteral("applications"), reinstallList},
             {QStringLiteral("settings"), QJsonArray{}},
             {QStringLiteral("userData"), QJsonArray{}},
         }},
        {QStringLiteral("secretsIncluded"), false},
    };
}

bool BackupManifestContract::validate(const QJsonObject &manifest, QString *error)
{
    if (manifest.value(QStringLiteral("schema")).toString() != kSchema) {
        setContractError(error, QStringLiteral("Unsupported backup manifest format."));
        return false;
    }
    if (manifest.value(QStringLiteral("secretsIncluded")).toBool(true)) {
        setContractError(error, QStringLiteral("Backup manifests containing secrets are not accepted."));
        return false;
    }
    const QDateTime created = QDateTime::fromString(
        manifest.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
    if (!created.isValid()) {
        setContractError(error, QStringLiteral("The backup timestamp is invalid."));
        return false;
    }
    const QJsonValue contentsValue = manifest.value(QStringLiteral("contents"));
    if (!contentsValue.isObject()) {
        setContractError(error, QStringLiteral("The backup contents section is missing."));
        return false;
    }
    const QJsonObject contents = contentsValue.toObject();
    const QJsonValue applicationsValue = contents.value(QStringLiteral("applications"));
    if (!applicationsValue.isArray()) {
        setContractError(error, QStringLiteral("The backup application list is missing."));
        return false;
    }
    const QJsonArray applications = applicationsValue.toArray();
    if (applications.size() > kMaximumApplications) {
        setContractError(error, QStringLiteral("The backup application list is too large."));
        return false;
    }
    for (const QJsonValue &value : applications) {
        if (!value.isObject()) {
            setContractError(error, QStringLiteral("The backup application list is invalid."));
            return false;
        }
        const QJsonObject row = value.toObject();
        if (!safeAppId(row.value(QStringLiteral("id")).toString())
            || !safeSourceId(row.value(QStringLiteral("sourceId")).toString())
            || !safeVersion(row.value(QStringLiteral("version")).toString())) {
            setContractError(error, QStringLiteral("The backup application list contains unsafe metadata."));
            return false;
        }
        const QSet<QString> allowedKeys{
            QStringLiteral("id"), QStringLiteral("sourceId"), QStringLiteral("version")};
        for (auto iterator = row.begin(); iterator != row.end(); ++iterator) {
            if (!allowedKeys.contains(iterator.key())) {
                setContractError(error, QStringLiteral("The backup application list contains unsupported fields."));
                return false;
            }
        }
    }

    const QJsonValue settingsValue = contents.value(QStringLiteral("settings"));
    const QJsonValue userDataValue = contents.value(QStringLiteral("userData"));
    if (!settingsValue.isArray() || !settingsValue.toArray().isEmpty()
        || !userDataValue.isArray() || !userDataValue.toArray().isEmpty()) {
        setContractError(error, QStringLiteral("This build only accepts application-list backup manifests."));
        return false;
    }
    return true;
}

BackupBackend::BackupBackend(OmniStoreAppsBackend *appsBackend, QObject *parent)
    : BackendBase(parent)
    , m_appsBackend(appsBackend)
{
    setAvailable(m_appsBackend != nullptr);
}

QString BackupBackend::summary() const
{
    if (!available())
        return tr("Backup support is unavailable in this build.");
    if (m_lastBackupPath.isEmpty())
        return tr("No local Meo backup manifest has been created yet.");
    return tr("Last local backup manifest: %1").arg(m_lastBackupAt);
}

QString BackupBackend::lastBackupPath() const
{
    return m_lastBackupPath;
}

QString BackupBackend::lastBackupAt() const
{
    return m_lastBackupAt;
}

QString BackupBackend::previewSummary() const
{
    return m_previewSummary;
}

QString BackupBackend::previewPath() const
{
    return m_previewPath;
}

bool BackupBackend::previewValid() const
{
    return m_previewValid;
}

bool BackupBackend::createLocalManifest()
{
    clearError();
    if (!m_appsBackend) {
        setError(tr("The application inventory backend is unavailable."));
        return false;
    }
    if (m_appsBackend->busy()) {
        setError(tr("Wait for the application inventory to finish refreshing."));
        return false;
    }
    if (!m_appsBackend->available()) {
        setError(tr("Refresh the OmniStore application inventory before creating a backup."));
        return false;
    }

    setBusy(true);
    const QString createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString contractError;
    const QJsonObject manifest = BackupManifestContract::build(
        m_appsBackend->applications(), createdAt, &contractError);
    if (manifest.isEmpty()) {
        setBusy(false);
        setError(contractError);
        return false;
    }

    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        setBusy(false);
        setError(tr("A writable Documents folder is not available."));
        return false;
    }
    QDir directory(documents);
    if (!directory.mkpath(QStringLiteral("Meo Backups"))) {
        setBusy(false);
        setError(tr("The Meo Backups folder could not be created."));
        return false;
    }
    directory.cd(QStringLiteral("Meo Backups"));
    const QString fileName = QStringLiteral("meo-backup-%1.json")
                                 .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = directory.filePath(fileName);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setBusy(false);
        setError(tr("The backup manifest could not be opened for writing."));
        return false;
    }
    const QByteArray payload = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    if (payload.size() > kMaximumManifestBytes || file.write(payload) != payload.size()
        || !file.commit()) {
        setBusy(false);
        setError(tr("The backup manifest could not be saved safely."));
        return false;
    }

    m_lastBackupPath = path;
    m_lastBackupAt = createdAt;
    setBusy(false);
    Q_EMIT changed();
    return true;
}

bool BackupBackend::previewLocalManifest(const QString &path)
{
    clearError();
    clearPreview();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink()
        || info.size() < 2 || info.size() > kMaximumManifestBytes) {
        setError(tr("Choose a regular Meo backup manifest up to 2 MiB."));
        return false;
    }

    QFile file(info.canonicalFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("The backup manifest could not be read."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(tr("The selected file is not a valid Meo backup manifest."));
        return false;
    }

    QString contractError;
    const QJsonObject manifest = document.object();
    if (!BackupManifestContract::validate(manifest, &contractError)) {
        setError(contractError);
        return false;
    }
    const QJsonArray applications = manifest.value(QStringLiteral("contents"))
                                        .toObject()
                                        .value(QStringLiteral("applications"))
                                        .toArray();
    m_previewPath = info.canonicalFilePath();
    m_previewValid = true;
    m_previewSummary = tr("Valid Meo backup manifest · %n application(s) · created %1", "", applications.size())
                           .arg(manifest.value(QStringLiteral("createdAt")).toString());
    Q_EMIT changed();
    return true;
}

void BackupBackend::clearPreview()
{
    if (m_previewPath.isEmpty() && m_previewSummary.isEmpty() && !m_previewValid)
        return;
    m_previewPath.clear();
    m_previewSummary.clear();
    m_previewValid = false;
    Q_EMIT changed();
}
