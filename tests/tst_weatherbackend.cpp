#include "../src/backends/weatherbackend.h"

#include <QtTest>

class WeatherBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizesCityWithoutTreatingItAsACommand();
    void rejectsEmptyCity();
};

void WeatherBackendTest::normalizesCityWithoutTreatingItAsACommand()
{
    QString error;
    QCOMPARE(WeatherBackend::normalizeCity(QStringLiteral("  Singapore   Central  "), &error),
             QStringLiteral("Singapore Central"));
    QVERIFY(error.isEmpty());
    QCOMPARE(WeatherBackend::normalizeCity(QStringLiteral("$(not executed)"), &error),
             QStringLiteral("$(not executed)"));
    QVERIFY(error.isEmpty());
}

void WeatherBackendTest::rejectsEmptyCity()
{
    QString error;
    QVERIFY(WeatherBackend::normalizeCity(QStringLiteral(" \t "), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(WeatherBackendTest)

#include "tst_weatherbackend.moc"
