#include "../src/backends/shellsettingsbackend.h"

#include <QTest>

class ShellSettingsBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizesShelfConfiguration();
    void validatesPresentationRanges();
    void scriptsStayScopedToMeoShellApplets();
};

void ShellSettingsBackendTest::normalizesShelfConfiguration()
{
    const auto normalized = ShellSettingsBackend::normalizedShelf({
        {QStringLiteral("showLauncherButton"), false},
        {QStringLiteral("launcherDefaultPage"), QStringLiteral("invalid")},
        {QStringLiteral("launcherWidth"), QStringLiteral("huge")},
        {QStringLiteral("launcherShowFavorites"), false},
    });

    QCOMPARE(normalized.value(QStringLiteral("showLauncherButton")).toBool(), false);
    QCOMPARE(normalized.value(QStringLiteral("launcherDefaultPage")).toString(), QStringLiteral("home"));
    QCOMPARE(normalized.value(QStringLiteral("launcherWidth")).toString(), QStringLiteral("standard"));
    QCOMPARE(normalized.value(QStringLiteral("launcherShowFavorites")).toBool(), false);

    QString error;
    auto candidate = normalized;
    candidate.insert(QStringLiteral("launcherWidth"), QStringLiteral("wide"));
    QVERIFY2(!ShellSettingsBackend::serializeShelf(candidate, &error).isEmpty(), qPrintable(error));

    candidate.insert(QStringLiteral("launcherDefaultPage"), QStringLiteral("broken"));
    error.clear();
    QVERIFY(ShellSettingsBackend::serializeShelf(candidate, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void ShellSettingsBackendTest::validatesPresentationRanges()
{
    QString error;
    auto notifications = ShellSettingsBackend::normalizedNotifications({
        {QStringLiteral("surfaceOpacityPercent"), 90},
        {QStringLiteral("notificationPreview"), QStringLiteral("summary")},
    });
    QVERIFY2(!ShellSettingsBackend::serializeNotifications(notifications, &error).isEmpty(), qPrintable(error));

    notifications.insert(QStringLiteral("surfaceOpacityPercent"), 60);
    error.clear();
    QVERIFY(ShellSettingsBackend::serializeNotifications(notifications, &error).isEmpty());
    QVERIFY(!error.isEmpty());

    auto timeCenter = ShellSettingsBackend::normalizedTimeCenter({
        {QStringLiteral("textScalePercent"), 145},
        {QStringLiteral("clockFormat"), QStringLiteral("24h")},
        {QStringLiteral("popupLayout"), QStringLiteral("wide")},
        {QStringLiteral("defaultPage"), QStringLiteral("calendar")},
    });
    error.clear();
    QVERIFY2(!ShellSettingsBackend::serializeTimeCenter(timeCenter, &error).isEmpty(), qPrintable(error));

    auto topTasks = ShellSettingsBackend::normalizedTopTasks({{QStringLiteral("taskLimit"), 12}});
    error.clear();
    QVERIFY2(!ShellSettingsBackend::serializeTopTasks(topTasks, &error).isEmpty(), qPrintable(error));
    topTasks.insert(QStringLiteral("taskLimit"), 13);
    QVERIFY(ShellSettingsBackend::serializeTopTasks(topTasks, &error).isEmpty());
}

void ShellSettingsBackendTest::scriptsStayScopedToMeoShellApplets()
{
    const auto read = ShellSettingsBackend::readScript();
    QVERIFY(read.contains(QStringLiteral("org.meo.shelf")));
    QVERIFY(read.contains(QStringLiteral("org.meo.notifications")));
    QVERIFY(read.contains(QStringLiteral("org.meo.timecenter")));
    QVERIFY(read.contains(QStringLiteral("org.meo.toptasks")));
    QVERIFY(read.contains(QStringLiteral("currentConfigGroup = [\"Appearance\"]")));

    const auto shelf = ShellSettingsBackend::writeShelfScript(ShellSettingsBackend::normalizedShelf({}));
    QVERIFY(shelf.contains(QStringLiteral("org.meo.shelf")));
    QVERIFY(shelf.contains(QStringLiteral("showLauncherButton")));
    QVERIFY(shelf.contains(QStringLiteral("launcherShowRecents")));
    QVERIFY(shelf.contains(QStringLiteral("target.reloadConfig()")));

    const auto notifications = ShellSettingsBackend::writeNotificationsScript(
        ShellSettingsBackend::normalizedNotifications({}));
    QVERIFY(notifications.contains(QStringLiteral("org.meo.notifications")));
    QVERIFY(notifications.contains(QStringLiteral("notificationPreview")));

    const auto timeCenter = ShellSettingsBackend::writeTimeCenterScript(
        ShellSettingsBackend::normalizedTimeCenter({}));
    QVERIFY(timeCenter.contains(QStringLiteral("org.meo.timecenter")));
    QVERIFY(timeCenter.contains(QStringLiteral("showWeekNumbers")));

    const auto topTasks = ShellSettingsBackend::writeTopTasksScript(
        ShellSettingsBackend::normalizedTopTasks({}));
    QVERIFY(topTasks.contains(QStringLiteral("org.meo.toptasks")));
    QVERIFY(topTasks.contains(QStringLiteral("taskLimit")));
}

QTEST_GUILESS_MAIN(ShellSettingsBackendTest)

#include "tst_shellsettingsbackend.moc"
