#include "../src/core/settingsregistry.h"

#include <QTest>

#include <algorithm>

class SettingsRegistryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesKeywordAliases();
    void exposesCompleteTopLevelArchitecture();
    void exposesCapabilityAwareCoreEntries();
    void separatesDisplayAndSoundFromDevices();
    void retainsDirectRoutes();
    void exposesNativeNotificationsRoute();
    void exposesControlCenterRoute();
    void exposesNativePowerAndNightLightRoutes();
    void exposesCuratedCategoryEntries();
    void resolvesKcmRoutes();
    void exposesStorageAndSafetyMetadata();
    void exposesNativeAccountAndUpdateRoutes();
    void unknownEntryIsEmpty();
};

void SettingsRegistryTest::resolvesKeywordAliases()
{
    SettingsRegistry registry;
    const auto results = registry.search(QStringLiteral("speaker"));
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("sound"));

    const auto wlanResults = registry.search(QStringLiteral("wlan"));
    QVERIFY(std::any_of(wlanResults.cbegin(), wlanResults.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("wifi");
    }));
}

void SettingsRegistryTest::exposesCompleteTopLevelArchitecture()
{
    SettingsRegistry registry;
    const QStringList requiredCategories{
        QStringLiteral("network"),
        QStringLiteral("devices"),
        QStringLiteral("display-sound"),
        QStringLiteral("personalization"),
        QStringLiteral("apps"),
        QStringLiteral("accounts"),
        QStringLiteral("storage"),
        QStringLiteral("system"),
        QStringLiteral("privacy"),
        QStringLiteral("accessibility"),
        QStringLiteral("updates"),
    };

    for (const auto &categoryId : requiredCategories) {
        const auto category = registry.category(categoryId);
        QVERIFY2(!category.isEmpty(), qPrintable(categoryId));
        QCOMPARE(category.value(QStringLiteral("route")).toString(),
                 QStringLiteral("category:") + categoryId);
        QVERIFY(!registry.entriesForCategory(categoryId).isEmpty());
    }

    const auto sidebar = registry.sidebarEntries();
    QVERIFY(std::any_of(sidebar.cbegin(), sidebar.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("type")).toString() == QStringLiteral("header");
    }));
}

void SettingsRegistryTest::exposesCapabilityAwareCoreEntries()
{
    SettingsRegistry registry;
    const auto wifi = registry.entry(QStringLiteral("wifi"));
    QCOMPARE(wifi.value(QStringLiteral("capability")).toString(), QStringLiteral("wifi"));

    const auto appearance = registry.entry(QStringLiteral("appearance"));
    QCOMPARE(appearance.value(QStringLiteral("route")).toString(), QStringLiteral("appearance"));
    QVERIFY(appearance.value(QStringLiteral("direct")).toBool());
    QCOMPARE(appearance.value(QStringLiteral("pageKind")).toString(), QStringLiteral("control"));
    QCOMPARE(appearance.value(QStringLiteral("risk")).toString(), QStringLiteral("system"));
}

void SettingsRegistryTest::separatesDisplayAndSoundFromDevices()
{
    SettingsRegistry registry;
    const auto category = registry.category(QStringLiteral("display-sound"));
    QCOMPARE(category.value(QStringLiteral("route")).toString(), QStringLiteral("category:display-sound"));

    const auto entries = registry.entriesForCategory(QStringLiteral("display-sound"));
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("display");
    }));
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("sound");
    }));
}

void SettingsRegistryTest::retainsDirectRoutes()
{
    SettingsRegistry registry;
    const auto display = registry.entry(QStringLiteral("display"));
    QCOMPARE(display.value(QStringLiteral("route")).toString(), QStringLiteral("display"));
    QVERIFY(display.value(QStringLiteral("direct")).toBool());
}

void SettingsRegistryTest::exposesNativeNotificationsRoute()
{
    SettingsRegistry registry;
    const auto notifications = registry.entry(QStringLiteral("notifications"));
    QCOMPARE(notifications.value(QStringLiteral("route")).toString(), QStringLiteral("notifications"));
    QVERIFY(notifications.value(QStringLiteral("direct")).toBool());
    QCOMPARE(notifications.value(QStringLiteral("pageKind")).toString(), QStringLiteral("control"));
    QCOMPARE(notifications.value(QStringLiteral("risk")).toString(), QStringLiteral("reversible"));

    const auto results = registry.search(QStringLiteral("popup timeout"));
    QVERIFY(std::any_of(results.cbegin(), results.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("notifications");
    }));

    const auto focusResults = registry.search(QStringLiteral("notification focus"));
    QVERIFY(std::any_of(focusResults.cbegin(), focusResults.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("notifications");
    }));
}

void SettingsRegistryTest::exposesControlCenterRoute()
{
    SettingsRegistry registry;
    const auto controlCenter = registry.entry(QStringLiteral("control-center"));
    QCOMPARE(controlCenter.value(QStringLiteral("route")).toString(), QStringLiteral("control-center"));
    QCOMPARE(controlCenter.value(QStringLiteral("categoryId")).toString(), QStringLiteral("system"));
    QVERIFY(controlCenter.value(QStringLiteral("direct")).toBool());
    QCOMPARE(controlCenter.value(QStringLiteral("authority")).toString(), QStringLiteral("meo"));

    const auto results = registry.search(QStringLiteral("quick settings"));
    QVERIFY(std::any_of(results.cbegin(), results.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("control-center");
    }));
}

