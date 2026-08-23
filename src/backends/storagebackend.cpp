#include "storagebackend.h"

#include <Solid/Device>
#include <Solid/StorageDrive>

#include <QtConcurrentRun>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QHash>
#include <QStandardPaths>

#include <algorithm>
#include <limits>

namespace
{
struct SolidMountInfo
{
    QString displayName;
    bool removable = false;
    bool hotpluggable = false;
};

bool isPseudoFilesystem(const QString &filesystem)
{
    // QStorageInfo reports every mount namespace entry.  Kernel, memory-backed,
    // and sandbox filesystems are not user-manageable disks, so do not present
    // them as storage volumes.
    static const QSet<QString> pseudoFilesystems{
        QStringLiteral("proc"),       QStringLiteral("sysfs"),      QStringLiteral("devtmpfs"),
        QStringLiteral("devpts"),     QStringLiteral("cgroup"),     QStringLiteral("cgroup2"),
        QStringLiteral("pstore"),     QStringLiteral("securityfs"), QStringLiteral("debugfs"),
        QStringLiteral("tracefs"),    QStringLiteral("configfs"),   QStringLiteral("fusectl"),
        QStringLiteral("bpf"),        QStringLiteral("mqueue"),     QStringLiteral("hugetlbfs"),
        QStringLiteral("autofs"),     QStringLiteral("efivarfs"),   QStringLiteral("binfmt_misc"),
        QStringLiteral("rpc_pipefs"), QStringLiteral("tmpfs"),      QStringLiteral("ramfs"),
        QStringLiteral("overlay"),    QStringLiteral("fuse"),       QStringLiteral("fuseblk"),
    };
    const QString normalized = filesystem.toLower();
    // Per-application browser-profile overlays, GVFS portals, and other FUSE
    // runtime mounts are not physical volumes. Showing them alongside disks
    // creates duplicate “root” entries and encourages the wrong cleanup
    // mental model; network/share UIs need their own explicit owner.
    return pseudoFilesystems.contains(normalized) || normalized.startsWith(QStringLiteral("fuse."));
}

SolidMountInfo solidInfoForMount(const QString &mountPoint)
{
    const Solid::Device mountDevice = Solid::Device::storageAccessFromPath(mountPoint);
    SolidMountInfo result;
    if (!mountDevice.isValid())
    {
        return result;
    }

    result.displayName = mountDevice.displayName();
    for (Solid::Device device = mountDevice; device.isValid();)
    {
        if (const auto *drive = device.as<const Solid::StorageDrive>())
        {
            result.removable = drive->isRemovable();
            result.hotpluggable = drive->isHotpluggable();
            break;
        }

        const Solid::Device parent = device.parent();
        if (!parent.isValid() || parent.udi() == device.udi())
        {
            break;
        }
        device = parent;
    }
    return result;
}

QString displayNameFor(const QStorageInfo &storage, const SolidMountInfo &solidInfo)
{
    if (!solidInfo.displayName.isEmpty())
    {
        return solidInfo.displayName;
    }
    if (!storage.displayName().isEmpty())
    {
        return storage.displayName();
    }
    if (!storage.device().isEmpty())
    {
        return QString::fromLocal8Bit(storage.device());
    }
    return storage.rootPath();
}

QString canonicalDeviceName(const QByteArray &device)
{
    QString value = QString::fromLocal8Bit(device);
    // Btrfs reports the mounted subvolume as `/dev/nvme…[/@home]`. For an
    // overview these are one physical filesystem; retain their mount paths in
    // the detail map instead of rendering a visually duplicated disk per
    // subvolume.
    const int subvolumeMarker = value.indexOf(QLatin1Char('['));
    if (subvolumeMarker >= 0) {
        value.truncate(subvolumeMarker);
    }
    return value;
}

int mountDepth(const QString &mountPoint)
{
    if (mountPoint == QStringLiteral("/")) {
        return 0;
    }
    return mountPoint.count(QLatin1Char('/'));
}

bool isMoreRepresentativeMount(const QVariantMap &candidate, const QVariantMap &current)
{
    const QString candidateMount = candidate.value(QStringLiteral("mountPoint")).toString();
    const QString currentMount = current.value(QStringLiteral("mountPoint")).toString();
    if (candidateMount == QStringLiteral("/")) {
        return currentMount != QStringLiteral("/");
    }
    if (currentMount == QStringLiteral("/")) {
        return false;
    }
    return mountDepth(candidateMount) < mountDepth(currentMount);
}

bool hasSafePathPrefix(const QString &path, const QString &prefix)
{
    if (path == prefix) {
        return true;
    }
    return path.startsWith(prefix + QLatin1Char('/'));
}

bool checkedCancellation(const std::atomic_bool *cancellation)
{
    return cancellation && cancellation->load(std::memory_order_relaxed);
}

QSet<QString> suffixesForCategory(const QString &categoryId)
{
    if (categoryId == QLatin1String("images")) {
        return {QStringLiteral("avif"), QStringLiteral("bmp"), QStringLiteral("gif"),
                QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("ico"),
                QStringLiteral("jpeg"), QStringLiteral("jpg"), QStringLiteral("jxl"),
                QStringLiteral("png"), QStringLiteral("raw"), QStringLiteral("svg"),
                QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("webp"),
                QStringLiteral("xcf")};
    }
    if (categoryId == QLatin1String("videos")) {
        return {QStringLiteral("3gp"), QStringLiteral("avi"), QStringLiteral("flv"),
                QStringLiteral("m2ts"), QStringLiteral("m4v"), QStringLiteral("mkv"),
                QStringLiteral("mov"), QStringLiteral("mp4"), QStringLiteral("mpeg"),
                QStringLiteral("mpg"), QStringLiteral("mts"), QStringLiteral("ogv"),
                QStringLiteral("webm"), QStringLiteral("wmv")};
    }
    if (categoryId == QLatin1String("documents")) {
        return {QStringLiteral("azw"), QStringLiteral("azw3"), QStringLiteral("csv"),
                QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("epub"),
                QStringLiteral("md"), QStringLiteral("odg"), QStringLiteral("odp"),
                QStringLiteral("ods"), QStringLiteral("odt"), QStringLiteral("pdf"),
                QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("rtf"),
                QStringLiteral("tex"), QStringLiteral("txt"), QStringLiteral("xls"),
                QStringLiteral("xlsx")};
    }
    if (categoryId == QLatin1String("audio")) {
        return {QStringLiteral("aac"), QStringLiteral("aiff"), QStringLiteral("alac"),
                QStringLiteral("flac"), QStringLiteral("m4a"), QStringLiteral("mp3"),
                QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
                QStringLiteral("wav"), QStringLiteral("wma")};
    }
    return {};
}

QVariantMap unscannedCategory(const StorageUsageScanTarget &target)
{
    return {
        {QStringLiteral("id"), target.id},
        {QStringLiteral("label"), target.label},
        {QStringLiteral("bytes"), QVariant::fromValue<qulonglong>(0)},
        {QStringLiteral("fileCount"), 0},
        {QStringLiteral("state"), QStringLiteral("not-scanned")},
        {QStringLiteral("candidateRootCount"), target.candidateRoots.size()},
        {QStringLiteral("scannedRootCount"), 0},
        {QStringLiteral("missingRootCount"), 0},
        {QStringLiteral("unreadableRootCount"), 0},
        {QStringLiteral("unsafeRootCount"), 0},
        {QStringLiteral("skippedSymlinkCount"), 0},
        {QStringLiteral("unreadableEntryCount"), 0},
        {QStringLiteral("visitedEntryCount"), 0},
        {QStringLiteral("roots"), QStringList{}},
    };
}
}  // namespace

