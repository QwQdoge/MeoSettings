#include "../src/backends/bluetoothbackend.h"
#include "../src/backends/bluetoothpairingagent.h"

#include <QtTest>

class BluetoothBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pairingAgentValidatesSecretsWithoutRetainingThem();
    void pairingAgentUsesKeyboardDisplayObjectPath();
    void backendPublishesTheNativeQmlContract();
};

void BluetoothBackendTest::pairingAgentValidatesSecretsWithoutRetainingThem()
{
    QVERIFY(BluetoothPairingAgent::isValidPin(QStringLiteral("1234")));
    QVERIFY(BluetoothPairingAgent::isValidPin(QStringLiteral("MeoPIN9")));
    QVERIFY(!BluetoothPairingAgent::isValidPin(QString{}));
    QVERIFY(!BluetoothPairingAgent::isValidPin(QStringLiteral("12345678901234567")));
    QVERIFY(!BluetoothPairingAgent::isValidPin(QStringLiteral("12-34")));

    QCOMPARE(BluetoothPairingAgent::parsePasskey(QStringLiteral("000001")), std::optional<quint32>(1));
    QCOMPARE(BluetoothPairingAgent::parsePasskey(QStringLiteral("999999")), std::optional<quint32>(999999));
    QVERIFY(!BluetoothPairingAgent::parsePasskey(QStringLiteral("1000000")));
    QVERIFY(!BluetoothPairingAgent::parsePasskey(QStringLiteral("12a")));

    BluetoothPairingAgent agent;
    agent.beginPairing(QStringLiteral("/org/bluez/hci0/dev_00_11_22_33_44_55"),
                       QStringLiteral("00:11:22:33:44:55"), QStringLiteral("Test device"));
    QVERIFY(!agent.hasAuthentication());
    QVERIFY(!agent.hasResponseRequest());
    agent.endPairing();
    QVERIFY(!agent.hasAuthentication());
}

void BluetoothBackendTest::pairingAgentUsesKeyboardDisplayObjectPath()
{
    const auto path = BluetoothPairingAgent::objectPath();
    QCOMPARE(path.path(), QStringLiteral("/org/meo/settings/BluetoothAgent"));
    QVERIFY(path.path().startsWith(QLatin1Char('/')));
}

void BluetoothBackendTest::backendPublishesTheNativeQmlContract()
{
    BluetoothBackend backend;
    const auto *meta = backend.metaObject();

    QVERIFY(meta->indexOfProperty("adapters") >= 0);
    QVERIFY(meta->indexOfProperty("activeAdapterUbi") >= 0);
    QVERIFY(meta->indexOfProperty("authentication") >= 0);
    QVERIFY(meta->indexOfProperty("authenticationPending") >= 0);
    QVERIFY(meta->indexOfMethod("pairDevice(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("cancelPairing()") >= 0);
    QVERIFY(meta->indexOfMethod("setDeviceTrusted(QString,bool)") >= 0);
    QVERIFY(meta->indexOfMethod("setDeviceBlocked(QString,bool)") >= 0);
    QVERIFY(meta->indexOfMethod("renameDevice(QString,QString)") >= 0);
    QVERIFY(meta->indexOfMethod("setAdapterDiscoverable(QString,bool)") >= 0);
    QVERIFY(meta->indexOfMethod("setAdapterPairable(QString,bool)") >= 0);
}

QTEST_MAIN(BluetoothBackendTest)

#include "tst_bluetoothbackend.moc"
