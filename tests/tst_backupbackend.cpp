#include "../src/backends/backupbackend.h"

#include <QJsonArray>
#include <QTest>

class BackupBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void buildsOnlyWhitelistedApplicationMetadata();
    void rejectsSecretsAndUnexpectedFields();
    void rejectsUnsafeApplicationMetadata();
};

void BackupBackendTest::buildsOnlyWhitelistedApplicationMetadata()
{
    const QVariantList apps{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("org.example.App")},
            {QStringLiteral("name"), QStringLiteral("Example")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
            {QStringLiteral("version"), QStringLiteral("1.2.3")},
            {QStringLiteral("storage"), QVariantList{QStringLiteral("/home/user/.secret")}},
            {QStringLiteral("settings"), QVariantMap{{QStringLiteral("path"), QStringLiteral("/tmp/no")}}},
        },
    };

    QString error;
    const auto manifest = BackupManifestContract::build(
        apps, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY2(!manifest.isEmpty(), qPrintable(error));
    QCOMPARE(manifest.value(QStringLiteral("schema")).toString(),
             QStringLiteral("org.meo.backup/v1"));
    QCOMPARE(manifest.value(QStringLiteral("secretsIncluded")).toBool(), false);

    const auto applications = manifest.value(QStringLiteral("contents"))
                                  .toObject()
                                  .value(QStringLiteral("applications"))
                                  .toArray();
    QCOMPARE(applications.size(), 1);
    const auto app = applications.first().toObject();
    QCOMPARE(app.value(QStringLiteral("id")).toString(), QStringLiteral("org.example.App"));
    QCOMPARE(app.value(QStringLiteral("sourceId")).toString(), QStringLiteral("flatpak"));
    QCOMPARE(app.value(QStringLiteral("version")).toString(), QStringLiteral("1.2.3"));
    QVERIFY(!app.contains(QStringLiteral("name")));
    QVERIFY(!app.contains(QStringLiteral("storage")));
    QVERIFY(!app.contains(QStringLiteral("settings")));
    QVERIFY(BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::rejectsSecretsAndUnexpectedFields()
{
    QString error;
    QJsonObject manifest{
        {QStringLiteral("schema"), QStringLiteral("org.meo.backup/v1")},
        {QStringLiteral("createdAt"), QStringLiteral("2026-10-10T02:00:00Z")},
        {QStringLiteral("secretsIncluded"), true},
        {QStringLiteral("contents"), QJsonObject{
             {QStringLiteral("applications"), QJsonArray{}},
             {QStringLiteral("settings"), QJsonArray{}},
             {QStringLiteral("userData"), QJsonArray{}},
         }},
    };
    QVERIFY(!BackupManifestContract::validate(manifest, &error));

    manifest.insert(QStringLiteral("secretsIncluded"), false);
    manifest[QStringLiteral("contents")].toObject();
    QJsonObject contents = manifest.value(QStringLiteral("contents")).toObject();
    contents.insert(QStringLiteral("applications"), QJsonArray{
        QJsonObject{
            {QStringLiteral("id"), QStringLiteral("org.example.App")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
            {QStringLiteral("token"), QStringLiteral("must-not-be-accepted")},
        },
    });
    manifest.insert(QStringLiteral("contents"), contents);
    QVERIFY(!BackupManifestContract::validate(manifest, &error));
}

void BackupBackendTest::rejectsUnsafeApplicationMetadata()
{
    const QVariantList apps{
        QVariantMap{
            {QStringLiteral("id"), QStringLiteral("../etc/passwd")},
            {QStringLiteral("sourceId"), QStringLiteral("flatpak")},
        },
    };
    QString error;
    const auto manifest = BackupManifestContract::build(
        apps, QStringLiteral("2026-10-10T02:00:00Z"), &error);
    QVERIFY(manifest.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(BackupBackendTest)
#include "tst_backupbackend.moc"
