#include "../src/backends/configbackend.h"

#include <QtTest>

class ConfigBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsUnknownConfiguration()
    {
        ConfigBackend backend;
        QVERIFY(!backend.validate(QStringLiteral("unknown"), QString()).value(QStringLiteral("valid")).toBool());
    }

    void rejectsExecutableNvidiaHook()
    {
        ConfigBackend backend;
        const auto result = backend.validate(QStringLiteral("nvidia.modprobe"), QStringLiteral("install nvidia /bin/false"));
        QVERIFY(!result.value(QStringLiteral("valid")).toBool());
    }

    void previewDoesNotWrite()
    {
        ConfigBackend backend;
        const auto result = backend.preview(QStringLiteral("nvidia.modprobe"), QStringLiteral("options nvidia_drm modeset=1\n"));
        QVERIFY(result.value(QStringLiteral("valid")).toBool());
        QVERIFY(result.value(QStringLiteral("requiresRecoveryPoint")).toBool());
        QCOMPARE(backend.getOrigin(QStringLiteral("nvidia.modprobe")).value(QStringLiteral("managedBy")).toString(), QStringLiteral("Meo"));
    }
};

QTEST_MAIN(ConfigBackendTest)
#include "tst_configbackend.moc"
