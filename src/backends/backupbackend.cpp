#include "backupbackend.h"

#include "controlcenterbackend.h"
#include "omnistoreappsbackend.h"
#include "shellsettingsbackend.h"

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

bool containsOnlyKeys(const QJsonObject &object, const QSet<QString> &allowed)
{
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!allowed.contains(iterator.key()))
            return false;
    }
    return true;
}

bool validateSerializedMap(const QJsonObject &value,
                           const std::function<QVariantMap(const QVariantMap &, QString *)> &serializer,
                           QString *error)
{
    QString validationError;
    const QVariantMap serialized = serializer(value.toVariantMap(), &validationError);
    if (serialized.isEmpty() || QJsonObject::fromVariantMap(serialized) != value) {
        setContractError(error, validationError.isEmpty()
                                    ? QStringLiteral("A Meo setting contains unsupported fields or values.")
                                    : validationError);
        return false;
    }
    return true;
}

bool validateSettings(const QJsonObject &settings, QString *error)
{
    const QSet<QString> allowedSettingsKeys{
        QStringLiteral("controlCenter"), QStringLiteral("shell")};
    if (!containsOnlyKeys(settings, allowedSettingsKeys)) {
        setContractError(error, QStringLiteral("The backup settings section contains unsupported products."));
        return false;
    }

    if (settings.contains(QStringLiteral("controlCenter"))) {
        const QJsonValue controlCenterValue = settings.value(QStringLiteral("controlCenter"));
        if (!controlCenterValue.isObject()) {
            setContractError(error, QStringLiteral("The Control Center backup settings are invalid."));
            return false;
        }
        const QJsonObject controlCenter = controlCenterValue.toObject();
        const QSet<QString> allowedControlCenterKeys{
            QStringLiteral("layout"), QStringLiteral("topBar")};
        if (!containsOnlyKeys(controlCenter, allowedControlCenterKeys)
            || !controlCenter.contains(QStringLiteral("layout"))
            || !controlCenter.contains(QStringLiteral("topBar"))) {
            setContractError(error, QStringLiteral("The Control Center backup settings are incomplete."));
            return false;
        }

        const QJsonValue layoutValue = controlCenter.value(QStringLiteral("layout"));
        if (!layoutValue.isObject()) {
            setContractError(error, QStringLiteral("The Control Center layout backup is invalid."));
            return false;
        }
        const QJsonObject layout = layoutValue.toObject();
        const QSet<QString> allowedLayoutKeys{
            QStringLiteral("tiles"), QStringLiteral("density")};
        if (!containsOnlyKeys(layout, allowedLayoutKeys)
            || !layout.value(QStringLiteral("tiles")).isArray()
            || !layout.value(QStringLiteral("density")).isString()) {
            setContractError(error, QStringLiteral("The Control Center layout backup contains unsupported fields."));
            return false;
        }
        const QJsonArray tileArray = layout.value(QStringLiteral("tiles")).toArray();
        for (const QJsonValue &tileValue : tileArray) {
            if (!tileValue.isObject()) {
                setContractError(error, QStringLiteral("The Control Center tile backup is invalid."));
                return false;
            }
            const QJsonObject tile = tileValue.toObject();
            const QSet<QString> allowedTileKeys{
                QStringLiteral("id"), QStringLiteral("span"), QStringLiteral("visible")};
            if (!containsOnlyKeys(tile, allowedTileKeys)
                || !tile.value(QStringLiteral("id")).isString()
                || !tile.value(QStringLiteral("span")).isDouble()
                || !tile.value(QStringLiteral("visible")).isBool()) {
                setContractError(error, QStringLiteral("The Control Center tile backup contains unsupported fields."));
                return false;
            }
        }
        QString layoutError;
        const QVariantMap serializedLayout = ControlCenterBackend::serializeLayout(
            tileArray.toVariantList(), layout.value(QStringLiteral("density")).toString(), &layoutError);
        if (serializedLayout.isEmpty()
            || QJsonArray::fromVariantList(serializedLayout.value(QStringLiteral("tiles")).toList()) != tileArray
            || serializedLayout.value(QStringLiteral("density")).toString()
                   != layout.value(QStringLiteral("density")).toString()) {
            setContractError(error, layoutError.isEmpty()
                                        ? QStringLiteral("The Control Center layout backup is not canonical.")
                                        : layoutError);
            return false;
        }

        const QJsonValue topBarValue = controlCenter.value(QStringLiteral("topBar"));
        if (!topBarValue.isObject()
            || !validateSerializedMap(topBarValue.toObject(), ControlCenterBackend::serializeTopBar, error)) {
            if (error && error->isEmpty())
                *error = QStringLiteral("The top-bar backup settings are invalid.");
            return false;
        }
    }

    if (settings.contains(QStringLiteral("shell"))) {
        const QJsonValue shellValue = settings.value(QStringLiteral("shell"));
        if (!shellValue.isObject()) {
            setContractError(error, QStringLiteral("The Meo Shell backup settings are invalid."));
            return false;
        }
        const QJsonObject shell = shellValue.toObject();
        const QSet<QString> allowedShellKeys{
            QStringLiteral("shelf"), QStringLiteral("notifications"),
            QStringLiteral("timeCenter"), QStringLiteral("topTasks")};
        if (!containsOnlyKeys(shell, allowedShellKeys)
            || shell.size() != allowedShellKeys.size()) {
            setContractError(error, QStringLiteral("The Meo Shell backup settings are incomplete."));
            return false;
        }

        const auto validateSurface = [&shell, error](
                                         const QString &key,
                                         const std::function<QVariantMap(const QVariantMap &, QString *)> &serializer) {
            const QJsonValue value = shell.value(key);
            if (!value.isObject()) {
                setContractError(error, QStringLiteral("A Meo Shell backup surface is invalid."));
                return false;
            }
            return validateSerializedMap(value.toObject(), serializer, error);
        };
        if (!validateSurface(QStringLiteral("shelf"), ShellSettingsBackend::serializeShelf)
            || !validateSurface(QStringLiteral("notifications"), ShellSettingsBackend::serializeNotifications)
            || !validateSurface(QStringLiteral("timeCenter"), ShellSettingsBackend::serializeTimeCenter)
            || !validateSurface(QStringLiteral("topTasks"), ShellSettingsBackend::serializeTopTasks)) {
            return false;
        }
    }

    return true;
}
}

