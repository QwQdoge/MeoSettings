#include "../src/backends/applicationiconbackend.h"

#include <QColor>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QString sha256File(const QString &path)
{
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&input);
    return QString::fromLatin1(hash.result().toHex());
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile output(path);
    return output.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && output.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) > 0;
}
}

class ApplicationIconBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void argumentsAreStrictAndArgumentSafe();
    void rejectsUntrustedGeneratedImageData();
    void applicationDiscoveryDoesNotBlockTheGuiThread();
    void attestedPackPreviewsBeforeAtomicApply();
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
                          QStringLiteral("--shape"), QStringLiteral("pixel")}));
    QCOMPARE(ApplicationIconBackend::applyArguments(QStringLiteral("original"), QStringLiteral("pixel"), QString(), &error),
             QStringList({QStringLiteral("--apply"), QStringLiteral("--style"), QStringLiteral("original"),
                          QStringLiteral("--shape"), QStringLiteral("pixel")}));
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
                          QStringLiteral("--shape"), QStringLiteral("circle")}));
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

    QElapsedTimer constructionTimer;
    constructionTimer.start();
    ApplicationIconBackend backend(nullptr, toolPath);
    const qint64 constructionElapsed = constructionTimer.elapsed();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.applicationsLoading(), 2500);

    // The fake package-owned discovery tool deliberately runs for one second.
    // Construction must return promptly so QML can present its first frame and
    // skeleton without consulting PATH or waiting for a process.
    QVERIFY2(constructionElapsed < 250,
             qPrintable(QStringLiteral("Construction blocked for %1 ms").arg(constructionElapsed)));
}

