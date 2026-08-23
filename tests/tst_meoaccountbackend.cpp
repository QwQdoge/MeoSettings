#include "../src/backends/meoaccountbackend.h"

#include <QTest>

class MeoAccountBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyAcceptsSafeProfileText();
    void onlyAcceptsHttpsRemoteAvatars();
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

QTEST_GUILESS_MAIN(MeoAccountBackendTest)

#include "tst_meoaccountbackend.moc"
