#include "../src/backends/powerbackend.h"

#include <QTest>

class PowerBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesAConsistentReadOnlyContract();
};

void PowerBackendTest::exposesAConsistentReadOnlyContract()
{
    PowerBackend backend;

    QVERIFY(backend.percent() >= -1);
    QVERIFY(backend.percent() <= 100);
    QCOMPARE(backend.percentKnown(), backend.percent() >= 0);
    QCOMPARE(backend.timeRemainingKnown(), backend.timeRemaining() >= 0);

    if (backend.available()) {
        QVERIFY(backend.state() != QStringLiteral("unavailable"));
        QVERIFY(!backend.stateLabel().isEmpty());
        QVERIFY(!backend.summary().isEmpty());
    } else {
        QCOMPARE(backend.percent(), -1);
        QCOMPARE(backend.state(), QStringLiteral("unavailable"));
        QVERIFY(!backend.charging());
        QVERIFY(!backend.timeRemainingKnown());
    }
}

QTEST_GUILESS_MAIN(PowerBackendTest)

#include "tst_powerbackend.moc"
