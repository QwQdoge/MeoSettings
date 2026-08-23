#include "../src/backends/omnistoreappsbackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class OmniStoreAppsBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesAValidScopedUsageSnapshot();
    void rejectsInconsistentOrUnsafePayloads();
};

void OmniStoreAppsBackendTest::parsesAValidScopedUsageSnapshot()
{
    const QByteArray payload = R"json({
        "schema": "org.meo.omnistore.installed-usage",
        "version": 1,
        "status": "success",
        "generatedAt": "2026-08-23T10:00:00Z",
        "applicationCount": 3,
        "knownSizeBytes": 2300,
        "unknownSizeCount": 1,
        "sources": [],
        "applications": [
            {"name": "Exact App", "sourceId": "appimage", "sourceName": "AppImage", "sizeKind": "exact", "sizeBytes": 300},
            {"name": "Reported App", "sourceId": "flatpak", "sourceName": "Flatpak", "sizeKind": "reported", "sizeBytes": 2000},
            {"name": "Unknown App", "sourceId": "pacman", "sourceName": "Pacman", "sizeKind": "unknown"}
        ]
    })json";

    QString error;
    const auto snapshot = OmniStoreAppsContract::parse(payload, &error);
    QVERIFY2(snapshot.has_value(), qPrintable(error));
    QCOMPARE(snapshot->applicationCount, 3);
    QCOMPARE(snapshot->knownSizeBytes, 2300ULL);
    QCOMPARE(snapshot->unknownSizeCount, 1);
    QCOMPARE(snapshot->exactSizeCount, 1);
    QCOMPARE(snapshot->reportedSizeCount, 1);
    QCOMPARE(snapshot->sources.size(), 3);
    QCOMPARE(snapshot->topApplications.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Reported App"));
    QCOMPARE(snapshot->sources.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Flatpak"));
    QCOMPARE(snapshot->sources.first().toMap().value(QStringLiteral("sharePercent")).toDouble(),
             2000.0 / 23.0);
}

void OmniStoreAppsBackendTest::rejectsInconsistentOrUnsafePayloads()
{
    QJsonObject payload{
        {QStringLiteral("schema"), QStringLiteral("org.meo.omnistore.installed-usage")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("status"), QStringLiteral("success")},
        {QStringLiteral("generatedAt"), QStringLiteral("2026-08-23T10:00:00Z")},
        {QStringLiteral("applicationCount"), 1},
        {QStringLiteral("knownSizeBytes"), 10},
        {QStringLiteral("unknownSizeCount"), 0},
        {QStringLiteral("sources"), QJsonArray{}},
        {QStringLiteral("applications"), QJsonArray{QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Unsafe\nName")},
            {QStringLiteral("sourceId"), QStringLiteral("pacman")},
            {QStringLiteral("sourceName"), QStringLiteral("Pacman")},
            {QStringLiteral("sizeKind"), QStringLiteral("reported")},
            {QStringLiteral("sizeBytes"), 10},
        }}},
    };

    QString error;
    QVERIFY(!OmniStoreAppsContract::parse(QJsonDocument(payload).toJson(QJsonDocument::Compact), &error));
    QVERIFY(!error.isEmpty());

    payload.insert(QStringLiteral("applications"), QJsonArray{QJsonObject{
        {QStringLiteral("name"), QStringLiteral("Safe Name")},
        {QStringLiteral("sourceId"), QStringLiteral("pacman")},
        {QStringLiteral("sourceName"), QStringLiteral("Pacman")},
        {QStringLiteral("sizeKind"), QStringLiteral("reported")},
        {QStringLiteral("sizeBytes"), 10},
    }});
    payload.insert(QStringLiteral("knownSizeBytes"), 9);
    QVERIFY(!OmniStoreAppsContract::parse(QJsonDocument(payload).toJson(QJsonDocument::Compact), &error));
}

QTEST_GUILESS_MAIN(OmniStoreAppsBackendTest)

#include "tst_omnistoreappsbackend.moc"
