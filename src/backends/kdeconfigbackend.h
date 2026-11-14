#pragma once
#include "../core/backendbase.h"
#include <KConfigWatcher>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

// Data-driven native settings for KDE-owned configuration that has no
// dedicated Meo backend (window behavior, effects, screen edges, search,
// session, ...). Each page is a fixed whitelist of typed rows backed by one
// key in one KConfig file. Reads and writes go through KSharedConfig, which
// honours XDG_CONFIG_HOME, so inside the Meo Desktop session they edit the
// private ~/.config/meo-desktop profile rather than the ordinary Plasma one.
class KdeConfigBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool kwinRunning READ kwinRunning NOTIFY changed)
    Q_PROPERTY(bool systemMonitorAvailable READ systemMonitorAvailable NOTIFY changed)
public:
    explicit KdeConfigBackend(QObject *parent = nullptr);
    int revision() const { return m_revision; }
    bool kwinRunning() const;
    bool systemMonitorAvailable() const;

    Q_INVOKABLE bool hasSchema(const QString &pageId) const;
    // [{title, rows:[{id,label,description,type,value,choices,min,max,step,suffix}]}]
    Q_INVOKABLE QVariantList schema(const QString &pageId) const;
    Q_INVOKABLE void setValue(const QString &pageId, const QString &rowId, const QVariant &value);
    Q_INVOKABLE void resetPage(const QString &pageId);
    Q_INVOKABLE void refresh();
    // Opens Meo System Monitor (single instance; it raises an open window).
    Q_INVOKABLE bool openSystemMonitor();

Q_SIGNALS:
    void changed();

private:
    QVariantList sectionsFor(const QString &pageId) const;
    QVariantMap findRow(const QString &pageId, const QString &rowId) const;
    void runHook(const QString &hooks, const QVariant &value);
    void applyExclusiveEffects(const QVariantMap &row, const QString &selected);

    QList<KConfigWatcher::Ptr> m_watchers;
    int m_revision = 0;
};