bool StorageUsageContract::isRecognizedFileForCategory(const QString &categoryId,
                                                        const QString &fileName)
{
    if (categoryId == QLatin1String("ai")) {
        // The AI category consists exclusively of named model/cache roots. Do
        // not infer that a random large file elsewhere is an AI model.
        return true;
    }
    const QSet<QString> suffixes = suffixesForCategory(categoryId);
    if (suffixes.isEmpty()) {
        return false;
    }
    return suffixes.contains(QFileInfo(fileName).suffix().toCaseFolded());
}

bool StorageUsageContract::isSafeRoot(const QString &candidateRoot,
                                      const QString &canonicalHomePath)
{
    if (candidateRoot.isEmpty() || canonicalHomePath.isEmpty()) {
        return false;
    }

    const QString home = QDir::cleanPath(QDir::fromNativeSeparators(canonicalHomePath));
    const QString absoluteRoot = QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(candidateRoot).absoluteFilePath()));
    // A user-configured standard directory must remain below the actual home
    // directory. In particular, never accept $HOME itself as a "category" —
    // that would accidentally become an unbounded whole-home scan.
    if (!hasSafePathPrefix(absoluteRoot, home) || absoluteRoot == home) {
        return false;
    }

    QString inspectedPath = home;
    const QString relative = QDir(home).relativeFilePath(absoluteRoot);
    for (const QString &segment : relative.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        inspectedPath = QDir(inspectedPath).filePath(segment);
        if (QFileInfo(inspectedPath).isSymLink()) {
            return false;
        }
    }

    const QFileInfo rootInfo(absoluteRoot);
    if (rootInfo.isSymLink() || !rootInfo.exists()) {
        return false;
    }
    const QString canonicalRoot = rootInfo.canonicalFilePath();
    return !canonicalRoot.isEmpty() && hasSafePathPrefix(canonicalRoot, home)
           && canonicalRoot != home;
}

