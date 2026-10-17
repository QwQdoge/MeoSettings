#include "../src/backends/backupbackend.h"

#include <QFile>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

namespace
{
QVariantList canonicalTiles()
{
    const QStringList ids{
        QStringLiteral("wifi"), QStringLiteral("bluetooth"), QStringLiteral("focus"),
        QStringLiteral("nightLight"), QStringLiteral("keepAwake"), QStringLiteral("powerMode"),
        QStringLiteral("microphone"), QStringLiteral("audioDevices"),
        QStringLiteral("display"), QStringLiteral("screenshot")};
    QVariantList tiles;
    for (const QString &id : ids) {
        tiles.push_back(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("span"), 2},
            {QStringLiteral("visible"), true},
        });
    }
    return tiles;
}

QVariantMap portableSettings()
{
    const QVariantMap notifications{
        {QStringLiteral("density"), QStringLiteral("comfortable")},
        {QStringLiteral("surfaceStyle"), QStringLiteral("theme")},
        {QStringLiteral("surfaceOpacityPercent"), 100},
        {QStringLiteral("showUnreadBadge"), true},
        {QStringLiteral("showJobs"), true},
        {QStringLiteral("showNotificationHistory"), true},
        {QStringLiteral("notificationView"), QStringLiteral("cards")},
        {QStringLiteral("notificationPreview"), QStringLiteral("full")},
    };
    QVariantMap timeCenter = notifications;
    timeCenter.insert(QStringLiteral("textScalePercent"), 100);
    timeCenter.insert(QStringLiteral("clockFormat"), QStringLiteral("system"));
    timeCenter.insert(QStringLiteral("showSeconds"), false);
    timeCenter.insert(QStringLiteral("popupLayout"), QStringLiteral("standard"));
    timeCenter.insert(QStringLiteral("defaultPage"), QStringLiteral("notifications"));
    timeCenter.insert(QStringLiteral("showWeekNumbers"), false);
    timeCenter.insert(QStringLiteral("showSecondaryCalendar"), true);
    timeCenter.insert(QStringLiteral("showDate"), true);
    timeCenter.insert(QStringLiteral("showNotifications"), true);
    timeCenter.insert(QStringLiteral("use24HourClock"), true);

    return {
        {QStringLiteral("controlCenter"), QVariantMap{
             {QStringLiteral("layout"), QVariantMap{
                  {QStringLiteral("tiles"), canonicalTiles()},
                  {QStringLiteral("density"), QStringLiteral("comfortable")},
              }},
             {QStringLiteral("topBar"), QVariantMap{
                  {QStringLiteral("textScalePercent"), 100},
                  {QStringLiteral("density"), QStringLiteral("comfortable")},
                  {QStringLiteral("surfaceStyle"), QStringLiteral("theme")},
                  {QStringLiteral("surfaceOpacityPercent"), 100},
                  {QStringLiteral("motionProfile"), QStringLiteral("pixel")},
                  {QStringLiteral("showUnreadBadge"), true},
                  {QStringLiteral("showJobs"), true},
                  {QStringLiteral("showNetwork"), true},
                  {QStringLiteral("showBluetooth"), true},
                  {QStringLiteral("showVolume"), true},
                  {QStringLiteral("batteryDisplay"), 2},
                  {QStringLiteral("showDate"), true},
                  {QStringLiteral("showNotifications"), true},
                  {QStringLiteral("use24HourClock"), true},
              }},
         }},
        {QStringLiteral("shell"), QVariantMap{
             {QStringLiteral("shelf"), QVariantMap{
                  {QStringLiteral("showLauncherButton"), true},
                  {QStringLiteral("filterTasksByVirtualDesktop"), false},
                  {QStringLiteral("showRunningIndicators"), true},
                  {QStringLiteral("showTooltips"), true},
                  {QStringLiteral("launcherDefaultPage"), QStringLiteral("home")},
                  {QStringLiteral("launcherWidth"), QStringLiteral("standard")},
                  {QStringLiteral("launcherShowFavorites"), true},
                  {QStringLiteral("launcherShowRecents"), true},
              }},
             {QStringLiteral("notifications"), notifications},
             {QStringLiteral("timeCenter"), timeCenter},
             {QStringLiteral("topTasks"), QVariantMap{{QStringLiteral("taskLimit"), 8}}},
         }},
    };
}

