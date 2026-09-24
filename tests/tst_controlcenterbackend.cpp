#include "../src/backends/controlcenterbackend.h"

#include <QTest>

class ControlCenterBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizesLegacyAndIncompleteConfiguration();
    void serializesOnlyCompleteSafeLayouts();
    void normalizesAndValidatesTopBarAppearance();
    void scriptsAreScopedToTheMeoTopbarAuthority();
};

void ControlCenterBackendTest::normalizesLegacyAndIncompleteConfiguration()
{
    const auto layout = ControlCenterBackend::normalizedLayout(
        QStringLiteral("screenshot,wifi,wifi"),
        QStringLiteral("wifi:1,screenshot:1,unknown:1"),
        QStringLiteral("wifi,screenshot,unknown"),
        QStringLiteral("not-a-density"));

    const auto tiles = layout.value(QStringLiteral("tiles")).toList();
    QCOMPARE(tiles.size(), ControlCenterBackend::defaultTileIds().size());
    QCOMPARE(tiles.constFirst().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("screenshot"));
    QCOMPARE(tiles.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("wifi"));
    QCOMPARE(tiles.at(1).toMap().value(QStringLiteral("span")).toInt(), 1);
    QCOMPARE(tiles.constFirst().toMap().value(QStringLiteral("visible")).toBool(), true);
    QCOMPARE(tiles.at(2).toMap().value(QStringLiteral("visible")).toBool(), false);
    QCOMPARE(layout.value(QStringLiteral("density")).toString(), QStringLiteral("comfortable"));
}

void ControlCenterBackendTest::serializesOnlyCompleteSafeLayouts()
{
    const auto baseline = ControlCenterBackend::normalizedLayout({}, {}, {}, QStringLiteral("comfortable"));
    auto tiles = baseline.value(QStringLiteral("tiles")).toList();

    QString error;
    auto serialized = ControlCenterBackend::serializeLayout(tiles, QStringLiteral("compact"), &error);
    QVERIFY2(!serialized.isEmpty(), qPrintable(error));
    QCOMPARE(serialized.value(QStringLiteral("density")).toString(), QStringLiteral("compact"));
    QCOMPARE(serialized.value(QStringLiteral("visibility")).toString(),
             ControlCenterBackend::defaultTileIds().join(QLatin1Char(',')));

    tiles.removeLast();
    QVERIFY(ControlCenterBackend::serializeLayout(tiles, QStringLiteral("compact"), &error).isEmpty());
    QVERIFY(!error.isEmpty());

    const auto allHidden = baseline.value(QStringLiteral("tiles")).toList();
    QVariantList hiddenTiles;
    for (auto tile : allHidden) {
        auto map = tile.toMap();
        map.insert(QStringLiteral("visible"), false);
        hiddenTiles.push_back(map);
    }
    QVERIFY(ControlCenterBackend::serializeLayout(hiddenTiles, QStringLiteral("comfortable"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void ControlCenterBackendTest::normalizesAndValidatesTopBarAppearance()
{
    const auto normalized = ControlCenterBackend::normalizedTopBar({
        {QStringLiteral("textScalePercent"), 999},
        {QStringLiteral("density"), QStringLiteral("invalid")},
        {QStringLiteral("surfaceStyle"), QStringLiteral("invalid")},
        {QStringLiteral("surfaceOpacityPercent"), 10},
        {QStringLiteral("motionProfile"), QStringLiteral("invalid")},
        {QStringLiteral("batteryDisplay"), 99},
        {QStringLiteral("showDate"), false},
    });

    QCOMPARE(normalized.value(QStringLiteral("textScalePercent")).toInt(), 150);
    QCOMPARE(normalized.value(QStringLiteral("density")).toString(), QStringLiteral("comfortable"));
    QCOMPARE(normalized.value(QStringLiteral("surfaceStyle")).toString(), QStringLiteral("theme"));
    QCOMPARE(normalized.value(QStringLiteral("surfaceOpacityPercent")).toInt(), 70);
    QCOMPARE(normalized.value(QStringLiteral("motionProfile")).toString(), QStringLiteral("pixel"));
    QCOMPARE(normalized.value(QStringLiteral("batteryDisplay")).toInt(), 3);
    QCOMPARE(normalized.value(QStringLiteral("showDate")).toBool(), false);

    QString error;
    auto valid = ControlCenterBackend::normalizedTopBar({
        {QStringLiteral("textScalePercent"), 110},
        {QStringLiteral("density"), QStringLiteral("compact")},
        {QStringLiteral("surfaceStyle"), QStringLiteral("translucent")},
        {QStringLiteral("surfaceOpacityPercent"), 90},
        {QStringLiteral("motionProfile"), QStringLiteral("playful")},
        {QStringLiteral("batteryDisplay"), 1},
    });
    QVERIFY2(!ControlCenterBackend::serializeTopBar(valid, &error).isEmpty(), qPrintable(error));

    valid.insert(QStringLiteral("textScalePercent"), 200);
    error.clear();
    QVERIFY(ControlCenterBackend::serializeTopBar(valid, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void ControlCenterBackendTest::scriptsAreScopedToTheMeoTopbarAuthority()
{
    const auto readScript = ControlCenterBackend::readLayoutScript();
    const auto writeScript = ControlCenterBackend::writeLayoutScript(
        QStringLiteral("wifi,bluetooth"), QStringLiteral("wifi:2,bluetooth:1"),
        QStringLiteral("wifi"), QStringLiteral("spacious"));
    const auto writeTopBarScript = ControlCenterBackend::writeTopBarScript(
        ControlCenterBackend::normalizedTopBar({}));

    QVERIFY(readScript.contains(QStringLiteral("org.meo.topbar")));
    QVERIFY(readScript.contains(QStringLiteral("currentConfigGroup = [\"Appearance\"]")));
    QVERIFY(readScript.contains(QStringLiteral("quickTileVisibility")));
    QVERIFY(readScript.contains(QStringLiteral("quickTileDensity")));
    QVERIFY(writeScript.contains(QStringLiteral("matches.length !== 1")));
    QVERIFY(writeScript.contains(QStringLiteral("quickTileOrder")));
    QVERIFY(writeScript.contains(QStringLiteral("quickTileSizes")));
    QVERIFY(writeScript.contains(QStringLiteral("quickTileVisibility")));
    QVERIFY(writeScript.contains(QStringLiteral("quickTileDensity")));
    QVERIFY(writeScript.contains(QStringLiteral("topbar.reloadConfig()")));

    QVERIFY(readScript.contains(QStringLiteral("topBar")));
    QVERIFY(readScript.contains(QStringLiteral("textScalePercent")));
    QVERIFY(readScript.contains(QStringLiteral("surfaceOpacityPercent")));
    QVERIFY(readScript.contains(QStringLiteral("showNotifications")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("org.meo.topbar")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("currentConfigGroup = [\"Appearance\"]")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("textScalePercent")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("surfaceStyle")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("motionProfile")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("batteryDisplay")));
    QVERIFY(writeTopBarScript.contains(QStringLiteral("topbar.reloadConfig()")));
}

QTEST_GUILESS_MAIN(ControlCenterBackendTest)

#include "tst_controlcenterbackend.moc"
