#include "../src/backends/applicationiconbackend.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ApplicationIconBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void argumentsAreStrictAndArgumentSafe();
    void rejectsUntrustedGeneratedImageData();
    void applicationDiscoveryDoesNotBlockTheGuiThread();
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

void ApplicationIconBackendTest::applicationDiscoveryDoesNotBlockTheGuiThread()
{
    QTemporaryDir toolsDirectory;
    QVERIFY(toolsDirectory.isValid());
    const QString toolPath = toolsDirectory.filePath(QStringLiteral("meo-app-icon-studio"));
    QFile tool(toolPath);
    QVERIFY(tool.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(tool.write("#!/bin/sh\nsleep 1\nprintf '[]'\n") > 0);
    tool.close();
    QVERIFY(tool.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                | QFileDevice::ExeOwner));

    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", toolsDirectory.path().toUtf8() + ':' + originalPath);
    QElapsedTimer constructionTimer;
    constructionTimer.start();
    ApplicationIconBackend backend;
    const qint64 constructionElapsed = constructionTimer.elapsed();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.applicationsLoading(), 2500);
    qputenv("PATH", originalPath);

    // The fake discovery tool deliberately runs for one second. Construction
    // must return promptly so QML can present its first frame and skeleton.
    QVERIFY2(constructionElapsed < 250,
             qPrintable(QStringLiteral("Construction blocked for %1 ms").arg(constructionElapsed)));
}

QTEST_MAIN(ApplicationIconBackendTest)

#include "tst_applicationiconbackend.moc"