StorageUsageScanSnapshot StorageUsageContract::scan(const QList<StorageUsageScanTarget> &targets,
                                                     const QString &canonicalHomePath,
                                                     const std::atomic_bool *cancellation,
                                                     int maximumEntries)
{
    StorageUsageScanSnapshot snapshot;
    const QString home = QDir::cleanPath(QDir::fromNativeSeparators(canonicalHomePath));
    const int entryLimit = std::max(1, maximumEntries);
    bool stopped = false;

    for (const StorageUsageScanTarget &target : targets) {
        if (stopped || checkedCancellation(cancellation)) {
            snapshot.canceled = snapshot.canceled || checkedCancellation(cancellation);
            snapshot.categories.push_back(unscannedCategory(target));
            continue;
        }

        QVariantMap category = unscannedCategory(target);
        QStringList scannedRoots;
        QSet<QString> seenRoots;
        qulonglong bytes = 0;
        int matchedFiles = 0;
        int visitedEntries = 0;
        int skippedSymlinks = 0;
        int unreadableEntries = 0;
        int missingRoots = 0;
        int unreadableRoots = 0;
        int unsafeRoots = 0;
        int scannedRootCount = 0;
        bool categoryStopped = false;

        for (const QString &candidate : target.candidateRoots) {
            if (checkedCancellation(cancellation)) {
                snapshot.canceled = true;
                categoryStopped = true;
                break;
            }

            const QFileInfo rootInfo(candidate);
            if (rootInfo.isSymLink()) {
                ++unsafeRoots;
                ++snapshot.unsafeRoots;
                continue;
            }
            if (!rootInfo.exists()) {
                ++missingRoots;
                continue;
            }
            if (!rootInfo.isDir() || !isSafeRoot(candidate, home)) {
                ++unsafeRoots;
                ++snapshot.unsafeRoots;
                continue;
            }

            const QString canonicalRoot = rootInfo.canonicalFilePath();
            if (seenRoots.contains(canonicalRoot)) {
                continue;
            }
            seenRoots.insert(canonicalRoot);
            if (!rootInfo.isReadable()) {
                ++unreadableRoots;
                continue;
            }

            ++scannedRootCount;
            scannedRoots.push_back(canonicalRoot);
            QDirIterator iterator(canonicalRoot,
                                  QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                if (checkedCancellation(cancellation)) {
                    snapshot.canceled = true;
                    categoryStopped = true;
                    break;
                }
                if (snapshot.visitedEntries >= entryLimit) {
                    snapshot.limitReached = true;
                    categoryStopped = true;
                    break;
                }

                iterator.next();
                ++snapshot.visitedEntries;
                ++visitedEntries;
                const QFileInfo entry = iterator.fileInfo();
                if (entry.isSymLink()) {
                    ++skippedSymlinks;
                    ++snapshot.skippedSymlinkEntries;
                    continue;
                }
                if (entry.isDir()) {
                    if (!entry.isReadable()) {
                        ++unreadableEntries;
                        ++snapshot.unreadableEntries;
                    }
                    continue;
                }
                if (!entry.isFile() || !isRecognizedFileForCategory(target.id, entry.fileName())) {
                    continue;
                }

                const qint64 fileSize = entry.size();
                if (fileSize < 0) {
                    ++unreadableEntries;
                    ++snapshot.unreadableEntries;
                    continue;
                }
                const qulonglong size = static_cast<qulonglong>(fileSize);
                if (size > std::numeric_limits<qulonglong>::max() - bytes) {
                    // Never wrap a storage total. The category remains partial
                    // and QML is told not to present it as a complete value.
                    snapshot.limitReached = true;
                    categoryStopped = true;
                    break;
                }
                bytes += size;
                ++matchedFiles;
            }

            if (categoryStopped) {
                break;
            }
        }

        QString state;
        if (snapshot.canceled) {
            state = QStringLiteral("canceled");
        } else if (snapshot.limitReached || categoryStopped
                   || (scannedRootCount > 0 && (unreadableRoots > 0 || unsafeRoots > 0))) {
            state = QStringLiteral("partial");
        } else if (scannedRootCount > 0) {
            state = QStringLiteral("complete");
        } else if (unreadableRoots > 0) {
            state = QStringLiteral("permission-denied");
        } else if (unsafeRoots > 0) {
            state = QStringLiteral("unsafe-root");
        } else {
            state = QStringLiteral("not-present");
        }

        category.insert(QStringLiteral("bytes"), QVariant::fromValue(bytes));
        category.insert(QStringLiteral("fileCount"), matchedFiles);
        category.insert(QStringLiteral("state"), state);
        category.insert(QStringLiteral("scannedRootCount"), scannedRootCount);
        category.insert(QStringLiteral("missingRootCount"), missingRoots);
        category.insert(QStringLiteral("unreadableRootCount"), unreadableRoots);
        category.insert(QStringLiteral("unsafeRootCount"), unsafeRoots);
        category.insert(QStringLiteral("skippedSymlinkCount"), skippedSymlinks);
        category.insert(QStringLiteral("unreadableEntryCount"), unreadableEntries);
        category.insert(QStringLiteral("visitedEntryCount"), visitedEntries);
        category.insert(QStringLiteral("roots"), scannedRoots);
        snapshot.categories.push_back(std::move(category));

        if (snapshot.canceled || snapshot.limitReached) {
            stopped = true;
        }
    }

    return snapshot;
}