bool writeManifest(const QString &path, const QJsonObject &manifest)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact)) > 0;
}
}

class FakeControlSettingsSource final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit FakeControlSettingsSource(QStringList *calls, QObject *parent = nullptr)
        : QObject(parent), m_calls(calls) {}

    bool available() const { return true; }
    bool busy() const { return false; }
    QString error() const { return m_error; }

    QVariantList receivedTiles;
    QString receivedDensity;
    QVariantMap receivedTopBar;

    Q_INVOKABLE void saveLayout(const QVariantList &tiles, const QString &density)
    {
        receivedTiles = tiles;
        receivedDensity = density;
        m_calls->push_back(QStringLiteral("control-center"));
        Q_EMIT layoutSaved();
    }

    Q_INVOKABLE void saveTopBar(const QVariantMap &values)
    {
        receivedTopBar = values;
        m_calls->push_back(QStringLiteral("top-bar"));
        Q_EMIT topBarSaved();
    }

Q_SIGNALS:
    void errorChanged();
    void layoutSaved();
    void topBarSaved();

private:
    QStringList *m_calls = nullptr;
    QString m_error;
};

class FakeShellSettingsSource final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit FakeShellSettingsSource(QStringList *calls,
                                     const QString &failStep = {},
                                     QObject *parent = nullptr)
        : QObject(parent), m_calls(calls), m_failStep(failStep) {}

    bool available() const { return true; }
    bool busy() const { return false; }
    QString error() const { return m_error; }

    QVariantMap receivedShelf, receivedNotifications, receivedTimeCenter, receivedTopTasks;
    Q_INVOKABLE void saveShelf(const QVariantMap &values) { receivedShelf = values; complete(QStringLiteral("shelf"), &FakeShellSettingsSource::shelfSaved); }
    Q_INVOKABLE void saveNotifications(const QVariantMap &values) { receivedNotifications = values; complete(QStringLiteral("notifications"), &FakeShellSettingsSource::notificationsSaved); }
    Q_INVOKABLE void saveTimeCenter(const QVariantMap &values) { receivedTimeCenter = values; complete(QStringLiteral("time-center"), &FakeShellSettingsSource::timeCenterSaved); }
    Q_INVOKABLE void saveTopTasks(const QVariantMap &values) { receivedTopTasks = values; complete(QStringLiteral("top-tasks"), &FakeShellSettingsSource::topTasksSaved); }

Q_SIGNALS:
    void errorChanged();
    void shelfSaved();
    void notificationsSaved();
    void timeCenterSaved();
    void topTasksSaved();

private:
    using SavedSignal = void (FakeShellSettingsSource::*)();

    void complete(const QString &step, SavedSignal signal)
    {
        m_calls->push_back(step);
        if (m_failStep == step) {
            m_error = QStringLiteral("Fake %1 failure").arg(step);
            Q_EMIT errorChanged();
            return;
        }
        Q_EMIT (this->*signal)();
    }

    QStringList *m_calls = nullptr;
    QString m_failStep;
    QString m_error;
};

class BackupBrokerFixture final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.meo.Accounts1")
public:
    QVariantMap payload;
    QString lastAction;
    QVariantMap lastArguments;
public Q_SLOTS:
    QString StartAccountOperation(const QString &action, const QVariantMap &arguments) {
        lastArguments = arguments;
        lastAction = action;
        return QStringLiteral("test-request");
    }
    QVariantMap GetRequest(const QString &) {
        return {{QStringLiteral("state"), QStringLiteral("completed")},
                {QStringLiteral("backupResultJson"), QString::fromUtf8(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact))}};
    }
};

class BackupBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cloudDownloadRequiresLocalValidation();
    void buildsOnlyWhitelistedApplicationMetadata();
    void buildsPortableMeoSettings();
    void representsUnavailableApplicationInventory();
    void buildsStructuredRestorePreviewPlan();
    void applyRejectsChangedPreviewManifest();
    void applyRejectsManifestWithoutPortableSettings();
    void appliesPortableSettingsInSerializedOrder();
    void stopsSerializedRestoreAfterFailure();
    void rejectsSecretsAndUnexpectedFields();
    void rejectsNonCanonicalSettings();
    void rejectsUnsafeApplicationMetadata();
};

