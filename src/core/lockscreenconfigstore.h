#pragma once

#include "sessionentrydocument.h"

#include <QByteArray>
#include <QString>
#include <QVariantMap>

// Atomic, user-scoped storage for the validated lock-screen presentation
// document.  This class intentionally cannot write the system login document;
// that path requires the separately authorized system transaction service.
class LockScreenConfigStore final
{
public:
    struct LoadResult {
        bool ok = false;
        bool exists = false;
        QString error;
        QVariantMap document;
    };

    explicit LockScreenConfigStore(QString filePath = defaultFilePath());

    QString filePath() const;

    LoadResult load() const;
    bool save(const QVariantMap &document, QString *error = nullptr) const;
    bool resetToDefaults(QString *error = nullptr) const;

    static QString defaultFilePath();

private:
    struct Snapshot {
        bool exists = false;
        QByteArray bytes;
    };

    bool ensureParentDirectory(QString *error) const;
    bool readSnapshot(Snapshot *snapshot, QString *error) const;
    bool writeAtomically(const QByteArray &bytes, QString *error) const;
    bool restoreSnapshot(const Snapshot &snapshot,
                         const QByteArray &expectedCurrentBytes,
                         QString *error) const;

    QString m_filePath;
};
