#include <QFile>
#include <QString>
#include <QtTest>

#ifndef MEO_SETTINGS_SOURCE_DIR
#error MEO_SETTINGS_SOURCE_DIR must point at the source tree
#endif

namespace
{
QString sourceFile(const QString &relativePath)
{
    QFile file(QStringLiteral(MEO_SETTINGS_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}
}

class WelcomeReadinessTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void installedRuntimeBoundaryIsExplicit()
    {
        const QString main = sourceFile(QStringLiteral("src/welcome-main.cpp"));
        QVERIFY(!main.isEmpty());
        QVERIFY(main.contains(QStringLiteral("runtimeEnvironment")));
        QVERIFY(main.contains(QStringLiteral("QStringLiteral(\"installed\")")));
    }

    void networkReadinessUsesMachineWideNetworkManagerState()
    {
        const QString header = sourceFile(QStringLiteral("src/backends/networkbackend.h"));
        const QString implementation = sourceFile(QStringLiteral("src/backends/networkbackend.cpp"));
        QVERIFY(!header.isEmpty());
        QVERIFY(!implementation.isEmpty());

        QVERIFY(header.contains(QStringLiteral("systemConnected READ systemConnected")));
        QVERIFY(header.contains(QStringLiteral("internetAvailable READ internetAvailable")));
        QVERIFY(header.contains(QStringLiteral("connectivityState READ connectivityState")));
        QVERIFY(header.contains(QStringLiteral("primaryConnectionName READ primaryConnectionName")));

        QVERIFY(implementation.contains(QStringLiteral("NetworkManager::status()")));
        QVERIFY(implementation.contains(QStringLiteral("NetworkManager::connectivity()")));
        QVERIFY(implementation.contains(QStringLiteral("NetworkManager::primaryConnection()")));
    }

    void welcomeOwnsAccountConnectionButNotCredentials()
    {
        const QString qml = sourceFile(QStringLiteral("qml/Welcome.qml"));
        QVERIFY(!qml.isEmpty());

        QVERIFY(qml.contains(QStringLiteral("required property var accountBackend")));
        QVERIFY(qml.contains(QStringLiteral("requestAuthentication()")));
        QVERIFY(qml.contains(QStringLiteral("openAccountSettings()")));
        QVERIFY(!qml.contains(QStringLiteral("access_token")));
        QVERIFY(!qml.contains(QStringLiteral("refresh_token")));
        QVERIFY(!qml.contains(QStringLiteral("client_secret")));
    }

    void settingsHomeNamesInstalledCheckExplicitly()
    {
        const QString home = sourceFile(QStringLiteral("qml/pages/HomePage.qml"));
        QVERIFY(!home.isEmpty());
        QVERIFY(home.contains(QStringLiteral("Installed system readiness")));
        QVERIFY(home.contains(QStringLiteral("Run installed system check")));
        QVERIFY(!home.contains(QStringLiteral("six-step first-login guide")));
    }
};

QTEST_GUILESS_MAIN(WelcomeReadinessTest)
#include "tst_welcomereadiness.moc"