StorageBackend::StorageBackend(QObject *parent) : BackendBase(parent)
{
    m_usageScanWatcher = new QFutureWatcher<StorageUsageScanSnapshot>(this);
    connect(m_usageScanWatcher, &QFutureWatcher<StorageUsageScanSnapshot>::finished,
            this, &StorageBackend::finishUsageScan);
    m_usageScanSummary = tr("Run a category scan to inspect selected personal folders.");
    refresh();
}

StorageBackend::~StorageBackend()
{
    if (m_usageScanCancellation) {
        m_usageScanCancellation->store(true, std::memory_order_relaxed);
    }
}

QVariantList StorageBackend::volumes() const
{
    return m_volumes;
}

QString StorageBackend::summary() const
{
    if (m_volumes.isEmpty())
    {
        return tr("No mounted storage volumes");
    }
    // Do not add the capacities together: separate mount points can share one
    // physical disk or Btrfs filesystem, which would make a total misleading.
    return tr("%n mounted storage volume(s)", "", m_volumes.size());
}

int StorageBackend::mountedVolumeCount() const
{
    return m_volumes.size();
}

QVariantList StorageBackend::categoryUsage() const
{
    return m_categoryUsage;
}

bool StorageBackend::usageScanActive() const
{
    return m_usageScanActive;
}

