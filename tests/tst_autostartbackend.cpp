#include "../src/backends/autostartbackend.h"
#include <KDesktopFile>
#include <KConfigGroup>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class AutostartTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void overridePreservesSystemEntryAndUnknownKeys() {
        const QString system = qEnvironmentVariable("XDG_CONFIG_DIRS") + QStringLiteral("/autostart/example.desktop");
        QVERIFY(QDir().mkpath(QFileInfo(system).absolutePath()));
        QFile fixture(system); QVERIFY(fixture.open(QIODevice::WriteOnly));
        fixture.write("[Desktop Entry]\nType=Application\nName=Example\nExec=/usr/bin/true\nX-Vendor-Option=keep-this\nHidden=false\n"); fixture.close();
        AutostartBackend backend;
        backend.setEnabled(QStringLiteral("example.desktop"), false);
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
        const QString personal = qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/autostart/example.desktop");
        {
            const KDesktopFile entry(personal);
            QVERIFY(entry.desktopGroup().readEntry("Hidden", false));
            QCOMPARE(entry.desktopGroup().readEntry("X-Vendor-Option", QString()), QStringLiteral("keep-this"));
            QCOMPARE(entry.desktopGroup().readEntry("Exec", QString()), QStringLiteral("/usr/bin/true"));
        }
        const KDesktopFile original(system);
        QVERIFY(!original.desktopGroup().readEntry("Hidden", true));
        backend.removeUserEntry(QStringLiteral("example.desktop"));
        QVERIFY(backend.error().isEmpty());
        QVERIFY(!QFileInfo::exists(personal));
        QCOMPARE(backend.entries().size(), 1);
        QVERIFY(backend.entries().first().toMap().value("enabled").toBool());
    }
    void refusesTraversalAndPersonalSymlinkWrites() {
        const QString target = qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/outside.desktop");
        const QByteArray content("[Desktop Entry]\nName=Outside\nType=Application\nExec=/usr/bin/true\nHidden=false\n");
        QFile fixture(target); QVERIFY(fixture.open(QIODevice::WriteOnly)); fixture.write(content); fixture.close();
        const QString link = qEnvironmentVariable("XDG_CONFIG_HOME") + QStringLiteral("/autostart/link.desktop");
        QVERIFY(QFile::link(target, link));
        AutostartBackend backend;
        backend.setEnabled(QStringLiteral("link.desktop"), false);
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(fixture.open(QIODevice::ReadOnly)); QCOMPARE(fixture.readAll(), content); fixture.close();
        backend.setEnabled(QStringLiteral("../outside.desktop"), false);
        QVERIFY(!backend.error().isEmpty());
        QVERIFY(fixture.open(QIODevice::ReadOnly)); QCOMPARE(fixture.readAll(), content);
    }
};
int main(int argc, char **argv) {
    QTemporaryDir root; if (!root.isValid()) return 2;
    qputenv("XDG_CONFIG_HOME", (root.path() + "/user").toUtf8());
    qputenv("XDG_CONFIG_DIRS", (root.path() + "/system").toUtf8());
    qputenv("XDG_CACHE_HOME", (root.path() + "/cache").toUtf8());
    QCoreApplication app(argc, argv); AutostartTest test; return QTest::qExec(&test, argc, argv);
}
#include "tst_autostartbackend.moc"
