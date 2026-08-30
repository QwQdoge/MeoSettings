#include "../src/backends/applicationiconbackend.h"

#include <QtTest>

class ApplicationIconBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void argumentsAreStrictAndArgumentSafe();
    void rejectsUntrustedGeneratedImageData();
};

void ApplicationIconBackendTest::argumentsAreStrictAndArgumentSafe()
{
    QString error;
    QCOMPARE(ApplicationIconBackend::applyArguments(QStringLiteral("monet"), QStringLiteral("circle"), QStringLiteral("Keep identity"), &error),
             QStringList({QStringLiteral("--apply"), QStringLiteral("--style"), QStringLiteral("monet"),
                          QStringLiteral("--shape"), QStringLiteral("circle"),
                          QStringLiteral("--prompt"), QStringLiteral("Keep identity")}));
    QCOMPARE(ApplicationIconBackend::applyArguments(QStringLiteral("original"), QStringLiteral("pixel"), QStringLiteral("Keep identity"), &error),
             QStringList({QStringLiteral("--apply"), QStringLiteral("--style"), QStringLiteral("original"),
                          QStringLiteral("--shape"), QStringLiteral("pixel"),
                          QStringLiteral("--prompt"), QStringLiteral("Keep identity")}));
    QVERIFY(ApplicationIconBackend::applyArguments(QStringLiteral("outline"), QStringLiteral("pixel"), QStringLiteral("Prompt"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(ApplicationIconBackend::applyArguments(QStringLiteral("monet"), QStringLiteral("triangle"), QStringLiteral("Prompt"), &error).isEmpty());
    QVERIFY(ApplicationIconBackend::applyArguments(QStringLiteral("monet"), QStringLiteral("circle"), QString(), &error).isEmpty());
    QCOMPARE(ApplicationIconBackend::applyArguments(QStringLiteral("pure"), QStringLiteral("circle"), QStringLiteral("Prompt"), &error),
             QStringList({QStringLiteral("--apply"), QStringLiteral("--style"), QStringLiteral("monet"),
                          QStringLiteral("--shape"), QStringLiteral("circle"),
                          QStringLiteral("--prompt"), QStringLiteral("Prompt")}));
    QVERIFY(ApplicationIconBackend::applyArguments(QStringLiteral("mono"), QStringLiteral("squircle"), QString(4001, QLatin1Char('x')), &error).isEmpty());
    QCOMPARE(ApplicationIconBackend::applyArguments(
                 QStringLiteral("original"), QStringLiteral("circle"),
                 QStringLiteral("Prompt"), &error,
                 {QStringLiteral("org.kde.dolphin.desktop")}),
             QStringList({QStringLiteral("--apply"), QStringLiteral("--app"),
                          QStringLiteral("org.kde.dolphin.desktop"),
                          QStringLiteral("--style"), QStringLiteral("original"),
                          QStringLiteral("--shape"), QStringLiteral("circle"),
                          QStringLiteral("--prompt"), QStringLiteral("Prompt")}));
    QVERIFY(ApplicationIconBackend::applyArguments(
        QStringLiteral("original"), QStringLiteral("circle"), QStringLiteral("Prompt"),
        &error, {QStringLiteral("bad\n.desktop")}).isEmpty());
}

void ApplicationIconBackendTest::rejectsUntrustedGeneratedImageData()
{
    ApplicationIconBackend backend;
    backend.applyAiImage(QStringLiteral("org.example.App.desktop"),
                         QStringLiteral("data:image/png;base64,bm90LWEtcG5n"),
                         QStringLiteral("pixel"), QStringLiteral("Keep identity"));
    QVERIFY(!backend.error().isEmpty());
}

QTEST_MAIN(ApplicationIconBackendTest)

#include "tst_applicationiconbackend.moc"
