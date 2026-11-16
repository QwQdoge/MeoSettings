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

    void cornerScaleDefaultsToTheDesignRadii()
    {
        SurfaceEffectsBackend backend;
        QCOMPARE(backend.cornerScale(), 100);
    }

    void cornerScalePersistsAndLeavesSurfaceEffectsAlone()
    {
        SurfaceEffectsBackend backend;
        backend.setBlur(true);
        backend.setCornerScale(40);
        QCOMPARE(backend.cornerScale(), 40);
        QVERIFY(backend.blur());

        const auto config = KSharedConfig::openConfig(QStringLiteral("meo-shellrc"));
        config->reparseConfiguration();
        const KConfigGroup group(config, QStringLiteral("Appearance"));
        QCOMPARE(group.readEntry("CornerScale", 0), 40);
        QVERIFY(group.readEntry("SurfaceBlur", false));
    }

    void cornerScaleIsClampedToTheShellRange()
    {
        SurfaceEffectsBackend backend;
        backend.setCornerScale(400);
        QCOMPARE(backend.cornerScale(), SurfaceEffectsBackend::maximumCornerScale());
        backend.setCornerScale(-20);
        QCOMPARE(backend.cornerScale(), SurfaceEffectsBackend::minimumCornerScale());
    }

    void cornerScaleFollowsExternalChanges()
    {
        SurfaceEffectsBackend backend;
        auto config = KSharedConfig::openConfig(QStringLiteral("meo-shellrc"));
        KConfigGroup(config, QStringLiteral("Appearance")).writeEntry("CornerScale", 130);
        config->sync();
        backend.refresh();
        QCOMPARE(backend.cornerScale(), 130);
    }
};

QTEST_GUILESS_MAIN(SurfaceEffectsBackendTest)
#include "tst_surfaceeffectsbackend.moc"