QString StorageBackend::usageScanSummary() const
{
    return m_usageScanSummary;
}

QString StorageBackend::usageScanError() const
{
    return m_usageScanError;
}

QString StorageBackend::usageScanUpdatedAt() const
{
    return m_usageScanUpdatedAt;
}

void StorageBackend::refresh()
{
    clearError();

    QHash<QString, QVariantMap> volumesByFilesystem;
    QHash<QString, QStringList> mountPointsByFilesystem;
    QSet<QString> seenMounts;
    const auto mountedVolumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &storage : mountedVolumes)
    {
        if (!storage.isValid() || !storage.isReady() || storage.rootPath().isEmpty())
        {
            continue;
        }

        const QString filesystem = QString::fromLatin1(storage.fileSystemType());
        if (isPseudoFilesystem(filesystem))
        {
            continue;
        }

        const QString mountPoint = storage.rootPath();
        const QString rawDevice = QString::fromLocal8Bit(storage.device());
        const QString mountKey = rawDevice + QChar::Null + mountPoint;
        if (seenMounts.contains(mountKey))
        {
            continue;
        }
        seenMounts.insert(mountKey);

        const qint64 total = std::max<qint64>(0, storage.bytesTotal());
        const qint64 physicalFree = std::max<qint64>(0, storage.bytesFree());
        const qint64 available = std::max<qint64>(0, storage.bytesAvailable());
        const qint64 used = std::max<qint64>(0, total - physicalFree);
        const SolidMountInfo solidInfo = solidInfoForMount(mountPoint);

        const QString device = canonicalDeviceName(storage.device());
        const QString filesystemKey = (device.isEmpty() ? mountPoint : device)
            + QChar::Null + filesystem;
        QVariantMap volume{
            {QStringLiteral("id"), filesystemKey},
            {QStringLiteral("displayName"), displayNameFor(storage, solidInfo)},
            {QStringLiteral("device"), device},
            {QStringLiteral("mountPoint"), mountPoint},
            {QStringLiteral("filesystem"), filesystem},
            {QStringLiteral("capacityBytes"), static_cast<qulonglong>(total)},
            {QStringLiteral("totalBytes"), static_cast<qulonglong>(total)},
            {QStringLiteral("usedBytes"), static_cast<qulonglong>(used)},
            // "free" means space usable by the current user; physicalFree is
            // also exposed because filesystem reserves can make it different.
            {QStringLiteral("freeBytes"), static_cast<qulonglong>(available)},
            {QStringLiteral("physicalFreeBytes"), static_cast<qulonglong>(physicalFree)},
            {QStringLiteral("usedPercent"),
             total > 0 ? static_cast<int>((static_cast<qreal>(used) * 100.0) / static_cast<qreal>(total)) : 0},
            {QStringLiteral("readOnly"), storage.isReadOnly()},
            {QStringLiteral("removable"), solidInfo.removable},
            {QStringLiteral("hotpluggable"), solidInfo.hotpluggable},
            {QStringLiteral("external"), solidInfo.removable || solidInfo.hotpluggable},
        };
        mountPointsByFilesystem[filesystemKey].push_back(mountPoint);
        if (!volumesByFilesystem.contains(filesystemKey)
            || isMoreRepresentativeMount(volume, volumesByFilesystem.value(filesystemKey))) {
            volumesByFilesystem.insert(filesystemKey, std::move(volume));
        }
    }

    QVariantList nextVolumes;
    nextVolumes.reserve(volumesByFilesystem.size());
    for (auto it = volumesByFilesystem.cbegin(); it != volumesByFilesystem.cend(); ++it) {
        QVariantMap volume = it.value();
        auto mountPoints = mountPointsByFilesystem.value(it.key());
        std::sort(mountPoints.begin(), mountPoints.end(), [](const QString &left, const QString &right) {
            if (left == QStringLiteral("/")) return true;
            if (right == QStringLiteral("/")) return false;
            return mountDepth(left) < mountDepth(right);
        });
        volume.insert(QStringLiteral("mountPoints"), mountPoints);
        volume.insert(QStringLiteral("mountCount"), mountPoints.size());
        volume.insert(QStringLiteral("additionalMountCount"), std::max(0, static_cast<int>(mountPoints.size()) - 1));
        nextVolumes.push_back(std::move(volume));
    }

    std::sort(nextVolumes.begin(), nextVolumes.end(),
              [](const QVariant &left, const QVariant &right)
              {
                  const QVariantMap a = left.toMap();
                  const QVariantMap b = right.toMap();
                  const QString aMountPoint = a.value(QStringLiteral("mountPoint")).toString();
                  const QString bMountPoint = b.value(QStringLiteral("mountPoint")).toString();
                  if (aMountPoint == QStringLiteral("/"))
                  {
                      return bMountPoint != QStringLiteral("/");
                  }
                  if (bMountPoint == QStringLiteral("/"))
                  {
                      return false;
                  }
                  return aMountPoint.localeAwareCompare(bMountPoint) < 0;
              });

    setAvailable(true);
    if (m_volumes == nextVolumes)
    {
        return;
    }
    m_volumes = std::move(nextVolumes);
    Q_EMIT changed();
}

