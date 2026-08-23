#include "../src/backends/dynamiccolorbackend.h"

#include <QtTest>

class DynamicColorBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sourceArgumentsAreStrictAndArgumentSafe();
};

void DynamicColorBackendTest::sourceArgumentsAreStrictAndArgumentSafe()
{
    QString error;
    QCOMPARE(DynamicColorBackend::applyArguments(QStringLiteral("accent"), QString(), &error),
             QStringList({QStringLiteral("--source"), QStringLiteral("accent"),
                          QStringLiteral("--apply"), QStringLiteral("--remember-source")}));
    QCOMPARE(DynamicColorBackend::applyArguments(QStringLiteral("wallpaper"), QString(), &error),
             QStringList({QStringLiteral("--source"), QStringLiteral("wallpaper"),
                          QStringLiteral("--apply"), QStringLiteral("--remember-source")}));
    QCOMPARE(DynamicColorBackend::applyArguments(QStringLiteral("manual"), QStringLiteral("#4285f4"), &error),
             QStringList({QStringLiteral("--source"), QStringLiteral("manual"),
                          QStringLiteral("--accent"), QStringLiteral("#4285f4"),
                          QStringLiteral("--apply"), QStringLiteral("--remember-source")}));
    QVERIFY(DynamicColorBackend::applyArguments(QStringLiteral("manual"), QStringLiteral("blue"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(DynamicColorBackend::applyArguments(QStringLiteral("unknown"), QString(), &error).isEmpty());
}

QTEST_MAIN(DynamicColorBackendTest)

#include "tst_dynamiccolorbackend.moc"
