#include "../src/backends/meoaccountbackend.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class MeoAccountBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyAcceptsSafeProfileText();
    void onlyAcceptsHttpsRemoteAvatars();
    void clientManifestSeparatesLocalAiCapabilityFromOauthScopes();
    void normalizesAttestedAiIconPackRequests();
    void rejectsUntrustedAiIconPackRequests();
};

void MeoAccountBackendTest::onlyAcceptsSafeProfileText()
{
    QCOMPARE(MeoAccountContract::safeProfileText(QStringLiteral("Cloud User")),
             QStringLiteral("Cloud User"));
    QVERIFY(MeoAccountContract::safeProfileText(QStringLiteral("unsafe\nname")).isEmpty());
    QVERIFY(MeoAccountContract::safeProfileText(QString()).isEmpty());
}

void MeoAccountBackendTest::onlyAcceptsHttpsRemoteAvatars()
{
    QCOMPARE(MeoAccountContract::safeRemoteAvatarSource(
                 QStringLiteral("https://profile.example/avatar.png")),
             QStringLiteral("https://profile.example/avatar.png"));
    QVERIFY(MeoAccountContract::safeRemoteAvatarSource(
                 QStringLiteral("file:///home/user/.face")).isEmpty());
    QVERIFY(MeoAccountContract::safeRemoteAvatarSource(
                 QStringLiteral("http://profile.example/avatar.png")).isEmpty());
}

void MeoAccountBackendTest::clientManifestSeparatesLocalAiCapabilityFromOauthScopes()
{
    const QString path = QFINDTESTDATA(
        "../data/meo-account/clients/org.meo.Settings.json");
    QVERIFY2(!path.isEmpty(), "Meo Settings client manifest was not found");
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject manifest = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(manifest.value(QStringLiteral("scopes")).toArray(),
             QJsonArray({QStringLiteral("openid"), QStringLiteral("profile")}));
    QCOMPARE(manifest.value(QStringLiteral("capabilities")).toArray(),
             QJsonArray({QStringLiteral("local_ai")}));
}

void MeoAccountBackendTest::normalizesAttestedAiIconPackRequests()
{
    const QString firstHash(64, QLatin1Char('a'));
    const QString secondHash(64, QLatin1Char('b'));
    const QVariantMap submitted{
        {QStringLiteral("schema"), QStringLiteral("org.meo.ai-icon-pack-request/v1")},
        {QStringLiteral("contractVersion"), 1},
        {QStringLiteral("styleId"), QStringLiteral("watercolor")},
        {QStringLiteral("shape"), QStringLiteral("PIXEL")},
        {QStringLiteral("items"), QVariantList{
            QVariantMap{{QStringLiteral("desktopId"), QStringLiteral("zebra.desktop")},
                        {QStringLiteral("sourceIconHash"), secondHash},
                        {QStringLiteral("name"), QStringLiteral("Zebra")},
                        {QStringLiteral("prompt"), QStringLiteral("do not send this")}},
            QVariantMap{{QStringLiteral("desktopId"), QStringLiteral("alpha.desktop")},
                        {QStringLiteral("sourceIconHash"), firstHash},
                        {QStringLiteral("imageSource"), QStringLiteral("data:image/png;base64,no")}},
        }},
        {QStringLiteral("providerPrompt"), QStringLiteral("never forward this")},
    };

    const QVariantMap normalized = MeoAccountContract::normalizedAiIconPackRequest(submitted);
    QCOMPARE(normalized.value(QStringLiteral("schema")).toString(),
             QStringLiteral("org.meo.ai-icon-pack-request/v1"));
    QCOMPARE(normalized.value(QStringLiteral("contractVersion")).toInt(), 1);
    QCOMPARE(normalized.value(QStringLiteral("styleId")).toString(), QStringLiteral("watercolor"));
    QCOMPARE(normalized.value(QStringLiteral("shape")).toString(), QStringLiteral("pixel"));
    QVERIFY(!normalized.contains(QStringLiteral("providerPrompt")));

    const QVariantList items = normalized.value(QStringLiteral("items")).toList();
    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("desktopId")).toString(),
             QStringLiteral("alpha.desktop"));
    QCOMPARE(items.at(0).toMap().value(QStringLiteral("sourceIconHash")).toString(), firstHash);
    QVERIFY(!items.at(0).toMap().contains(QStringLiteral("imageSource")));
    QCOMPARE(items.at(1).toMap().value(QStringLiteral("desktopId")).toString(),
             QStringLiteral("zebra.desktop"));
    QVERIFY(!items.at(1).toMap().contains(QStringLiteral("name")));
    QVERIFY(!items.at(1).toMap().contains(QStringLiteral("prompt")));
}

void MeoAccountBackendTest::rejectsUntrustedAiIconPackRequests()
{
    const QString hash(64, QLatin1Char('c'));
    const QVariantMap valid{
        {QStringLiteral("schema"), QStringLiteral("org.meo.ai-icon-pack-request/v1")},
        {QStringLiteral("contractVersion"), 1},
        {QStringLiteral("styleId"), QStringLiteral("paper")},
        {QStringLiteral("shape"), QStringLiteral("circle")},
        {QStringLiteral("items"), QVariantList{QVariantMap{
            {QStringLiteral("desktopId"), QStringLiteral("org.meo.settings.desktop")},
            {QStringLiteral("sourceIconHash"), hash},
        }}},
    };
    QVERIFY(!MeoAccountContract::normalizedAiIconPackRequest(valid).isEmpty());

    QVariantMap duplicate = valid;
    duplicate.insert(QStringLiteral("items"), QVariantList{
        valid.value(QStringLiteral("items")).toList().first(),
        valid.value(QStringLiteral("items")).toList().first(),
    });
    QVERIFY(MeoAccountContract::normalizedAiIconPackRequest(duplicate).isEmpty());

    QVariantMap unsafeId = valid;
    unsafeId.insert(QStringLiteral("items"), QVariantList{QVariantMap{
        {QStringLiteral("desktopId"), QStringLiteral("../../outside.desktop")},
        {QStringLiteral("sourceIconHash"), hash},
    }});
    QVERIFY(MeoAccountContract::normalizedAiIconPackRequest(unsafeId).isEmpty());

    QVariantMap upperHash = valid;
    upperHash.insert(QStringLiteral("items"), QVariantList{QVariantMap{
        {QStringLiteral("desktopId"), QStringLiteral("org.meo.settings.desktop")},
        {QStringLiteral("sourceIconHash"), hash.toUpper()},
    }});
    QVERIFY(MeoAccountContract::normalizedAiIconPackRequest(upperHash).isEmpty());
}

QTEST_GUILESS_MAIN(MeoAccountBackendTest)

#include "tst_meoaccountbackend.moc"
