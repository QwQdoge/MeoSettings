#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QStringList>

class DefaultAppsBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList roles READ roles NOTIFY changed)
    Q_PROPERTY(QStringList mimeTypes READ mimeTypes NOTIFY changed)
public:
    explicit DefaultAppsBackend(QObject *parent = nullptr);
    QVariantList roles() const { return m_roles; }
    QStringList mimeTypes() const { return m_mimeTypes; }
    Q_INVOKABLE QVariantList applicationsFor(const QString &mime) const;
    Q_INVOKABLE QString preferredApplication(const QString &mime) const;
    Q_INVOKABLE void setPreferredApplication(const QString &mime, const QString &storageId);
    Q_INVOKABLE void refresh();
Q_SIGNALS:
    void changed();
private:
    void ensureMimeTypes();
    bool supportedMime(const QString &mime) const;
    QVariantList m_roles;
    QStringList m_mimeTypes;
};