void BackupBackendTest::cloudDownloadRequiresLocalValidation()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) QSKIP("Run under dbus-run-session for cloud broker checks");
    if (bus.interface()->isServiceRegistered(QStringLiteral("org.meo.Accounts1")))
        QSKIP("Do not replace a real Account broker; use an isolated session bus");
    BackupBrokerFixture broker;
    QVERIFY(bus.registerService(QStringLiteral("org.meo.Accounts1")));
    QVERIFY(bus.registerObject(QStringLiteral("/org/meo/Accounts1"), &broker, QDBusConnection::ExportAllSlots));
    BackupBackend backend(nullptr);
    broker.payload = {{QStringLiteral("backups"), QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("fixture-backup")}}}}};
    backend.refreshCloudBackups();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.cloudBusy(), 2000);
    QCOMPARE(backend.cloudBackups().size(), 1);
    QJsonObject manifest = BackupManifestContract::build({}, portableSettings(), QStringLiteral("2026-10-16T12:00:00Z"));
    const QString fixturePath = qEnvironmentVariable("MEO_BACKUP_TEST_MANIFEST");
    if (!fixturePath.isEmpty()) {
        QFile fixture(fixturePath);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        manifest = QJsonDocument::fromJson(fixture.readAll()).object();
        QVERIFY(BackupManifestContract::validate(manifest));
    }
    broker.payload = {{QStringLiteral("backup"), QVariantMap{{QStringLiteral("content"), manifest.toVariantMap()}}}};
    backend.previewCloudBackup(QStringLiteral("fixture-backup"));
    QTRY_VERIFY_WITH_TIMEOUT(!backend.cloudBusy(), 2000);
    QVERIFY(backend.previewValid());
    QCOMPARE(backend.restoreState(), QStringLiteral("idle"));
    const QFileInfo staged(backend.previewPath());
    QVERIFY(staged.exists());
    QVERIFY(!(staged.permissions() & (QFile::ReadOther | QFile::WriteOther)));
    QStringList applied;
    FakeControlSettingsSource control(&applied);
    FakeShellSettingsSource shell(&applied);
    backend.setPortableSettingsSources(&control, &shell);
    QVERIFY(backend.applyPreviewSettings());
    QCOMPARE(backend.restoreState(), QStringLiteral("succeeded"));
    QCOMPARE(applied, QStringList({QStringLiteral("control-center"), QStringLiteral("top-bar"),
        QStringLiteral("shelf"), QStringLiteral("notifications"), QStringLiteral("time-center"), QStringLiteral("top-tasks")}));
    QCOMPARE(backend.restoreResults().size(), 6);
    const auto expected = manifest.value(QStringLiteral("contents")).toObject().value(QStringLiteral("settings")).toObject();
    const auto expectedControl = expected.value(QStringLiteral("controlCenter")).toObject();
    const auto expectedShell = expected.value(QStringLiteral("shell")).toObject();
    QCOMPARE(control.receivedTiles, expectedControl.value(QStringLiteral("layout")).toObject().value(QStringLiteral("tiles")).toArray().toVariantList());
    QCOMPARE(control.receivedDensity, expectedControl.value(QStringLiteral("layout")).toObject().value(QStringLiteral("density")).toString());
    QCOMPARE(control.receivedTopBar, expectedControl.value(QStringLiteral("topBar")).toObject().toVariantMap());
    QCOMPARE(shell.receivedShelf, expectedShell.value(QStringLiteral("shelf")).toObject().toVariantMap());
    QCOMPARE(shell.receivedNotifications, expectedShell.value(QStringLiteral("notifications")).toObject().toVariantMap());
    QCOMPARE(shell.receivedTimeCenter, expectedShell.value(QStringLiteral("timeCenter")).toObject().toVariantMap());
    QCOMPARE(shell.receivedTopTasks, expectedShell.value(QStringLiteral("topTasks")).toObject().toVariantMap());


    auto unsafe = manifest;
    unsafe.insert(QStringLiteral("secretsIncluded"), true);
    broker.payload = {{QStringLiteral("backup"), QVariantMap{{QStringLiteral("content"), unsafe.toVariantMap()}}}};
    backend.previewCloudBackup(QStringLiteral("fixture-backup"));
    QTRY_VERIFY_WITH_TIMEOUT(!backend.cloudBusy(), 2000);
    QCOMPARE(backend.cloudState(), QStringLiteral("failed"));
    QVERIFY(QMetaObject::invokeMethod(&backend, "clearCloudAccountState"));
    QVERIFY(backend.cloudBackups().isEmpty());
    QVERIFY(!backend.previewValid());
    bus.unregisterObject(QStringLiteral("/org/meo/Accounts1"));
    bus.unregisterService(QStringLiteral("org.meo.Accounts1"));
}

