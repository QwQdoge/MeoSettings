#include "../src/backends/apppermissionsbackend.h"

#include <QtTest>

class AppPermissionsBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesEffectiveFlatpakPermissionSections();
};

void AppPermissionsBackendTest::parsesEffectiveFlatpakPermissionSections()
{
    const QByteArray payload = R"([Context]
shared=network;ipc;
sockets=wayland;pulseaudio;
devices=all;
filesystems=xdg-download;home:ro;
persistent=cache;

[Session Bus Policy]
org.freedesktop.Notifications=talk
)";
    const QVariantList permissions = AppPermissionsBackend::parsePermissionOutput(payload);
    QCOMPARE(permissions.size(), 6);
    QCOMPARE(permissions.at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Shared services"));
    QCOMPARE(permissions.at(3).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Files"));
    QCOMPARE(permissions.last().toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Session Bus Policy"));
}

QTEST_MAIN(AppPermissionsBackendTest)

#include "tst_apppermissionsbackend.moc"
