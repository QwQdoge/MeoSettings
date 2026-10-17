#include "backupbackend.h"

#include "omnistoreappsbackend.h"

#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <cmath>

namespace
{
QVariantMap backupMap(const QVariant &value)
{
    return value.metaType().id() == qMetaTypeId<QDBusArgument>()
        ? qdbus_cast<QVariantMap>(value.value<QDBusArgument>()) : value.toMap();
}
constexpr qsizetype kMaximumManifestBytes = 2 * 1024 * 1024;
constexpr int kMaximumApplications = 10000;
const QString kSchema = QStringLiteral("org.meo.backup/v1");

void setContractError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

bool containsOnlyKeys(const QJsonObject &object, const QSet<QString> &allowed)
{
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (!allowed.contains(iterator.key()))
            return false;
    }
    return true;
}

bool hasExactlyKeys(const QJsonObject &object, const QSet<QString> &keys)
{
    return object.size() == keys.size() && containsOnlyKeys(object, keys);
}

bool oneOf(const QString &value, std::initializer_list<const char *> allowed)
{
    for (const char *candidate : allowed) {
        if (value == QLatin1String(candidate))
            return true;
    }
    return false;
}

bool integerInRange(const QJsonValue &value, int minimum, int maximum)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::floor(number) == number && number >= minimum && number <= maximum;
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

QVariantMap projectMap(const QVariantMap &source, const QStringList &keys)
{
    QVariantMap projected;
    for (const QString &key : keys)
        projected.insert(key, source.value(key));
    return projected;
}

QVariantList projectTiles(const QVariantList &source)
{
    QVariantList projected;
    projected.reserve(source.size());
    for (const QVariant &value : source) {
        const QVariantMap tile = value.toMap();
        projected.push_back(QVariantMap{
            {QStringLiteral("id"), tile.value(QStringLiteral("id"))},
            {QStringLiteral("span"), tile.value(QStringLiteral("span"))},
            {QStringLiteral("visible"), tile.value(QStringLiteral("visible"))},
        });
    }
    return projected;
}

bool validateControlCenterLayout(const QJsonObject &layout, QString *error)
{
    const QSet<QString> layoutKeys{QStringLiteral("tiles"), QStringLiteral("density")};
    if (!hasExactlyKeys(layout, layoutKeys)
        || !layout.value(QStringLiteral("tiles")).isArray()
        || !layout.value(QStringLiteral("density")).isString()
        || !oneOf(layout.value(QStringLiteral("density")).toString(),
                  {"compact", "comfortable", "spacious"})) {
        setContractError(error, QStringLiteral("The Control Center layout backup is invalid."));
        return false;
    }

    const QSet<QString> canonicalIds{
        QStringLiteral("wifi"), QStringLiteral("bluetooth"), QStringLiteral("focus"),
        QStringLiteral("nightLight"), QStringLiteral("keepAwake"), QStringLiteral("powerMode"),
        QStringLiteral("microphone"), QStringLiteral("audioDevices"),
        QStringLiteral("display"), QStringLiteral("screenshot")};
    const QSet<QString> tileKeys{
        QStringLiteral("id"), QStringLiteral("span"), QStringLiteral("visible")};
    const QJsonArray tiles = layout.value(QStringLiteral("tiles")).toArray();
    if (tiles.size() != canonicalIds.size()) {
        setContractError(error, QStringLiteral("The Control Center layout must include every supported tile."));
        return false;
    }

    QSet<QString> seen;
    int visibleCount = 0;
    for (const QJsonValue &value : tiles) {
        if (!value.isObject()) {
            setContractError(error, QStringLiteral("The Control Center tile backup is invalid."));
            return false;
        }
        const QJsonObject tile = value.toObject();
        const QString id = tile.value(QStringLiteral("id")).toString();
        if (!hasExactlyKeys(tile, tileKeys)
            || !canonicalIds.contains(id) || seen.contains(id)
            || !integerInRange(tile.value(QStringLiteral("span")), 1, 2)
            || !tile.value(QStringLiteral("visible")).isBool()) {
            setContractError(error, QStringLiteral("The Control Center tile backup contains unsupported fields or values."));
            return false;
        }
        seen.insert(id);
        if (tile.value(QStringLiteral("visible")).toBool())
            ++visibleCount;
    }
    if (seen != canonicalIds || visibleCount == 0) {
        setContractError(error, QStringLiteral("The Control Center layout is incomplete."));
        return false;
    }
    return true;
}

