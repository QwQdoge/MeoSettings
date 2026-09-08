#include <QtTest>

#include "../src/backends/fingerprintbackend.h"

class FingerprintBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultPolicyIsSafeAndPrivate()
    {
        FingerprintBackend backend;
        const QVariantMap policy = backend.supportPackage();

        QCOMPARE(policy.value(QStringLiteral("defaultPAMTargets")).toStringList(),
                 QStringList{QStringLiteral("lock-screen")});
        QCOMPARE(policy.value(QStringLiteral("disabledPAMTargets")).toStringList(),
                 QStringList({QStringLiteral("login"), QStringLiteral("polkit"), QStringLiteral("sudo")}));
        QVERIFY(policy.value(QStringLiteral("passwordFallbackRequired")).toBool());
        QVERIFY(!backend.rawBiometricDataExposed());
    }
};

QTEST_MAIN(FingerprintBackendTest)

#include "tst_fingerprintbackend.moc"