QJsonObject BackupManifestContract::build(const QVariantList &applications,
                                          const QVariantMap &settings,
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

    const QJsonObject settingsObject = QJsonObject::fromVariantMap(settings);
    if (!validateSettings(settingsObject, error))
        return {};

    const QJsonObject manifest{
        {QStringLiteral("schema"), kSchema},
        {QStringLiteral("createdAt"), created.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("contents"), QJsonObject{
             {QStringLiteral("applications"), reinstallList},
             {QStringLiteral("settings"), settingsObject},
             {QStringLiteral("userData"), QJsonArray{}},
         }},
        {QStringLiteral("secretsIncluded"), false},
    };
    QString validationError;
    if (!validate(manifest, &validationError)) {
        setContractError(error, validationError);
        return {};
    }
    return manifest;
}

QJsonObject BackupManifestContract::build(const QVariantList &applications,
                                          const QString &createdAt,
                                          QString *error)
{
    return build(applications, {}, createdAt, error);
}

bool BackupManifestContract::validate(const QJsonObject &manifest, QString *error)
{
    const QSet<QString> allowedManifestKeys{
        QStringLiteral("schema"),
        QStringLiteral("createdAt"),
        QStringLiteral("contents"),
        QStringLiteral("secretsIncluded"),
    };
    if (!containsOnlyKeys(manifest, allowedManifestKeys)) {
        setContractError(error, QStringLiteral("The backup manifest contains unsupported top-level fields."));
        return false;
    }
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
    const QSet<QString> allowedContentKeys{
        QStringLiteral("applications"),
        QStringLiteral("settings"),
        QStringLiteral("userData"),
    };
    if (!containsOnlyKeys(contents, allowedContentKeys)) {
        setContractError(error, QStringLiteral("The backup contents section contains unsupported fields."));
        return false;
    }
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
        if (!containsOnlyKeys(row, allowedKeys)) {
            setContractError(error, QStringLiteral("The backup application list contains unsupported fields."));
            return false;
        }
    }

    const QJsonValue settingsValue = contents.value(QStringLiteral("settings"));
    if (!settingsValue.isObject() || !validateSettings(settingsValue.toObject(), error))
        return false;

    const QJsonValue userDataValue = contents.value(QStringLiteral("userData"));
    if (!userDataValue.isArray() || !userDataValue.toArray().isEmpty()) {
        setContractError(error, QStringLiteral("This build does not accept user-data payloads in backup manifests."));
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
    return createLocalManifestInternal({});
}

bool BackupBackend::createLocalManifestWithSettings(const QVariantMap &settings)
{
    return createLocalManifestInternal(settings);
}

bool BackupBackend::createLocalManifestInternal(const QVariantMap &settings)
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
        m_appsBackend->applications(), settings, createdAt, &contractError);
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
    const QJsonObject contents = manifest.value(QStringLiteral("contents")).toObject();
    const QJsonArray applications = contents.value(QStringLiteral("applications")).toArray();
    const QJsonObject settings = contents.value(QStringLiteral("settings")).toObject();
    int settingsGroups = 0;
    if (settings.contains(QStringLiteral("controlCenter")))
        settingsGroups += 2;
    if (settings.contains(QStringLiteral("shell")))
        settingsGroups += 4;

    m_previewPath = info.canonicalFilePath();
    m_previewValid = true;
    m_previewSummary = tr("Valid Meo backup manifest · %n application(s)", "", applications.size())
                           + tr(" · %n portable setting group(s)", "", settingsGroups)
                           + tr(" · created %1").arg(manifest.value(QStringLiteral("createdAt")).toString());
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
