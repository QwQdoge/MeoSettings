#include "../src/backends/updatesbackend.h"

#include <QTest>

class UpdatesBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesNativeUpdateLine();
    void rejectsUnsafeUpdateLine();
    void identifiesRepositoryFromCStableSyncInfo();
    void readsOnlyActiveRepositorySections();
    void detectsChannelFromResolvedRepositoryOrder();
    void classifiesMeoKdeCustomAndSystemUpdates();
    void parsesSharedOrchestratorState();
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

void UpdatesBackendTest::readsOnlyActiveRepositorySections()
{
    const QByteArray config = R"([options]
#[testing]
[core]
Include = /etc/pacman.d/mirrorlist

[meo]
Server = https://packages.example/meo
# [ignored]
)";
    QCOMPARE(SystemUpdatesContract::configuredRepositoryNames(config),
             QStringList({QStringLiteral("core"), QStringLiteral("meo")}));
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
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("meo-settings"), QStringLiteral("meo"), {}, configured),
             QStringLiteral("meo"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("plasma-desktop"), QStringLiteral("extra"), {}, configured),
             QStringLiteral("kde"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("my-tool"), QStringLiteral("local-tools"), {}, configured),
             QStringLiteral("custom"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("linux"), QStringLiteral("core"), {}, configured),
             QStringLiteral("system"));
    QCOMPARE(SystemUpdatesContract::updateFamily(QStringLiteral("foreign-tool"), QString(),
                                                  {QStringLiteral("foreign-tool")}, configured),
             QStringLiteral("aur"));
}

void UpdatesBackendTest::parsesSharedOrchestratorState()
{
    const auto state = SystemUpdatesContract::parseSharedUpdateState(R"({
        "schema":"org.meo.update-state","version":1,"count":2,
        "checked_at":"2026-08-30T01:02:03+00:00",
        "sources":{"pacman":1,"flatpak":1},"updates":[{},{}]
    })");
    QCOMPARE(state.value(QStringLiteral("count")).toInt(), 2);
    QCOMPARE(state.value(QStringLiteral("sources")).toList().size(), 2);
    QVERIFY(SystemUpdatesContract::parseSharedUpdateState(R"({"schema":"wrong"})").isEmpty());
}

QTEST_GUILESS_MAIN(UpdatesBackendTest)

#include "tst_updatesbackend.moc"
