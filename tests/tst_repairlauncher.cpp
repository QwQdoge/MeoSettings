#include "backends/repairlauncher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class RepairLauncherTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesOnlyVersionedCategories();
    void relevantSettingsPagesExposeTheMatchingFlow();
    void launchesTheFixedProgramWithOneValidatedCategory();
};

void RepairLauncherTest::exposesOnlyVersionedCategories()
{
    RepairLauncher launcher;
    const QStringList expected{
        QStringLiteral("all"), QStringLiteral("general"), QStringLiteral("audio"),
        QStringLiteral("display"), QStringLiteral("network"), QStringLiteral("boot"),
        QStringLiteral("packages"), QStringLiteral("storage"), QStringLiteral("graphics"),
        QStringLiteral("security"),
    };
    QCOMPARE(launcher.supportedCategories(), expected);
    QVERIFY(launcher.supportsCategory(QStringLiteral(" AUDIO ")));
    QVERIFY(!launcher.supportsCategory(QStringLiteral("audio;rm -rf /")));
}

void RepairLauncherTest::relevantSettingsPagesExposeTheMatchingFlow()
{
    const QList<QPair<QString, QString>> routes{
        {QStringLiteral("SoundPage.qml"), QStringLiteral("audio")},
        {QStringLiteral("DisplayPage.qml"), QStringLiteral("display")},
        {QStringLiteral("WifiPage.qml"), QStringLiteral("network")},
        {QStringLiteral("StoragePage.qml"), QStringLiteral("storage")},
        {QStringLiteral("UpdatesPage.qml"), QStringLiteral("packages")},
        {QStringLiteral("HardwarePage.qml"), QStringLiteral("graphics")},
        {QStringLiteral("RecoveryPage.qml"), QStringLiteral("boot")},
        {QStringLiteral("PrivacyPage.qml"), QStringLiteral("security")},
        {QStringLiteral("SystemCenterPage.qml"), QStringLiteral("general")},
    };
    const QDir pages(QStringLiteral(MEO_SETTINGS_SOURCE_DIR "/qml/pages"));
    for (const auto &[fileName, category] : routes) {
        QFile page(pages.filePath(fileName));
        QVERIFY2(page.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("Unable to read %1").arg(page.fileName())));
        const QByteArray source = page.readAll();
        QVERIFY2(source.contains("RepairEntry {"), qPrintable(fileName));
        QVERIFY2(source.contains(QStringLiteral("category: \"%1\"").arg(category).toUtf8()),
                 qPrintable(fileName));
    }
}

void RepairLauncherTest::launchesTheFixedProgramWithOneValidatedCategory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = directory.filePath(QStringLiteral("meoarch-repair"));
    const QString output = directory.filePath(QStringLiteral("arguments.txt"));
    QFile script(executable);
    QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
    script.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$MEO_SETTINGS_REPAIR_TEST_OUTPUT\"\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));

    qputenv("PATH", directory.path().toUtf8());
    qputenv("MEO_SETTINGS_REPAIR_TEST_OUTPUT", output.toUtf8());
    RepairLauncher launcher;
    QVERIFY(launcher.available());
    QVERIFY(launcher.open(QStringLiteral("network")));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(output), 3000);

    QFile arguments(output);
    QVERIFY(arguments.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(arguments.readAll(), QByteArray("--category\nnetwork\n"));
    arguments.close();
    QVERIFY(QFile::remove(output));

    QVERIFY(!launcher.open(QStringLiteral("bluetooth")));
    QVERIFY(!launcher.error().isEmpty());
    QTest::qWait(100);
    QVERIFY(!QFileInfo::exists(output));
}

QTEST_GUILESS_MAIN(RepairLauncherTest)

#include "tst_repairlauncher.moc"