void BackupBackendTest::buildsOnlyWhitelistedApplicationMetadata()
{
    const QVariantList apps{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("org.example.App")},
            {QStringLiteral("name"), QStringLiteral("Example")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
            {QStringLiteral("version"), QStringLiteral("1.2.3")},
            {QStringLiteral("storage"), QVariantList{QStringLiteral("/home/user/.secret")}},
            {QStringLiteral("settings"), QVariantMap{{QStringLiteral("path"), QStringLiteral("/tmp/no")}}},
        },
    };

    QString error;
    const auto manifest = BackupManifestContract::build(
        apps, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));
    QCOMPARE(manifest.value(QStringLiteral("schema")).toString(), QStringLiteral("org.meo.backup/v1"));
    QCOMPARE(manifest.value(QStringLiteral("secretsIncluded")).toBool(), false);

    const auto contents = manifest.value(QStringLiteral("contents")).toObject();
    QCOMPARE(contents.value(QStringLiteral("applicationsState")).toString(), QStringLiteral("included"));
    const auto applications = contents.value(QStringLiteral("applications")).toArray();
    QCOMPARE(applications.size(), 1);
    const auto app = applications.first().toObject();
    QCOMPARE(app.value(QStringLiteral("id")).toString(), QStringLiteral("org.example.App"));
    QCOMPARE(app.value(QStringLiteral("sourceId")).toString(), QStringLiteral("flatpak"));
    QCOMPARE(app.value(QStringLiteral("version")).toString(), QStringLiteral("1.2.3"));
    QVERIFY(!app.contains(QStringLiteral("name")));
    QVERIFY(!app.contains(QStringLiteral("storage")));
    QVERIFY(!app.contains(QStringLiteral("settings")));
    QVERIFY(contents.value(QStringLiteral("settings")).toObject().isEmpty());
    QVERIFY(BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::buildsPortableMeoSettings()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));
    const auto settings = manifest.value(QStringLiteral("contents")).toObject().value(QStringLiteral("settings")).toObject();
    QVERIFY(settings.contains(QStringLiteral("controlCenter")));
    QVERIFY(settings.contains(QStringLiteral("shell")));
    QVERIFY(!QJsonDocument(settings).toJson(QJsonDocument::Compact).contains("token"));
    QVERIFY(!QJsonDocument(settings).toJson(QJsonDocument::Compact).contains("/home/"));
    QVERIFY(BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::representsUnavailableApplicationInventory()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, false, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));
    const auto contents = manifest.value(QStringLiteral("contents")).toObject();
    QCOMPARE(contents.value(QStringLiteral("applicationsState")).toString(), QStringLiteral("unavailable"));
    QVERIFY(contents.value(QStringLiteral("applications")).toArray().isEmpty());
    QVERIFY(BackupManifestContract::validate(manifest, &error));

    const QVariantList impossibleApps{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("org.example.App")},
                    {QStringLiteral("sourceId"), QStringLiteral("flatpak")}},
    };
    const auto impossibleManifest = BackupManifestContract::build(
        impossibleApps, false, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(impossibleManifest.isEmpty());

    QJsonObject tampered = manifest;
    QJsonObject tamperedContents = tampered.value(QStringLiteral("contents")).toObject();
    tamperedContents.insert(QStringLiteral("applications"), QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("org.example.App")},
                    {QStringLiteral("sourceId"), QStringLiteral("flatpak")}},
    });
    tampered.insert(QStringLiteral("contents"), tamperedContents);
    QVERIFY(!BackupManifestContract::validate(tampered, &error));
}

