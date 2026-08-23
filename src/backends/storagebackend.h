#pragma once

#include "../core/backendbase.h"

#include <atomic>
#include <memory>
#include <QFutureWatcher>
#include <QList>
#include <QStringList>
#include <QVariantList>

/**
 * One explicitly allowed personal-data scan target.  The caller supplies only
 * user-facing folders; the scanner still rechecks every candidate before it
 * reads it.
 */
struct StorageUsageScanTarget
{
    QString id;
    QString label;
    QStringList candidateRoots;
};

/** The bounded result returned by the background personal-data scan. */
struct StorageUsageScanSnapshot
{
    QVariantList categories;
    int visitedEntries = 0;
    int skippedSymlinkEntries = 0;
    int unreadableEntries = 0;
    int unsafeRoots = 0;
    bool canceled = false;
    bool limitReached = false;
};

/**
 * Small, deterministic storage-classification contract.
 *
 * This is deliberately not a whole-home or whole-disk scanner.  It accepts
 * named, user-owned roots only, never follows a symlink, and stops after a
 * fixed number of directory entries.  The public contract makes those limits
 * testable without requiring a mounted disk or a package manager.
 */
class StorageUsageContract final
{
public:
    static constexpr int defaultMaximumEntries = 200'000;

    static bool isRecognizedFileForCategory(const QString &categoryId, const QString &fileName);
    static bool isSafeRoot(const QString &candidateRoot, const QString &canonicalHomePath);
    static StorageUsageScanSnapshot scan(const QList<StorageUsageScanTarget> &targets,
                                         const QString &canonicalHomePath,
                                         const std::atomic_bool *cancellation = nullptr,
                                         int maximumEntries = defaultMaximumEntries);
};

/**
 * Exposes a factual, read-only view of mounted filesystems.
 *
 * This backend intentionally has no operations for mounting, unmounting,
 * formatting, partitioning, encrypting, repairing, or otherwise changing a
 * disk.  Those actions require their own explicit owner and recovery model.
 */
class StorageBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(int mountedVolumeCount READ mountedVolumeCount NOTIFY changed)
    Q_PROPERTY(QVariantList categoryUsage READ categoryUsage NOTIFY usageChanged)
    Q_PROPERTY(bool usageScanActive READ usageScanActive NOTIFY usageChanged)
    Q_PROPERTY(QString usageScanSummary READ usageScanSummary NOTIFY usageChanged)
    Q_PROPERTY(QString usageScanError READ usageScanError NOTIFY usageChanged)
    Q_PROPERTY(QString usageScanUpdatedAt READ usageScanUpdatedAt NOTIFY usageChanged)

public:
    explicit StorageBackend(QObject *parent = nullptr);
    ~StorageBackend() override;

    QVariantList volumes() const;
    QString summary() const;
    int mountedVolumeCount() const;
    QVariantList categoryUsage() const;
    bool usageScanActive() const;
    QString usageScanSummary() const;
    QString usageScanError() const;
    QString usageScanUpdatedAt() const;

    /// Re-reads the operating system's current mount information only.
    Q_INVOKABLE void refresh();
    /**
     * Starts an explicit, bounded read-only scan of selected standard folders.
     * It never scans all of $HOME, package databases, or a mounted disk.
     */
    Q_INVOKABLE void startUsageScan();
    /// Requests cancellation. Any visible result is explicitly marked partial.
    Q_INVOKABLE void cancelUsageScan();

Q_SIGNALS:
    void changed();
    void usageChanged();

private:
    QList<StorageUsageScanTarget> usageScanTargets() const;
    void finishUsageScan();

    QVariantList m_volumes;
    QVariantList m_categoryUsage;
    std::shared_ptr<std::atomic_bool> m_usageScanCancellation;
    QFutureWatcher<StorageUsageScanSnapshot> *m_usageScanWatcher = nullptr;
    QString m_usageScanSummary;
    QString m_usageScanError;
    QString m_usageScanUpdatedAt;
    bool m_usageScanActive = false;
};
