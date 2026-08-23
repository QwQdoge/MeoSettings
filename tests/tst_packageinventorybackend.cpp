#include "../src/backends/packageinventorybackend.h"

#include <QTest>

class PackageInventoryBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesCLocalePacmanLocalInfo();
    void skipsMalformedRecords();
    void separatesOfficialAndForeignPackages();
};

void PackageInventoryBackendTest::parsesCLocalePacmanLocalInfo()
{
    const QByteArray payload = R"(Name            : meo-settings
Installed Size  : 1.50 MiB

Name            : local-tool
Installed Size  : 512.00 KiB

)";
    const QVariantList packages = PackageInventoryContract::parsePacmanLocalInfo(payload);
    QCOMPARE(packages.size(), 2);
    QCOMPARE(packages.at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("meo-settings"));
    QCOMPARE(packages.at(0).toMap().value(QStringLiteral("sizeBytes")).toULongLong(),
             1572864ULL);
    QVERIFY(packages.at(1).toMap().value(QStringLiteral("sizeKnown")).toBool());
}

void PackageInventoryBackendTest::skipsMalformedRecords()
{
    const QByteArray payload = R"(Name            : malformed name
Installed Size  : 4.00 MiB

Name            : no-size
Installed Size  : unknown

)";
    const QVariantList packages = PackageInventoryContract::parsePacmanLocalInfo(payload);
    QCOMPARE(packages.size(), 1);
    QCOMPARE(packages.constFirst().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("no-size"));
    QVERIFY(!packages.constFirst().toMap().value(QStringLiteral("sizeKnown")).toBool());
}

void PackageInventoryBackendTest::separatesOfficialAndForeignPackages()
{
    const QVariantList packages = PackageInventoryContract::parsePacmanLocalInfo(R"(Name            : official
Installed Size  : 2.00 MiB

Name            : foreign-package
Installed Size  : 3.00 MiB

)");
    const QVariantList groups = PackageInventoryContract::summarize(
        packages, {QStringLiteral("foreign-package")});
    QCOMPARE(groups.size(), 2);
    const QVariantMap official = groups.at(0).toMap();
    const QVariantMap foreign = groups.at(1).toMap();
    QCOMPARE(official.value(QStringLiteral("id")).toString(), QStringLiteral("pacman"));
    QCOMPARE(official.value(QStringLiteral("packageCount")).toInt(), 1);
    QCOMPARE(official.value(QStringLiteral("knownSizeBytes")).toULongLong(), 2097152ULL);
    QCOMPARE(foreign.value(QStringLiteral("id")).toString(), QStringLiteral("foreign"));
    QCOMPARE(foreign.value(QStringLiteral("packageCount")).toInt(), 1);
    QCOMPARE(foreign.value(QStringLiteral("knownSizeBytes")).toULongLong(), 3145728ULL);
}

QTEST_GUILESS_MAIN(PackageInventoryBackendTest)

#include "tst_packageinventorybackend.moc"