void BackupBackendTest::buildsStructuredRestorePreviewPlan()
{
    QString error;
    const QVariantList apps{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("org.example.App")},
                    {QStringLiteral("sourceId"), QStringLiteral("flatpak")}},
    };
    const auto manifest = BackupManifestContract::build(
        apps, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("backup.json"));
    QVERIFY(writeManifest(path, manifest));

    BackupBackend backend(nullptr);
    QVERIFY(backend.previewLocalManifest(path));
    QVERIFY(backend.previewValid());
    const QVariantList plan = backend.previewPlan();
    QCOMPARE(plan.size(), 9);

    const auto rowFor = [&plan](const QString &id) {
        for (const QVariant &value : plan) {
            const QVariantMap row = value.toMap();
            if (row.value(QStringLiteral("id")).toString() == id)
                return row;
        }
        return QVariantMap{};
    };

    QCOMPARE(rowFor(QStringLiteral("applications")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("applications")).value(QStringLiteral("count")).toInt(), 1);
    QCOMPARE(rowFor(QStringLiteral("control-center")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("top-bar")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("shelf")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("notifications")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("time-center")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("top-tasks")).value(QStringLiteral("state")).toString(), QStringLiteral("included"));
    QCOMPARE(rowFor(QStringLiteral("user-data")).value(QStringLiteral("state")).toString(), QStringLiteral("not-included"));
    QCOMPARE(rowFor(QStringLiteral("accounts-secrets")).value(QStringLiteral("state")).toString(), QStringLiteral("not-included"));
}

void BackupBackendTest::applyRejectsChangedPreviewManifest()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("backup.json"));
    QVERIFY(writeManifest(path, manifest));

    BackupBackend backend(nullptr);
    QVERIFY(backend.previewLocalManifest(path));
    QVERIFY(backend.previewValid());
    QVERIFY(backend.canApplyRestore());

    QJsonObject tampered = manifest;
    tampered.insert(QStringLiteral("unexpected"), QStringLiteral("changed-after-preview"));
    QVERIFY(writeManifest(path, tampered));

    QVERIFY(!backend.applyPreviewSettings());
    QVERIFY(backend.error().contains(QStringLiteral("changed"), Qt::CaseInsensitive));
    QCOMPARE(backend.restoreState(), QStringLiteral("idle"));
}

void BackupBackendTest::applyRejectsManifestWithoutPortableSettings()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("backup.json"));
    QVERIFY(writeManifest(path, manifest));

    BackupBackend backend(nullptr);
    QVERIFY(backend.previewLocalManifest(path));
    QVERIFY(backend.previewValid());
    QVERIFY(!backend.applyPreviewSettings());
    QVERIFY(backend.error().contains(QStringLiteral("portable Meo settings")));
    QCOMPARE(backend.restoreState(), QStringLiteral("idle"));
}

void BackupBackendTest::appliesPortableSettingsInSerializedOrder()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("backup.json"));
    QVERIFY(writeManifest(path, manifest));

    QStringList calls;
    FakeControlSettingsSource control(&calls);
    FakeShellSettingsSource shell(&calls);
    BackupBackend backend(nullptr);
    backend.setPortableSettingsSources(&control, &shell);
    QVERIFY(backend.previewLocalManifest(path));
    QVERIFY(backend.applyPreviewSettings());

    QCOMPARE(backend.restoreState(), QStringLiteral("succeeded"));
    QCOMPARE(calls, QStringList({
        QStringLiteral("control-center"), QStringLiteral("top-bar"),
        QStringLiteral("shelf"), QStringLiteral("notifications"),
        QStringLiteral("time-center"), QStringLiteral("top-tasks") }));
    QCOMPARE(backend.restoreResults().size(), 6);
    for (const QVariant &value : backend.restoreResults())
        QCOMPARE(value.toMap().value(QStringLiteral("state")).toString(), QStringLiteral("applied"));
}

