#include "../src/backends/lockscreenpresentationbackend.h"
#include "../src/core/lockscreenconfigstore.h"
#include "../src/core/sessionentrydocument.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QVariantMap nestedMap(const QVariantMap &parent, const QString &key)
{
    return parent.value(key).toMap();
}
}

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

    void cleanup()
    {
        const QString documentPath = LockScreenConfigStore::defaultFilePath();
        if (!documentPath.isEmpty()) {
            QFile::remove(documentPath);
        }
        QFile::remove(QDir(m_configHome.path()).filePath(QStringLiteral("kscreenlockerrc")));
    }

    void defaultsMatchTheV1PrivacyContract()
    {
        const QVariantMap values = LockScreenPresentationBackend::defaults();
        QCOMPARE(values.value(QStringLiteral("showWeather")).toBool(), true);
        QCOMPARE(values.value(QStringLiteral("showWeatherLocation")).toBool(), true);
        QCOMPARE(values.value(QStringLiteral("showMediaControls")).toBool(), true);
        QCOMPARE(values.value(QStringLiteral("showAlbumArtwork")).toBool(), false);
        QCOMPARE(values.value(QStringLiteral("showAudioControls")).toBool(), true);
        QCOMPARE(values.value(QStringLiteral("notificationVisibility")).toString(), QStringLiteral("count"));
        QVERIFY(!values.contains(QStringLiteral("showPerformance")));
        QVERIFY(!values.contains(QStringLiteral("showSystemSummary")));
        QVERIFY(!values.contains(QStringLiteral("showSessionControls")));
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

    void rejectsNonBooleanToggleAndUnknownFields()
    {
        QVariantMap input = LockScreenPresentationBackend::defaults();
        input[QStringLiteral("showWeather")] = QStringLiteral("false");
        QString error;
        QVERIFY(LockScreenPresentationBackend::normalized(input, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        input = LockScreenPresentationBackend::defaults();
        input[QStringLiteral("showPerformance")] = true;
        error.clear();
        QVERIFY(LockScreenPresentationBackend::normalized(input, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("showPerformance")));
    }

    void rejectsUnknownPrivacyMode()
    {
        QVariantMap input = LockScreenPresentationBackend::defaults();
        input[QStringLiteral("notificationVisibility")] = QStringLiteral("everything");
        QString error;
        QVERIFY(LockScreenPresentationBackend::normalized(input, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void savesOnlyTheSupportedV1PresentationProjection()
    {
        LockScreenPresentationBackend backend;
        QVariantMap values = LockScreenPresentationBackend::defaults();
        values[QStringLiteral("showWeatherLocation")] = false;
        values[QStringLiteral("showAlbumArtwork")] = true;
        values[QStringLiteral("notificationVisibility")] = QStringLiteral("app-name");
        backend.save(values);
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));

        backend.refresh();
        QCOMPARE(backend.settings().value(QStringLiteral("showWeatherLocation")).toBool(), false);
        QCOMPARE(backend.settings().value(QStringLiteral("showAlbumArtwork")).toBool(), true);
        QCOMPARE(backend.settings().value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("app-name"));

        const QString documentPath = LockScreenConfigStore::defaultFilePath();
        QFile file(documentPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto parsed = SessionEntryDocument::parse(
            file.readAll(), SessionEntryDocument::Scope::LockScreen);
        QVERIFY2(parsed.ok, qPrintable(parsed.error));
        QCOMPARE(parsed.document.value(QStringLiteral("scope")).toString(), QStringLiteral("lockscreen"));
        QCOMPARE(nestedMap(parsed.document, QStringLiteral("modules"))
                     .value(QStringLiteral("weather")).toBool(), true);
        QCOMPARE(nestedMap(parsed.document, QStringLiteral("privacy"))
                     .value(QStringLiteral("weatherLocation")).toString(), QStringLiteral("hidden"));
        QCOMPARE(nestedMap(parsed.document, QStringLiteral("privacy"))
                     .value(QStringLiteral("showAlbumArtwork")).toBool(), true);
        QCOMPARE(nestedMap(parsed.document, QStringLiteral("privacy"))
                     .value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("app-name"));

        // The v1 writer must never fall back to the retired KScreenLocker LnF
        // store just because it exists on a Plasma system.
        QVERIFY(!QFile::exists(QDir(m_configHome.path()).filePath(QStringLiteral("kscreenlockerrc"))));
    }

    void resetRemovesTheUserDocumentAndRestoresSafeDefaults()
    {
        LockScreenPresentationBackend backend;
        QVariantMap values = LockScreenPresentationBackend::defaults();
        values[QStringLiteral("showAlbumArtwork")] = true;
        backend.save(values);
        QVERIFY(QFile::exists(LockScreenConfigStore::defaultFilePath()));

        backend.resetToDefaults();
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
        QVERIFY(!QFile::exists(LockScreenConfigStore::defaultFilePath()));
        QCOMPARE(backend.settings().value(QStringLiteral("showAlbumArtwork")).toBool(), false);
        QCOMPARE(backend.settings().value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("count"));
    }

private:
    QTemporaryDir m_configHome;
};

QTEST_GUILESS_MAIN(LockScreenPresentationBackendTest)
#include "tst_lockscreenpresentationbackend.moc"
