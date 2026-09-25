#include "../src/backends/lockscreenpresentationbackend.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class LockScreenPresentationBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_configHome.isValid());
        qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
    }

    void cleanupTestCase()
    {
        qunsetenv("XDG_CONFIG_HOME");
    }

    void defaultsArePrivacyBounded()
    {
        const QVariantMap values = LockScreenPresentationBackend::defaults();
        QCOMPARE(values.value(QStringLiteral("showWeather")).toBool(), true);
        QCOMPARE(values.value(QStringLiteral("showWeatherLocation")).toBool(), false);
        QCOMPARE(values.value(QStringLiteral("notificationVisibility")).toString(), QStringLiteral("count"));
    }

    void normalizesDependentChoices()
    {
        QVariantMap input = LockScreenPresentationBackend::defaults();
        input[QStringLiteral("showWeather")] = false;
        input[QStringLiteral("showWeatherLocation")] = true;
        input[QStringLiteral("showMediaControls")] = false;
        input[QStringLiteral("showAlbumArtwork")] = true;

        QString error;
        const QVariantMap values = LockScreenPresentationBackend::normalized(input, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(values.value(QStringLiteral("showWeatherLocation")).toBool(), false);
        QCOMPARE(values.value(QStringLiteral("showAlbumArtwork")).toBool(), false);
    }

    void rejectsUnknownPrivacyMode()
    {
        QVariantMap input = LockScreenPresentationBackend::defaults();
        input[QStringLiteral("notificationVisibility")] = QStringLiteral("everything");
        QString error;
        QVERIFY(LockScreenPresentationBackend::normalized(input, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void savesOnlyPresentationKeys()
    {
        LockScreenPresentationBackend backend;
        QVariantMap values = LockScreenPresentationBackend::defaults();
        values[QStringLiteral("showWeatherLocation")] = true;
        values[QStringLiteral("showPerformance")] = false;
        values[QStringLiteral("notificationVisibility")] = QStringLiteral("app-name");
        backend.save(values);

        backend.refresh();
        QCOMPARE(backend.settings().value(QStringLiteral("showWeatherLocation")).toBool(), true);
        QCOMPARE(backend.settings().value(QStringLiteral("showPerformance")).toBool(), false);
        QCOMPARE(backend.settings().value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("app-name"));

        const QString configPath = QDir(m_configHome.path()).filePath(QStringLiteral("kscreenlockerrc"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(config.readAll());
        QVERIFY(text.contains(QStringLiteral("[Greeter][LnF]")));
        QVERIFY(!text.contains(QStringLiteral("Authenticator")));
        QVERIFY(!text.contains(QStringLiteral("Autolock")));
    }

private:
    QTemporaryDir m_configHome;
};

QTEST_GUILESS_MAIN(LockScreenPresentationBackendTest)
#include "tst_lockscreenpresentationbackend.moc"