void ApplicationIconBackendTest::attestedPackPreviewsBeforeAtomicApply()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    constexpr auto desktopId = "org.example.App.desktop";
    const QString sourceHash(64, QLatin1Char('a'));
    const QString fixturePath = workspace.filePath(QStringLiteral("fixture.png"));
    QImage fixture(4, 4, QImage::Format_RGBA8888);
    fixture.fill(QColor(80, 120, 200));
    QVERIFY(fixture.save(fixturePath, "PNG"));
    const QString materialPath = workspace.filePath(QStringLiteral("material.png"));
    QVERIFY(fixture.save(materialPath, "PNG"));
    const QString materialHash = sha256File(materialPath);
    QVERIFY(!materialHash.isEmpty());

    const QJsonObject manifest{
        {QStringLiteral("schema"), 2},
        {QStringLiteral("packId"), QStringLiteral("pack-v1")},
        {QStringLiteral("styleId"), QStringLiteral("paper")},
        {QStringLiteral("provider"), QStringLiteral("account-managed")},
        {QStringLiteral("model"), QStringLiteral("image-model")},
        {QStringLiteral("promptRecipeVersion"), QStringLiteral("v2")},
        {QStringLiteral("items"), QJsonArray{QJsonObject{
            {QStringLiteral("desktopId"), QString::fromLatin1(desktopId)},
            {QStringLiteral("image"), QStringLiteral("material.png")},
            {QStringLiteral("imageSha256"), materialHash},
            {QStringLiteral("sourceIconHash"), sourceHash},
            {QStringLiteral("shape"), QStringLiteral("circle")},
            {QStringLiteral("prompt"), QStringLiteral("meo-style:paper:v2")},
        }}},
    };
    const QString manifestPath = workspace.filePath(QStringLiteral("pack.json"));
    QVERIFY(writeJson(manifestPath, manifest));

    const QString toolPath = workspace.filePath(QStringLiteral("meo-app-icon-studio"));
    QFile tool(toolPath);
    QVERIFY(tool.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray toolScript = QByteArrayLiteral("#!/bin/sh\n"
        "script_dir=$(dirname \"$0\")\n"
        "printf '%s\\n' \"$*\" >> \"$script_dir/invocations\"\n"
        "if [ \"$1\" = \"--list\" ]; then printf '[]'; exit 0; fi\n"
        "if [ \"$1\" = \"--describe\" ]; then\n"
        "  printf '[{\"schema\":1,\"desktopId\":\"org.example.App.desktop\",\"name\":\"Example\",\"canonicalIdentityHash\":\"")
        + sourceHash.toUtf8()
        + QByteArrayLiteral("\",\"canonicalIdentityAvailable\":true}]'; exit 0\n"
        "fi\n"
        "if [ \"$1\" = \"--preview-ai-pack\" ]; then\n"
        "  cp \"$script_dir/fixture.png\" \"$4/000.png\"\n"
        "  printf '{\"schema\":1,\"packId\":\"pack-v1\",\"applicationCount\":1,\"previews\":[{\"desktopId\":\"org.example.App.desktop\",\"preview\":\"%s/000.png\",\"sourceIconHash\":\"")
        + sourceHash.toUtf8()
        + QByteArrayLiteral("\",\"shape\":\"circle\"}]}' \"$4\"; exit 0\n"
        "fi\n"
        "if [ \"$1\" = \"--apply\" ] || [ \"$1\" = \"--reset\" ]; then printf '{\"atomic\":true}'; exit 0; fi\n"
        "exit 2\n");
    QVERIFY(tool.write(toolScript) == toolScript.size());
    tool.close();
    QVERIFY(tool.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                | QFileDevice::ExeOwner));

    ApplicationIconBackend backend(nullptr, toolPath);
    QTRY_VERIFY_WITH_TIMEOUT(!backend.applicationsLoading(), 1500);

    // A pack cannot be previewed merely from a path: its identity description
    // must originate in the package-owned Studio process first.
    backend.previewAttestedAiPack(manifestPath);
    QVERIFY(!backend.error().isEmpty());

    backend.describeAiPackApplications({QString::fromLatin1(desktopId)});
    QTRY_VERIFY_WITH_TIMEOUT(!backend.aiPackDescribing(), 1500);
    QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
    QCOMPARE(backend.aiPackDescriptors().size(), 1);
    QCOMPARE(backend.aiPackDescriptors().first().toMap().value(QStringLiteral("canonicalIdentityHash")).toString(),
             sourceHash);

    backend.previewAttestedAiPack(manifestPath);
    QTRY_VERIFY_WITH_TIMEOUT(!backend.aiPackPreviewing(), 1500);
    QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
    QVERIFY(backend.aiPackPreviewReady());
    QCOMPARE(backend.aiPackPreviews().size(), 1);
    const QString previewUrl = backend.aiPackPreviews().first().toMap()
                                   .value(QStringLiteral("preview")).toString();
    QVERIFY(previewUrl.startsWith(QLatin1String("file:")));

    QFile invocations(workspace.filePath(QStringLiteral("invocations")));
    QVERIFY(invocations.open(QIODevice::ReadOnly));
    QVERIFY(!invocations.readAll().contains("--apply"));
    invocations.close();

    // A manifest substitution after rendering cannot re-use the old preview.
    QFile tampered(manifestPath);
    QVERIFY(tampered.open(QIODevice::Append));
    QVERIFY(tampered.write("\n") == 1);
    tampered.close();
    backend.applyAttestedAiPack();
    QVERIFY(!backend.aiPackPreviewReady());
    QVERIFY(!backend.error().isEmpty());

    QVERIFY(writeJson(manifestPath, manifest));
    backend.previewAttestedAiPack(manifestPath);
    QTRY_VERIFY_WITH_TIMEOUT(!backend.aiPackPreviewing(), 1500);
    QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
    QVERIFY(backend.aiPackPreviewReady());

    backend.applyAttestedAiPack();
    QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 1500);
    QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
    QVERIFY(backend.aiPackCommitted());
    QVERIFY(invocations.open(QIODevice::ReadOnly));
    const QByteArray afterApply = invocations.readAll();
    QVERIFY(afterApply.contains("--apply --ai-pack"));
    invocations.close();

    backend.resetApplications({QString::fromLatin1(desktopId)});
    QTRY_VERIFY_WITH_TIMEOUT(!backend.busy(), 1500);
    QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
    QVERIFY(!backend.aiPackCommitted());
    QVERIFY(invocations.open(QIODevice::ReadOnly));
    QVERIFY(invocations.readAll().contains("--reset --app org.example.App.desktop"));
}

QTEST_MAIN(ApplicationIconBackendTest)

#include "tst_applicationiconbackend.moc"