QList<StorageUsageScanTarget> StorageBackend::usageScanTargets() const
{
    const QString canonicalHome = QFileInfo(QDir::homePath()).canonicalFilePath();
    if (canonicalHome.isEmpty()) {
        return {};
    }

    const auto standardFolderCandidates = [&canonicalHome](const QStandardPaths::StandardLocation location,
                                                            const QString &fallbackDirectory) {
        QStringList candidates;
        const auto appendCandidate = [&candidates](const QString &candidate) {
            if (candidate.isEmpty()) {
                return;
            }
            const QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(candidate));
            if (!candidates.contains(cleaned)) {
                candidates.push_back(cleaned);
            }
        };
        appendCandidate(QStandardPaths::writableLocation(location));
        appendCandidate(QDir(canonicalHome).filePath(fallbackDirectory));
        return candidates;
    };

    const auto underHome = [&canonicalHome](const QString &relativePath) {
        return QDir(canonicalHome).filePath(relativePath);
    };

    return {
        {QStringLiteral("images"), tr("Images"),
         standardFolderCandidates(QStandardPaths::PicturesLocation, QStringLiteral("Pictures"))},
        {QStringLiteral("videos"), tr("Videos"),
         standardFolderCandidates(QStandardPaths::MoviesLocation, QStringLiteral("Videos"))},
        {QStringLiteral("documents"), tr("Documents"),
         standardFolderCandidates(QStandardPaths::DocumentsLocation, QStringLiteral("Documents"))},
        {QStringLiteral("audio"), tr("Audio"),
         standardFolderCandidates(QStandardPaths::MusicLocation, QStringLiteral("Music"))},
        // These are intentionally specific vendor model/cache roots. A file is
        // never called "AI" merely because it is large or has a model-like
        // extension elsewhere in the home directory.
        {QStringLiteral("ai"), tr("Local AI models & caches"),
         {underHome(QStringLiteral(".ollama/models")),
          underHome(QStringLiteral(".cache/huggingface/hub")),
          underHome(QStringLiteral(".cache/lm-studio/models")),
          underHome(QStringLiteral(".local/share/LM Studio/models")),
          underHome(QStringLiteral(".local/share/ollama/models"))}},
    };
}

