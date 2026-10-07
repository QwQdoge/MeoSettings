#include "lockscreenconfigstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
QString combinedError(const SessionEntryDocument::ValidationResult &validation)
{
    if (validation.path.isEmpty()) {
        return validation.error;
    }
    return QStringLiteral("%1: %2").arg(validation.path, validation.error);
}
}

LockScreenConfigStore::LockScreenConfigStore(QString filePath)
    : m_filePath(QDir::cleanPath(std::move(filePath)))
{
}

QString LockScreenConfigStore::filePath() const
{
    return m_filePath;
}

QString LockScreenConfigStore::defaultFilePath()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configRoot.isEmpty()) {
        return {};
    }
    return QDir(configRoot).filePath(QStringLiteral("meo/session-entry/lockscreen-v1.json"));
}

LockScreenConfigStore::LoadResult LockScreenConfigStore::load() const
{
    if (m_filePath.isEmpty() || !QFileInfo(m_filePath).isAbsolute()) {
        return {false, false, QObject::tr("Lock-screen configuration path must be absolute."), {}};
    }

    QFile file(m_filePath);
    if (!file.exists()) {
        return {true, false, {}, SessionEntryDocument::defaults(SessionEntryDocument::Scope::LockScreen)};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, true,
                QObject::tr("Cannot read lock-screen configuration: %1").arg(file.errorString()),
                {}};
    }
    if (file.size() > SessionEntryDocument::MaximumSerializedBytes) {
        return {false, true, QObject::tr("Lock-screen configuration is too large."), {}};
    }

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return {false, true,
                QObject::tr("Cannot finish reading lock-screen configuration: %1").arg(file.errorString()),
                {}};
    }

    const auto validation = SessionEntryDocument::parse(
        bytes, SessionEntryDocument::Scope::LockScreen);
    if (!validation.ok) {
        return {false, true, combinedError(validation), {}};
    }
    return {true, true, {}, validation.document};
}

bool LockScreenConfigStore::ensureParentDirectory(QString *error) const
{
    const QFileInfo fileInfo(m_filePath);
    const QString parentPath = fileInfo.absolutePath();
    if (parentPath.isEmpty()) {
        if (error) {
            *error = QObject::tr("Lock-screen configuration has no parent directory.");
        }
        return false;
    }

    const bool existed = QFileInfo::exists(parentPath);
    QDir parent;
    if (!parent.mkpath(parentPath)) {
        if (error) {
            *error = QObject::tr("Cannot create lock-screen configuration directory.");
        }
        return false;
    }

    // The document may contain a city preference and notification privacy
    // choices.  A newly created Meo-owned directory therefore starts private.
    // Existing directories keep their administrator/user-selected permissions.
    if (!existed) {
        QFile::setPermissions(parentPath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    return true;
}

bool LockScreenConfigStore::readSnapshot(Snapshot *snapshot, QString *error) const
{
    snapshot->exists = false;
    snapshot->bytes.clear();

    QFile file(m_filePath);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QObject::tr("Cannot snapshot existing lock-screen configuration: %1")
                         .arg(file.errorString());
        }
        return false;
    }
    if (file.size() > SessionEntryDocument::MaximumSerializedBytes) {
        if (error) {
            *error = QObject::tr("Existing lock-screen configuration is too large to snapshot safely.");
        }
        return false;
    }

    snapshot->bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (error) {
            *error = QObject::tr("Cannot finish snapshotting lock-screen configuration: %1")
                         .arg(file.errorString());
        }
        return false;
    }

    // A rollback target must itself be trustworthy.  Do not save over a
    // malformed file and then claim that it can be safely restored later.
    const auto validation = SessionEntryDocument::parse(
        snapshot->bytes, SessionEntryDocument::Scope::LockScreen);
    if (!validation.ok) {
        if (error) {
            *error = QObject::tr("Existing lock-screen configuration is invalid; reset it before applying a new document. %1")
                         .arg(combinedError(validation));
        }
        return false;
    }

    snapshot->exists = true;
    return true;
}

