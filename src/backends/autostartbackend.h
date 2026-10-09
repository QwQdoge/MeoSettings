#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QFileSystemWatcher>
#include <QTimer>

class AutostartBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
public:
    explicit AutostartBackend(QObject *parent = nullptr);
    QVariantList entries() const { return m_entries; }
    QVariantList applications() const { return m_applications; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void addApplication(const QString &storageId);
    Q_INVOKABLE void removeUserEntry(const QString &id);
Q_SIGNALS:
    void changed();
private:
    QString sourceFor(const QString &id) const;
    bool writeEntry(const QString &source, const QString &id, bool enabled);
    QVariantList m_entries;
    QVariantList m_applications;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
};
