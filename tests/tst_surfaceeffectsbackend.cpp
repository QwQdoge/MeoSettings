#include "../src/backends/surfaceeffectsbackend.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QtTest>

class SurfaceEffectsBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                      + QStringLiteral("/meo-shellrc"));
    }

    void offByDefault()
    {
        SurfaceEffectsBackend backend;
        QVERIFY(!backend.blur());
        QVERIFY(!backend.transparency());
    }

    void blurImpliesTransparencyAndPersists()
    {
        SurfaceEffectsBackend backend;
        backend.setBlur(true);
        QVERIFY(backend.blur());
        QVERIFY(backend.transparency());

        const auto config = KSharedConfig::openConfig(QStringLiteral("meo-shellrc"));
        config->reparseConfiguration();
        const KConfigGroup group(config, QStringLiteral("Appearance"));
        QVERIFY(group.readEntry("SurfaceBlur", false));
        QVERIFY(group.readEntry("SurfaceTransparency", false));
    }

    void enablingBlurRaisesKWinStrengthOnlyWhenUnset()
    {
        const auto kwinrc = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                            + QStringLiteral("/kwinrc");
        QFile::remove(kwinrc);
        {
            SurfaceEffectsBackend backend;
            backend.setBlur(true);
        }
        auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        config->reparseConfiguration();
        QCOMPARE(KConfigGroup(config, QStringLiteral("Effect-blur")).readEntry("BlurStrength", 0), 10);

        KConfigGroup(config, QStringLiteral("Effect-blur")).writeEntry("BlurStrength", 3);
        config->sync();
        SurfaceEffectsBackend again;
        again.setBlur(false);
        again.setBlur(true);
        config->reparseConfiguration();
        QCOMPARE(KConfigGroup(config, QStringLiteral("Effect-blur")).readEntry("BlurStrength", 0), 3);
    }

    void disablingBlurKeepsTransparency()
    {
        SurfaceEffectsBackend backend;
        backend.setBlur(true);
        backend.setBlur(false);
        QVERIFY(!backend.blur());
        QVERIFY(backend.transparency());
    }

    void disablingTransparencyDisablesBlur()
    {
        SurfaceEffectsBackend backend;
        backend.setBlur(true);
        backend.setTransparency(false);
        QVERIFY(!backend.blur());
        QVERIFY(!backend.transparency());
    }
};

QTEST_GUILESS_MAIN(SurfaceEffectsBackendTest)
#include "tst_surfaceeffectsbackend.moc"
