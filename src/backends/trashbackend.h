#pragma once

#include "../core/backendbase.h"

#include <QFutureWatcher>
#include <QPair>

// Size of the user's Trash and an Empty Trash action. The size is read from
// the freedesktop.org trash directory ($XDG_DATA_HOME/Trash/files); emptying
// goes through KDE's `ktrash6 --empty` (or `gio trash --empty`), so KIO keeps
// owning trash metadata.
class TrashBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(qulonglong bytes READ bytes NOTIFY changed)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY changed)
    Q_PROPERTY(bool canEmpty READ canEmpty NOTIFY changed)

public:
    explicit TrashBackend(QObject *parent = nullptr);

    qulonglong bytes() const { return m_bytes; }
    int itemCount() const { return m_itemCount; }
    bool canEmpty() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void empty();

    // Exposed for tests: total size and top-level entry count of a directory.
    static QPair<qulonglong, int> measure(const QString &filesDirectory);
    static QString filesDirectory();

Q_SIGNALS:
    void changed();

private:
    QFutureWatcher<QPair<qulonglong, int>> m_watcher;
    qulonglong m_bytes = 0;
    int m_itemCount = 0;
};
