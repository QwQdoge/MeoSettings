#include "../src/backends/storagebackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace
{
bool writeFile(const QString &path, const int byteCount)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(QByteArray(byteCount, 'x')) == byteCount;
}

QVariantMap categoryById(const StorageUsageScanSnapshot &snapshot, const QString &id)
{
    for (const QVariant &value : snapshot.categories) {
        const QVariantMap category = value.toMap();
        if (category.value(QStringLiteral("id")).toString() == id) {
            return category;
        }
    }
    return {};
}
} // namespace

class StorageBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recognizesOnlyDeclaredFileKinds();
    void rejectsWholeHomeOutsideAndSymlinkRoots();
    void scansDeclaredRootsWithoutFollowingOrInventingCategories();
    void stopsAtLimitAndHonorsCancellation();
};

void StorageBackendTest::recognizesOnlyDeclaredFileKinds()
{
    QVERIFY(StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("images"),
                                                               QStringLiteral("photo.JPEG")));
    QVERIFY(StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("videos"),
                                                               QStringLiteral("clip.mkv")));
    QVERIFY(StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("documents"),
                                                               QStringLiteral("plan.odt")));
    QVERIFY(StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("audio"),
                                                               QStringLiteral("track.opus")));
    QVERIFY(StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("ai"),
                                                               QStringLiteral("model.bin")));
    QVERIFY(!StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("images"),
                                                                QStringLiteral("notes.txt")));
    QVERIFY(!StorageUsageContract::isRecognizedFileForCategory(QStringLiteral("unknown"),
                                                                QStringLiteral("anything.bin")));
}

void StorageBackendTest::rejectsWholeHomeOutsideAndSymlinkRoots()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = QDir(temporary.path()).filePath(QStringLiteral("home"));
    const QString pictures = QDir(home).filePath(QStringLiteral("Pictures"));
    const QString outside = QDir(temporary.path()).filePath(QStringLiteral("outside"));
    QVERIFY(QDir().mkpath(pictures));
    QVERIFY(QDir().mkpath(outside));
    const QString canonicalHome = QFileInfo(home).canonicalFilePath();

    QVERIFY(!StorageUsageContract::isSafeRoot(home, canonicalHome));
    QVERIFY(!StorageUsageContract::isSafeRoot(outside, canonicalHome));
    QVERIFY(StorageUsageContract::isSafeRoot(pictures, canonicalHome));

#ifdef Q_OS_UNIX
    const QByteArray target = QFile::encodeName(pictures);
    const QString linkedPictures = QDir(home).filePath(QStringLiteral("LinkedPictures"));
    QVERIFY(::symlink(target.constData(), QFile::encodeName(linkedPictures).constData()) == 0);
    QVERIFY(!StorageUsageContract::isSafeRoot(linkedPictures, canonicalHome));
#endif
}

void StorageBackendTest::scansDeclaredRootsWithoutFollowingOrInventingCategories()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = QDir(temporary.path()).filePath(QStringLiteral("home"));
    const QString pictures = QDir(home).filePath(QStringLiteral("Pictures"));
    const QString documents = QDir(home).filePath(QStringLiteral("Documents"));
    const QString aiModels = QDir(home).filePath(QStringLiteral(".ollama/models"));
    QVERIFY(QDir().mkpath(pictures));
    QVERIFY(QDir().mkpath(documents));
    QVERIFY(QDir().mkpath(aiModels));
    QVERIFY(writeFile(QDir(pictures).filePath(QStringLiteral("photo.jpg")), 17));
    QVERIFY(writeFile(QDir(pictures).filePath(QStringLiteral("notes.txt")), 101));
    QVERIFY(writeFile(QDir(documents).filePath(QStringLiteral("budget.pdf")), 23));
    QVERIFY(writeFile(QDir(aiModels).filePath(QStringLiteral("model-blob")), 31));

#ifdef Q_OS_UNIX
    const QString outsidePhoto = QDir(temporary.path()).filePath(QStringLiteral("outside.jpg"));
    QVERIFY(writeFile(outsidePhoto, 71));
    const QString linkedPhoto = QDir(pictures).filePath(QStringLiteral("linked-outside.jpg"));
    QVERIFY(::symlink(QFile::encodeName(outsidePhoto).constData(),
                      QFile::encodeName(linkedPhoto).constData()) == 0);
#endif

    const StorageUsageScanSnapshot snapshot = StorageUsageContract::scan(
        {{QStringLiteral("images"), QStringLiteral("Images"), {pictures}},
         {QStringLiteral("documents"), QStringLiteral("Documents"), {documents}},
         {QStringLiteral("ai"), QStringLiteral("AI"), {aiModels}}},
        QFileInfo(home).canonicalFilePath());

    QVERIFY(!snapshot.canceled);
    QVERIFY(!snapshot.limitReached);
    const QVariantMap images = categoryById(snapshot, QStringLiteral("images"));
    const QVariantMap docs = categoryById(snapshot, QStringLiteral("documents"));
    const QVariantMap ai = categoryById(snapshot, QStringLiteral("ai"));
    QCOMPARE(images.value(QStringLiteral("state")).toString(), QStringLiteral("complete"));
    QCOMPARE(images.value(QStringLiteral("fileCount")).toInt(), 1);
    QCOMPARE(images.value(QStringLiteral("bytes")).toULongLong(), static_cast<qulonglong>(17));
    QCOMPARE(docs.value(QStringLiteral("bytes")).toULongLong(), static_cast<qulonglong>(23));
    QCOMPARE(ai.value(QStringLiteral("bytes")).toULongLong(), static_cast<qulonglong>(31));
    QCOMPARE(ai.value(QStringLiteral("fileCount")).toInt(), 1);
#ifdef Q_OS_UNIX
    QVERIFY(snapshot.skippedSymlinkEntries >= 1);
#endif
}

void StorageBackendTest::stopsAtLimitAndHonorsCancellation()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = QDir(temporary.path()).filePath(QStringLiteral("home"));
    const QString pictures = QDir(home).filePath(QStringLiteral("Pictures"));
    QVERIFY(QDir().mkpath(pictures));
    QVERIFY(writeFile(QDir(pictures).filePath(QStringLiteral("first.jpg")), 5));
    QVERIFY(writeFile(QDir(pictures).filePath(QStringLiteral("second.jpg")), 7));

    const QList<StorageUsageScanTarget> targets{
        {QStringLiteral("images"), QStringLiteral("Images"), {pictures}},
        {QStringLiteral("ai"), QStringLiteral("AI"), {QDir(home).filePath(QStringLiteral(".ollama/models"))}},
    };
    const QString canonicalHome = QFileInfo(home).canonicalFilePath();
    const StorageUsageScanSnapshot limited = StorageUsageContract::scan(targets, canonicalHome, nullptr, 1);
    QVERIFY(limited.limitReached);
    QCOMPARE(categoryById(limited, QStringLiteral("images")).value(QStringLiteral("state")).toString(),
             QStringLiteral("partial"));
    QCOMPARE(categoryById(limited, QStringLiteral("ai")).value(QStringLiteral("state")).toString(),
             QStringLiteral("not-scanned"));

    std::atomic_bool cancelled(true);
    const StorageUsageScanSnapshot canceled = StorageUsageContract::scan(targets, canonicalHome, &cancelled);
    QVERIFY(canceled.canceled);
    QCOMPARE(categoryById(canceled, QStringLiteral("images")).value(QStringLiteral("state")).toString(),
             QStringLiteral("not-scanned"));
}

QTEST_GUILESS_MAIN(StorageBackendTest)

#include "tst_storagebackend.moc"