void SettingsRegistryTest::exposesNativePowerAndNightLightRoutes()
{
    SettingsRegistry registry;

    const auto power = registry.entry(QStringLiteral("power"));
    QCOMPARE(power.value(QStringLiteral("route")).toString(), QStringLiteral("power"));
    QVERIFY(power.value(QStringLiteral("direct")).toBool());
    QCOMPARE(power.value(QStringLiteral("pageKind")).toString(), QStringLiteral("control"));
    QCOMPARE(power.value(QStringLiteral("risk")).toString(), QStringLiteral("reversible"));

    const auto nightLight = registry.entry(QStringLiteral("night-light"));
    QCOMPARE(nightLight.value(QStringLiteral("route")).toString(), QStringLiteral("display"));
    QVERIFY(nightLight.value(QStringLiteral("direct")).toBool());
    QCOMPARE(nightLight.value(QStringLiteral("pageKind")).toString(), QStringLiteral("control"));
}

void SettingsRegistryTest::exposesCuratedCategoryEntries()
{
    SettingsRegistry registry;
    const auto network = registry.category(QStringLiteral("network"));
    QCOMPARE(network.value(QStringLiteral("route")).toString(), QStringLiteral("category:network"));

    const auto entries = registry.entriesForCategory(QStringLiteral("network"));
    QVERIFY(entries.size() >= 3);
    QVERIFY(std::any_of(entries.cbegin(), entries.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("wifi");
    }));
}

void SettingsRegistryTest::resolvesKcmRoutes()
{
    SettingsRegistry registry;
    const auto fallback = registry.entry(QStringLiteral("kcm:kcm_componentchooser"));
    QCOMPARE(fallback.value(QStringLiteral("id")).toString(), QStringLiteral("default-apps"));
    QVERIFY(!fallback.value(QStringLiteral("direct")).toBool());
}

void SettingsRegistryTest::exposesStorageAndSafetyMetadata()
{
    SettingsRegistry registry;
    const auto storageCategory = registry.category(QStringLiteral("storage"));
    QCOMPARE(storageCategory.value(QStringLiteral("route")).toString(), QStringLiteral("category:storage"));
    QCOMPARE(storageCategory.value(QStringLiteral("depth")).toInt(), 1);

    const auto storage = registry.entry(QStringLiteral("storage"));
    QCOMPARE(storage.value(QStringLiteral("route")).toString(), QStringLiteral("storage"));
    QCOMPARE(storage.value(QStringLiteral("pageKind")).toString(), QStringLiteral("inspector"));
    QCOMPARE(storage.value(QStringLiteral("risk")).toString(), QStringLiteral("read-only"));
    QCOMPARE(storage.value(QStringLiteral("depth")).toInt(), 2);

    const auto appUsageResults = registry.search(QStringLiteral("omnistore app usage"));
    QVERIFY(std::any_of(appUsageResults.cbegin(), appUsageResults.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("storage");
    }));

    const auto partitions = registry.entry(QStringLiteral("disk-health"));
    QCOMPARE(partitions.value(QStringLiteral("presentation")).toString(), QStringLiteral("external"));
    QCOMPARE(partitions.value(QStringLiteral("authority")).toString(), QStringLiteral("kde"));
}

void SettingsRegistryTest::exposesNativeAccountAndUpdateRoutes()
{
    SettingsRegistry registry;

    const auto accounts = registry.entry(QStringLiteral("accounts"));
    QCOMPARE(accounts.value(QStringLiteral("route")).toString(), QStringLiteral("accounts"));
    QVERIFY(accounts.value(QStringLiteral("direct")).toBool());
    QCOMPARE(accounts.value(QStringLiteral("pageKind")).toString(), QStringLiteral("account"));
    QVERIFY(registry.entry(QStringLiteral("users")).isEmpty());

    const auto updates = registry.entry(QStringLiteral("updates"));
    QCOMPARE(updates.value(QStringLiteral("route")).toString(), QStringLiteral("updates"));
    QVERIFY(updates.value(QStringLiteral("direct")).toBool());
    QCOMPARE(updates.value(QStringLiteral("risk")).toString(), QStringLiteral("read-only"));

    const auto advancedUpdater = registry.entry(QStringLiteral("kcm:kcm_updates"));
    QCOMPARE(advancedUpdater.value(QStringLiteral("id")).toString(), QStringLiteral("system-updates-advanced"));
    QVERIFY(!advancedUpdater.value(QStringLiteral("direct")).toBool());
}

void SettingsRegistryTest::unknownEntryIsEmpty()
{
    SettingsRegistry registry;
    QVERIFY(registry.entry(QStringLiteral("not-a-setting")).isEmpty());
}

QTEST_GUILESS_MAIN(SettingsRegistryTest)

#include "tst_registry.moc"
