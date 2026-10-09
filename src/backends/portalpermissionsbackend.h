#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QMap>
#include <QStringList>

using PortalPermissionMap = QMap<QString, QStringList>;
Q_DECLARE_METATYPE(PortalPermissionMap)

class PortalPermissionsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList permissions READ permissions NOTIFY changed)
public:
    explicit PortalPermissionsBackend(QObject *parent = nullptr);
    QVariantList permissions() const { return m_permissions; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setPermission(const QString &resource, const QString &appId, const QString &decision);
Q_SIGNALS:
    void changed();
private:
    QVariantList m_permissions;
    int m_generation = 0;
};