bool LockScreenConfigStore::writeAtomically(const QByteArray &bytes, QString *error) const
{
    if (bytes.size() > SessionEntryDocument::MaximumSerializedBytes) {
        if (error) {
            *error = QObject::tr("Refusing to write an oversized lock-screen configuration.");
        }
        return false;
    }
    if (!ensureParentDirectory(error)) {
        return false;
    }

    QSaveFile file(m_filePath);
    // Never trade the atomic replacement guarantee for an in-place fallback.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QObject::tr("Cannot open lock-screen configuration for atomic write: %1")
                         .arg(file.errorString());
        }
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    if (file.write(bytes) != bytes.size()) {
        if (error) {
            *error = QObject::tr("Cannot write complete lock-screen configuration: %1")
                         .arg(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = QObject::tr("Cannot commit lock-screen configuration atomically: %1")
                         .arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool LockScreenConfigStore::restoreSnapshot(const Snapshot &snapshot,
                                            const QByteArray &expectedCurrentBytes,
                                            QString *error) const
{
    Snapshot current;
    QString readError;
    if (!readSnapshot(&current, &readError)) {
        if (error) {
            *error = QObject::tr("Post-write verification failed and the current document cannot be read safely; refusing a blind rollback. %1")
                         .arg(readError);
        }
        return false;
    }

    // Another process may legitimately update the file between commit and
    // verification.  Never overwrite that newer state with our older snapshot.
    if (!current.exists || current.bytes != expectedCurrentBytes) {
        if (error) {
            *error = QObject::tr("Lock-screen configuration changed concurrently; refusing to overwrite the newer document during rollback.");
        }
        return false;
    }

    if (snapshot.exists) {
        return writeAtomically(snapshot.bytes, error);
    }

    if (QFile::exists(m_filePath) && !QFile::remove(m_filePath)) {
        if (error) {
            *error = QObject::tr("Cannot remove the newly written lock-screen configuration during rollback.");
        }
        return false;
    }
    return true;
}

bool LockScreenConfigStore::save(const QVariantMap &document, QString *error) const
{
    if (error) {
        error->clear();
    }
    if (m_filePath.isEmpty() || !QFileInfo(m_filePath).isAbsolute()) {
        if (error) {
            *error = QObject::tr("Lock-screen configuration path must be absolute.");
        }
        return false;
    }

    const auto validation = SessionEntryDocument::validate(
        document, SessionEntryDocument::Scope::LockScreen);
    if (!validation.ok) {
        if (error) {
            *error = combinedError(validation);
        }
        return false;
    }

    const QByteArray serialized = SessionEntryDocument::serialize(validation.document);
    if (serialized.size() > SessionEntryDocument::MaximumSerializedBytes) {
        if (error) {
            *error = QObject::tr("Validated lock-screen configuration exceeds the storage limit.");
        }
        return false;
    }

    Snapshot snapshot;
    if (!readSnapshot(&snapshot, error)) {
        return false;
    }
    if (!writeAtomically(serialized, error)) {
        return false;
    }

    const LoadResult verified = load();
    if (verified.ok && verified.exists && verified.document == validation.document) {
        return true;
    }

    QString rollbackError;
    const bool rolledBack = restoreSnapshot(snapshot, serialized, &rollbackError);
    if (error) {
        const QString verificationError = verified.error.isEmpty()
            ? QObject::tr("Persisted lock-screen configuration differs from the validated request.")
            : verified.error;
        *error = rolledBack
            ? QObject::tr("Post-write verification failed and the previous configuration was restored. %1")
                  .arg(verificationError)
            : QObject::tr("Post-write verification failed. %1 Rollback was not completed: %2")
                  .arg(verificationError, rollbackError);
    }
    return false;
}

bool LockScreenConfigStore::resetToDefaults(QString *error) const
{
    if (error) {
        error->clear();
    }
    if (m_filePath.isEmpty() || !QFileInfo(m_filePath).isAbsolute()) {
        if (error) {
            *error = QObject::tr("Lock-screen configuration path must be absolute.");
        }
        return false;
    }
    if (!QFile::exists(m_filePath)) {
        return true;
    }
    if (!QFile::remove(m_filePath)) {
        if (error) {
            *error = QObject::tr("Cannot restore lock-screen defaults by removing the scoped Meo document.");
        }
        return false;
    }
    return true;
}
