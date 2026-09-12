#include "core/qmlimportpolicy.h"

#include <QDir>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>

class QmlImportPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void installedBuildOutranksInheritedOverride();
    void developmentBuildKeepsExplicitRootFirst();
};

void QmlImportPolicyTest::installedBuildOutranksInheritedOverride()
{
    QTemporaryDir staleImportRoot;
    QVERIFY(staleImportRoot.isValid());

    const QByteArray previousImportPath = qgetenv("QML_IMPORT_PATH");
    qputenv("QML_IMPORT_PATH", staleImportRoot.path().toUtf8());

    QQmlEngine engine;
    QVERIFY(engine.importPathList().contains(staleImportRoot.path()));
    MeoQmlImportPolicy::prioritizeMeoUi(engine);
    QCOMPARE(QDir::cleanPath(engine.importPathList().constFirst()),
             MeoQmlImportPolicy::systemImportRoot());

    if (previousImportPath.isNull()) {
        qunsetenv("QML_IMPORT_PATH");
    } else {
        qputenv("QML_IMPORT_PATH", previousImportPath);
    }
}

void QmlImportPolicyTest::developmentBuildKeepsExplicitRootFirst()
{
    QTemporaryDir developmentImportRoot;
    QVERIFY(developmentImportRoot.isValid());

    QQmlEngine engine;
    MeoQmlImportPolicy::prioritizeMeoUi(engine, developmentImportRoot.path());
    QCOMPARE(QDir::cleanPath(engine.importPathList().constFirst()),
             QDir::cleanPath(developmentImportRoot.path()));
}

QTEST_GUILESS_MAIN(QmlImportPolicyTest)

#include "tst_qmlimportpolicy.moc"