bool validateTopBar(const QJsonObject &topBar, QString *error)
{
    const QSet<QString> keys{
        QStringLiteral("textScalePercent"), QStringLiteral("density"),
        QStringLiteral("surfaceStyle"), QStringLiteral("surfaceOpacityPercent"),
        QStringLiteral("motionProfile"), QStringLiteral("showUnreadBadge"),
        QStringLiteral("showJobs"), QStringLiteral("showNetwork"),
        QStringLiteral("showBluetooth"), QStringLiteral("showVolume"),
        QStringLiteral("batteryDisplay"), QStringLiteral("showDate"),
        QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")};
    if (!hasExactlyKeys(topBar, keys)
        || !integerInRange(topBar.value(QStringLiteral("textScalePercent")), 75, 150)
        || !integerInRange(topBar.value(QStringLiteral("surfaceOpacityPercent")), 70, 100)
        || !integerInRange(topBar.value(QStringLiteral("batteryDisplay")), 0, 3)
        || !oneOf(topBar.value(QStringLiteral("density")).toString(), {"compact", "comfortable"})
        || !oneOf(topBar.value(QStringLiteral("surfaceStyle")).toString(),
                  {"theme", "flat", "tonal", "translucent"})
        || !oneOf(topBar.value(QStringLiteral("motionProfile")).toString(),
                  {"calm", "pixel", "playful"})) {
        setContractError(error, QStringLiteral("The top-bar backup settings are invalid."));
        return false;
    }
    const QStringList booleanKeys{
        QStringLiteral("showUnreadBadge"), QStringLiteral("showJobs"),
        QStringLiteral("showNetwork"), QStringLiteral("showBluetooth"),
        QStringLiteral("showVolume"), QStringLiteral("showDate"),
        QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")};
    for (const QString &key : booleanKeys) {
        if (!topBar.value(key).isBool()) {
            setContractError(error, QStringLiteral("The top-bar backup contains a non-boolean switch."));
            return false;
        }
    }
    return true;
}

bool validateShelf(const QJsonObject &shelf, QString *error)
{
    const QSet<QString> keys{
        QStringLiteral("showLauncherButton"), QStringLiteral("filterTasksByVirtualDesktop"),
        QStringLiteral("showRunningIndicators"), QStringLiteral("showTooltips"),
        QStringLiteral("launcherDefaultPage"), QStringLiteral("launcherWidth"),
        QStringLiteral("launcherShowFavorites"), QStringLiteral("launcherShowRecents")};
    if (!hasExactlyKeys(shelf, keys)
        || !oneOf(shelf.value(QStringLiteral("launcherDefaultPage")).toString(), {"home", "apps"})
        || !oneOf(shelf.value(QStringLiteral("launcherWidth")).toString(),
                  {"compact", "standard", "wide"})) {
        setContractError(error, QStringLiteral("The Shelf backup settings are invalid."));
        return false;
    }
    for (const QString &key : keys) {
        if (key == QLatin1String("launcherDefaultPage") || key == QLatin1String("launcherWidth"))
            continue;
        if (!shelf.value(key).isBool()) {
            setContractError(error, QStringLiteral("The Shelf backup contains a non-boolean switch."));
            return false;
        }
    }
    return true;
}

bool validateNotificationSurface(const QJsonObject &surface, bool exactKeys, QString *error)
{
    const QSet<QString> keys{
        QStringLiteral("density"), QStringLiteral("surfaceStyle"),
        QStringLiteral("surfaceOpacityPercent"), QStringLiteral("showUnreadBadge"),
        QStringLiteral("showJobs"), QStringLiteral("showNotificationHistory"),
        QStringLiteral("notificationView"), QStringLiteral("notificationPreview")};
    if ((exactKeys ? !hasExactlyKeys(surface, keys) : !containsOnlyKeys(surface, keys))
        || !integerInRange(surface.value(QStringLiteral("surfaceOpacityPercent")), 70, 100)
        || !oneOf(surface.value(QStringLiteral("density")).toString(), {"compact", "comfortable"})
        || !oneOf(surface.value(QStringLiteral("surfaceStyle")).toString(),
                  {"theme", "flat", "tonal", "translucent"})
        || !oneOf(surface.value(QStringLiteral("notificationView")).toString(), {"cards", "compact"})
        || !oneOf(surface.value(QStringLiteral("notificationPreview")).toString(),
                  {"full", "summary", "hidden"})
        || !surface.value(QStringLiteral("showUnreadBadge")).isBool()
        || !surface.value(QStringLiteral("showJobs")).isBool()
        || !surface.value(QStringLiteral("showNotificationHistory")).isBool()) {
        setContractError(error, QStringLiteral("The notification presentation backup settings are invalid."));
        return false;
    }
    return true;
}

bool validateTimeCenter(const QJsonObject &timeCenter, QString *error)
{
    const QSet<QString> keys{
        QStringLiteral("density"), QStringLiteral("surfaceStyle"),
        QStringLiteral("surfaceOpacityPercent"), QStringLiteral("showUnreadBadge"),
        QStringLiteral("showJobs"), QStringLiteral("showNotificationHistory"),
        QStringLiteral("notificationView"), QStringLiteral("notificationPreview"),
        QStringLiteral("textScalePercent"), QStringLiteral("clockFormat"),
        QStringLiteral("showSeconds"), QStringLiteral("popupLayout"),
        QStringLiteral("defaultPage"), QStringLiteral("showWeekNumbers"),
        QStringLiteral("showSecondaryCalendar"), QStringLiteral("showDate"),
        QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")};
    if (!hasExactlyKeys(timeCenter, keys)) {
        setContractError(error, QStringLiteral("The Time Center backup settings are incomplete."));
        return false;
    }
    QJsonObject notificationSubset;
    for (const QString &key : {
             QStringLiteral("density"), QStringLiteral("surfaceStyle"),
             QStringLiteral("surfaceOpacityPercent"), QStringLiteral("showUnreadBadge"),
             QStringLiteral("showJobs"), QStringLiteral("showNotificationHistory"),
             QStringLiteral("notificationView"), QStringLiteral("notificationPreview")}) {
        notificationSubset.insert(key, timeCenter.value(key));
    }
    if (!validateNotificationSurface(notificationSubset, true, error)
        || !integerInRange(timeCenter.value(QStringLiteral("textScalePercent")), 75, 150)
        || !oneOf(timeCenter.value(QStringLiteral("clockFormat")).toString(), {"system", "24h", "12h"})
        || !oneOf(timeCenter.value(QStringLiteral("popupLayout")).toString(), {"standard", "wide"})
        || !oneOf(timeCenter.value(QStringLiteral("defaultPage")).toString(), {"notifications", "calendar"})) {
        if (error && error->isEmpty())
            *error = QStringLiteral("The Time Center backup settings are invalid.");
        return false;
    }
    const QStringList booleanKeys{
        QStringLiteral("showSeconds"), QStringLiteral("showWeekNumbers"),
        QStringLiteral("showSecondaryCalendar"), QStringLiteral("showDate"),
        QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")};
    for (const QString &key : booleanKeys) {
        if (!timeCenter.value(key).isBool()) {
            setContractError(error, QStringLiteral("The Time Center backup contains a non-boolean switch."));
            return false;
        }
    }
    return true;
}

bool validateSettings(const QJsonObject &settings, QString *error)
{
    const QSet<QString> settingsKeys{QStringLiteral("controlCenter"), QStringLiteral("shell")};
    if (!containsOnlyKeys(settings, settingsKeys)) {
        setContractError(error, QStringLiteral("The backup settings section contains unsupported products."));
        return false;
    }

    if (settings.contains(QStringLiteral("controlCenter"))) {
        const QJsonValue value = settings.value(QStringLiteral("controlCenter"));
        if (!value.isObject()) {
            setContractError(error, QStringLiteral("The Control Center backup settings are invalid."));
            return false;
        }
        const QJsonObject controlCenter = value.toObject();
        const QSet<QString> keys{QStringLiteral("layout"), QStringLiteral("topBar")};
        if (!hasExactlyKeys(controlCenter, keys)
            || !controlCenter.value(QStringLiteral("layout")).isObject()
            || !controlCenter.value(QStringLiteral("topBar")).isObject()
            || !validateControlCenterLayout(controlCenter.value(QStringLiteral("layout")).toObject(), error)
            || !validateTopBar(controlCenter.value(QStringLiteral("topBar")).toObject(), error)) {
            return false;
        }
    }

    if (settings.contains(QStringLiteral("shell"))) {
        const QJsonValue value = settings.value(QStringLiteral("shell"));
        if (!value.isObject()) {
            setContractError(error, QStringLiteral("The Meo Shell backup settings are invalid."));
            return false;
        }
        const QJsonObject shell = value.toObject();
        const QSet<QString> keys{
            QStringLiteral("shelf"), QStringLiteral("notifications"),
            QStringLiteral("timeCenter"), QStringLiteral("topTasks")};
        if (!hasExactlyKeys(shell, keys)
            || !shell.value(QStringLiteral("shelf")).isObject()
            || !shell.value(QStringLiteral("notifications")).isObject()
            || !shell.value(QStringLiteral("timeCenter")).isObject()
            || !shell.value(QStringLiteral("topTasks")).isObject()) {
            setContractError(error, QStringLiteral("The Meo Shell backup settings are incomplete."));
            return false;
        }
        if (!validateShelf(shell.value(QStringLiteral("shelf")).toObject(), error)
            || !validateNotificationSurface(shell.value(QStringLiteral("notifications")).toObject(), true, error)
            || !validateTimeCenter(shell.value(QStringLiteral("timeCenter")).toObject(), error)) {
            return false;
        }
        const QJsonObject topTasks = shell.value(QStringLiteral("topTasks")).toObject();
        const QSet<QString> topTaskKeys{QStringLiteral("taskLimit")};
        if (!hasExactlyKeys(topTasks, topTaskKeys)
            || !integerInRange(topTasks.value(QStringLiteral("taskLimit")), 1, 12)) {
            setContractError(error, QStringLiteral("The Top Tasks backup settings are invalid."));
            return false;
        }
    }
    return true;
}
}

QJsonObject BackupManifestContract::build(const QVariantList &applications,
                                          bool applicationInventoryIncluded,
                                          const QVariantMap &settings,
                                          const QString &createdAt,
                                          QString *error)
{
    if (applications.size() > kMaximumApplications) {
        setContractError(error, QStringLiteral("Too many applications for one backup manifest."));
        return {};
    }
    if (!applicationInventoryIncluded && !applications.isEmpty()) {
        setContractError(error, QStringLiteral("An unavailable application inventory must not contain application entries."));
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
             {QStringLiteral("applicationsState"),
              applicationInventoryIncluded ? QStringLiteral("included") : QStringLiteral("unavailable")},
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
                                          const QVariantMap &settings,
                                          const QString &createdAt,
                                          QString *error)
{
    return build(applications, true, settings, createdAt, error);
}

QJsonObject BackupManifestContract::build(const QVariantList &applications,
                                          const QString &createdAt,
                                          QString *error)
{
    return build(applications, true, {}, createdAt, error);
}

bool BackupManifestContract::validate(const QJsonObject &manifest, QString *error)
{
    const QSet<QString> manifestKeys{
        QStringLiteral("schema"), QStringLiteral("createdAt"),
        QStringLiteral("contents"), QStringLiteral("secretsIncluded")};
    if (!hasExactlyKeys(manifest, manifestKeys)) {
        setContractError(error, QStringLiteral("The backup manifest contains unsupported or missing top-level fields."));
        return false;
    }
    if (manifest.value(QStringLiteral("schema")).toString() != kSchema) {
        setContractError(error, QStringLiteral("Unsupported backup manifest format."));
        return false;
    }
    if (!manifest.value(QStringLiteral("secretsIncluded")).isBool()
        || manifest.value(QStringLiteral("secretsIncluded")).toBool()) {
        setContractError(error, QStringLiteral("Backup manifests containing secrets are not accepted."));
        return false;
    }
    if (!QDateTime::fromString(manifest.value(QStringLiteral("createdAt")).toString(), Qt::ISODate).isValid()) {
        setContractError(error, QStringLiteral("The backup timestamp is invalid."));
        return false;
    }

    const QJsonValue contentsValue = manifest.value(QStringLiteral("contents"));
    if (!contentsValue.isObject()) {
        setContractError(error, QStringLiteral("The backup contents section is missing."));
        return false;
    }
    const QJsonObject contents = contentsValue.toObject();
    const QSet<QString> contentKeys{
        QStringLiteral("applications"), QStringLiteral("applicationsState"),
        QStringLiteral("settings"), QStringLiteral("userData")};
    if (!hasExactlyKeys(contents, contentKeys)) {
        setContractError(error, QStringLiteral("The backup contents section contains unsupported or missing fields."));
        return false;
    }

    const QJsonValue applicationsStateValue = contents.value(QStringLiteral("applicationsState"));
    if (!applicationsStateValue.isString()
        || !oneOf(applicationsStateValue.toString(), {"included", "unavailable"})) {
        setContractError(error, QStringLiteral("The backup application inventory state is invalid."));
        return false;
    }
    const QString applicationsState = applicationsStateValue.toString();

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
    if (applicationsState == QLatin1String("unavailable") && !applications.isEmpty()) {
        setContractError(error, QStringLiteral("An unavailable application inventory must be empty."));
        return false;
    }
    for (const QJsonValue &value : applications) {
        if (!value.isObject()) {
            setContractError(error, QStringLiteral("The backup application list is invalid."));
            return false;
        }
        const QJsonObject row = value.toObject();
        const QSet<QString> allowedKeys{
            QStringLiteral("id"), QStringLiteral("sourceId"), QStringLiteral("version")};
        if (!containsOnlyKeys(row, allowedKeys)
            || !row.contains(QStringLiteral("id")) || !row.contains(QStringLiteral("sourceId"))
            || !safeAppId(row.value(QStringLiteral("id")).toString())
            || !safeSourceId(row.value(QStringLiteral("sourceId")).toString())
            || (row.contains(QStringLiteral("version"))
                && (!row.value(QStringLiteral("version")).isString()
                    || !safeVersion(row.value(QStringLiteral("version")).toString())))) {
            setContractError(error, QStringLiteral("The backup application list contains unsafe metadata."));
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
    setAvailable(true);
    QDBusConnection::sessionBus().connect(QStringLiteral("org.meo.Accounts1"),
        QStringLiteral("/org/meo/Accounts1"), QStringLiteral("org.meo.Accounts1"),
        QStringLiteral("accountChanged"), this, SLOT(clearCloudAccountState()));
}

QString BackupBackend::summary() const
{
    if (!available())
        return tr("Backup support is unavailable in this build.");
    if (m_lastBackupPath.isEmpty())
        return tr("No local Meo backup manifest has been created yet.");
    if (m_lastApplicationInventoryIncluded)
        return tr("Last local backup: %1 · application inventory included").arg(m_lastBackupAt);
    return tr("Last local backup: %1 · application inventory unavailable").arg(m_lastBackupAt);
}

QString BackupBackend::lastBackupPath() const { return m_lastBackupPath; }
QString BackupBackend::lastBackupAt() const { return m_lastBackupAt; }
QString BackupBackend::previewSummary() const { return m_previewSummary; }
QString BackupBackend::previewPath() const { return m_previewPath; }
bool BackupBackend::previewValid() const { return m_previewValid; }

void BackupBackend::setPortableSettingsSources(QObject *controlCenterBackend,
                                               QObject *shellSettingsBackend)
{
    m_controlCenterBackend = controlCenterBackend;
    m_shellSettingsBackend = shellSettingsBackend;
}

QVariantMap BackupBackend::portableSettingsSnapshot(QString *error) const
{
    QVariantMap settings;

    if (m_controlCenterBackend
        && m_controlCenterBackend->property("available").toBool()) {
        const QVariantList tiles = projectTiles(
            m_controlCenterBackend->property("tiles").toList());
        const QString density = m_controlCenterBackend->property("density").toString();
        const QVariantMap topBarSource = m_controlCenterBackend->property("topBar").toMap();
        const QVariantMap topBar = projectMap(topBarSource, {
            QStringLiteral("textScalePercent"), QStringLiteral("density"),
            QStringLiteral("surfaceStyle"), QStringLiteral("surfaceOpacityPercent"),
            QStringLiteral("motionProfile"), QStringLiteral("showUnreadBadge"),
            QStringLiteral("showJobs"), QStringLiteral("showNetwork"),
            QStringLiteral("showBluetooth"), QStringLiteral("showVolume"),
            QStringLiteral("batteryDisplay"), QStringLiteral("showDate"),
            QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")});

        settings.insert(QStringLiteral("controlCenter"), QVariantMap{
            {QStringLiteral("layout"), QVariantMap{
                 {QStringLiteral("tiles"), tiles},
                 {QStringLiteral("density"), density},
             }},
            {QStringLiteral("topBar"), topBar},
        });
    }

    if (m_shellSettingsBackend
        && m_shellSettingsBackend->property("available").toBool()) {
        const QVariantMap shelfSource = m_shellSettingsBackend->property("shelf").toMap();
        const QVariantMap notificationsSource = m_shellSettingsBackend->property("notifications").toMap();
        const QVariantMap timeCenterSource = m_shellSettingsBackend->property("timeCenter").toMap();
        const QVariantMap topTasksSource = m_shellSettingsBackend->property("topTasks").toMap();

        const QVariantMap shelf = projectMap(shelfSource, {
            QStringLiteral("showLauncherButton"), QStringLiteral("filterTasksByVirtualDesktop"),
            QStringLiteral("showRunningIndicators"), QStringLiteral("showTooltips"),
            QStringLiteral("launcherDefaultPage"), QStringLiteral("launcherWidth"),
            QStringLiteral("launcherShowFavorites"), QStringLiteral("launcherShowRecents")});
        const QVariantMap notifications = projectMap(notificationsSource, {
            QStringLiteral("density"), QStringLiteral("surfaceStyle"),
            QStringLiteral("surfaceOpacityPercent"), QStringLiteral("showUnreadBadge"),
            QStringLiteral("showJobs"), QStringLiteral("showNotificationHistory"),
            QStringLiteral("notificationView"), QStringLiteral("notificationPreview")});
        const QVariantMap timeCenter = projectMap(timeCenterSource, {
            QStringLiteral("density"), QStringLiteral("surfaceStyle"),
            QStringLiteral("surfaceOpacityPercent"), QStringLiteral("showUnreadBadge"),
            QStringLiteral("showJobs"), QStringLiteral("showNotificationHistory"),
            QStringLiteral("notificationView"), QStringLiteral("notificationPreview"),
            QStringLiteral("textScalePercent"), QStringLiteral("clockFormat"),
            QStringLiteral("showSeconds"), QStringLiteral("popupLayout"),
            QStringLiteral("defaultPage"), QStringLiteral("showWeekNumbers"),
            QStringLiteral("showSecondaryCalendar"), QStringLiteral("showDate"),
            QStringLiteral("showNotifications"), QStringLiteral("use24HourClock")});
        const QVariantMap topTasks = projectMap(topTasksSource, {
            QStringLiteral("taskLimit")});

        settings.insert(QStringLiteral("shell"), QVariantMap{
            {QStringLiteral("shelf"), shelf},
            {QStringLiteral("notifications"), notifications},
            {QStringLiteral("timeCenter"), timeCenter},
            {QStringLiteral("topTasks"), topTasks},
        });
    }

    QString validationError;
    const QJsonObject settingsObject = QJsonObject::fromVariantMap(settings);
    if (!validateSettings(settingsObject, &validationError)) {
        setContractError(error, validationError);
        return {};
    }
    return settings;
}

bool BackupBackend::createLocalManifest()
{
    clearError();
    QString snapshotError;
    const QVariantMap settings = portableSettingsSnapshot(&snapshotError);
    if (!snapshotError.isEmpty()) {
        setError(tr("Portable Meo settings could not be prepared: %1").arg(snapshotError));
        return false;
    }
    return createLocalManifestInternal(settings);
}

bool BackupBackend::createLocalManifestWithSettings(const QVariantMap &settings)
{
    return createLocalManifestInternal(settings);
}

bool BackupBackend::createLocalManifestInternal(const QVariantMap &settings)
{
    clearError();

    const bool applicationsIncluded = m_appsBackend
        && m_appsBackend->available() && !m_appsBackend->busy();
    const QVariantList applications = applicationsIncluded
        ? m_appsBackend->applications() : QVariantList{};

    setBusy(true);
    const QString createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString contractError;
    const QJsonObject manifest = BackupManifestContract::build(
        applications, applicationsIncluded, settings, createdAt, &contractError);
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
    if (payload.size() > kMaximumManifestBytes || file.write(payload) != payload.size() || !file.commit()) {
        setBusy(false);
        setError(tr("The backup manifest could not be saved safely."));
        return false;
    }

    m_lastBackupPath = path;
    m_lastBackupAt = createdAt;
    m_lastApplicationInventoryIncluded = applicationsIncluded;
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
    const QString applicationsState = contents.value(QStringLiteral("applicationsState")).toString();
    const QJsonObject settings = contents.value(QStringLiteral("settings")).toObject();
    int settingsGroups = 0;
    if (settings.contains(QStringLiteral("controlCenter")))
        settingsGroups += 2;
    if (settings.contains(QStringLiteral("shell")))
        settingsGroups += 4;

    const QString applicationSummary = applicationsState == QLatin1String("included")
        ? tr("%n application(s)", "", applications.size())
        : tr("application inventory unavailable");
    m_previewPath = info.canonicalFilePath();
    m_previewValid = true;
    m_previewSummary = tr("Valid Meo backup manifest · %1").arg(applicationSummary)
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

void BackupBackend::refreshCloudBackups()
{
    startCloudOperation(QStringLiteral("list_backups"));
}

void BackupBackend::clearCloudAccountState()
{
    ++m_cloudGeneration;
    m_cloudBusy = false;
    m_cloudState = QStringLiteral("not_loaded");
    m_cloudBackups.clear();
    if (m_cloudPreviewFile && m_previewPath == m_cloudPreviewFile->fileName()
        && m_restoreState != QLatin1String("applying")) {
        clearPreview();
        delete m_cloudPreviewFile;
    }
    Q_EMIT changed();
}

void BackupBackend::uploadCloudBackup()
{
    if (m_cloudBusy || busy()) return;
    if (!createLocalManifest()) return;
    QFile file(m_lastBackupPath);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
        setError(tr("The local backup could not be opened for upload."));
        return;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || !BackupManifestContract::validate(document.object())) {
        setError(tr("The local backup is no longer valid."));
        return;
    }
    if (document.toJson(QJsonDocument::Compact).size() > 524288) {
        setError(tr("Cloud backups accept manifests up to 512 KiB. Your local backup is still available."));
        return;
    }
    startCloudOperation(QStringLiteral("upload_backup"),
                        {{QStringLiteral("manifestJson"), QString::fromUtf8(document.toJson(QJsonDocument::Compact))}});
}

void BackupBackend::previewCloudBackup(const QString &id)
{
    startCloudOperation(QStringLiteral("download_backup"), {{QStringLiteral("backupId"), id}});
}

void BackupBackend::deleteCloudBackup(const QString &id)
{
    startCloudOperation(QStringLiteral("delete_backup"), {{QStringLiteral("backupId"), id}});
}

void BackupBackend::startCloudOperation(const QString &action, const QVariantMap &arguments)
{
    if (m_cloudBusy || busy() || m_restoreState == QLatin1String("applying")) return;
    clearError();
    if (action == QLatin1String("download_backup")) clearPreview();
    m_cloudBusy = true;
    m_cloudState = QStringLiteral("working");
    const quint64 generation = ++m_cloudGeneration;
    Q_EMIT changed();
    QDBusInterface broker(QStringLiteral("org.meo.Accounts1"), QStringLiteral("/org/meo/Accounts1"),
                          QStringLiteral("org.meo.Accounts1"), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        broker.asyncCall(QStringLiteral("StartAccountOperation"), action, arguments), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, action, generation](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        if (generation != m_cloudGeneration) return;
        if (reply.isError() || reply.value().isEmpty()) {
            m_cloudBusy = false;
            m_cloudState = QStringLiteral("failed");
            setError(tr("Meo Account could not start the backup operation. Connect your account and retry."));
            Q_EMIT changed();
            return;
        }
        pollCloudOperation(reply.value(), action, 0);
    });
}

void BackupBackend::pollCloudOperation(const QString &requestId, const QString &action, int attempts)
{
    const quint64 generation = m_cloudGeneration;
    QDBusInterface broker(QStringLiteral("org.meo.Accounts1"), QStringLiteral("/org/meo/Accounts1"),
                          QStringLiteral("org.meo.Accounts1"), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(broker.asyncCall(QStringLiteral("GetRequest"), requestId), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, requestId, action, attempts, generation](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QVariantMap> reply = *call;
        call->deleteLater();
        if (generation != m_cloudGeneration) return;
        const QVariantMap result = reply.value();
        const QString state = result.value(QStringLiteral("state")).toString();
        if (reply.isError() || result.isEmpty() || attempts >= 120
            || QStringList{QStringLiteral("failed"), QStringLiteral("expired"), QStringLiteral("cancelled")}.contains(state)) {
            m_cloudBusy = false;
            m_cloudState = QStringLiteral("failed");
            setError(tr("The cloud backup operation failed or timed out. Retry from Meo Account."));
            Q_EMIT changed();
            return;
        }
        if (state == QLatin1String("completed")) {
            const QByteArray bytes = result.value(QStringLiteral("backupResultJson")).toString().toUtf8();
            const QJsonDocument document = QJsonDocument::fromJson(bytes);
            if (bytes.size() > 600000 || !document.isObject()) {
                m_cloudBusy = false;
                m_cloudState = QStringLiteral("failed");
                setError(tr("The backup service returned an unreadable response."));
                Q_EMIT changed();
                return;
            }
            finishCloudOperation(action, document.object().toVariantMap());
            return;
        }
        QTimer::singleShot(500, this, [this, requestId, action, attempts, generation] {
            if (generation != m_cloudGeneration) return;
            pollCloudOperation(requestId, action, attempts + 1);
        });
    });
}

void BackupBackend::finishCloudOperation(const QString &action, const QVariantMap &result)
{
    m_cloudBusy = false;
    m_cloudState = QStringLiteral("ready");
    if (action == QLatin1String("list_backups")) {
        const QVariant rows = result.value(QStringLiteral("backups"));
        m_cloudBackups = rows.metaType().id() == qMetaTypeId<QDBusArgument>()
            ? qdbus_cast<QVariantList>(rows.value<QDBusArgument>()) : rows.toList();
        for (QVariant &row : m_cloudBackups) row = backupMap(row);
    } else if (action == QLatin1String("download_backup")) {
        const QVariantMap backup = backupMap(result.value(QStringLiteral("backup")));
        const QJsonObject manifest = QJsonObject::fromVariantMap(backupMap(backup.value(QStringLiteral("content"))));
        if (!BackupManifestContract::validate(manifest)) {
            m_cloudState = QStringLiteral("failed");
            setError(tr("The downloaded backup did not pass local validation."));
            Q_EMIT changed();
            return;
        }
        auto *file = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/meo-cloud-backup-XXXXXX.json"), this);
        const QByteArray bytes = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
        if (!file->open() || !file->setPermissions(QFile::ReadOwner | QFile::WriteOwner)
            || file->write(bytes) != bytes.size() || !file->flush()) {
            delete file;
            m_cloudState = QStringLiteral("failed");
            setError(tr("The downloaded backup could not be staged privately."));
        } else {
            file->close();
            if (m_cloudPreviewFile) delete m_cloudPreviewFile;
            m_cloudPreviewFile = file;
            if (!previewLocalManifest(file->fileName())) m_cloudState = QStringLiteral("failed");
        }
    } else {
        refreshCloudBackups();
    }
    Q_EMIT changed();
}
