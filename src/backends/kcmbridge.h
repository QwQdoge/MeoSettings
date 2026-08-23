#pragma once

#include "../core/backendbase.h"

#include <QHash>
#include <QVariantList>

class KcmBridge final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(bool launcherAvailable READ launcherAvailable NOTIFY changed)
    Q_PROPERTY(bool partitionManagerAvailable READ partitionManagerAvailable NOTIFY changed)
    Q_PROPERTY(QVariantList modules READ modules NOTIFY changed)

public:
    explicit KcmBridge(QObject *parent = nullptr);

    bool launcherAvailable() const;
    bool partitionManagerAvailable() const;
    QVariantList modules() const;

    Q_INVOKABLE bool isAvailable(const QString &pluginId) const;
    Q_INVOKABLE void open(const QString &pluginId);
    // This is a deliberately narrow external-tool escape hatch.  It never
    // passes user-controlled arguments; partition/format operations remain
    // owned by KDE Partition Manager and its own confirmation/recovery flow.
    Q_INVOKABLE void openPartitionManager();
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    QString m_launcher;
    QString m_partitionManager;
    QVariantList m_modules;
    QHash<QString, QVariantMap> m_modulesById;
};