void BackupBackendTest::stopsSerializedRestoreAfterFailure()
{
    QString error;
    const auto manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("backup.json"));
    QVERIFY(writeManifest(path, manifest));

    QStringList calls;
    FakeControlSettingsSource control(&calls);
    FakeShellSettingsSource shell(&calls, QStringLiteral("notifications"));
    BackupBackend backend(nullptr);
    backend.setPortableSettingsSources(&control, &shell);
    QVERIFY(backend.previewLocalManifest(path));
    QVERIFY(backend.applyPreviewSettings());

    QCOMPARE(backend.restoreState(), QStringLiteral("partial-failure"));
    QCOMPARE(calls, QStringList({
        QStringLiteral("control-center"), QStringLiteral("top-bar"),
        QStringLiteral("shelf"), QStringLiteral("notifications") }));
    QCOMPARE(backend.restoreResults().size(), 4);
    QCOMPARE(backend.restoreResults().last().toMap().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
    QVERIFY(backend.restoreSummary().contains(QStringLiteral("3")));
}

void BackupBackendTest::rejectsSecretsAndUnexpectedFields()
{
    QString error;
    QJsonObject manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(!manifest.isEmpty());

    manifest.insert(QStringLiteral("secretsIncluded"), true);
    QVERIFY(!BackupManifestContract::validate(manifest, &error));
    manifest.insert(QStringLiteral("secretsIncluded"), false);

    manifest.insert(QStringLiteral("token"), QStringLiteral("must-not-be-accepted"));
    QVERIFY(!BackupManifestContract::validate(manifest, &error));
    manifest.remove(QStringLiteral("token"));

    QJsonObject contents = manifest.value(QStringLiteral("contents")).toObject();
    contents.insert(QStringLiteral("path"), QStringLiteral("/home/user"));
    manifest.insert(QStringLiteral("contents"), contents);
    QVERIFY(!BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::rejectsNonCanonicalSettings()
{
    QString error;
    QJsonObject manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(!manifest.isEmpty());

    QJsonObject contents = manifest.value(QStringLiteral("contents")).toObject();
    QJsonObject settings = contents.value(QStringLiteral("settings")).toObject();
    QJsonObject shell = settings.value(QStringLiteral("shell")).toObject();
    QJsonObject shelf = shell.value(QStringLiteral("shelf")).toObject();
    shelf.insert(QStringLiteral("token"), QStringLiteral("secret"));
    shell.insert(QStringLiteral("shelf"), shelf);
    settings.insert(QStringLiteral("shell"), shell);
    contents.insert(QStringLiteral("settings"), settings);
    manifest.insert(QStringLiteral("contents"), contents);
    QVERIFY(!BackupManifestContract::validate(manifest, &error));

    manifest = BackupManifestContract::build(
        {}, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    contents = manifest.value(QStringLiteral("contents")).toObject();
    settings = contents.value(QStringLiteral("settings")).toObject();
    QJsonObject controlCenter = settings.value(QStringLiteral("controlCenter")).toObject();
    QJsonObject layout = controlCenter.value(QStringLiteral("layout")).toObject();
    QJsonArray tiles = layout.value(QStringLiteral("tiles")).toArray();
    QJsonObject firstTile = tiles.first().toObject();
    firstTile.insert(QStringLiteral("span"), 3);
    tiles.replace(0, firstTile);
    layout.insert(QStringLiteral("tiles"), tiles);
    controlCenter.insert(QStringLiteral("layout"), layout);
    settings.insert(QStringLiteral("controlCenter"), controlCenter);
    contents.insert(QStringLiteral("settings"), settings);
    manifest.insert(QStringLiteral("contents"), contents);
    QVERIFY(!BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::rejectsUnsafeApplicationMetadata()
{
    const QVariantList apps{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("../etc/passwd")},
                    {QStringLiteral("sourceId"), QStringLiteral("flatpak")}},
    };
    QString error;
    const auto manifest = BackupManifestContract::build(
        apps, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(manifest.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(BackupBackendTest)
#include "tst_backupbackend.moc"
