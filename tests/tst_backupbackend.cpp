#include "../src/backends/backupbackend.h"

#include <QJsonArray>
#include <QJsonDocument>
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
}

class BackupBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void buildsOnlyWhitelistedApplicationMetadata();
    void buildsPortableMeoSettings();
    void representsUnavailableApplicationInventory();
    void rejectsSecretsAndUnexpectedFields();
    void rejectsNonCanonicalSettings();
    void rejectsUnsafeApplicationMetadata();
};

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
    QCOMPARE(manifest.value(QStringLiteral("schema")).toString(),
             QStringLiteral("org.meo.backup/v1"));
    QCOMPARE(manifest.value(QStringLiteral("secretsIncluded")).toBool(), false);

    const auto contents = manifest.value(QStringLiteral("contents")).toObject();
    QCOMPARE(contents.value(QStringLiteral("applicationsState")).toString(),
             QStringLiteral("included"));
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
    const auto settings = manifest.value(QStringLiteral("contents"))
                              .toObject().value(QStringLiteral("settings")).toObject();
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
    QCOMPARE(contents.value(QStringLiteral("applicationsState")).toString(),
             QStringLiteral("unavailable"));
    QVERIFY(contents.value(QStringLiteral("applications")).toArray().isEmpty());
    QVERIFY(BackupManifestContract::validate(manifest, &error));

    const QVariantList impossibleApps{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("org.example.App")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
        },
    };
    const auto impossibleManifest = BackupManifestContract::build(
        impossibleApps, false, portableSettings(), QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(impossibleManifest.isEmpty());

    QJsonObject tampered = manifest;
    QJsonObject tamperedContents = tampered.value(QStringLiteral("contents")).toObject();
    tamperedContents.insert(QStringLiteral("applications"), QJsonArray{
        QJsonObject{
            {QStringLiteral("id"), QStringLiteral("org.example.App")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
        },
    });
    tampered.insert(QStringLiteral("contents"), tamperedContents);
    QVERIFY(!BackupManifestContract::validate(tampered, &error));
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
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("../etc/passwd")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
        },
    };
    QString error;
    const auto manifest = BackupManifestContract::build(
        apps, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(manifest.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(BackupBackendTest)
#include "tst_backupbackend.moc"