void StorageBackend::startUsageScan()
{
    if (m_usageScanActive) {
        return;
    }

    const QString canonicalHome = QFileInfo(QDir::homePath()).canonicalFilePath();
    if (canonicalHome.isEmpty() || !QFileInfo(canonicalHome).isDir()) {
        m_usageScanError = tr("The current home directory is unavailable, so category scanning cannot start.");
        m_usageScanSummary = tr("Category scan unavailable");
        Q_EMIT usageChanged();
        return;
    }

    const QList<StorageUsageScanTarget> targets = usageScanTargets();
    if (targets.isEmpty()) {
        m_usageScanError = tr("No safe standard folders are configured for category scanning.");
        m_usageScanSummary = tr("Category scan unavailable");
        Q_EMIT usageChanged();
        return;
    }

    m_usageScanCancellation = std::make_shared<std::atomic_bool>(false);
    m_usageScanActive = true;
    m_usageScanError.clear();
    m_usageScanSummary = tr("Scanning selected personal folders…");
    setBusy(true);
    const auto cancellation = m_usageScanCancellation;
    m_usageScanWatcher->setFuture(QtConcurrent::run([targets, canonicalHome, cancellation]() {
        return StorageUsageContract::scan(targets, canonicalHome, cancellation.get());
    }));
    Q_EMIT usageChanged();
}

void StorageBackend::cancelUsageScan()
{
    if (!m_usageScanActive || !m_usageScanCancellation) {
        return;
    }
    m_usageScanCancellation->store(true, std::memory_order_relaxed);
    m_usageScanSummary = tr("Stopping the category scan…");
    Q_EMIT usageChanged();
}

void StorageBackend::finishUsageScan()
{
    if (!m_usageScanWatcher) {
        return;
    }

    const StorageUsageScanSnapshot snapshot = m_usageScanWatcher->result();
    m_categoryUsage = snapshot.categories;
    m_usageScanActive = false;
    m_usageScanCancellation.reset();
    m_usageScanUpdatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    setBusy(false);

    if (snapshot.canceled) {
        m_usageScanSummary = tr("Scan canceled after %n directory entries; partial results are shown.",
                                "", snapshot.visitedEntries);
    } else if (snapshot.limitReached) {
        m_usageScanSummary = tr("Scan stopped at the %n-entry safety limit; results are partial.",
                                "", StorageUsageContract::defaultMaximumEntries);
    } else if (snapshot.unreadableEntries > 0 || snapshot.unsafeRoots > 0) {
        m_usageScanSummary = tr("%n directory entries checked; protected or unsafe paths were skipped.",
                                "", snapshot.visitedEntries);
    } else {
        m_usageScanSummary = tr("%n directory entries checked in selected personal folders.",
                                "", snapshot.visitedEntries);
    }

    QStringList warnings;
    if (snapshot.unsafeRoots > 0) {
        warnings.push_back(tr("%n configured folder(s) were skipped because they are outside the safe home-folder scope or use symlinks.",
                              "", snapshot.unsafeRoots));
    }
    if (snapshot.unreadableEntries > 0) {
        warnings.push_back(tr("%n protected folder(s) could not be fully read.", "",
                              snapshot.unreadableEntries));
    }
    m_usageScanError = warnings.join(QLatin1Char(' '));
    Q_EMIT usageChanged();
}
