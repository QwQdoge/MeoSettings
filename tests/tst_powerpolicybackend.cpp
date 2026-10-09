#include "../src/backends/powerpolicybackend.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QDBusConnection>
#include <QDBusContext>
#include <QTemporaryDir>
#include <QTest>

class PowerOwner final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement")
public:
    bool closed = false;
    bool failReload = false;
    int reloads = 0;
public Q_SLOTS:
    bool isLidPresent() const { return true; }
    bool isLidClosed() const { return closed; }
    bool isActionSupported(const QString &) const { return true; }
    void refreshStatus() {
        ++reloads;
        if (failReload) sendErrorReply(QDBusError::Failed, QStringLiteral("Test reload failure"));
    }
    void reparseConfiguration() {}
};

class PowerPolicyTest final : public QObject
{
    Q_OBJECT
    PowerOwner owner;
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(qEnvironmentVariableIsSet("MEO_POWER_POLICY_TEST_BUS"));
        QVERIFY(QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.Solid.PowerManagement")));
        QVERIFY(QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/Solid/PowerManagement"), &owner, QDBusConnection::ExportAllSlots));
    }
    void closedLidDoesNotWriteOrReload() {
        owner.closed = true;
        PowerPolicyBackend backend;
        backend.setTimeout(QStringLiteral("AC"), QStringLiteral("screen"), 5);
        QTRY_VERIFY(!backend.busy());
        QVERIFY(!backend.error().isEmpty());
        const auto group = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"))->group(QStringLiteral("AC")).group(QStringLiteral("Display"));
        QVERIFY(!group.hasKey("TurnOffDisplayWhenIdle"));
        QCOMPARE(owner.reloads, 0);
    }
    void applyPreservesUnrelatedKeysAndFailureRestoresAbsence() {
        owner.closed = false;
        const auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
        auto group = config->group(QStringLiteral("Battery")).group(QStringLiteral("SuspendAndShutdown"));
        group.writeEntry("UnrelatedOption", 17); QVERIFY(group.sync());
        PowerPolicyBackend backend;
        QTRY_VERIFY(backend.lidPresent());
        backend.setLidPolicy(QStringLiteral("Battery"), 32, false);
        QTRY_VERIFY(!backend.busy());
        QVERIFY(backend.error().isEmpty());
        QCOMPARE(group.readEntry("LidAction", -1), 32);
        QCOMPARE(group.readEntry("UnrelatedOption", 0), 17);
        group.deleteEntry("InhibitLidActionWhenExternalMonitorPresent"); QVERIFY(group.sync());
        owner.failReload = true;
        backend.setLidPolicy(QStringLiteral("Battery"), 0, true);
        QTRY_VERIFY(!backend.busy());
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(group.readEntry("LidAction", -1), 32);
        QVERIFY(!group.hasKey("InhibitLidActionWhenExternalMonitorPresent"));
        QCOMPARE(group.readEntry("UnrelatedOption", 0), 17);
    }
    void batteryThresholdsAreOrderedAndDefaultActionIsPreserved() {
        PowerPolicyBackend backend;
        backend.setBatteryPolicy(5, 10, 0);
        QVERIFY(!backend.error().isEmpty());
        auto group = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"))->group(QStringLiteral("BatteryManagement"));
        QVERIFY(!group.hasKey("BatteryLowLevel"));
        backend.setBatteryPolicy(15, 7, -1);
        QTRY_VERIFY(!backend.busy());
        QVERIFY(backend.error().isEmpty());
        QCOMPARE(group.readEntry("BatteryLowLevel", 0), 15);
        QCOMPARE(group.readEntry("BatteryCriticalLevel", 0), 7);
        QVERIFY(!group.hasKey("BatteryCriticalAction"));
    }
};

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("MEO_POWER_POLICY_TEST_BUS")) return 2;
    QTemporaryDir config;
    if (!config.isValid()) return 3;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("XDG_CONFIG_DIRS", config.path().toUtf8());
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", qgetenv("DBUS_SESSION_BUS_ADDRESS"));
    QCoreApplication app(argc, argv);
    PowerPolicyTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_powerpolicybackend.moc"
