#include "../src/core/lockscreenconfigstore.h"
#include "../src/core/sessionentrydocument.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QVariantMap nestedMap(const QVariantMap &parent, const QString &key)
{
    return parent.value(key).toMap();
}

void replaceNestedMap(QVariantMap *parent, const QString &key, QVariantMap child)
{
    parent->insert(key, std::move(child));
}
}

class SessionEntryDocumentTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsAreValidAndPrivacyBounded()
    {
        const QVariantMap lock = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        const auto lockResult = SessionEntryDocument::validate(
            lock, SessionEntryDocument::Scope::LockScreen);
        QVERIFY2(lockResult.ok, qPrintable(lockResult.error));
        QCOMPARE(nestedMap(lockResult.document, QStringLiteral("privacy"))
                     .value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("count"));

        const QVariantMap login = SessionEntryDocument::defaults(SessionEntryDocument::Scope::Login);
        const auto loginResult = SessionEntryDocument::validate(
            login, SessionEntryDocument::Scope::Login);
        QVERIFY2(loginResult.ok, qPrintable(loginResult.error));
        const QVariantMap loginModules = nestedMap(loginResult.document, QStringLiteral("modules"));
        QVERIFY(!loginModules.value(QStringLiteral("media")).toBool());
        QVERIFY(!loginModules.value(QStringLiteral("weather")).toBool());
        QVERIFY(!loginModules.value(QStringLiteral("audio")).toBool());
        QCOMPARE(nestedMap(loginResult.document, QStringLiteral("privacy"))
                     .value(QStringLiteral("notificationVisibility")).toString(),
                 QStringLiteral("hidden"));
    }

    void rejectsUnknownFieldsAtEveryTrustBoundary()
    {
        QVariantMap document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        document.insert(QStringLiteral("pamModule"), QStringLiteral("never-allowed"));
        auto result = SessionEntryDocument::validate(document);
        QVERIFY(!result.ok);
        QCOMPARE(result.path, QStringLiteral("$.pamModule"));

        document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        QVariantMap privacy = nestedMap(document, QStringLiteral("privacy"));
        privacy.insert(QStringLiteral("showPassword"), true);
        replaceNestedMap(&document, QStringLiteral("privacy"), privacy);
        result = SessionEntryDocument::validate(document);
        QVERIFY(!result.ok);
        QCOMPARE(result.path, QStringLiteral("$.privacy.showPassword"));
    }

    void rejectsUnsafeWallpaperIdentifiers()
    {
        for (const QString &assetId : {
                 QStringLiteral("../secret"),
                 QStringLiteral("wallpapers/../secret"),
                 QStringLiteral("https://example.invalid/a.png"),
                 QStringLiteral("wallpapers//image.png"),
             }) {
            QVariantMap document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
            QVariantMap appearance = nestedMap(document, QStringLiteral("appearance"));
            QVariantMap wallpaper = nestedMap(appearance, QStringLiteral("wallpaper"));
            wallpaper[QStringLiteral("source")] = QStringLiteral("current-user");
            wallpaper[QStringLiteral("assetId")] = assetId;
            replaceNestedMap(&appearance, QStringLiteral("wallpaper"), wallpaper);
            replaceNestedMap(&document, QStringLiteral("appearance"), appearance);

            const auto result = SessionEntryDocument::validate(document);
            QVERIFY2(!result.ok, qPrintable(assetId));
            QCOMPARE(result.path, QStringLiteral("$.appearance.wallpaper.assetId"));
        }
    }

    void enforcesLoginIsolation()
    {
        QVariantMap document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::Login);
        QVariantMap modules = nestedMap(document, QStringLiteral("modules"));
        modules[QStringLiteral("media")] = true;
        replaceNestedMap(&document, QStringLiteral("modules"), modules);
        auto result = SessionEntryDocument::validate(document, SessionEntryDocument::Scope::Login);
        QVERIFY(!result.ok);
        QCOMPARE(result.path, QStringLiteral("$.modules"));

        document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::Login);
        QVariantMap appearance = nestedMap(document, QStringLiteral("appearance"));
        QVariantMap wallpaper = nestedMap(appearance, QStringLiteral("wallpaper"));
        wallpaper[QStringLiteral("source")] = QStringLiteral("current-user");
        wallpaper[QStringLiteral("assetId")] = QStringLiteral("user-wallpaper");
        replaceNestedMap(&appearance, QStringLiteral("wallpaper"), wallpaper);
        replaceNestedMap(&document, QStringLiteral("appearance"), appearance);
        result = SessionEntryDocument::validate(document, SessionEntryDocument::Scope::Login);
        QVERIFY(!result.ok);
        QCOMPARE(result.path, QStringLiteral("$.appearance.wallpaper.source"));
    }

    void rejectsDuplicateDisplayIdentities()
    {
        QVariantMap document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        QVariantMap layout = nestedMap(document, QStringLiteral("layout"));
        const QVariantMap wallpaper{
            {QStringLiteral("source"), QStringLiteral("system-default")},
            {QStringLiteral("assetId"), QString()},
            {QStringLiteral("fillMode"), QStringLiteral("cover")},
        };
        const QVariantMap overrideValue{
            {QStringLiteral("outputKey"), QStringLiteral("opaque-screen-id")},
            {QStringLiteral("wallpaper"), wallpaper},
        };
        layout[QStringLiteral("displayOverrides")] = QVariantList{overrideValue, overrideValue};
        replaceNestedMap(&document, QStringLiteral("layout"), layout);

        const auto result = SessionEntryDocument::validate(document);
        QVERIFY(!result.ok);
        QCOMPARE(result.path, QStringLiteral("$.layout.displayOverrides[1].outputKey"));
    }

    void storeRoundTripsWithoutTouchingSystemConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("lockscreen-v1.json"));
        const LockScreenConfigStore store(path);

        auto initial = store.load();
        QVERIFY(initial.ok);
        QVERIFY(!initial.exists);

        QVariantMap document = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        QVariantMap privacy = nestedMap(document, QStringLiteral("privacy"));
        privacy[QStringLiteral("notificationVisibility")] = QStringLiteral("app-name");
        replaceNestedMap(&document, QStringLiteral("privacy"), privacy);

        QString error;
        QVERIFY2(store.save(document, &error), qPrintable(error));
        auto loaded = store.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        QVERIFY(loaded.exists);
        QCOMPARE(loaded.document, SessionEntryDocument::validate(document).document);

        const QFileInfo info(path);
        QVERIFY(info.exists());
        QVERIFY(info.permission(QFileDevice::ReadOwner));
        QVERIFY(info.permission(QFileDevice::WriteOwner));
        QVERIFY(!info.permission(QFileDevice::ReadGroup));
        QVERIFY(!info.permission(QFileDevice::ReadOther));

        QVERIFY2(store.resetToDefaults(&error), qPrintable(error));
        loaded = store.load();
        QVERIFY(loaded.ok);
        QVERIFY(!loaded.exists);
        QCOMPARE(loaded.document,
                 SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen));
    }

    void invalidSaveCannotDamageExistingDocument()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("lockscreen-v1.json"));
        const LockScreenConfigStore store(path);

        const QVariantMap valid = SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen);
        QString error;
        QVERIFY2(store.save(valid, &error), qPrintable(error));

        QFile beforeFile(path);
        QVERIFY(beforeFile.open(QIODevice::ReadOnly));
        const QByteArray before = beforeFile.readAll();
        beforeFile.close();

        QVariantMap invalid = valid;
        invalid[QStringLiteral("scope")] = QStringLiteral("login");
        QVERIFY(!store.save(invalid, &error));
        QVERIFY(!error.isEmpty());

        QFile afterFile(path);
        QVERIFY(afterFile.open(QIODevice::ReadOnly));
        QCOMPARE(afterFile.readAll(), before);
    }

    void malformedExistingDocumentFailsClosed()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("lockscreen-v1.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("{not-json") > 0);
        file.close();

        const LockScreenConfigStore store(path);
        const auto loaded = store.load();
        QVERIFY(!loaded.ok);
        QVERIFY(loaded.exists);
        QVERIFY(loaded.document.isEmpty());

        QString error;
        QVERIFY(!store.save(SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SessionEntryDocumentTest)
#include "tst_sessionentrydocument.moc"
