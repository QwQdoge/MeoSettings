#include "../src/backends/updatesbackend.h"

#include <QTest>

class UpdatesBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesNativeUpdateLine();
    void rejectsUnsafeUpdateLine();
    void identifiesRepositoryFromCStableSyncInfo();
    void readsOfficialPackageCatalogWithoutPrefixInference();
    void detectsChannelFromResolvedRepositoryOrder();
    void classifiesMeoKdeCustomAndSystemUpdates();
};

void UpdatesBackendTest::parsesNativeUpdateLine()
{
    const auto update = SystemUpdatesContract::parsePacmanUpdateLine(
        QStringLiteral("meo-settings 1.2.0-1 -> 1.3.0-1"));
    QCOMPARE(update.value(QStringLiteral("name")).toString(), QStringLiteral("meo-settings"));
    QCOMPARE(update.value(QStringLiteral("installedVersion")).toString(), QStringLiteral("1.2.0-1"));
    QCOMPARE(update.value(QStringLiteral("availableVersion")).toString(), QStringLiteral("1.3.0-1"));
}

void UpdatesBackendTest::rejectsUnsafeUpdateLine()
{
    QVERIFY(SystemUpdatesContract::parsePacmanUpdateLine(
                QStringLiteral("bad package 1 -> 2")).isEmpty());
    QVERIFY(SystemUpdatesContract::parsePacmanUpdateLine(
                QStringLiteral("pkg 1 -> 2\nother")).isEmpty());
}

void UpdatesBackendTest::identifiesRepositoryFromCStableSyncInfo()
{
    const QByteArray output = R"(Repository      : meo
Name            : meo-settings
Version         : 1.3.0-1

Repository      : extra
Name            : plasma-desktop
Version         : 6.0.0-1
)";
    const auto info = SystemUpdatesContract::syncInfoForPackage(output, QStringLiteral("meo-settings"));
    QCOMPARE(info.value(QStringLiteral("repository")).toString(), QStringLiteral("meo"));
    QVERIFY(SystemUpdatesContract::syncInfoForPackage(output, QStringLiteral("absent")).isEmpty());
}

void UpdatesBackendTest::readsOfficialPackageCatalogWithoutPrefixInference()
{
    const QByteArray catalog = R"({
      "schemaVersion": 2,
      "officialPackages": ["meo-settings", "omnistore-bin"]
    })";
    QCOMPARE(SystemUpdatesContract::officialPackageNames(catalog),
             QStringList({QStringLiteral("meo-settings"), QStringLiteral("omnistore-bin")}));
    QVERIFY(SystemUpdatesContract::officialPackageNames(
                QByteArrayLiteral("{\"schemaVersion\": 1, \"officialPackages\": [\"meo-settings\"]}"))
                .isEmpty());
}

void UpdatesBackendTest::detectsChannelFromResolvedRepositoryOrder()
{
    QCOMPARE(SystemUpdatesContract::channelForRepositories(
                 {QStringLiteral("meo-beta"), QStringLiteral("meo")}),
             QStringLiteral("beta"));
    QCOMPARE(SystemUpdatesContract::channelForRepositories(
                 {QStringLiteral("meo"), QStringLiteral("meo-beta")}),
             QStringLiteral("invalid"));
    QCOMPARE(SystemUpdatesContract::channelForRepositories({QStringLiteral("extra"), QStringLiteral("meo")}),
             QStringLiteral("stable"));
}

void UpdatesBackendTest::classifiesMeoKdeCustomAndSystemUpdates()
{
    const QStringList configured{QStringLiteral("core"), QStringLiteral("meo"), QStringLiteral("local-tools")};
    const QStringList official{QStringLiteral("meo-settings")};
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("meo-settings"), QStringLiteral("meo"), {}, configured, official),
             QStringLiteral("meo"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("meo-unofficial"), QStringLiteral("meo"), {}, configured, official),
             QStringLiteral("system"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("plasma-desktop"), QStringLiteral("extra"), {}, configured, official),
             QStringLiteral("kde"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("my-tool"), QStringLiteral("local-tools"), {}, configured, official),
             QStringLiteral("custom"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("linux"), QStringLiteral("core"), {}, configured, official),
             QStringLiteral("system"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("foreign-tool"), QString(),
                                                  {QStringLiteral("foreign-tool")}, configured, official),
             QStringLiteral("aur"));
}

QTEST_GUILESS_MAIN(UpdatesBackendTest)

#include "tst_updatesbackend.moc"
